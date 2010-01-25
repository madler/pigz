/* pigz.c -- parallel implementation of gzip
 * Copyright (C) 2007 Mark Adler
 * Version 1.22  19 February 2007  Mark Adler
 */

/* Version history:
   1.0   17 Jan 2007  First version
   1.1   28 Jan 2007  Avoid void * arithmetic (some compilers don't get that)
                      Add note about requiring zlib 1.2.3
                      Allow compression level 0 (no compression)
                      Completely rewrite parallelism -- add a write thread
                      Use deflateSetDictionary() to make use of history
                      Tune argument defaults to best performance on four cores
   1.21   1 Feb 2007  Add long command line options, add all gzip options
                      Add debugging options
   1.22  19 Feb 2007  Add list (-l) function
                      Process file names on command line, write .gz output
                      Write name and time in gzip header, set output file time
                      Implement all command line options except --recursive
                      Add --keep option to prevent deleting input files
                      Add thread tracing information with -vv used
                      Copy crc32_combine() from zlib (solves thread problem --
                         don't know why)
 */

/* To-do:
    - implement --recursive
    - look for GZIP symbol to get options from
    - incorporate gun into pigz?
    - list (-l) LZW files?
    - optionally have list (-l) decompress entire input to find it all?
    - add progress indicator on write thread?
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
   crc32_combine() function.

   pigz uses the POSIX pthread library for thread control and communication.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <unistd.h>
#include <fcntl.h>
#include "zlib.h"

#define local static

/* prevent end-of-line conversions on MSDOSish operating systems */
#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <io.h>
#  define SET_BINARY_MODE(fd) setmode(fd, O_BINARY)
#else
#  define SET_BINARY_MODE(fd)
#endif

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

/* read-only globals (set by main/read thread before others started) */
local int ind;              /* input file descriptor */
local int outd;             /* output file descriptor */
local char *in;             /* input file name */
local char *out;            /* output file name */
local int verbosity;        /* 0 = quiet, 1 = normal, 2 = verbose, 3 = trace */
local int headis;           /* 1 to store name, 2 to store date, 3 both */
local int pipeout;          /* write output to stdout even if file */
local int keep;             /* true to prevent deletion of input file */
local int force;            /* true to overwrite, compress links */
local int recurse;          /* true to dive down into directory structure %% */
local char *sufx;           /* suffix to use (.gz or user supplied) */
local char *name;           /* name for gzip header */
local time_t mtime;         /* time stamp for gzip header */
local int list;             /* true to list files instead of compress */
local int level;            /* compression level */
local int procs;            /* number of compression threads (>= 2) */
local size_t size;          /* uncompressed input size per thread (>= 32K) */
local struct timeval start; /* starting time of day for tracing */

/* exit with error */
local void bail(char *why, char *what)
{
    if (out != NULL)
        unlink(out);
    fprintf(stderr, "pigz abort: %s%s\n", why, what);
    exit(1);
}

/* trace log */
struct log {
    struct timeval when;    /* time of entry */
    char *msg;              /* message */
    struct log *next;       /* next entry */
} *log_head = NULL, **log_tail = &log_head;
local pthread_mutex_t logex = PTHREAD_MUTEX_INITIALIZER;

/* add entry to trace log */
local void log_add(char *fmt, ...)
{
    int ret;
    struct timeval now;
    struct log *me;
    va_list ap;

    gettimeofday(&now, NULL);
    me = malloc(sizeof(struct log));
    if (me == NULL)
        bail("not enough memory", "");
    me->when = now;
    va_start(ap, fmt);
    ret = vasprintf(&(me->msg), fmt, ap);
    va_end(ap);
    if (ret == -1) {
        free(me);
        bail("not enough memory", "");
    }
    me->next = NULL;
    ret = pthread_mutex_lock(&logex);
    if (ret)
        bail("mutex_lock error in ", "log_add");
    *log_tail = me;
    log_tail = &(me->next);
    pthread_mutex_unlock(&logex);
}

/* pull entry from trace log and print it, return false if empty */
local int log_show(void)
{
    int ret;
    struct log *me;
    struct timeval diff;

    ret = pthread_mutex_lock(&logex);
    if (ret)
        bail("mutex_lock error in ", "log_show");
    me = log_head;
    if (me != NULL) {
        log_head = me->next;
        if (me->next == NULL)
            log_tail = &log_head;
    }
    pthread_mutex_unlock(&logex);
    if (me == NULL)
        return 0;
    diff.tv_usec = me->when.tv_usec - start.tv_usec;
    diff.tv_sec = me->when.tv_sec - start.tv_sec;
    if (diff.tv_usec < 0) {
        diff.tv_usec += 1000000L;
        diff.tv_sec--;
    }
    fprintf(stderr, "trace %ld.%06d %s\n", diff.tv_sec, diff.tv_usec, me->msg);
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
    log_add("termination by user");
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

/* work units to feed to compress_thread() -- it is assumed that the out
   buffer is large enough to hold the maximum size len bytes could deflate to,
   plus five bytes for the final sync marker */
local struct work {
    size_t len;                 /* length of input */
    unsigned long crc;          /* crc of input */
    unsigned char *buf;         /* input */
    unsigned char *out;         /* space for output (guaranteed big enough) */
    z_stream strm;              /* pre-initialized z_stream */
    struct flag busy;           /* busy flag indicating work unit in use */
    pthread_t comp;             /* this compression thread */
} *jobs = NULL;

/* busy flag values */
#define IDLE 0          /* compress and writing done -- can start compress */
#define COMP 1          /* compress -- input and output buffers in use */
#define WRITE 2         /* compress done, writing output -- can read input */

/* next and previous jobs[] indices */
#define NEXT(n) ((n) == procs - 1 ? 0 : (n) + 1)
#define PREV(n) ((n) == 0 ? procs - 1 : (n) - 1)

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
        flag_init(&(jobs[n].busy), IDLE);
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
        flag_done(&(jobs[n].busy));
    }
    free(jobs);
}

/* sliding dictionary size for deflate */
#define DICT 32768U

/* largest power of 2 that fits in an unsigned int -- used to limit requests
   to zlib functions that use unsigned int lengths */
#define MAX ((((unsigned)-1) >> 1) + 1)

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
    if (prev->buf != NULL && len != 0)
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

/* put a 4-byte integer into a byte array in LSB order */
#define PUT4(a,b) (*(a)=(b),(a)[1]=(b)>>8,(a)[2]=(b)>>16,(a)[3]=(b)>>24)

/* write thread: wait for compression threads to complete, write output in
   order, also write gzip header and trailer around the compressed data */
local void *write_thread(void *arg)
{
    int n;                          /* compress thread index */
    size_t len;                     /* length of input processed */
    unsigned long tot;              /* total uncompressed size (overflow ok) */
    unsigned long crc;              /* CRC-32 of uncompressed data */
    unsigned char wrap[10];         /* gzip header or trailer */

    /* build and write gzip header */
    Trace(("-- write thread running"));
    wrap[0] = 31;
    wrap[1] = 139;
    wrap[2] = 8;                    /* deflate */
    wrap[3] = name != NULL ? 8 : 0;
    PUT4(wrap + 4, mtime);
    wrap[8] = level == 9 ? 2 : (level == 1 ? 4 : 0);
    wrap[9] = 3;                    /* unix */
    writen(outd, wrap, 10);
    if (name != NULL)
        writen(outd, (unsigned char *)name, strlen(name) + 1);

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

        /* an input buffer less than size in length indicates end of input */
    } while (len == size);

    /* write final static block and gzip trailer (crc and len mod 2^32) */
    wrap[0] = 3;  wrap[1] = 0;
    PUT4(wrap + 2, crc);
    PUT4(wrap + 6, tot);
    writen(outd, wrap, 10);
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

    /* make sure we do binary i/o on defective operating systems */
    SET_BINARY_MODE(ind);
    SET_BINARY_MODE(outd);

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

/* defines for listgz() */
#define NAMEMAX1 45     /* name display limit at vebosity 1 */
#define NAMEMAX2 13     /* name display limit at vebosity 2 */
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
            if (left == 0) \
                bail(in, " ended prematurely"); \
        } \
        left -= togo; \
    } while (0)
#define PULLSTR(buf, max) \
    do { \
        unsigned char *zer; \
        unsigned char *str = (buf); \
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
            if (left == 0) \
                bail(in, " ended prematurely"); \
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
            if (left == 0) \
                bail(in, " ended prematurely"); \
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
    unsigned char head[10];
    time_t stamp;
    char mod[27];
    unsigned char name[NAMEMAX1 + 1];
    int longname;
    unsigned char tail[8];
    unsigned long crc, len;

    /* locals for buffered reading */
    off_t tot = 0;
    size_t left = 0;
    unsigned char buf[BUF], *next = buf;

    /* read header */
    PULLFIX(head, 10);          /* fixed-size part of header */
    if (head[0] != 31 || head[1] != 139)
        bail(in, " is not a gzip file");
    if (head[3] & 0xe0)
        bail(in, " has unknown gzip extensions");
    stamp = head[4] + (head[5] << 8) + (head[6] << 16) + (head[7] << 24);
    if (stamp) {
        ctime_r(&stamp, mod);
        stamp = time(NULL);
        if (strcmp(mod + 20, ctime(&stamp) + 20) != 0)
            strcpy(mod + 11, mod + 19);
    }
    else
        strcpy(mod + 4, "------ -----");
    mod[16] = 0;
    if (head[3] & 4) {          /* skip extra field */
        n = GET();
        n += GET() << 8;
        SKIP(n);
    }
    if (head[3] & 8) {          /* get file name (or part of it) */
        PULLSTR(name, NAMEMAX1 + 1);
        max = verbosity > 1 ? NAMEMAX2 : NAMEMAX1;
        longname = name[max];
        name[max] = 0;
    }
    else
        name[0] = longname = 0;
    if (head[3] & 16)           /* skip comment */
        PULLSTR(NULL, 0);
    if (head[3] & 2)            /* skip header crc */
        SKIP(2);

    /* skip to end to get trailer (8 bytes), compute compressed length */
    if (next - buf < BUF - left) {      /* read whole thing already */
        if (left < 8)
            bail(in, " ended prematurely");
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
            if (n + left < 8)
                bail(in, " ended prematurely");
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
    if (tot < 2)
        bail(in, " ended prematurely");

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
        if (head[2] == 8)
            printf("def%s  %08lx  %s  ",
                head[8] & 2 ? "(9)" : (head[8] & 4 ? "(1)" : "   "),
                crc, mod + 4);
        else
            printf("%6u  %08lx  %s  ", head[2], crc, mod + 4);
    }
    if (verbosity > 0) {
        if (head[2] == 8 && tot > (len + (len >> 10) + 12))
            printf("%10llu %10lu?  unk   %s%s\n",
                   tot, len, name, longname ? "..." : "");
        else
            printf("%10llu %10lu %6.1f%%  %s%s\n",
                   tot, len,
                   len == 0 ? 0 : 100 * (len - tot)/(double)len,
                   name, longname ? "..." : "");
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
local void touch(int fd, time_t t)
{
    struct timeval times[2];

    times[0].tv_sec = t;
    times[0].tv_usec = 0;
    times[1].tv_sec = t;
    times[1].tv_usec = 0;
    futimes(fd, times);
}

/* long options conversion to short options */
local char *longopts[][2] = {
    {"LZW", "Z"}, {"ascii", "a"}, {"best", "9"}, {"bits", "Z"},
    {"blocksize", "b"}, {"decompress", "d"}, {"fast", "1"}, {"force", "f"},
    {"help", "h"}, {"keep", "k"}, {"license", "L"}, {"list", "l"},
    {"name", "N"}, {"no-name", "n"}, {"no-time", "T"}, {"processes", "p"},
    {"quiet", "q"}, {"recursive", "r"}, {"silent", "q"}, {"stdout", "c"},
    {"suffix", "s"}, {"test", "t"}, {"to-stdout", "c"}, {"uncompress", "d"},
    {"verbose", "v"}, {"version", "V"}};
#define NLOPTS (sizeof(longopts) / (sizeof(char *) << 1))

/* Process arguments, compress in the gzip format.  Note that procs must be at
   least two in order to provide a dictionary in one work unit for the other
   work unit, and that size must be at least 32K to store a full dictionary. */
int main(int argc, char **argv)
{
    int n;                          /* general index */
    int none;                       /* true if no input names provided */
    int get;                        /* command line parameters to get */
    char *arg;                      /* command line argument */
    struct stat st;                 /* to get file mod time */

    /* note starting time */
    gettimeofday(&start, NULL);

    /* set defaults -- 32 processes and 128K buffers was found to provide
       good utilization of four cores (about 97%) and balanced the overall
       execution time impact of more threads against more dictionary
       processing for a fixed amount of memory -- the memory usage for these
       settings and full use of all work units (at least 4 MB of input) is
       16.2 MB
       */
    level = Z_DEFAULT_COMPRESSION;
    procs = 32;
    size = 131072UL;
    verbosity = 1;                  /* normal message level */
    headis = 3;                     /* store name and timestamp */
    pipeout = 0;                    /* don't force output to stdout */
    sufx = ".gz";                   /* compressed file suffix */
    list = 0;                       /* compress */
    keep = 0;                       /* delete input file once compressed */
    force = 0;                      /* don't overwrite, don't compress links */
    recurse = 0;                    /* don't go into directories */

    /* process command-line arguments */
    get = 0;
    none = 1;
    out = NULL;
    signal(SIGINT, cutshort);
    for (n = 1; n < argc; n++) {
        arg = argv[n];

        /* option */
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
                    fputs("pigz 1.22\n", stderr);
                    fputs("Copyright (C) 2007 Mark Adler\n", stderr);
                    fputs("Subject to the terms of the zlib license.\n",
                          stderr);
                    fputs("No warranty is provided or implied.\n", stderr);
                    return 0;
                    break;
                case 'N':  headis = 3;  break;
                case 'T':  headis &= ~2;  break;
                case 'V':
                    fputs("pigz 1.22\n", stderr);
                    return 0;
                    break;
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
                case 'h':
                    fputs("usage: pigz [-0..9] [-b blocksizeinK]", stderr);
                    fputs(" [-p processes] < foo > foo.gz\n", stderr);
                    return 0;
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
        }

        /* value for an option (-b, -p, or -s) */
        else if (get) {
            if (get != 1 && get != 2 && get != 4)
                bail("you need to separate ",
                     get == 3 ? "-b and -p" :
                                (get == 5 ? "-b and -s" : "-p and -s"));
            if (get == 1) {
                size = (size_t)(atol(arg)) << 10;   /* chunk size */
                if (size < DICT)
                    bail("block size too small (must be >= 32K)", "");
            }
            else if (get == 2) {
                procs = atoi(arg);                  /* # processes */
                if (procs < 2)
                    bail("need at least two processes", "");
            }
            else if (get == 4)
                sufx = arg;                         /* gz suffix */
            get = 0;
        }

        /* input file name */
        else {
            in = arg;
            none = 0;

            /* don't compress .gz files (unless -f) */
            if (!list && strlen(in) >= strlen(sufx) &&
                    strcmp(in + strlen(in) - strlen(sufx), sufx) == 0 &&
                    !force) {
                fprintf(stderr, "%s ends with %s -- skipping\n", in, sufx);
                continue;
            }

            /* only compress regular files, but allow symbolic links if -f,
               recurse into directories if -r */
            if (lstat(in, &st)) {
                fprintf(stderr, "%s does not exist -- skipping\n", in);
                continue;
            }
            if ((st.st_mode & S_IFMT) != S_IFREG &&
                (st.st_mode & S_IFMT) != S_IFLNK &&
                (st.st_mode & S_IFMT) != S_IFDIR) {
                fprintf(stderr, "%s is a special file or device -- skipping\n",
                        in);
                continue;
            }
            if ((st.st_mode & S_IFMT) == S_IFLNK && !force) {
                fprintf(stderr, "%s is a symbolic link -- skipping\n", in);
                continue;
            }
            if ((st.st_mode & S_IFMT) == S_IFDIR && !recurse) {
                fprintf(stderr, "%s is a directory -- skipping\n", in);
                continue;
            }

            /* recurse into directory */
            if ((st.st_mode & S_IFMT) == S_IFDIR) {
                /* %% recurse into directory */
                fprintf(stderr, "recursion not implemented -- skipping %s\n",
                        in);
                continue;
            }

            /* open input file */
            ind = open(in, O_RDONLY, 0);
            if (ind < 0)
                bail("read error on ", in);

            /* process input file, either listing or compressing */
            if (list)
                listgz();
            else {                                  /* compress */
                /* set gzip header information */
                name = headis & 1 ? justname(in) : NULL;
                mtime = headis & 2 ? st.st_mtime : 0;

                /* create output file */
                if (pipeout) {
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
                    if (outd < 0 && errno == EEXIST && isatty(0)) {
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
                        fprintf(stderr, "%s exists -- skipping\n", out);
                        free(out);
                        out = NULL;
                        continue;
                    }

                    /* if some other error, give up */
                    if (outd < 0)
                        bail("write error on ", out);
                }

                /* compress ind to outd */
                read_thread();

                /* finish up, set times */
                close(ind);
                if (out != NULL) {
                    if (mtime)
                        touch(outd, mtime);
                    if (close(outd))
                        bail("write error on ", out);
                    free(out);
                    out = NULL;
                    if (!keep)
                        unlink(in);
                }
            }
        }
    }
    if (get)
        bail("missing option argument for -",
             get & 1 ? "b" : (get & 2 ? "p" : "s"));

    /* do stdin to stdout if no file names provided */
    if (none) {
        in = "<stdin>";
        ind = 0;
        if (list)
            listgz();
        else {
            name = NULL;
            mtime = headis & 2 ?
                    (fstat(ind, &st) ? time(NULL) : st.st_mtime) : 0;
            out = malloc(strlen("<stdout>") + 1);
            if (out == NULL)
                bail("not enough memory", "");
            strcpy(out, "<stdout>");
            outd = 1;
            if (isatty(outd) && !force)
                bail("trying to write compressed data to a terminal", "");
            read_thread();
            if (mtime && !pipeout)
                touch(outd, mtime);
            free(out);
            out = NULL;
        }
    }

    /* done -- release work units allocated by read_thread() */
    jobs_free();
    log_dump();
    return 0;
}
