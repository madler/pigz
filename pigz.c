/* pigz.c -- parallel implementation of gzip
 * Copyright (C) 2007 Mark Adler
 * Version 1.4  4 March 2007  Mark Adler
 */

/*
  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the author be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Mark Adler
  madler@alumni.caltech.edu
 */

/* Version history:
   1.0    17 Jan 2007  First version, pipe only
   1.1    28 Jan 2007  Avoid void * arithmetic (some compilers don't get that)
                       Add note about requiring zlib 1.2.3
                       Allow compression level 0 (no compression)
                       Completely rewrite parallelism -- add a write thread
                       Use deflateSetDictionary() to make use of history
                       Tune argument defaults to best performance on four cores
   1.2.1   1 Feb 2007  Add long command line options, add all gzip options
                       Add debugging options
   1.2.2  19 Feb 2007  Add list (--list) function
                       Process file names on command line, write .gz output
                       Write name and time in gzip header, set output file time
                       Implement all command line options except --recursive
                       Add --keep option to prevent deleting input files
                       Add thread tracing information with -vv used
                       Copy crc32_combine() from zlib (possible thread issue)
   1.3    25 Feb 2007  Implement --recursive
                       Expand help to show all options
                       Show help if no arguments or output piping are provided
                       Process options in GZIP environment variable
                       Add progress indicator to write thread if --verbose
   1.4     4 Mar 2007  Add --independent to facilitate damaged file recovery
                       Reallocate jobs for new --blocksize or --processes
                       Do not delete original if writing to stdout
                       Allow --processes 1, which does no threading
                       Add NOTHREAD define to compile without threads
                       Incorporate license text from zlib in source code
 */

#define VERSION "pigz 1.4\n"

/* To-do:
    - add --rsyncable (or -R) [use my own algorithm, set min block size]
    - incorporate gun into pigz
    - list (-l) LZW files
    - optionally have list (-l) decompress entire input to find it all (if -d)
 */

/*
   pigz compresses from stdin to stdout using threads to make use of multiple
   processors and cores.  The input is broken up into 128 KB chunks, and each
   is compressed separately.  The CRC for each chunk is also calculated
   separately.  The compressed chunks are written in order to the output, and
   the overall CRC is calculated from the CRC's of the chunks.

   The compressed data format generated is the gzip format using the deflate
   compression method.  First a gzip header is written, followed by raw deflate
   partial streams.  They are partial, in that they do not have a terminating
   block.  At the end, the deflate stream is terminated with a final empty
   static block, and lastly a gzip trailer is written with the CRC and the
   number of input bytes.

   Each raw deflate partial stream is terminated by an empty stored block
   (using the Z_SYNC_FLUSH option of zlib), in order to end that partial
   bit stream at a byte boundary.  That allows the partial streams to be
   concantenated simply as sequences of bytes.  This adds a very small four
   or five byte overhead to the output for each input chunk.

   zlib's crc32_combine() routine allows the calcuation of the CRC of the
   entire input using the independent CRC's of the chunks.  pigz requires zlib
   version 1.2.3 or later, since that is the first version that provides the
   crc32_combine() function. [Note: in this version, the crc32_combine() code
   has been copied into this source file.  You still need zlib 1.2.1 or later
   to allow dictionary setting with raw deflate.]

   pigz uses the POSIX pthread library for thread control and communication.
 */

/* add a dash of portability */
#define _GNU_SOURCE
#define _POSIX_PTHREAD_SEMANTICS
#define _LARGEFILE64_SUPPORT
#define _FILE_OFFSET_BITS 64

/* included headers and what is used from them */
#include <stdio.h>      /* fflush(), fprintf(), fputs(), getchar(), putc(), */
                        /* puts(), printf(), vasprintf(), stderr, EOF, NULL,
                           SEEK_END, size_t, off_t */
#include <stdlib.h>     /* exit(), malloc(), free(), realloc(), atol(), */
                        /* atoi(), getenv() */
#include <stdarg.h>     /* va_start(), va_end(), va_list */
#include <string.h>     /* memset(), memchr(), memcpy(), strcmp(), */
                        /* strcpy(), strncpy(), strlen(), strcat() */
#include <errno.h>      /* errno, EEXIST */
#include <time.h>       /* ctime(), ctime_r(), time(), time_t */
#include <signal.h>     /* signal(), SIGINT */
#ifndef NOTHREAD
#include <pthread.h>    /* pthread_attr_destroy(), pthread_attr_init(), */
                        /* pthread_attr_setdetachstate(),
                           pthread_attr_setstacksize(), pthread_attr_t,
                           pthread_cond_destroy(), pthread_cond_init(),
                           pthread_cond_signal(), pthread_cond_wait(),
                           pthread_create(), pthread_join(),
                           pthread_mutex_destroy(), pthread_mutex_init(),
                           pthread_mutex_lock(), pthread_mutex_t,
                           pthread_mutex_unlock(), pthread_t,
                           PTHREAD_CREATE_JOINABLE */
#endif
#include <sys/types.h>  /* ssize_t */
#include <sys/stat.h>   /* fstat(), lstat(), struct stat, S_IFDIR, S_IFLNK, */
                        /* S_IFMT, S_IFREG */
#include <sys/time.h>   /* utimes(), gettimeofday(), struct timeval */
#include <unistd.h>     /* unlink(), _exit(), read(), write(), close(), */
                        /* lseek(), isatty() */
#include <fcntl.h>      /* open(), O_CREAT, O_EXCL, O_RDONLY, O_TRUNC, */
                        /* O_WRONLY */
#include <dirent.h>     /* opendir(), readdir(), closedir(), DIR, */
                        /* struct dirent */
#include <limits.h>     /* PATH_MAX */
#include "zlib.h"       /* deflateInit2(), deflateReset(), deflate(), */
                        /* deflateEnd(), deflateSetDictionary(), crc32(),
                           Z_DEFAULT_COMPRESSION, Z_DEFAULT_STRATEGY,
                           Z_DEFLATED, Z_NO_FLUSH, Z_NULL, Z_OK,
                           Z_SYNC_FLUSH, z_stream */

/* for local functions and globals */
#define local static

/* prevent end-of-line conversions on MSDOSish operating systems */
#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <io.h>       /* setmode(), O_BINARY */
#  define SET_BINARY_MODE(fd) setmode(fd, O_BINARY)
#else
#  define SET_BINARY_MODE(fd)
#endif

#ifndef NOTHREAD

/* combine two crc's (copied from zlib 1.2.3) */

local unsigned long gf2_matrix_times(unsigned long *mat, unsigned long vec)
{
    unsigned long sum;

    sum = 0;
    while (vec) {
        if (vec & 1)
            sum ^= *mat;
        vec >>= 1;
        mat++;
    }
    return sum;
}

local void gf2_matrix_square(unsigned long *square, unsigned long *mat)
{
    int n;

    for (n = 0; n < 32; n++)
        square[n] = gf2_matrix_times(mat, mat[n]);
}

local unsigned long crc32_comb(unsigned long crc1, unsigned long crc2,
                               size_t len2)
{
    int n;
    unsigned long row;
    unsigned long even[32];     /* even-power-of-two zeros operator */
    unsigned long odd[32];      /* odd-power-of-two zeros operator */

    /* degenerate case */
    if (len2 == 0)
        return crc1;

    /* put operator for one zero bit in odd */
    odd[0] = 0xedb88320UL;          /* CRC-32 polynomial */
    row = 1;
    for (n = 1; n < 32; n++) {
        odd[n] = row;
        row <<= 1;
    }

    /* put operator for two zero bits in even */
    gf2_matrix_square(even, odd);

    /* put operator for four zero bits in odd */
    gf2_matrix_square(odd, even);

    /* apply len2 zeros to crc1 (first square will put the operator for one
       zero byte, eight zero bits, in even) */
    do {
        /* apply zeros operator for this bit of len2 */
        gf2_matrix_square(even, odd);
        if (len2 & 1)
            crc1 = gf2_matrix_times(even, crc1);
        len2 >>= 1;

        /* if no more bits set, then done */
        if (len2 == 0)
            break;

        /* another iteration of the loop with odd and even swapped */
        gf2_matrix_square(odd, even);
        if (len2 & 1)
            crc1 = gf2_matrix_times(odd, crc1);
        len2 >>= 1;

        /* if no more bits set, then done */
    } while (len2 != 0);

    /* return combined crc */
    crc1 ^= crc2;
    return crc1;
}

#endif

/* read-only globals (set by main/read thread before others started) */
local int ind;              /* input file descriptor */
local int outd;             /* output file descriptor */
local char in[PATH_MAX+1];  /* input file name (accommodate recursion) */
local char *out;            /* output file name */
local int verbosity;        /* 0 = quiet, 1 = normal, 2 = verbose, 3 = trace */
local int headis;           /* 1 to store name, 2 to store date, 3 both */
local int pipeout;          /* write output to stdout even if file */
local int keep;             /* true to prevent deletion of input file */
local int force;            /* true to overwrite, compress links */
local int recurse;          /* true to dive down into directory structure */
local char *sufx;           /* suffix to use (.gz or user supplied) */
local char *name;           /* name for gzip header */
local time_t mtime;         /* time stamp for gzip header */
local int list;             /* true to list files instead of compress */
local int level;            /* compression level */
local int procs;            /* number of compression threads (>= 2) */
local int dict;             /* true to initialize dictionary in each thread */
local size_t size;          /* uncompressed input size per thread (>= 32K) */
local struct timeval start; /* starting time of day for tracing */

/* exit with error */
local void bail(char *why, char *what)
{
    if (out != NULL)
        unlink(out);
    if (verbosity > 0)
        fprintf(stderr, "pigz abort: %s%s\n", why, what);
    exit(1);
}

/* trace log */
struct log {
    struct timeval when;    /* time of entry */
    char *msg;              /* message */
    struct log *next;       /* next entry */
} *log_head = NULL, **log_tail = &log_head;
#ifndef NOTHREAD
local pthread_mutex_t logex = PTHREAD_MUTEX_INITIALIZER;
#endif

/* maximum log entry length */
#define MAXMSG 256

/* add entry to trace log */
local void log_add(char *fmt, ...)
{
    struct timeval now;
    struct log *me;
    va_list ap;
    char msg[MAXMSG];

    gettimeofday(&now, NULL);
    me = malloc(sizeof(struct log));
    if (me == NULL)
        bail("not enough memory", "");
    me->when = now;
    va_start(ap, fmt);
    vsnprintf(msg, MAXMSG, fmt, ap);
    va_end(ap);
    me->msg = malloc(strlen(msg) + 1);
    if (me->msg == NULL) {
        free(me);
        bail("not enough memory", "");
    }
    strcpy(me->msg, msg);
    me->next = NULL;
#ifndef NOTHREAD
    if (pthread_mutex_lock(&logex))
        bail("mutex_lock error in ", "log_add");
#endif
    *log_tail = me;
    log_tail = &(me->next);
#ifndef NOTHREAD
    pthread_mutex_unlock(&logex);
#endif
}

/* pull entry from trace log and print it, return false if empty */
local int log_show(void)
{
    struct log *me;
    struct timeval diff;

#ifndef NOTHREAD
    if (pthread_mutex_lock(&logex))
        bail("mutex_lock error in ", "log_show");
#endif
    me = log_head;
    if (me != NULL) {
        log_head = me->next;
        if (me->next == NULL)
            log_tail = &log_head;
    }
#ifndef NOTHREAD
    pthread_mutex_unlock(&logex);
#endif
    if (me == NULL)
        return 0;
    diff.tv_usec = me->when.tv_usec - start.tv_usec;
    diff.tv_sec = me->when.tv_sec - start.tv_sec;
    if (diff.tv_usec < 0) {
        diff.tv_usec += 1000000L;
        diff.tv_sec--;
    }
    fprintf(stderr, "trace %ld.%06ld %s\n",
            (long)diff.tv_sec, (long)diff.tv_usec, me->msg);
    fflush(stderr);
    free(me->msg);
    free(me);
    return 1;
}

/* show entries until no more */
local void log_dump(void)
{
    while (log_show())
        ;
}

/* debugging macro */
#define Trace(x) \
    do { \
        if (verbosity > 2) { \
            log_add x; \
        } \
    } while (0)

/* catch termination signal */
void cutshort(int sig)
{
    Trace(("termination by user"));
    if (out != NULL)
        unlink(out);
    log_dump();
    log_dump();
    _exit(1);
}

/* read up to len bytes into buf, repeating read() calls as needed */
local size_t readn(int desc, unsigned char *buf, size_t len)
{
    ssize_t ret;
    size_t got;

    got = 0;
    while (len) {
        ret = read(desc, buf, len);
        if (ret < 0)
            bail("read error on ", in);
        if (ret == 0)
            break;
        buf += ret;
        len -= ret;
        got += ret;
    }
    return got;
}

/* write len bytes, repeating write() calls as needed */
local void writen(int desc, unsigned char *buf, size_t len)
{
    ssize_t ret;

    while (len) {
        ret = write(desc, buf, len);
        if (ret < 1)
            bail("write error on ", out);
        buf += ret;
        len -= ret;
    }
}

#ifndef NOTHREAD

/* a flag variable for communication between two threads */
struct flag {
    int value;              /* value of flag */
    pthread_mutex_t lock;   /* lock for checking and changing flag */
    pthread_cond_t cond;    /* condition for signaling on flag change */
};

/* initialize a flag for use, starting with value val */
local void flag_init(struct flag *me, int val)
{
    me->value = val;
    pthread_mutex_init(&(me->lock), NULL);
    pthread_cond_init(&(me->cond), NULL);
}

/* set the flag to val, signal another process that may be waiting for it */
local void flag_set(struct flag *me, int val)
{
    int ret;

    ret = pthread_mutex_lock(&(me->lock));
    if (ret)
        bail("mutex_lock error in ", "flag_set");
    me->value = val;
    pthread_cond_signal(&(me->cond));
    pthread_mutex_unlock(&(me->lock));
}

/* if it isn't already, wait for some other thread to set the flag to val */
local void flag_wait(struct flag *me, int val)
{
    int ret;

    ret = pthread_mutex_lock(&(me->lock));
    if (ret)
        bail("mutex_lock error in ", "flag_wait");
    while (me->value != val)
        pthread_cond_wait(&(me->cond), &(me->lock));
    pthread_mutex_unlock(&(me->lock));
}

/* if flag is equal to val, wait for some other thread to change it */
local void flag_wait_not(struct flag *me, int val)
{
    int ret;

    ret = pthread_mutex_lock(&(me->lock));
    if (ret)
        bail("mutex_lock error in ", "flag_wait_not");
    while (me->value == val)
        pthread_cond_wait(&(me->cond), &(me->lock));
    pthread_mutex_unlock(&(me->lock));
}

/* clean up the flag when done with it */
local void flag_done(struct flag *me)
{
    pthread_cond_destroy(&(me->cond));
    pthread_mutex_destroy(&(me->lock));
}

/* busy flag values */
#define IDLE 0          /* compress and writing done -- can start compress */
#define COMP 1          /* compress -- input and output buffers in use */
#define WRITE 2         /* compress done, writing output -- can read input */

/* next and previous jobs[] indices */
#define NEXT(n) ((n) == procs - 1 ? 0 : (n) + 1)
#define PREV(n) ((n) == 0 ? procs - 1 : (n) - 1)

#endif

/* work units to feed to compress_thread() -- it is assumed that the out
   buffer is large enough to hold the maximum size len bytes could deflate to,
   plus five bytes for the final sync marker */
local struct work {
    size_t len;                 /* length of input */
    unsigned long crc;          /* crc of input */
    unsigned char *buf;         /* input */
    unsigned char *out;         /* space for output (guaranteed big enough) */
    z_stream strm;              /* pre-initialized z_stream */
#ifndef NOTHREAD
    struct flag busy;           /* busy flag indicating work unit in use */
    pthread_t comp;             /* this compression thread */
#endif
} *jobs = NULL;

/* set up the jobs[] work units -- full initialization of each unit is
   deferred until the unit is actually needed, also make sure that
   size arithmetic does not overflow for here and for job_init() */
local void jobs_new(void)
{
    int n;

    if (jobs != NULL)
        return;
    if (size + (size >> 11) + 10 < (size >> 11) + 10 ||
        (ssize_t)(size + (size >> 11) + 10) < 0 ||
        ((size_t)0 - 1) / procs <= sizeof(struct work) ||
        (jobs = malloc(procs * sizeof(struct work))) == NULL)
        bail("not enough memory", "");
    for (n = 0; n < procs; n++) {
        jobs[n].buf = NULL;
#ifndef NOTHREAD
        flag_init(&(jobs[n].busy), IDLE);
#endif
    }
}

/* one-time initialization of a work unit -- this is where we set the deflate
   compression level and request raw deflate, and also where we set the size
   of the output buffer to guarantee enough space for a worst-case deflate
   ending with a Z_SYNC_FLUSH */
local void job_init(struct work *job)
{
    int ret;                        /* deflateInit2() return value */

    Trace(("-- initializing %d", job - jobs));
    job->buf = malloc(size);
    job->out = malloc(size + (size >> 11) + 10);
    job->strm.zfree = Z_NULL;
    job->strm.zalloc = Z_NULL;
    job->strm.opaque = Z_NULL;
    ret = deflateInit2(&(job->strm), level, Z_DEFLATED, -15, 8,
                       Z_DEFAULT_STRATEGY);
    if (job->buf == NULL || job->out == NULL || ret != Z_OK)
        bail("not enough memory", "");
}

/* release resources used by the job[] work units */
local void jobs_free(void)
{
    int n;

    if (jobs == NULL)
        return;
    for (n = procs - 1; n >= 0; n--) {
        if (jobs[n].buf != NULL) {
            (void)deflateEnd(&(jobs[n].strm));
            free(jobs[n].out);
            free(jobs[n].buf);
        }
#ifndef NOTHREAD
        flag_done(&(jobs[n].busy));
#endif
    }
    free(jobs);
    jobs = NULL;
}

/* sliding dictionary size for deflate */
#define DICT 32768U

/* largest power of 2 that fits in an unsigned int -- used to limit requests
   to zlib functions that use unsigned int lengths */
#define MAX ((((unsigned)-1) >> 1) + 1)

/* put a 4-byte integer into a byte array in LSB order */
#define PUT4(a,b) (*(a)=(b),(a)[1]=(b)>>8,(a)[2]=(b)>>16,(a)[3]=(b)>>24)

/* write a gzip header using the information in the global descriptors */
local void header(void)
{
    unsigned char head[10];

    head[0] = 31;
    head[1] = 139;
    head[2] = 8;                    /* deflate */
    head[3] = name != NULL ? 8 : 0;
    PUT4(head + 4, mtime);
    head[8] = level == 9 ? 2 : (level == 1 ? 4 : 0);
    head[9] = 3;                    /* unix */
    writen(outd, head, 10);
    if (name != NULL)
        writen(outd, (unsigned char *)name, strlen(name) + 1);
}

#ifndef NOTHREAD

/* compress thread: compress the input in the provided work unit and compute
   its crc -- assume that the amount of space at job->out is guaranteed to be
   enough for the compressed output, as determined by the maximum expansion
   of deflate compression -- use the input in the previous work unit (if there
   is one) to set the deflate dictionary for better compression */
local void *compress_thread(void *arg)
{
    size_t len;                     /* input length for this work unit */
    unsigned long crc;              /* crc of input data */
    struct work *prev;              /* previous work unit */
    struct work *job = arg;         /* work unit for this thread */
    z_stream *strm = &(job->strm);  /* zlib stream for this work unit */

    /* reset state for a new compressed stream */
    Trace(("-- compressing %d", job - jobs));
    (void)deflateReset(strm);

    /* initialize input, output, and crc */
    strm->next_in = job->buf;
    strm->next_out = job->out;
    len = job->len;
    crc = crc32(0L, Z_NULL, 0);

    /* set dictionary if this isn't the first work unit, and if we will be
       compressing something (the read thread assures that the dictionary
       data in the previous work unit is still there) */
    prev = jobs + PREV(job - jobs);
    if (dict && prev->buf != NULL && len != 0)
        deflateSetDictionary(strm, prev->buf + (size - DICT), DICT);

    /* run MAX-sized amounts of input through deflate and crc32 -- this loop
       is needed for those cases where the integer type is smaller than the
       size_t type, or when len is close to the limit of the size_t type */
    while (len > MAX) {
        strm->avail_in = MAX;
        strm->avail_out = (unsigned)-1;
        crc = crc32(crc, strm->next_in, strm->avail_in);
        (void)deflate(strm, Z_NO_FLUSH);
        len -= MAX;
    }

    /* run last piece through deflate and crc32, follow with a sync marker */
    if (len) {
        strm->avail_in = len;
        strm->avail_out = (unsigned)-1;
        crc = crc32(crc, strm->next_in, strm->avail_in);
        (void)deflate(strm, Z_SYNC_FLUSH);
    }

    /* don't need to Z_FINISH, since we'd delete the last two bytes anyway */

    /* return result */
    job->crc = crc;
    Trace(("-- compressed %d", job - jobs));
    return NULL;
}

/* write thread: wait for compression threads to complete, write output in
   order, also write gzip header and trailer around the compressed data */
local void *write_thread(void *arg)
{
    int n;                          /* compress thread index */
    size_t len;                     /* length of input processed */
    unsigned long tot;              /* total uncompressed size (overflow ok) */
    unsigned long crc;              /* CRC-32 of uncompressed data */
    unsigned char tail[10];         /* last deflate block and gzip trailer */

    /* build and write gzip header */
    Trace(("-- write thread running"));
    header();

    /* process output of compress threads until end of input */    
    tot = 0;
    crc = crc32(0L, Z_NULL, 0);
    n = 0;
    do {
        /* wait for compress thread to start, then wait to complete */
        flag_wait(&(jobs[n].busy), COMP);
        pthread_join(jobs[n].comp, NULL);

        /* now that compress is done, allow read thread to use input buffer */
        flag_set(&(jobs[n].busy), WRITE);

        /* write compressed data and update length and crc */
        Trace(("-- writing %d", n));
        writen(outd, jobs[n].out, jobs[n].strm.next_out - jobs[n].out);
        len = jobs[n].len;
        tot += len;
        Trace(("-- wrote %d", n));
        crc = crc32_comb(crc, jobs[n].crc, len);

        /* release this work unit and go to the next work unit */
        Trace(("-- releasing %d", n));
        flag_set(&(jobs[n].busy), IDLE);
        n = NEXT(n);
        if (n == 0 && verbosity > 1) {
            putc('.', stderr);
            fflush(stderr);
        }

        /* an input buffer less than size in length indicates end of input */
    } while (len == size);

    /* write final static block and gzip trailer (crc and len mod 2^32) */
    tail[0] = 3;  tail[1] = 0;
    PUT4(tail + 2, crc);
    PUT4(tail + 6, tot);
    writen(outd, tail, 10);
    return NULL;
}

/* compress ind to outd in the gzip format, using multiple threads for the
   compression and crc calculation and another thread for writing the output --
   the read thread is the main thread */
local void read_thread(void)
{
    int n;                          /* general index */
    size_t got;                     /* amount read */
    pthread_attr_t attr;            /* thread attributes (left at defaults) */
    pthread_t write;                /* write thread */

    /* set defaults (not all pthread implementations default to joinable) */
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_attr_setstacksize(&attr, 524288UL);

    /* allocate new or clean up existing work units */
    jobs_new();

    /* start write thread */
    pthread_create(&write, &attr, write_thread, NULL);

    /* read from input and start compress threads (write thread will pick up
       the output of the compress threads) */
    n = 0;
    do {
        /* initialize this work unit if it's the first time it's used */
        if (jobs[n].buf == NULL)
            job_init(jobs + n);

        /* read input data, but wait for last compress on this work unit to be
           done, and wait for the dictionary to be used by the last compress on
           the next work unit */
        flag_wait_not(&(jobs[n].busy), COMP);
        flag_wait_not(&(jobs[NEXT(n)].busy), COMP);
        got = readn(ind, jobs[n].buf, size);
        Trace(("-- have read %d", n));

        /* start compress thread, but wait for write to be done first */
        flag_wait(&(jobs[n].busy), IDLE);
        jobs[n].len = got;
        pthread_create(&(jobs[n].comp), &attr, compress_thread, jobs + n);

        /* mark work unit so write thread knows compress was started */
        flag_set(&(jobs[n].busy), COMP);

        /* go to the next work unit */
        n = NEXT(n);

        /* do until end of input, indicated by a read less than size */
    } while (got == size);

    /* wait for the write thread to complete -- the write thread will join with
       all of the compress threads, so this waits for all of the threads to
       complete */
    pthread_join(write, NULL);
    Trace(("-- all threads joined"));

    /* done -- release attribute */
    pthread_attr_destroy(&attr);
}

#endif

/* do a simple gzip in a single thread from ind to outd */
local void single_gzip(void)
{
    size_t got;                     /* amount read */
    unsigned long tot;              /* total uncompressed size (overflow ok) */
    unsigned long crc;              /* CRC-32 of uncompressed data */
    unsigned char tail[8];          /* gzip trailer */
    z_stream *strm;                 /* convenient pointer */

    /* write gzip header */
    header();

    /* if first time, initialize buffers and deflate */
    jobs_new();
    if (jobs->buf == NULL)
        job_init(jobs);

    /* do raw deflate and calculate crc */
    strm = &(jobs->strm);
    (void)deflateReset(strm);
    tot = 0;
    crc = crc32(0L, Z_NULL, 0);
    do {
        /* read some data to compress */
        got = readn(ind, jobs->buf, size);
        tot += (unsigned long)got;
        strm->next_in = jobs->buf;

        /* compress MAX-size chunks in case unsigned type is small */
        while (got > MAX) {
            strm->avail_in = MAX;
            crc = crc32(crc, strm->next_in, strm->avail_in);
            do {
                strm->avail_out = size;
                strm->next_out = jobs->out;
                (void)deflate(strm, Z_NO_FLUSH);
                writen(outd, jobs->out, size - strm->avail_out);
            } while (strm->avail_out == 0);
            got -= MAX;
        }

        /* compress the remainder, finishing if end of input */
        if (got)
            crc = crc32(crc, strm->next_in, got);
        strm->avail_in = got;
        do {
            strm->avail_out = size;
            strm->next_out = jobs->out;
            (void)deflate(strm, got < size ? Z_FINISH :
                            (dict ? Z_NO_FLUSH : Z_FULL_FLUSH));
            writen(outd, jobs->out, size - strm->avail_out);
        } while (strm->avail_out == 0);

        /* do until read doesn't fill buffer */
    } while (got == size);

    /* write gzip trailer */
    PUT4(tail, crc);
    PUT4(tail + 4, tot);
    writen(outd, tail, 8);
}

/* defines for listgz() */
#define NAMEMAX1 48     /* name display limit at vebosity 1 */
#define NAMEMAX2 16     /* name display limit at vebosity 2 */
#define BUF 16384       /* input buffer size */

/* macros for specialized buffered reading for listgz() */
#define LOAD() (left = readn(ind, next = buf, BUF), tot += left)
#define GET() (left == 0 ? LOAD() : 0, left == 0 ? EOF : (left--, *next++))
#define SKIP(dist) \
    do { \
        size_t togo; \
        togo = (dist); \
        while (togo > left) { \
            togo -= left; \
            LOAD(); \
            if (left == 0) { \
                if (verbosity > 0) \
                    fprintf(stderr, "%s ended prematurely\n", in); \
                return; \
            } \
        } \
        left -= togo; \
    } while (0)
#define PULLSTR(buf, max) \
    do { \
        unsigned char *zer; \
        unsigned char *str = (unsigned char *)(buf); \
        size_t copy, free = (max); \
        if (free && str != NULL) \
            memset(str, 0, free); \
        while ((zer = memchr(next, 0, left)) == NULL) { \
            if (free && str != NULL) { \
                copy = left > free ? free : left; \
                memcpy(str, next, copy); \
                str += copy; \
                free -= copy; \
            } \
            LOAD(); \
            if (left == 0) { \
                if (verbosity > 0) \
                    fprintf(stderr, "%s ended prematurely\n", in); \
                return; \
            } \
        } \
        zer++; \
        if (free && str != NULL) { \
            copy = zer - next; \
            if (copy > free) \
                copy = free; \
            memcpy(str, next, copy); \
        } \
        left -= zer - next; \
        next = zer; \
    } while (0)
#define PULLFIX(buf, len) \
    do { \
        size_t need = (len); \
        while (need > left) { \
            memcpy((buf) + ((len) - need), next, left); \
            need -= left; \
            LOAD(); \
            if (left == 0) { \
                if (verbosity > 0) \
                    fprintf(stderr, "%s ended prematurely\n", in); \
                return; \
            } \
        } \
        memcpy((buf) + ((len) - need), next, need); \
        next += need; \
        left -= need; \
    } while (0)

/* list content information about the gzip file at ind (only works if the gzip
   file contains a single gzip stream with no junk at the end, and only works
   well if the uncompressed length is less than 4 GB) */
local void listgz(void)
{
    int max;                /* maximum name length for current verbosity */
    size_t n;               /* extra field length, available trailer bytes */
    off_t at;               /* used to calculate compressed length */
    static int never = 1;   /* true if we've never been here */

    /* header information */
    unsigned char head[8];
    time_t stamp;
    char mod[27];
    char name[NAMEMAX1 + 1];
    unsigned char tail[8];
    unsigned long crc, len;

    /* locals for buffered reading */
    off_t tot = 0;
    size_t left = 0;
    unsigned char buf[BUF], *next = buf;

    /* see if it's a gzip file (if not, skip silently unless --verbose) */
    if (GET() != 31 || GET() != 139) {
        if (verbosity > 1)
            fprintf(stderr, "%s is not a gzip file\n", in);
        return;
    }

    /* read remainder of fixed-size header, get modification time */
    PULLFIX(head, 8);
    if (head[1] & 0xe0) {
        if (verbosity > 0)
            fprintf(stderr, "%s has unknown gzip extensions\n", in);
        return;
    }
    stamp = head[2] + (head[3] << 8) + (head[4] << 16) + (head[5] << 24);
    if (stamp) {
        ctime_r(&stamp, mod);
        stamp = time(NULL);
        if (strcmp(mod + 20, ctime(&stamp) + 20) != 0)
            strcpy(mod + 11, mod + 19);
    }
    else
        strcpy(mod + 4, "------ -----");
    mod[16] = 0;

    /* skip extra field, if present */
    if (head[1] & 4) {
        n = GET();
        n += GET() << 8;
        SKIP(n);
    }

    /* read file name, if present */
    max = verbosity > 1 ? NAMEMAX2 : NAMEMAX1;
    memset(name, 0, max + 1);
    if (head[1] & 8)            /* get file name (or part of it) */
        PULLSTR(name, NAMEMAX1 + 1);
    else                        /* if not in header, use input name */
        strncpy(name, in, max + 1);
    if (name[max])
        strcpy(name + max - 3, "...");

    /* skip comment and header crc, if present */
    if (head[1] & 16)
        PULLSTR(NULL, 0);
    if (head[1] & 2)
        SKIP(2);

    /* skip to end to get trailer (8 bytes), compute compressed length */
    if (next - buf < BUF - left) {      /* read whole thing already */
        if (left < 8) {
            if (verbosity > 0)
                fprintf(stderr, "%s ended prematurely\n", in);
            return;
        }
        tot = left - 8;                 /* compressed size */
        memcpy(tail, next + (left - 8), 8);
    }
    else if ((at = lseek(ind, -8, SEEK_END)) != -1) {
        tot = at - tot + left;          /* compressed size */
        readn(ind, tail, 8);            /* get trailer */
    }
    else {                              /* can't seek */
        at = tot - left;                /* save header size */
        do {
            n = left < 8 ? left : 8;
            memcpy(tail, next + (left - n), n);
            LOAD();
        } while (left == BUF);          /* read until end */
        if (left < 8) {
            if (n + left < 8) {
                if (verbosity > 0)
                    fprintf(stderr, "%s ended prematurely\n", in);
                return;
            }
            if (left) {
                if (n + left > 8)
                    memcpy(tail, tail + n - (8 - left), 8 - left);
                memcpy(tail + 8 - left, next, left);
            }
        }
        else
            memcpy(tail, next + (left - 8), 8);
        tot -= at + 8;
    }
    if (tot < 2) {
        if (verbosity > 0)
            fprintf(stderr, "%s ended prematurely\n", in);
        return;
    }

    /* convert trailer to crc and uncompressed length (modulo 2^32) */
    crc = tail[0] + (tail[1] << 8) + (tail[2] << 16) + (tail[3] << 24);
    len = tail[4] + (tail[5] << 8) + (tail[6] << 16) + (tail[7] << 24);

    /* list contents */
    if (never) {
        if (verbosity > 1)
            fputs("method    crc      timestamp    ", stdout);
        if (verbosity > 0)
            puts("compressed   original reduced  name");
        never = 0;
    }
    if (verbosity > 1) {
        if (head[0] == 8)
            printf("def%s  %08lx  %s  ",
                head[6] & 2 ? "(9)" : (head[6] & 4 ? "(1)" : "   "),
                crc, mod + 4);
        else
            printf("%6u  %08lx  %s  ", head[0], crc, mod + 4);
    }
    if (verbosity > 0) {
        if (head[0] == 8 && tot > (len + (len >> 10) + 12))
            printf("%10llu %10lu?  unk    %s\n",
                   tot, len, name);
        else
            printf("%10llu %10lu %6.1f%%  %s\n",
                   tot, len,
                   len == 0 ? 0 : 100 * (len - tot)/(double)len,
                   name);
    }
}

/* extract file name from path */
local char *justname(char *path)
{
    char *p;

    p = path + strlen(path);
    while (--p >= path)
        if (*p == '/')
            break;
    return p + 1;
}

/* set the access and modify times of fd to t */
local void touch(char *path, time_t t)
{
    struct timeval times[2];

    times[0].tv_sec = t;
    times[0].tv_usec = 0;
    times[1].tv_sec = t;
    times[1].tv_usec = 0;
    utimes(path, times);
}

/* compress provided input file, or stdin if path is NULL */
local void gzip(char *path)
{
    struct stat st;                 /* to get file type and mod time */

    /* open input file with name in, descriptor ind -- set name and mtime */
    if (path == NULL) {
        strcpy(in, "<stdin>");
        ind = 0;
        name = NULL;
        mtime = headis & 2 ?
                (fstat(ind, &st) ? time(NULL) : st.st_mtime) : 0;
    }
    else {
        /* set input file name (already set if recursed here) */
        if (path != in) {
            strncpy(in, path, sizeof(in));
            if (in[sizeof(in) - 1])
                bail("name too long: ", path);
        }

        /* don't compress .gz files (unless -f) */
        if (!list && strlen(in) >= strlen(sufx) &&
                strcmp(in + strlen(in) - strlen(sufx), sufx) == 0 &&
                !force) {
            if (verbosity > 0)
                fprintf(stderr, "%s ends with %s -- skipping\n", in, sufx);
            return;
        }

        /* only compress regular files, but allow symbolic links if -f,
           recurse into directory if -r */
        if (lstat(in, &st)) {
            if (verbosity > 0)
                fprintf(stderr, "%s does not exist -- skipping\n", in);
            return;
        }
        if ((st.st_mode & S_IFMT) != S_IFREG &&
            (st.st_mode & S_IFMT) != S_IFLNK &&
            (st.st_mode & S_IFMT) != S_IFDIR) {
            if (verbosity > 0)
                fprintf(stderr, "%s is a special file or device -- skipping\n",
                        in);
            return;
        }
        if ((st.st_mode & S_IFMT) == S_IFLNK && !force) {
            if (verbosity > 0)
                fprintf(stderr, "%s is a symbolic link -- skipping\n", in);
            return;
        }
        if ((st.st_mode & S_IFMT) == S_IFDIR && !recurse) {
            if (verbosity > 0)
                fprintf(stderr, "%s is a directory -- skipping\n", in);
            return;
        }

        /* recurse into directory (assumes Unix) */
        if ((st.st_mode & S_IFMT) == S_IFDIR) {
            char *roll, *item, *cut, *base, *bigger;
            size_t len, hold;
            DIR *here;
            struct dirent *next;

            /* accumulate list of entries (need to do this, since readdir()
               behavior not defined if changing the directory between calls) */
            here = opendir(in);
            if (here == NULL)
                return;
            hold = 512;
            roll = malloc(hold);
            if (roll == NULL)
                bail("not enough memory", "");
            *roll = 0;
            item = roll;
            while ((next = readdir(here)) != NULL) {
                if (next->d_name[0] == 0 ||
                    (next->d_name[0] == '.' && (next->d_name[1] == 0 ||
                     (next->d_name[1] == '.' && next->d_name[2] == 0))))
                    continue;
                len = strlen(next->d_name) + 1;
                if (item + len + 1 > roll + hold) {
                    do {                    /* make roll bigger */
                        hold <<= 1;
                    } while (item + len + 1 > roll + hold);
                    bigger = realloc(roll, hold);
                    if (bigger == NULL) {
                        free(roll);
                        bail("not enough memory", "");
                    }
                    item = bigger + (item - roll);
                    roll = bigger;
                }
                strcpy(item, next->d_name);
                item += len;
                *item = 0;
            }
            closedir(here);

            /* run gzip() for each entry in the directory */
            cut = base = in + strlen(in);
            if (base > in && base[-1] != '/') {
                if (base - in >= sizeof(in))
                    bail("path too long", in);
                *base++ = '/';
            }
            item = roll;
            while (*item) {
                strncpy(base, item, sizeof(in) - (base - in));
                if (in[sizeof(in) - 1]) {
                    strcpy(in + (sizeof(in) - 4), "...");
                    bail("path too long: ", in);
                }
                gzip(in);
                item += strlen(item) + 1;
            }
            *cut = 0;

            /* release list of entries */
            free(roll);
            return;
        }

        /* open input file */
        ind = open(in, O_RDONLY, 0);
        if (ind < 0)
            bail("read error on ", in);

        /* prepare gzip header information */
        name = headis & 1 ? justname(in) : NULL;
        mtime = headis & 2 ? st.st_mtime : 0;
    }
    SET_BINARY_MODE(ind);

    /* if requested, just list information about input file */
    if (list) {
        listgz();
        if (ind != 0)
            close(ind);
        return;
    }

    /* compressing -- create output file out, descriptor outd */
    if (path == NULL || pipeout) {
        /* write to stdout */
        out = malloc(strlen("<stdout>") + 1);
        if (out == NULL)
            bail("not enough memory", "");
        strcpy(out, "<stdout>");
        outd = 1;
        if (isatty(outd) && !force)
            bail("trying to write compressed data to a terminal",
                 " (use -f to force)");
    }
    else {
        /* create output name and open to write */
        out = malloc(strlen(in) + strlen(sufx) + 1);
        if (out == NULL)
            bail("not enough memory", "");
        strcpy(out, in);
        strcat(out, sufx);
        outd = open(out, O_CREAT | O_TRUNC | O_WRONLY |
                         (force ? 0 : O_EXCL), 0666);

        /* if exists and not -f, give user a chance to overwrite */
        if (outd < 0 && errno == EEXIST && isatty(0) && verbosity) {
            int ch, reply;

            fprintf(stderr, "%s exists -- overwrite (y/n)? ", out);
            fflush(stderr);
            reply = -1;
            do {
                ch = getchar();
                if (reply < 0 && ch != ' ' && ch != '\t')
                    reply = ch == 'y' || ch == 'Y' ? 1 : 0;
            } while (ch != EOF && ch != '\n' && ch != '\r');
            if (reply == 1)
                outd = open(out, O_CREAT | O_TRUNC | O_WRONLY,
                            0666);
        }

        /* if exists and no overwrite, report and go on to next */
        if (outd < 0 && errno == EEXIST) {
            if (verbosity > 0)
                fprintf(stderr, "%s exists -- skipping\n", out);
            free(out);
            out = NULL;
            return;
        }

        /* if some other error, give up */
        if (outd < 0)
            bail("write error on ", out);
    }
    SET_BINARY_MODE(outd);

    /* compress ind to outd */
    if (verbosity > 1)
        fprintf(stderr, "%s to %s ", in, out);
#ifndef NOTHREAD
    if (procs > 1)
        read_thread();
    else
#endif
        single_gzip();
    if (verbosity > 1) {
        putc('\n', stderr);
        fflush(stderr);
    }

    /* finish up, set times, delete original */
    if (outd != 1 && mtime)
        touch(out, mtime);
    if (outd != 1 && close(outd))
        bail("write error on ", out);
    free(out);
    out = NULL;
    if (ind != 0) {
        close(ind);
        if (outd != 1 && !keep)
            unlink(in);
    }
}

local char *helptext[] = {
"Usage: pigz [options] [files ...]",
"  will compress files in place, adding the suffix '.gz'.  If no files are",
#ifdef NOTHREAD
"  specified, stdin will be compressed to stdout.  pigz does what gzip does.",
#else
"  specified, stdin will be compressed to stdout.  pigz does what gzip does,",
"  but spreads the work over multiple processors and cores when compressing.",
#endif
"",
"Options:",
"  -0 to -9, --fast, --best   Compression levels, --fast is -1, --best is -9",
"  -b, --blocksize mmm  Set compression block size to mmmK (default 128K)",
#ifndef NOTHREAD
"  -p, --processes n    Allow up to n compression threads (default 32)",
#endif
"  -i, --independent    Compress blocks independently for damage recovery",
"  -f, --force          Force overwrite, compressing .gz, links, to terminal",
"  -r, --recursive      Compress (or list) the contents of all subdirectories",
"  -s, --suffix .sss    Use suffix .sss instead of .gz",
"  -k, --keep           Do not delete original file after compression",
"  -c, --stdout         Write all compressed output to stdout (won't delete)",
"  -N, --name           Put file name and time in gzip header (default)",
"  -n, --no-name        Do not put file name or time in gzip header",
"  -T, --no-time        Do not put modification time in gzip header",
"  -l, --list           List contents of input files instead of compressing",
"  -q, --quiet          Print no messages, even on error",
"  -v, --verbose        Provide more verbose output (-vv to debug)"
};

/* display the help text above */
local void help(void)
{
    int n;

    if (verbosity == 0)
        return;
    for (n = 0; n < sizeof(helptext) / sizeof(char *); n++)
        fprintf(stderr, "%s\n", helptext[n]);
    fflush(stderr);
    exit(0);
}

/* set option defaults */
local void defaults(void)
{
    /* 32 processes and 128K buffers were found to provide good utilization of
       four cores (about 97%) and balanced the overall execution time impact of
       more threads against more dictionary processing for a fixed amount of
       memory -- the memory usage for these settings and full use of all work
       units (at least 4 MB of input) is 16.2 MB */
    level = Z_DEFAULT_COMPRESSION;
#ifdef NOTHREAD
    procs = 1;
#else
    procs = 32;
#endif
    size = 131072UL;
    dict = 1;                       /* initialize dictionary each thread */
    verbosity = 1;                  /* normal message level */
    headis = 3;                     /* store name and timestamp */
    pipeout = 0;                    /* don't force output to stdout */
    sufx = ".gz";                   /* compressed file suffix */
    list = 0;                       /* compress */
    keep = 0;                       /* delete input file once compressed */
    force = 0;                      /* don't overwrite, don't compress links */
    recurse = 0;                    /* don't go into directories */
}

/* long options conversion to short options */
local char *longopts[][2] = {
    {"LZW", "Z"}, {"ascii", "a"}, {"best", "9"}, {"bits", "Z"},
    {"blocksize", "b"}, {"decompress", "d"}, {"fast", "1"}, {"force", "f"},
    {"help", "h"}, {"independent", "i"}, {"keep", "k"}, {"license", "L"},
    {"list", "l"}, {"name", "N"}, {"no-name", "n"}, {"no-time", "T"},
    {"processes", "p"}, {"quiet", "q"}, {"recursive", "r"}, {"silent", "q"},
    {"stdout", "c"}, {"suffix", "s"}, {"test", "t"}, {"to-stdout", "c"},
    {"uncompress", "d"}, {"verbose", "v"}, {"version", "V"}};
#define NLOPTS (sizeof(longopts) / (sizeof(char *) << 1))

/* process an option, return true if a file name and not an option */
local int option(char *arg)
{
    static int get = 0;

    /* if no argument, check status of get */
    if (arg == NULL) {
        if (get)
            bail("missing option argument for -",
                 get & 1 ? "b" : (get & 2 ? "p" : "s"));
        return 0;
    }

    /* process long option or short options */
    if (*arg == '-') {
        if (get)
            bail("require parameter after -",
                 get & 1 ? "b" : (get & 2 ? "p" : "s"));
        arg++;
        if (*arg == '-') {                      /* long option */
            int j;

            arg++;
            for (j = NLOPTS - 1; j >= 0; j--)
                if (strcmp(arg, longopts[j][0]) == 0) {
                    arg = longopts[j][1];
                    break;
                }
            if (j < 0)
                bail("invalid option: ", arg - 2);
        }
        while (*arg) {                          /* short options */
            switch (*arg) {
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                level = *arg - '0';
                break;
            case 'L':
                fputs(VERSION, stderr);
                fputs("Copyright (C) 2007 Mark Adler\n", stderr);
                fputs("Subject to the terms of the zlib license.\n",
                      stderr);
                fputs("No warranty is provided or implied.\n", stderr);
                exit(0);
            case 'N':  headis = 3;  break;
            case 'T':  headis &= ~2;  break;
            case 'V':  fputs(VERSION, stderr);  exit(0);
            case 'Z':
                bail("invalid option: LZW not supported", "");
            case 'a':
                bail("invalid option: ascii conversion not supported", "");
            case 'b':  get |= 1;  break;
            case 'c':  pipeout = 1;  break;
            case 'd':
            case 't':
                bail("invalid option: decompression not supported", "");
            case 'f':  force = 1;  break;
            case 'h':  help();  break;
            case 'i':  dict = 0;  break;
            case 'k':  keep = 1;  break;
            case 'l':  list = 1;  break;
            case 'n':  headis = 0;  break;
            case 'p':  get |= 2;  break;
            case 'q':  verbosity = 0;  break;
            case 'r':  recurse = 1;  break;
            case 's':  get |= 4;  break;
            case 'v':  verbosity++;  break;
            default:
                arg[1] = 0;
                bail("invalid option: -", arg);
            }
            arg++;
        }
        return 0;
    }

    /* process option parameter for -b, -p, or -s */
    if (get) {
        if (get != 1 && get != 2 && get != 4)
            bail("you need to separate ",
                 get == 3 ? "-b and -p" :
                            (get == 5 ? "-b and -s" : "-p and -s"));
        if (get == 1) {
            jobs_free();
            size = (size_t)(atol(arg)) << 10;   /* chunk size */
            if (size < DICT)
                bail("block size too small (must be >= 32K)", "");
        }
        else if (get == 2) {
            jobs_free();
            procs = atoi(arg);                  /* # processes */
            if (procs < 1)
                bail("need at least one process", "");
#ifdef NOTHREAD
            if (procs > 1)
                bail("this pigz compiled without threads", "");
#endif
        }
        else if (get == 4)
            sufx = arg;                         /* gz suffix */
        get = 0;
        return 0;
    }

    /* neither an option nor parameter */
    return 1;
}

/* Process arguments, compress in the gzip format.  Note that procs must be at
   least two in order to provide a dictionary in one work unit for the other
   work unit, and that size must be at least 32K to store a full dictionary. */
int main(int argc, char **argv)
{
    int n;                          /* general index */
    int none;                       /* true if no input names provided */
    char *opts, *p;                 /* environment default options, marker */

    /* prepare for interrupts, logging, and errors */
    signal(SIGINT, cutshort);
    gettimeofday(&start, NULL);
    out = NULL;

    /* set all options to defaults */
    defaults();

    /* process user environment variable defaults */
    opts = getenv("GZIP");
    if (opts != NULL) {
        while (*opts) {
            while (*opts == ' ' || *opts == '\t')
                opts++;
            p = opts;
            while (*p && *p != ' ' && *p != '\t')
                p++;
            n = *p;
            *p = 0;
            if (option(opts))
                bail("cannot provide files in GZIP environment variable", "");
            opts = p + (n ? 1 : 0);
        }
        option(NULL);
    }

    /* if no command line arguments and stdout is a terminal, show help */
    if (argc < 2 && isatty(1))
        help();

    /* process command-line arguments */
    none = 1;
    for (n = 1; n < argc; n++)
        if (option(argv[n])) {          /* true if not option or parameter */
            none = 0;
            gzip(argv[n]);              /* compress or list file */
        }
    option(NULL);

    /* list stdin or compress stdin to stdout if no file names provided */
    if (none)
        gzip(NULL);

    /* done -- release work units allocated by read_thread() calls */
    jobs_free();
    log_dump();
    return 0;
}
