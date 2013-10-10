/* pigz.c -- parallel implementation of gzip
 * Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012, 2013 Mark Adler
 * Version 2.3.1  9 Oct 2013  Mark Adler
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

  Mark accepts donations for providing this software.  Donations are not
  required or expected.  Any amount that you feel is appropriate would be
  appreciated.  You can use this link:

  https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=536055

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
                       Copy crc32_combine() from zlib (shared libraries issue)
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
   1.5    25 Mar 2007  Reinitialize jobs for new compression level
                       Copy attributes and owner from input file to output file
                       Add decompression and testing
                       Add -lt (or -ltv) to show all entries and proper lengths
                       Add decompression, testing, listing of LZW (.Z) files
                       Only generate and show trace log if DEBUG defined
                       Take "-" argument to mean read file from stdin
   1.6    30 Mar 2007  Add zlib stream compression (--zlib), and decompression
   1.7    29 Apr 2007  Decompress first entry of a zip file (if deflated)
                       Avoid empty deflate blocks at end of deflate stream
                       Show zlib check value (Adler-32) when listing
                       Don't complain when decompressing empty file
                       Warn about trailing junk for gzip and zlib streams
                       Make listings consistent, ignore gzip extra flags
                       Add zip stream compression (--zip)
   1.8    13 May 2007  Document --zip option in help output
   2.0    19 Oct 2008  Complete rewrite of thread usage and synchronization
                       Use polling threads and a pool of memory buffers
                       Remove direct pthread library use, hide in yarn.c
   2.0.1  20 Oct 2008  Check version of zlib at compile time, need >= 1.2.3
   2.1    24 Oct 2008  Decompress with read, write, inflate, and check threads
                       Remove spurious use of ctime_r(), ctime() more portable
                       Change application of job->calc lock to be a semaphore
                       Detect size of off_t at run time to select %lu vs. %llu
                       #define large file support macro even if not __linux__
                       Remove _LARGEFILE64_SOURCE, _FILE_OFFSET_BITS is enough
                       Detect file-too-large error and report, blame build
                       Replace check combination routines with those from zlib
   2.1.1  28 Oct 2008  Fix a leak for files with an integer number of blocks
                       Update for yarn 1.1 (yarn_prefix and yarn_abort)
   2.1.2  30 Oct 2008  Work around use of beta zlib in production systems
   2.1.3   8 Nov 2008  Don't use zlib combination routines, put back in pigz
   2.1.4   9 Nov 2008  Fix bug when decompressing very short files
   2.1.5  20 Jul 2009  Added 2008, 2009 to --license statement
                       Allow numeric parameter immediately after -p or -b
                       Enforce parameter after -p, -b, -s, before other options
                       Enforce numeric parameters to have only numeric digits
                       Try to determine the number of processors for -p default
                       Fix --suffix short option to be -S to match gzip [Bloch]
                       Decompress if executable named "unpigz" [Amundsen]
                       Add a little bit of testing to Makefile
   2.1.6  17 Jan 2010  Added pigz.spec to distribution for RPM systems [Brown]
                       Avoid some compiler warnings
                       Process symbolic links if piping to stdout [Hoffstätte]
                       Decompress if executable named "gunzip" [Hoffstätte]
                       Allow ".tgz" suffix [Chernookiy]
                       Fix adler32 comparison on .zz files
   2.1.7  17 Dec 2011  Avoid unused parameter warning in reenter()
                       Don't assume 2's complement ints in compress_thread()
                       Replicate gzip -cdf cat-like behavior
                       Replicate gzip -- option to suppress option decoding
                       Test output from make test instead of showing it
                       Updated pigz.spec to install unpigz, pigz.1 [Obermaier]
                       Add PIGZ environment variable [Mueller]
                       Replicate gzip suffix search when decoding or listing
                       Fix bug in load() to set in_left to zero on end of file
                       Do not check suffix when input file won't be modified
                       Decompress to stdout if name is "*cat" [Hayasaka]
                       Write data descriptor signature to be like Info-ZIP
                       Update and sort options list in help
                       Use CC variable for compiler in Makefile
                       Exit with code 2 if a warning has been issued
                       Fix thread synchronization problem when tracing
                       Change macro name MAX to MAX2 to avoid library conflicts
                       Determine number of processors on HP-UX [Lloyd]
   2.2    31 Dec 2011  Check for expansion bound busting (e.g. modified zlib)
                       Make the "threads" list head global variable volatile
                       Fix construction and printing of 32-bit check values
                       Add --rsyncable functionality
   2.2.1   1 Jan 2012  Fix bug in --rsyncable buffer management
   2.2.2   1 Jan 2012  Fix another bug in --rsyncable buffer management
   2.2.3  15 Jan 2012  Remove volatile in yarn.c
                       Reduce the number of input buffers
                       Change initial rsyncable hash to comparison value
                       Improve the efficiency of arriving at a byte boundary
                       Add thread portability #defines from yarn.c
                       Have rsyncable compression be independent of threading
                       Fix bug where constructed dictionaries not being used
   2.2.4  11 Mar 2012  Avoid some return value warnings
                       Improve the portability of printing the off_t type
                       Check for existence of compress binary before using
                       Update zlib version checking to 1.2.6 for new functions
                       Fix bug in zip (-K) output
                       Fix license in pigz.spec
                       Remove thread portability #defines in pigz.c
   2.2.5  28 Jul 2012  Avoid race condition in free_pool()
                       Change suffix to .tar when decompressing or listing .tgz
                       Print name of executable in error messages
                       Show help properly when the name is unpigz or gunzip
                       Fix permissions security problem before output is closed
   2.3     3 Mar 2013  Don't complain about missing suffix on stdout
                       Put all global variables in a structure for readability
                       Do not decompress concatenated zlib streams (just gzip)
                       Add option for compression level 11 to use zopfli
                       Fix handling of junk after compressed data
   2.3.1   9 Oct 2013  Fix builds of pigzt and pigzn to include zopfli
                       Add -lm, needed to link log function on some systems
                       Respect LDFLAGS in Makefile, use CFLAGS consistently
                       Add memory allocation tracking
                       Fix casting error in uncompressed length calculation
                       Update zopfli to Mar 10, 2013 Google state
                       Support zopfli in single thread case
                       Add -F, -I, -M, and -O options for zopfli tuning
 */

#define VERSION "pigz 2.3.1\n"

/* To-do:
    - make source portable for Windows, VMS, etc. (see gzip source code)
    - make build portable (currently good for Unixish)
 */

/*
   pigz compresses using threads to make use of multiple processors and cores.
   The input is broken up into 128 KB chunks with each compressed in parallel.
   The individual check value for each chunk is also calculated in parallel.
   The compressed data is written in order to the output, and a combined check
   value is calculated from the individual check values.

   The compressed data format generated is in the gzip, zlib, or single-entry
   zip format using the deflate compression method.  The compression produces
   partial raw deflate streams which are concatenated by a single write thread
   and wrapped with the appropriate header and trailer, where the trailer
   contains the combined check value.

   Each partial raw deflate stream is terminated by an empty stored block
   (using the Z_SYNC_FLUSH option of zlib), in order to end that partial bit
   stream at a byte boundary, unless that partial stream happens to already end
   at a byte boundary (the latter requires zlib 1.2.6 or later).  Ending on a
   byte boundary allows the partial streams to be concatenated simply as
   sequences of bytes.  This adds a very small four to five byte overhead
   (average 3.75 bytes) to the output for each input chunk.

   The default input block size is 128K, but can be changed with the -b option.
   The number of compress threads is set by default to 8, which can be changed
   using the -p option.  Specifying -p 1 avoids the use of threads entirely.
   pigz will try to determine the number of processors in the machine, in which
   case if that number is two or greater, pigz will use that as the default for
   -p instead of 8.

   The input blocks, while compressed independently, have the last 32K of the
   previous block loaded as a preset dictionary to preserve the compression
   effectiveness of deflating in a single thread.  This can be turned off using
   the --independent or -i option, so that the blocks can be decompressed
   independently for partial error recovery or for random access.

   Decompression can't be parallelized, at least not without specially prepared
   deflate streams for that purpose.  As a result, pigz uses a single thread
   (the main thread) for decompression, but will create three other threads for
   reading, writing, and check calculation, which can speed up decompression
   under some circumstances.  Parallel decompression can be turned off by
   specifying one process (-dp 1 or -tp 1).

   pigz requires zlib 1.2.1 or later to allow setting the dictionary when doing
   raw deflate.  Since zlib 1.2.3 corrects security vulnerabilities in zlib
   version 1.2.1 and 1.2.2, conditionals check for zlib 1.2.3 or later during
   the compilation of pigz.c.  zlib 1.2.4 includes some improvements to
   Z_FULL_FLUSH and deflateSetDictionary() that permit identical output for
   pigz with and without threads, which is not possible with zlib 1.2.3.  This
   may be important for uses of pigz -R where small changes in the contents
   should result in small changes in the archive for rsync.  Note that due to
   the details of how the lower levels of compression result in greater speed,
   compression level 3 and below does not permit identical pigz output with
   and without threads.

   pigz uses the POSIX pthread library for thread control and communication,
   through the yarn.h interface to yarn.c.  yarn.c can be replaced with
   equivalent implementations using other thread libraries.  pigz can be
   compiled with NOTHREAD #defined to not use threads at all (in which case
   pigz will not be able to live up to the "parallel" in its name).
 */

/*
   Details of parallel compression implementation:

   When doing parallel compression, pigz uses the main thread to read the input
   in 'size' sized chunks (see -b), and puts those in a compression job list,
   each with a sequence number to keep track of the ordering.  If it is not the
   first chunk, then that job also points to the previous input buffer, from
   which the last 32K will be used as a dictionary (unless -i is specified).
   This sets a lower limit of 32K on 'size'.

   pigz launches up to 'procs' compression threads (see -p).  Each compression
   thread continues to look for jobs in the compression list and perform those
   jobs until instructed to return.  When a job is pulled, the dictionary, if
   provided, will be loaded into the deflate engine and then that input buffer
   is dropped for reuse.  Then the input data is compressed into an output
   buffer that grows in size if necessary to hold the compressed data. The job
   is then put into the write job list, sorted by the sequence number. The
   compress thread however continues to calculate the check value on the input
   data, either a CRC-32 or Adler-32, possibly in parallel with the write
   thread writing the output data.  Once that's done, the compress thread drops
   the input buffer and also releases the lock on the check value so that the
   write thread can combine it with the previous check values.  The compress
   thread has then completed that job, and goes to look for another.

   All of the compress threads are left running and waiting even after the last
   chunk is processed, so that they can support the next input to be compressed
   (more than one input file on the command line).  Once pigz is done, it will
   call all the compress threads home (that'll do pig, that'll do).

   Before starting to read the input, the main thread launches the write thread
   so that it is ready pick up jobs immediately.  The compress thread puts the
   write jobs in the list in sequence sorted order, so that the first job in
   the list is always has the lowest sequence number.  The write thread waits
   for the next write job in sequence, and then gets that job.  The job still
   holds its input buffer, from which the write thread gets the input buffer
   length for use in check value combination.  Then the write thread drops that
   input buffer to allow its reuse.  Holding on to the input buffer until the
   write thread starts also has the benefit that the read and compress threads
   can't get way ahead of the write thread and build up a large backlog of
   unwritten compressed data.  The write thread will write the compressed data,
   drop the output buffer, and then wait for the check value to be unlocked
   by the compress thread.  Then the write thread combines the check value for
   this chunk with the total check value for eventual use in the trailer.  If
   this is not the last chunk, the write thread then goes back to look for the
   next output chunk in sequence.  After the last chunk, the write thread
   returns and joins the main thread.  Unlike the compress threads, a new write
   thread is launched for each input stream.  The write thread writes the
   appropriate header and trailer around the compressed data.

   The input and output buffers are reused through their collection in pools.
   Each buffer has a use count, which when decremented to zero returns the
   buffer to the respective pool.  Each input buffer has up to three parallel
   uses: as the input for compression, as the data for the check value
   calculation, and as a dictionary for compression.  Each output buffer has
   only one use, which is as the output of compression followed serially as
   data to be written.  The input pool is limited in the number of buffers, so
   that reading does not get way ahead of compression and eat up memory with
   more input than can be used.  The limit is approximately two times the
   number of compression threads.  In the case that reading is fast as compared
   to compression, that number allows a second set of buffers to be read while
   the first set of compressions are being performed.  The number of output
   buffers is not directly limited, but is indirectly limited by the release of
   input buffers to about the same number.
 */

/* use large file functions if available */
#define _FILE_OFFSET_BITS 64

/* included headers and what is expected from each */
#include <stdio.h>      /* fflush(), fprintf(), fputs(), getchar(), putc(), */
                        /* puts(), printf(), vasprintf(), stderr, EOF, NULL,
                           SEEK_END, size_t, off_t */
#include <stdlib.h>     /* exit(), malloc(), free(), realloc(), atol(), */
                        /* atoi(), getenv() */
#include <stdarg.h>     /* va_start(), va_end(), va_list */
#include <string.h>     /* memset(), memchr(), memcpy(), strcmp(), strcpy() */
                        /* strncpy(), strlen(), strcat(), strrchr() */
#include <errno.h>      /* errno, EEXIST */
#include <assert.h>     /* assert() */
#include <time.h>       /* ctime(), time(), time_t, mktime() */
#include <signal.h>     /* signal(), SIGINT */
#include <sys/types.h>  /* ssize_t */
#include <sys/stat.h>   /* chmod(), stat(), fstat(), lstat(), struct stat, */
                        /* S_IFDIR, S_IFLNK, S_IFMT, S_IFREG */
#include <sys/time.h>   /* utimes(), gettimeofday(), struct timeval */
#include <unistd.h>     /* unlink(), _exit(), read(), write(), close(), */
                        /* lseek(), isatty(), chown() */
#include <fcntl.h>      /* open(), O_CREAT, O_EXCL, O_RDONLY, O_TRUNC, */
                        /* O_WRONLY */
#include <dirent.h>     /* opendir(), readdir(), closedir(), DIR, */
                        /* struct dirent */
#include <limits.h>     /* PATH_MAX, UINT_MAX, INT_MAX */
#if __STDC_VERSION__-0 >= 199901L || __GNUC__-0 >= 3
#  include <inttypes.h> /* intmax_t */
#endif

#ifdef DEBUG
#  if defined(__APPLE__)
#    include <malloc/malloc.h>
#    define MALLOC_SIZE(p) malloc_size(p)
#  elif defined (__linux)
#    include <malloc.h>
#    define MALLOC_SIZE(p) malloc_usable_size(p)
#  elif defined (_WIN32) || defined(_WIN64)
#    include <malloc.h>
#    define MALLOC_SIZE(p) _msize(p)
#  else
#    define MALLOC_SIZE(p) (0)
#  endif
#endif

#ifdef __hpux
#  include <sys/param.h>
#  include <sys/pstat.h>
#endif

#include "zlib.h"       /* deflateInit2(), deflateReset(), deflate(), */
                        /* deflateEnd(), deflateSetDictionary(), crc32(),
                           inflateBackInit(), inflateBack(), inflateBackEnd(),
                           Z_DEFAULT_COMPRESSION, Z_DEFAULT_STRATEGY,
                           Z_DEFLATED, Z_NO_FLUSH, Z_NULL, Z_OK,
                           Z_SYNC_FLUSH, z_stream */
#if !defined(ZLIB_VERNUM) || ZLIB_VERNUM < 0x1230
#  error Need zlib version 1.2.3 or later
#endif

#ifndef NOTHREAD
#  include "yarn.h"     /* thread, launch(), join(), join_all(), */
                        /* lock, new_lock(), possess(), twist(), wait_for(),
                           release(), peek_lock(), free_lock(), yarn_name */
#endif
#include "zopfli/deflate.h"     /* ZopfliDeflatePart(), ZopfliInitOptions(),
                                   ZopfliOptions */

/* for local functions and globals */
#define local static

/* prevent end-of-line conversions on MSDOSish operating systems */
#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <io.h>       /* setmode(), O_BINARY */
#  define SET_BINARY_MODE(fd) setmode(fd, O_BINARY)
#else
#  define SET_BINARY_MODE(fd)
#endif

/* release an allocated pointer, if allocated, and mark as unallocated */
#define RELEASE(ptr) \
    do { \
        if ((ptr) != NULL) { \
            FREE(ptr); \
            ptr = NULL; \
        } \
    } while (0)

/* sliding dictionary size for deflate */
#define DICT 32768U

/* largest power of 2 that fits in an unsigned int -- used to limit requests
   to zlib functions that use unsigned int lengths */
#define MAXP2 (UINT_MAX - (UINT_MAX >> 1))

/* rsyncable constants -- RSYNCBITS is the number of bits in the mask for
   comparison.  For random input data, there will be a hit on average every
   1<<RSYNCBITS bytes.  So for an RSYNCBITS of 12, there will be an average of
   one hit every 4096 bytes, resulting in a mean block size of 4096.  RSYNCMASK
   is the resulting bit mask.  RSYNCHIT is what the hash value is compared to
   after applying the mask.

   The choice of 12 for RSYNCBITS is consistent with the original rsyncable
   patch for gzip which also uses a 12-bit mask.  This results in a relatively
   small hit to compression, on the order of 1.5% to 3%.  A mask of 13 bits can
   be used instead if a hit of less than 1% to the compression is desired, at
   the expense of more blocks transmitted for rsync updates.  (Your mileage may
   vary.)

   This implementation of rsyncable uses a different hash algorithm than what
   the gzip rsyncable patch uses in order to provide better performance in
   several regards.  The algorithm is simply to shift the hash value left one
   bit and exclusive-or that with the next byte.  This is masked to the number
   of hash bits (RSYNCMASK) and compared to all ones except for a zero in the
   top bit (RSYNCHIT). This rolling hash has a very small window of 19 bytes
   (RSYNCBITS+7).  The small window provides the benefit of much more rapid
   resynchronization after a change, than does the 4096-byte window of the gzip
   rsyncable patch.

   The comparison value is chosen to avoid matching any repeated bytes or short
   sequences.  The gzip rsyncable patch on the other hand uses a sum and zero
   for comparison, which results in certain bad behaviors, such as always
   matching everywhere in a long sequence of zeros.  Such sequences occur
   frequently in tar files.

   This hash efficiently discards history older than 19 bytes simply by
   shifting that data past the top of the mask -- no history needs to be
   retained to undo its impact on the hash value, as is needed for a sum.

   The choice of the comparison value (RSYNCHIT) has the virtue of avoiding
   extremely short blocks.  The shortest block is five bytes (RSYNCBITS-7) from
   hit to hit, and is unlikely.  Whereas with the gzip rsyncable algorithm,
   blocks of one byte are not only possible, but in fact are the most likely
   block size.

   Thanks and acknowledgement to Kevin Day for his experimentation and insights
   on rsyncable hash characteristics that led to some of the choices here.
 */
#define RSYNCBITS 12
#define RSYNCMASK ((1U << RSYNCBITS) - 1)
#define RSYNCHIT (RSYNCMASK >> 1)

/* initial pool counts and sizes -- INBUFS is the limit on the number of input
   spaces as a function of the number of processors (used to throttle the
   creation of compression jobs), OUTPOOL is the initial size of the output
   data buffer, chosen to make resizing of the buffer very unlikely and to
   allow prepending with a dictionary for use as an input buffer for zopfli */
#define INBUFS(p) (((p)<<1)+3)
#define OUTPOOL(s) ((s)+((s)>>4)+DICT)

/* input buffer size */
#define BUF 32768U

/* globals (modified by main thread only when it's the only thread) */
local struct {
    char *prog;             /* name by which pigz was invoked */
    int ind;                /* input file descriptor */
    int outd;               /* output file descriptor */
    char inf[PATH_MAX+1];   /* input file name (accommodate recursion) */
    char *outf;             /* output file name (allocated if not NULL) */
    int verbosity;          /* 0 = quiet, 1 = normal, 2 = verbose, 3 = trace */
    int headis;             /* 1 to store name, 2 to store date, 3 both */
    int pipeout;            /* write output to stdout even if file */
    int keep;               /* true to prevent deletion of input file */
    int force;              /* true to overwrite, compress links, cat */
    int form;               /* gzip = 0, zlib = 1, zip = 2 or 3 */
    unsigned char magic1;   /* first byte of possible header when decoding */
    int recurse;            /* true to dive down into directory structure */
    char *sufx;             /* suffix to use (".gz" or user supplied) */
    char *name;             /* name for gzip header */
    time_t mtime;           /* time stamp from input file for gzip header */
    int list;               /* true to list files instead of compress */
    int first;              /* true if we need to print listing header */
    int decode;             /* 0 to compress, 1 to decompress, 2 to test */
    int level;              /* compression level */
    ZopfliOptions zopts;    /* zopfli compression options */
    int rsync;              /* true for rsync blocking */
    int procs;              /* maximum number of compression threads (>= 1) */
    int setdict;            /* true to initialize dictionary in each thread */
    size_t block;           /* uncompressed input size per thread (>= 32K) */
    int warned;             /* true if a warning has been given */

    /* saved gzip/zip header data for decompression, testing, and listing */
    time_t stamp;               /* time stamp from gzip header */
    char *hname;                /* name from header (allocated) */
    unsigned long zip_crc;      /* local header crc */
    unsigned long zip_clen;     /* local header compressed length */
    unsigned long zip_ulen;     /* local header uncompressed length */

    /* globals for decompression and listing buffered reading */
    unsigned char in_buf[BUF];  /* input buffer */
    unsigned char *in_next; /* next unused byte in buffer */
    size_t in_left;         /* number of unused bytes in buffer */
    int in_eof;             /* true if reached end of file on input */
    int in_short;           /* true if last read didn't fill buffer */
    off_t in_tot;           /* total bytes read from input */
    off_t out_tot;          /* total bytes written to output */
    unsigned long out_check;    /* check value of output */

#ifndef NOTHREAD
    /* globals for decompression parallel reading */
    unsigned char in_buf2[BUF]; /* second buffer for parallel reads */
    size_t in_len;          /* data waiting in next buffer */
    int in_which;           /* -1: start, 0: in_buf2, 1: in_buf */
    lock *load_state;       /* value = 0 to wait, 1 to read a buffer */
    thread *load_thread;    /* load_read() thread for joining */
#endif
} g;

/* display a complaint with the program name on stderr */
local int complain(char *fmt, ...)
{
    va_list ap;

    if (g.verbosity > 0) {
        fprintf(stderr, "%s: ", g.prog);
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
        putc('\n', stderr);
        fflush(stderr);
        g.warned = 1;
    }
    return 0;
}

/* exit with error, delete output file if in the middle of writing it */
local int bail(char *why, char *what)
{
    if (g.outd != -1 && g.outf != NULL)
        unlink(g.outf);
    complain("abort: %s%s", why, what);
    exit(1);
    return 0;
}

#ifdef DEBUG

/* memory tracking */

local struct mem_track_s {
    size_t num;         /* current number of allocations */
    size_t size;        /* total size of current allocations */
    size_t max;         /* maximum size of allocations */
#ifndef NOTHREAD
    lock *lock;         /* lock for access across threads */
#endif
} mem_track;

#ifndef NOTHREAD
#  define mem_track_grab(m) possess((m)->lock)
#  define mem_track_drop(m) release((m)->lock)
#else
#  define mem_track_grab(m)
#  define mem_track_drop(m)
#endif

local void *malloc_track(struct mem_track_s *mem, size_t size)
{
    void *ptr;

    ptr = malloc(size);
    if (ptr != NULL) {
        size = MALLOC_SIZE(ptr);
        mem_track_grab(mem);
        mem->num++;
        mem->size += size;
        if (mem->size > mem->max)
            mem->max = mem->size;
        mem_track_drop(mem);
    }
    return ptr;
}

local void *realloc_track(struct mem_track_s *mem, void *ptr, size_t size)
{
    size_t was;

    if (ptr == NULL)
        return malloc_track(mem, size);
    was = MALLOC_SIZE(ptr);
    ptr = realloc(ptr, size);
    if (ptr != NULL) {
        size = MALLOC_SIZE(ptr);
        mem_track_grab(mem);
        mem->size -= was;
        mem->size += size;
        if (mem->size > mem->max)
            mem->max = mem->size;
        mem_track_drop(mem);
    }
    return ptr;
}

local void free_track(struct mem_track_s *mem, void *ptr)
{
    size_t size;

    if (ptr != NULL) {
        size = MALLOC_SIZE(ptr);
        mem_track_grab(mem);
        mem->num--;
        mem->size -= size;
        mem_track_drop(mem);
        free(ptr);
    }
}

#ifndef NOTHREAD
local void *yarn_malloc(size_t size)
{
    return malloc_track(&mem_track, size);
}

local void yarn_free(void *ptr)
{
    return free_track(&mem_track, ptr);
}
#endif

local voidpf zlib_alloc(voidpf opaque, uInt items, uInt size)
{
    return malloc_track(opaque, items * (size_t)size);
}

local void zlib_free(voidpf opaque, voidpf address)
{
    free_track(opaque, address);
}

#define MALLOC(s) malloc_track(&mem_track, s)
#define REALLOC(p, s) realloc_track(&mem_track, p, s)
#define FREE(p) free_track(&mem_track, p)
#define OPAQUE (&mem_track)
#define ZALLOC zlib_alloc
#define ZFREE zlib_free

/* starting time of day for tracing */
local struct timeval start;

/* trace log */
local struct log {
    struct timeval when;    /* time of entry */
    char *msg;              /* message */
    struct log *next;       /* next entry */
} *log_head, **log_tail = NULL;
#ifndef NOTHREAD
  local lock *log_lock = NULL;
#endif

/* maximum log entry length */
#define MAXMSG 256

/* set up log (call from main thread before other threads launched) */
local void log_init(void)
{
    if (log_tail == NULL) {
        mem_track.num = 0;
        mem_track.size = 0;
        mem_track.max = 0;
#ifndef NOTHREAD
        mem_track.lock = new_lock(0);
        yarn_mem(yarn_malloc, yarn_free);
        log_lock = new_lock(0);
#endif
        log_head = NULL;
        log_tail = &log_head;
    }
}

/* add entry to trace log */
local void log_add(char *fmt, ...)
{
    struct timeval now;
    struct log *me;
    va_list ap;
    char msg[MAXMSG];

    gettimeofday(&now, NULL);
    me = MALLOC(sizeof(struct log));
    if (me == NULL)
        bail("not enough memory", "");
    me->when = now;
    va_start(ap, fmt);
    vsnprintf(msg, MAXMSG, fmt, ap);
    va_end(ap);
    me->msg = MALLOC(strlen(msg) + 1);
    if (me->msg == NULL) {
        FREE(me);
        bail("not enough memory", "");
    }
    strcpy(me->msg, msg);
    me->next = NULL;
#ifndef NOTHREAD
    assert(log_lock != NULL);
    possess(log_lock);
#endif
    *log_tail = me;
    log_tail = &(me->next);
#ifndef NOTHREAD
    twist(log_lock, BY, +1);
#endif
}

/* pull entry from trace log and print it, return false if empty */
local int log_show(void)
{
    struct log *me;
    struct timeval diff;

    if (log_tail == NULL)
        return 0;
#ifndef NOTHREAD
    possess(log_lock);
#endif
    me = log_head;
    if (me == NULL) {
#ifndef NOTHREAD
        release(log_lock);
#endif
        return 0;
    }
    log_head = me->next;
    if (me->next == NULL)
        log_tail = &log_head;
#ifndef NOTHREAD
    twist(log_lock, BY, -1);
#endif
    diff.tv_usec = me->when.tv_usec - start.tv_usec;
    diff.tv_sec = me->when.tv_sec - start.tv_sec;
    if (diff.tv_usec < 0) {
        diff.tv_usec += 1000000L;
        diff.tv_sec--;
    }
    fprintf(stderr, "trace %ld.%06ld %s\n",
            (long)diff.tv_sec, (long)diff.tv_usec, me->msg);
    fflush(stderr);
    FREE(me->msg);
    FREE(me);
    return 1;
}

/* release log resources (need to do log_init() to use again) */
local void log_free(void)
{
    struct log *me;

    if (log_tail != NULL) {
#ifndef NOTHREAD
        possess(log_lock);
#endif
        while ((me = log_head) != NULL) {
            log_head = me->next;
            FREE(me->msg);
            FREE(me);
        }
#ifndef NOTHREAD
        twist(log_lock, TO, 0);
        free_lock(log_lock);
        log_lock = NULL;
        yarn_mem(malloc, free);
        free_lock(mem_track.lock);
#endif
        log_tail = NULL;
    }
}

/* show entries until no more, free log */
local void log_dump(void)
{
    if (log_tail == NULL)
        return;
    while (log_show())
        ;
    log_free();
    if (mem_track.num || mem_track.size)
        complain("memory leak: %lu allocs of %lu bytes total",
                 mem_track.num, mem_track.size);
    if (mem_track.max)
        fprintf(stderr, "%lu bytes of memory used\n", mem_track.max);
}

/* debugging macro */
#define Trace(x) \
    do { \
        if (g.verbosity > 2) { \
            log_add x; \
        } \
    } while (0)

#else /* !DEBUG */

#define MALLOC malloc
#define REALLOC realloc
#define FREE free
#define OPAQUE Z_NULL
#define ZALLOC Z_NULL
#define ZFREE Z_NULL

#define log_dump()
#define Trace(x)

#endif

/* read up to len bytes into buf, repeating read() calls as needed */
local size_t readn(int desc, unsigned char *buf, size_t len)
{
    ssize_t ret;
    size_t got;

    got = 0;
    while (len) {
        ret = read(desc, buf, len);
        if (ret < 0)
            bail("read error on ", g.inf);
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
        if (ret < 1) {
            complain("write error code %d", errno);
            bail("write error on ", g.outf);
        }
        buf += ret;
        len -= ret;
    }
}

/* convert Unix time to MS-DOS date and time, assuming current timezone
   (you got a better idea?) */
local unsigned long time2dos(time_t t)
{
    struct tm *tm;
    unsigned long dos;

    if (t == 0)
        t = time(NULL);
    tm = localtime(&t);
    if (tm->tm_year < 80 || tm->tm_year > 207)
        return 0;
    dos = (tm->tm_year - 80) << 25;
    dos += (tm->tm_mon + 1) << 21;
    dos += tm->tm_mday << 16;
    dos += tm->tm_hour << 11;
    dos += tm->tm_min << 5;
    dos += (tm->tm_sec + 1) >> 1;   /* round to double-seconds */
    return dos;
}

/* put a 4-byte integer into a byte array in LSB order or MSB order */
#define PUT2L(a,b) (*(a)=(b)&0xff,(a)[1]=(b)>>8)
#define PUT4L(a,b) (PUT2L(a,(b)&0xffff),PUT2L((a)+2,(b)>>16))
#define PUT4M(a,b) (*(a)=(b)>>24,(a)[1]=(b)>>16,(a)[2]=(b)>>8,(a)[3]=(b))

/* write a gzip, zlib, or zip header using the information in the globals */
local unsigned long put_header(void)
{
    unsigned long len;
    unsigned char head[30];

    if (g.form > 1) {               /* zip */
        /* write local header */
        PUT4L(head, 0x04034b50UL);  /* local header signature */
        PUT2L(head + 4, 20);        /* version needed to extract (2.0) */
        PUT2L(head + 6, 8);         /* flags: data descriptor follows data */
        PUT2L(head + 8, 8);         /* deflate */
        PUT4L(head + 10, time2dos(g.mtime));
        PUT4L(head + 14, 0);        /* crc (not here) */
        PUT4L(head + 18, 0);        /* compressed length (not here) */
        PUT4L(head + 22, 0);        /* uncompressed length (not here) */
        PUT2L(head + 26, g.name == NULL ? 1 :   /* length of name */
                                          strlen(g.name));
        PUT2L(head + 28, 9);        /* length of extra field (see below) */
        writen(g.outd, head, 30);   /* write local header */
        len = 30;

        /* write file name (use "-" for stdin) */
        if (g.name == NULL)
            writen(g.outd, (unsigned char *)"-", 1);
        else
            writen(g.outd, (unsigned char *)g.name, strlen(g.name));
        len += g.name == NULL ? 1 : strlen(g.name);

        /* write extended timestamp extra field block (9 bytes) */
        PUT2L(head, 0x5455);        /* extended timestamp signature */
        PUT2L(head + 2, 5);         /* number of data bytes in this block */
        head[4] = 1;                /* flag presence of mod time */
        PUT4L(head + 5, g.mtime);   /* mod time */
        writen(g.outd, head, 9);    /* write extra field block */
        len += 9;
    }
    else if (g.form) {              /* zlib */
        head[0] = 0x78;             /* deflate, 32K window */
        head[1] = (g.level >= 9 ? 3 :
                   (g.level == 1 ? 0 :
                    (g.level >= 6 || g.level == Z_DEFAULT_COMPRESSION ?
                        1 : 2))) << 6;
        head[1] += 31 - (((head[0] << 8) + head[1]) % 31);
        writen(g.outd, head, 2);
        len = 2;
    }
    else {                          /* gzip */
        head[0] = 31;
        head[1] = 139;
        head[2] = 8;                /* deflate */
        head[3] = g.name != NULL ? 8 : 0;
        PUT4L(head + 4, g.mtime);
        head[8] = g.level >= 9 ? 2 : (g.level == 1 ? 4 : 0);
        head[9] = 3;                /* unix */
        writen(g.outd, head, 10);
        len = 10;
        if (g.name != NULL)
            writen(g.outd, (unsigned char *)g.name, strlen(g.name) + 1);
        if (g.name != NULL)
            len += strlen(g.name) + 1;
    }
    return len;
}

/* write a gzip, zlib, or zip trailer */
local void put_trailer(unsigned long ulen, unsigned long clen,
                       unsigned long check, unsigned long head)
{
    unsigned char tail[46];

    if (g.form > 1) {               /* zip */
        unsigned long cent;

        /* write data descriptor (as promised in local header) */
        PUT4L(tail, 0x08074b50UL);
        PUT4L(tail + 4, check);
        PUT4L(tail + 8, clen);
        PUT4L(tail + 12, ulen);
        writen(g.outd, tail, 16);

        /* write central file header */
        PUT4L(tail, 0x02014b50UL);  /* central header signature */
        tail[4] = 63;               /* obeyed version 6.3 of the zip spec */
        tail[5] = 255;              /* ignore external attributes */
        PUT2L(tail + 6, 20);        /* version needed to extract (2.0) */
        PUT2L(tail + 8, 8);         /* data descriptor is present */
        PUT2L(tail + 10, 8);        /* deflate */
        PUT4L(tail + 12, time2dos(g.mtime));
        PUT4L(tail + 16, check);    /* crc */
        PUT4L(tail + 20, clen);     /* compressed length */
        PUT4L(tail + 24, ulen);     /* uncompressed length */
        PUT2L(tail + 28, g.name == NULL ? 1 :   /* length of name */
                                          strlen(g.name));
        PUT2L(tail + 30, 9);        /* length of extra field (see below) */
        PUT2L(tail + 32, 0);        /* no file comment */
        PUT2L(tail + 34, 0);        /* disk number 0 */
        PUT2L(tail + 36, 0);        /* internal file attributes */
        PUT4L(tail + 38, 0);        /* external file attributes (ignored) */
        PUT4L(tail + 42, 0);        /* offset of local header */
        writen(g.outd, tail, 46);   /* write central file header */
        cent = 46;

        /* write file name (use "-" for stdin) */
        if (g.name == NULL)
            writen(g.outd, (unsigned char *)"-", 1);
        else
            writen(g.outd, (unsigned char *)g.name, strlen(g.name));
        cent += g.name == NULL ? 1 : strlen(g.name);

        /* write extended timestamp extra field block (9 bytes) */
        PUT2L(tail, 0x5455);        /* extended timestamp signature */
        PUT2L(tail + 2, 5);         /* number of data bytes in this block */
        tail[4] = 1;                /* flag presence of mod time */
        PUT4L(tail + 5, g.mtime);   /* mod time */
        writen(g.outd, tail, 9);    /* write extra field block */
        cent += 9;

        /* write end of central directory record */
        PUT4L(tail, 0x06054b50UL);  /* end of central directory signature */
        PUT2L(tail + 4, 0);         /* number of this disk */
        PUT2L(tail + 6, 0);         /* disk with start of central directory */
        PUT2L(tail + 8, 1);         /* number of entries on this disk */
        PUT2L(tail + 10, 1);        /* total number of entries */
        PUT4L(tail + 12, cent);     /* size of central directory */
        PUT4L(tail + 16, head + clen + 16); /* offset of central directory */
        PUT2L(tail + 20, 0);        /* no zip file comment */
        writen(g.outd, tail, 22);   /* write end of central directory record */
    }
    else if (g.form) {              /* zlib */
        PUT4M(tail, check);
        writen(g.outd, tail, 4);
    }
    else {                          /* gzip */
        PUT4L(tail, check);
        PUT4L(tail + 4, ulen);
        writen(g.outd, tail, 8);
    }
}

/* compute check value depending on format */
#define CHECK(a,b,c) (g.form == 1 ? adler32(a,b,c) : crc32(a,b,c))

#ifndef NOTHREAD
/* -- threaded portions of pigz -- */

/* -- check value combination routines for parallel calculation -- */

#define COMB(a,b,c) (g.form == 1 ? adler32_comb(a,b,c) : crc32_comb(a,b,c))
/* combine two crc-32's or two adler-32's (copied from zlib 1.2.3 so that pigz
   can be compatible with older versions of zlib) */

/* we copy the combination routines from zlib here, in order to avoid
   linkage issues with the zlib 1.2.3 builds on Sun, Ubuntu, and others */

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

#define BASE 65521U     /* largest prime smaller than 65536 */
#define LOW16 0xffff    /* mask lower 16 bits */

local unsigned long adler32_comb(unsigned long adler1, unsigned long adler2,
                                 size_t len2)
{
    unsigned long sum1;
    unsigned long sum2;
    unsigned rem;

    /* the derivation of this formula is left as an exercise for the reader */
    rem = (unsigned)(len2 % BASE);
    sum1 = adler1 & LOW16;
    sum2 = (rem * sum1) % BASE;
    sum1 += (adler2 & LOW16) + BASE - 1;
    sum2 += ((adler1 >> 16) & LOW16) + ((adler2 >> 16) & LOW16) + BASE - rem;
    if (sum1 >= BASE) sum1 -= BASE;
    if (sum1 >= BASE) sum1 -= BASE;
    if (sum2 >= (BASE << 1)) sum2 -= (BASE << 1);
    if (sum2 >= BASE) sum2 -= BASE;
    return sum1 | (sum2 << 16);
}

/* -- pool of spaces for buffer management -- */

/* These routines manage a pool of spaces.  Each pool specifies a fixed size
   buffer to be contained in each space.  Each space has a use count, which
   when decremented to zero returns the space to the pool.  If a space is
   requested from the pool and the pool is empty, a space is immediately
   created unless a specified limit on the number of spaces has been reached.
   Only if the limit is reached will it wait for a space to be returned to the
   pool.  Each space knows what pool it belongs to, so that it can be returned.
 */

/* a space (one buffer for each space) */
struct space {
    lock *use;              /* use count -- return to pool when zero */
    unsigned char *buf;     /* buffer of size size */
    size_t size;            /* current size of this buffer */
    size_t len;             /* for application usage (initially zero) */
    struct pool *pool;      /* pool to return to */
    struct space *next;     /* for pool linked list */
};

/* pool of spaces (one pool for each type needed) */
struct pool {
    lock *have;             /* unused spaces available, lock for list */
    struct space *head;     /* linked list of available buffers */
    size_t size;            /* size of new buffers in this pool */
    int limit;              /* number of new spaces allowed, or -1 */
    int made;               /* number of buffers made */
};

/* initialize a pool (pool structure itself provided, not allocated) -- the
   limit is the maximum number of spaces in the pool, or -1 to indicate no
   limit, i.e., to never wait for a buffer to return to the pool */
local void new_pool(struct pool *pool, size_t size, int limit)
{
    pool->have = new_lock(0);
    pool->head = NULL;
    pool->size = size;
    pool->limit = limit;
    pool->made = 0;
}

/* get a space from a pool -- the use count is initially set to one, so there
   is no need to call use_space() for the first use */
local struct space *get_space(struct pool *pool)
{
    struct space *space;

    /* if can't create any more, wait for a space to show up */
    possess(pool->have);
    if (pool->limit == 0)
        wait_for(pool->have, NOT_TO_BE, 0);

    /* if a space is available, pull it from the list and return it */
    if (pool->head != NULL) {
        space = pool->head;
        possess(space->use);
        pool->head = space->next;
        twist(pool->have, BY, -1);      /* one less in pool */
        twist(space->use, TO, 1);       /* initially one user */
        space->len = 0;
        return space;
    }

    /* nothing available, don't want to wait, make a new space */
    assert(pool->limit != 0);
    if (pool->limit > 0)
        pool->limit--;
    pool->made++;
    release(pool->have);
    space = MALLOC(sizeof(struct space));
    if (space == NULL)
        bail("not enough memory", "");
    space->use = new_lock(1);           /* initially one user */
    space->buf = MALLOC(pool->size);
    if (space->buf == NULL)
        bail("not enough memory", "");
    space->size = pool->size;
    space->len = 0;
    space->pool = pool;                 /* remember the pool this belongs to */
    return space;
}

/* compute next size up by multiplying by about 2**(1/3) and round to the next
   power of 2 if we're close (so three applications results in doubling) -- if
   small, go up to at least 16, if overflow, go to max size_t value */
local size_t grow(size_t size)
{
    size_t was, top;
    int shift;

    was = size;
    size += size >> 2;
    top = size;
    for (shift = 0; top > 7; shift++)
        top >>= 1;
    if (top == 7)
        size = (size_t)1 << (shift + 3);
    if (size < 16)
        size = 16;
    if (size <= was)
        size = (size_t)0 - 1;
    return size;
}

/* increase the size of the buffer in space */
local void grow_space(struct space *space)
{
    size_t more;

    /* compute next size up */
    more = grow(space->size);
    if (more == space->size)
        bail("not enough memory", "");

    /* reallocate the buffer */
    space->buf = REALLOC(space->buf, more);
    if (space->buf == NULL)
        bail("not enough memory", "");
    space->size = more;
}

/* increment the use count to require one more drop before returning this space
   to the pool */
local void use_space(struct space *space)
{
    possess(space->use);
    twist(space->use, BY, +1);
}

/* drop a space, returning it to the pool if the use count is zero */
local void drop_space(struct space *space)
{
    int use;
    struct pool *pool;

    possess(space->use);
    use = peek_lock(space->use);
    assert(use != 0);
    if (use == 1) {
        pool = space->pool;
        possess(pool->have);
        space->next = pool->head;
        pool->head = space;
        twist(pool->have, BY, +1);
    }
    twist(space->use, BY, -1);
}

/* free the memory and lock resources of a pool -- return number of spaces for
   debugging and resource usage measurement */
local int free_pool(struct pool *pool)
{
    int count;
    struct space *space;

    possess(pool->have);
    count = 0;
    while ((space = pool->head) != NULL) {
        pool->head = space->next;
        FREE(space->buf);
        free_lock(space->use);
        FREE(space);
        count++;
    }
    assert(count == pool->made);
    release(pool->have);
    free_lock(pool->have);
    return count;
}

/* input and output buffer pools */
local struct pool in_pool;
local struct pool out_pool;
local struct pool dict_pool;
local struct pool lens_pool;

/* -- parallel compression -- */

/* compress or write job (passed from compress list to write list) -- if seq is
   equal to -1, compress_thread is instructed to return; if more is false then
   this is the last chunk, which after writing tells write_thread to return */
struct job {
    long seq;                   /* sequence number */
    int more;                   /* true if this is not the last chunk */
    struct space *in;           /* input data to compress */
    struct space *out;          /* dictionary or resulting compressed data */
    struct space *lens;         /* coded list of flush block lengths */
    unsigned long check;        /* check value for input data */
    lock *calc;                 /* released when check calculation complete */
    struct job *next;           /* next job in the list (either list) */
};

/* list of compress jobs (with tail for appending to list) */
local lock *compress_have = NULL;   /* number of compress jobs waiting */
local struct job *compress_head, **compress_tail;

/* list of write jobs */
local lock *write_first;            /* lowest sequence number in list */
local struct job *write_head;

/* number of compression threads running */
local int cthreads = 0;

/* write thread if running */
local thread *writeth = NULL;

/* setup job lists (call from main thread) */
local void setup_jobs(void)
{
    /* set up only if not already set up*/
    if (compress_have != NULL)
        return;

    /* allocate locks and initialize lists */
    compress_have = new_lock(0);
    compress_head = NULL;
    compress_tail = &compress_head;
    write_first = new_lock(-1);
    write_head = NULL;

    /* initialize buffer pools (initial size for out_pool not critical, since
       buffers will be grown in size if needed -- initial size chosen to make
       this unlikely -- same for lens_pool) */
    new_pool(&in_pool, g.block, INBUFS(g.procs));
    new_pool(&out_pool, OUTPOOL(g.block), -1);
    new_pool(&dict_pool, DICT, -1);
    new_pool(&lens_pool, g.block >> (RSYNCBITS - 1), -1);
}

/* command the compress threads to all return, then join them all (call from
   main thread), free all the thread-related resources */
local void finish_jobs(void)
{
    struct job job;
    int caught;

    /* only do this once */
    if (compress_have == NULL)
        return;

    /* command all of the extant compress threads to return */
    possess(compress_have);
    job.seq = -1;
    job.next = NULL;
    compress_head = &job;
    compress_tail = &(job.next);
    twist(compress_have, BY, +1);       /* will wake them all up */

    /* join all of the compress threads, verify they all came back */
    caught = join_all();
    Trace(("-- joined %d compress threads", caught));
    assert(caught == cthreads);
    cthreads = 0;

    /* free the resources */
    caught = free_pool(&lens_pool);
    Trace(("-- freed %d block lengths buffers", caught));
    caught = free_pool(&dict_pool);
    Trace(("-- freed %d dictionary buffers", caught));
    caught = free_pool(&out_pool);
    Trace(("-- freed %d output buffers", caught));
    caught = free_pool(&in_pool);
    Trace(("-- freed %d input buffers", caught));
    free_lock(write_first);
    free_lock(compress_have);
    compress_have = NULL;
}

/* compress all strm->avail_in bytes at strm->next_in to out->buf, updating
   out->len, grow the size of the buffer (out->size) if necessary -- respect
   the size limitations of the zlib stream data types (size_t may be larger
   than unsigned) */
local void deflate_engine(z_stream *strm, struct space *out, int flush)
{
    size_t room;

    do {
        room = out->size - out->len;
        if (room == 0) {
            grow_space(out);
            room = out->size - out->len;
        }
        strm->next_out = out->buf + out->len;
        strm->avail_out = room < UINT_MAX ? (unsigned)room : UINT_MAX;
        (void)deflate(strm, flush);
        out->len = strm->next_out - out->buf;
    } while (strm->avail_out == 0);
    assert(strm->avail_in == 0);
}

/* get the next compression job from the head of the list, compress and compute
   the check value on the input, and put a job in the write list with the
   results -- keep looking for more jobs, returning when a job is found with a
   sequence number of -1 (leave that job in the list for other incarnations to
   find) */
local void compress_thread(void *dummy)
{
    struct job *job;                /* job pulled and working on */
    struct job *here, **prior;      /* pointers for inserting in write list */
    unsigned long check;            /* check value of input */
    unsigned char *next;            /* pointer for blocks, check value data */
    size_t left;                    /* input left to process */
    size_t len;                     /* remaining bytes to compress/check */
#if ZLIB_VERNUM >= 0x1260
    int bits;                       /* deflate pending bits */
#endif
    struct space *temp;             /* temporary space for zopfli input */
    z_stream strm;                  /* deflate stream */

    (void)dummy;

    /* initialize the deflate stream for this thread */
    strm.zfree = ZFREE;
    strm.zalloc = ZALLOC;
    strm.opaque = OPAQUE;
    if (deflateInit2(&strm, 6, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        bail("not enough memory", "");

    /* keep looking for work */
    for (;;) {
        /* get a job (like I tell my son) */
        possess(compress_have);
        wait_for(compress_have, NOT_TO_BE, 0);
        job = compress_head;
        assert(job != NULL);
        if (job->seq == -1)
            break;
        compress_head = job->next;
        if (job->next == NULL)
            compress_tail = &compress_head;
        twist(compress_have, BY, -1);

        /* got a job -- initialize and set the compression level (note that if
           deflateParams() is called immediately after deflateReset(), there is
           no need to initialize the input/output for the stream) */
        Trace(("-- compressing #%ld", job->seq));
        if (g.level <= 9) {
            (void)deflateReset(&strm);
            (void)deflateParams(&strm, g.level, Z_DEFAULT_STRATEGY);
        }
        else {
            temp = get_space(&out_pool);
            temp->len = 0;
        }

        /* set dictionary if provided, release that input or dictionary buffer
           (not NULL if g.setdict is true and if this is not the first work
           unit) */
        if (job->out != NULL) {
            len = job->out->len;
            left = len < DICT ? len : DICT;
            if (g.level <= 9)
                deflateSetDictionary(&strm, job->out->buf + (len - left),
                                     left);
            else {
                memcpy(temp->buf, job->out->buf + (len - left), left);
                temp->len = left;
            }
            drop_space(job->out);
        }

        /* set up input and output */
        job->out = get_space(&out_pool);
        if (g.level <= 9) {
            strm.next_in = job->in->buf;
            strm.next_out = job->out->buf;
        }
        else
            memcpy(temp->buf + temp->len, job->in->buf, job->in->len);

        /* compress each block, either flushing or finishing */
        next = job->lens == NULL ? NULL : job->lens->buf;
        left = job->in->len;
        job->out->len = 0;
        do {
            /* decode next block length from blocks list */
            len = next == NULL ? 128 : *next++;
            if (len < 128)                          /* 64..32831 */
                len = (len << 8) + (*next++) + 64;
            else if (len == 128)                    /* end of list */
                len = left;
            else if (len < 192)                     /* 1..63 */
                len &= 0x3f;
            else {                                  /* 32832..4227135 */
                len = ((len & 0x3f) << 16) + (*next++ << 8) + 32832U;
                len += *next++;
            }
            left -= len;

            if (g.level <= 9) {
                /* run MAXP2-sized amounts of input through deflate -- this
                   loop is needed for those cases where the unsigned type is
                   smaller than the size_t type, or when len is close to the
                   limit of the size_t type */
                while (len > MAXP2) {
                    strm.avail_in = MAXP2;
                    deflate_engine(&strm, job->out, Z_NO_FLUSH);
                    len -= MAXP2;
                }

                /* run the last piece through deflate -- end on a byte
                   boundary, using a sync marker if necessary, or finish the
                   deflate stream if this is the last block */
                strm.avail_in = (unsigned)len;
                if (left || job->more) {
#if ZLIB_VERNUM >= 0x1260
                    deflate_engine(&strm, job->out, Z_BLOCK);

                    /* add enough empty blocks to get to a byte boundary */
                    (void)deflatePending(&strm, Z_NULL, &bits);
                    if (bits & 1)
                        deflate_engine(&strm, job->out, Z_SYNC_FLUSH);
                    else if (bits & 7) {
                        do {        /* add static empty blocks */
                            bits = deflatePrime(&strm, 10, 2);
                            assert(bits == Z_OK);
                            (void)deflatePending(&strm, Z_NULL, &bits);
                        } while (bits & 7);
                        deflate_engine(&strm, job->out, Z_BLOCK);
                    }
#else
                    deflate_engine(&strm, job->out, Z_SYNC_FLUSH);
#endif
                }
                else
                    deflate_engine(&strm, job->out, Z_FINISH);
            }
            else {
                /* compress len bytes using zopfli, bring to byte boundary */
                unsigned char bits, *out;
                size_t outsize;

                out = NULL;
                outsize = 0;
                bits = 0;
                ZopfliDeflatePart(&g.zopts, 2, !(left || job->more),
                                  temp->buf, temp->len, temp->len + len,
                                  &bits, &out, &outsize);
                assert(job->out->len + outsize + 5 <= job->out->size);
                memcpy(job->out->buf + job->out->len, out, outsize);
                free(out);
                job->out->len += outsize;
                if (left || job->more) {
                    bits &= 7;
                    if (bits & 1) {
                        if (bits == 7)
                            job->out->buf[job->out->len++] = 0;
                        job->out->buf[job->out->len++] = 0;
                        job->out->buf[job->out->len++] = 0;
                        job->out->buf[job->out->len++] = 0xff;
                        job->out->buf[job->out->len++] = 0xff;
                    }
                    else if (bits) {
                        do {
                            job->out->buf[job->out->len - 1] += 2 << bits;
                            job->out->buf[job->out->len++] = 0;
                            bits += 2;
                        } while (bits < 8);
                    }
                }
                temp->len += len;
            }
        } while (left);
        if (g.level > 9)
            drop_space(temp);
        if (job->lens != NULL) {
            drop_space(job->lens);
            job->lens = NULL;
        }
        Trace(("-- compressed #%ld%s", job->seq, job->more ? "" : " (last)"));

        /* reserve input buffer until check value has been calculated */
        use_space(job->in);

        /* insert write job in list in sorted order, alert write thread */
        possess(write_first);
        prior = &write_head;
        while ((here = *prior) != NULL) {
            if (here->seq > job->seq)
                break;
            prior = &(here->next);
        }
        job->next = here;
        *prior = job;
        twist(write_first, TO, write_head->seq);

        /* calculate the check value in parallel with writing, alert the write
           thread that the calculation is complete, and drop this usage of the
           input buffer */
        len = job->in->len;
        next = job->in->buf;
        check = CHECK(0L, Z_NULL, 0);
        while (len > MAXP2) {
            check = CHECK(check, next, MAXP2);
            len -= MAXP2;
            next += MAXP2;
        }
        check = CHECK(check, next, (unsigned)len);
        drop_space(job->in);
        job->check = check;
        Trace(("-- checked #%ld%s", job->seq, job->more ? "" : " (last)"));
        possess(job->calc);
        twist(job->calc, TO, 1);

        /* done with that one -- go find another job */
    }

    /* found job with seq == -1 -- free deflate memory and return to join */
    release(compress_have);
    (void)deflateEnd(&strm);
}

/* collect the write jobs off of the list in sequence order and write out the
   compressed data until the last chunk is written -- also write the header and
   trailer and combine the individual check values of the input buffers */
local void write_thread(void *dummy)
{
    long seq;                       /* next sequence number looking for */
    struct job *job;                /* job pulled and working on */
    size_t len;                     /* input length */
    int more;                       /* true if more chunks to write */
    unsigned long head;             /* header length */
    unsigned long ulen;             /* total uncompressed size (overflow ok) */
    unsigned long clen;             /* total compressed size (overflow ok) */
    unsigned long check;            /* check value of uncompressed data */

    (void)dummy;

    /* build and write header */
    Trace(("-- write thread running"));
    head = put_header();

    /* process output of compress threads until end of input */
    ulen = clen = 0;
    check = CHECK(0L, Z_NULL, 0);
    seq = 0;
    do {
        /* get next write job in order */
        possess(write_first);
        wait_for(write_first, TO_BE, seq);
        job = write_head;
        write_head = job->next;
        twist(write_first, TO, write_head == NULL ? -1 : write_head->seq);

        /* update lengths, save uncompressed length for COMB */
        more = job->more;
        len = job->in->len;
        drop_space(job->in);
        ulen += (unsigned long)len;
        clen += (unsigned long)(job->out->len);

        /* write the compressed data and drop the output buffer */
        Trace(("-- writing #%ld", seq));
        writen(g.outd, job->out->buf, job->out->len);
        drop_space(job->out);
        Trace(("-- wrote #%ld%s", seq, more ? "" : " (last)"));

        /* wait for check calculation to complete, then combine, once
           the compress thread is done with the input, release it */
        possess(job->calc);
        wait_for(job->calc, TO_BE, 1);
        release(job->calc);
        check = COMB(check, job->check, len);

        /* free the job */
        free_lock(job->calc);
        FREE(job);

        /* get the next buffer in sequence */
        seq++;
    } while (more);

    /* write trailer */
    put_trailer(ulen, clen, check, head);

    /* verify no more jobs, prepare for next use */
    possess(compress_have);
    assert(compress_head == NULL && peek_lock(compress_have) == 0);
    release(compress_have);
    possess(write_first);
    assert(write_head == NULL);
    twist(write_first, TO, -1);
}

/* encode a hash hit to the block lengths list -- hit == 0 ends the list */
local void append_len(struct job *job, size_t len)
{
    struct space *lens;

    assert(len < 4227136UL);
    if (job->lens == NULL)
        job->lens = get_space(&lens_pool);
    lens = job->lens;
    if (lens->size < lens->len + 3)
        grow_space(lens);
    if (len < 64)
        lens->buf[lens->len++] = len + 128;
    else if (len < 32832U) {
        len -= 64;
        lens->buf[lens->len++] = len >> 8;
        lens->buf[lens->len++] = len;
    }
    else {
        len -= 32832U;
        lens->buf[lens->len++] = (len >> 16) + 192;
        lens->buf[lens->len++] = len >> 8;
        lens->buf[lens->len++] = len;
    }
}

/* compress ind to outd, using multiple threads for the compression and check
   value calculations and one other thread for writing the output -- compress
   threads will be launched and left running (waiting actually) to support
   subsequent calls of parallel_compress() */
local void parallel_compress(void)
{
    long seq;                       /* sequence number */
    struct space *curr;             /* input data to compress */
    struct space *next;             /* input data that follows curr */
    struct space *hold;             /* input data that follows next */
    struct space *dict;             /* dictionary for next compression */
    struct job *job;                /* job for compress, then write */
    int more;                       /* true if more input to read */
    unsigned hash;                  /* hash for rsyncable */
    unsigned char *scan;            /* next byte to compute hash on */
    unsigned char *end;             /* after end of data to compute hash on */
    unsigned char *last;            /* position after last hit */
    size_t left;                    /* last hit in curr to end of curr */
    size_t len;                     /* for various length computations */

    /* if first time or after an option change, setup the job lists */
    setup_jobs();

    /* start write thread */
    writeth = launch(write_thread, NULL);

    /* read from input and start compress threads (write thread will pick up
       the output of the compress threads) */
    seq = 0;
    next = get_space(&in_pool);
    next->len = readn(g.ind, next->buf, next->size);
    hold = NULL;
    dict = NULL;
    scan = next->buf;
    hash = RSYNCHIT;
    left = 0;
    do {
        /* create a new job */
        job = MALLOC(sizeof(struct job));
        if (job == NULL)
            bail("not enough memory", "");
        job->calc = new_lock(0);

        /* update input spaces */
        curr = next;
        next = hold;
        hold = NULL;

        /* get more input if we don't already have some */
        if (next == NULL) {
            next = get_space(&in_pool);
            next->len = readn(g.ind, next->buf, next->size);
        }

        /* if rsyncable, generate block lengths and prepare curr for job to
           likely have less than size bytes (up to the last hash hit) */
        job->lens = NULL;
        if (g.rsync && curr->len) {
            /* compute the hash function starting where we last left off to
               cover either size bytes or to EOF, whichever is less, through
               the data in curr (and in the next loop, through next) -- save
               the block lengths resulting from the hash hits in the job->lens
               list */
            if (left == 0) {
                /* scan is in curr */
                last = curr->buf;
                end = curr->buf + curr->len;
                while (scan < end) {
                    hash = ((hash << 1) ^ *scan++) & RSYNCMASK;
                    if (hash == RSYNCHIT) {
                        len = scan - last;
                        append_len(job, len);
                        last = scan;
                    }
                }

                /* continue scan in next */
                left = scan - last;
                scan = next->buf;
            }

            /* scan in next for enough bytes to fill curr, or what is available
               in next, whichever is less (if next isn't full, then we're at
               the end of the file) -- the bytes in curr since the last hit,
               stored in left, counts towards the size of the first block */
            last = next->buf;
            len = curr->size - curr->len;
            if (len > next->len)
                len = next->len;
            end = next->buf + len;
            while (scan < end) {
                hash = ((hash << 1) ^ *scan++) & RSYNCMASK;
                if (hash == RSYNCHIT) {
                    len = (scan - last) + left;
                    left = 0;
                    append_len(job, len);
                    last = scan;
                }
            }
            append_len(job, 0);

            /* create input in curr for job up to last hit or entire buffer if
               no hits at all -- save remainder in next and possibly hold */
            len = (job->lens->len == 1 ? scan : last) - next->buf;
            if (len) {
                /* got hits in next, or no hits in either -- copy to curr */
                memcpy(curr->buf + curr->len, next->buf, len);
                curr->len += len;
                memmove(next->buf, next->buf + len, next->len - len);
                next->len -= len;
                scan -= len;
                left = 0;
            }
            else if (job->lens->len != 1 && left && next->len) {
                /* had hits in curr, but none in next, and last hit in curr
                   wasn't right at the end, so we have input there to save --
                   use curr up to the last hit, save the rest, moving next to
                   hold */
                hold = next;
                next = get_space(&in_pool);
                memcpy(next->buf, curr->buf + (curr->len - left), left);
                next->len = left;
                curr->len -= left;
            }
            else {
                /* else, last match happened to be right at the end of curr,
                   or we're at the end of the input compressing the rest */
                left = 0;
            }
        }

        /* compress curr->buf to curr->len -- compress thread will drop curr */
        job->in = curr;

        /* set job->more if there is more to compress after curr */
        more = next->len != 0;
        job->more = more;

        /* provide dictionary for this job, prepare dictionary for next job */
        job->out = dict;
        if (more && g.setdict) {
            if (curr->len >= DICT || job->out == NULL) {
                dict = curr;
                use_space(dict);
            }
            else {
                dict = get_space(&dict_pool);
                len = DICT - curr->len;
                memcpy(dict->buf, job->out->buf + (job->out->len - len), len);
                memcpy(dict->buf + len, curr->buf, curr->len);
                dict->len = DICT;
            }
        }

        /* preparation of job is complete */
        job->seq = seq;
        Trace(("-- read #%ld%s", seq, more ? "" : " (last)"));
        if (++seq < 1)
            bail("input too long: ", g.inf);

        /* start another compress thread if needed */
        if (cthreads < seq && cthreads < g.procs) {
            (void)launch(compress_thread, NULL);
            cthreads++;
        }

        /* put job at end of compress list, let all the compressors know */
        possess(compress_have);
        job->next = NULL;
        *compress_tail = job;
        compress_tail = &(job->next);
        twist(compress_have, BY, +1);
    } while (more);
    drop_space(next);

    /* wait for the write thread to complete (we leave the compress threads out
       there and waiting in case there is another stream to compress) */
    join(writeth);
    writeth = NULL;
    Trace(("-- write thread joined"));
}

#endif

/* repeated code in single_compress to compress available input and write it */
#define DEFLATE_WRITE(flush) \
    do { \
        do { \
            strm->avail_out = out_size; \
            strm->next_out = out; \
            (void)deflate(strm, flush); \
            writen(g.outd, out, out_size - strm->avail_out); \
            clen += out_size - strm->avail_out; \
        } while (strm->avail_out == 0); \
        assert(strm->avail_in == 0); \
    } while (0)

/* do a simple compression in a single thread from ind to outd -- if reset is
   true, instead free the memory that was allocated and retained for input,
   output, and deflate */
local void single_compress(int reset)
{
    size_t got;                     /* amount of data in in[] */
    size_t more;                    /* amount of data in next[] (0 if eof) */
    size_t start;                   /* start of data in next[] */
    size_t have;                    /* bytes in current block for -i */
    size_t hist;                    /* offset of permitted history */
    int fresh;                      /* if true, reset compression history */
    unsigned hash;                  /* hash for rsyncable */
    unsigned char *scan;            /* pointer for hash computation */
    size_t left;                    /* bytes left to compress after hash hit */
    unsigned long head;             /* header length */
    unsigned long ulen;             /* total uncompressed size (overflow ok) */
    unsigned long clen;             /* total compressed size (overflow ok) */
    unsigned long check;            /* check value of uncompressed data */
    static unsigned out_size;       /* size of output buffer */
    static unsigned char *in, *next, *out;  /* reused i/o buffers */
    static z_stream *strm = NULL;   /* reused deflate structure */

    /* if requested, just release the allocations and return */
    if (reset) {
        if (strm != NULL) {
            (void)deflateEnd(strm);
            FREE(strm);
            FREE(out);
            FREE(next);
            FREE(in);
            strm = NULL;
        }
        return;
    }

    /* initialize the deflate structure if this is the first time */
    if (strm == NULL) {
        out_size = g.block > MAXP2 ? MAXP2 : (unsigned)g.block;
        if ((in = MALLOC(g.block + DICT)) == NULL ||
            (next = MALLOC(g.block + DICT)) == NULL ||
            (out = MALLOC(out_size)) == NULL ||
            (strm = MALLOC(sizeof(z_stream))) == NULL)
            bail("not enough memory", "");
        strm->zfree = ZFREE;
        strm->zalloc = ZALLOC;
        strm->opaque = OPAQUE;
        if (deflateInit2(strm, 6, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) !=
                         Z_OK)
            bail("not enough memory", "");
    }

    /* write header */
    head = put_header();

    /* set compression level in case it changed */
    if (g.level <= 9) {
        (void)deflateReset(strm);
        (void)deflateParams(strm, g.level, Z_DEFAULT_STRATEGY);
    }

    /* do raw deflate and calculate check value */
    got = 0;
    more = readn(g.ind, next, g.block);
    ulen = (unsigned long)more;
    start = 0;
    hist = 0;
    clen = 0;
    have = 0;
    check = CHECK(0L, Z_NULL, 0);
    hash = RSYNCHIT;
    do {
        /* get data to compress, see if there is any more input */
        if (got == 0) {
            scan = in;  in = next;  next = scan;
            strm->next_in = in + start;
            got = more;
            if (g.level > 9) {
                left = start + more - hist;
                if (left > DICT)
                    left = DICT;
                memcpy(next, in + ((start + more) - left), left);
                start = left;
                hist = 0;
            }
            else
                start = 0;
            more = readn(g.ind, next + start, g.block);
            ulen += (unsigned long)more;
        }

        /* if rsyncable, compute hash until a hit or the end of the block */
        left = 0;
        if (g.rsync && got) {
            scan = strm->next_in;
            left = got;
            do {
                if (left == 0) {
                    /* went to the end -- if no more or no hit in size bytes,
                       then proceed to do a flush or finish with got bytes */
                    if (more == 0 || got == g.block)
                        break;

                    /* fill in[] with what's left there and as much as possible
                       from next[] -- set up to continue hash hit search */
                    if (g.level > 9) {
                        left = (strm->next_in - in) - hist;
                        if (left > DICT)
                            left = DICT;
                    }
                    memmove(in, strm->next_in - left, left + got);
                    hist = 0;
                    strm->next_in = in + left;
                    scan = in + left + got;
                    left = more > g.block - got ? g.block - got : more;
                    memcpy(scan, next + start, left);
                    got += left;
                    more -= left;
                    start += left;

                    /* if that emptied the next buffer, try to refill it */
                    if (more == 0) {
                        more = readn(g.ind, next, g.block);
                        ulen += (unsigned long)more;
                        start = 0;
                    }
                }
                left--;
                hash = ((hash << 1) ^ *scan++) & RSYNCMASK;
            } while (hash != RSYNCHIT);
            got -= left;
        }

        /* clear history for --independent option */
        fresh = 0;
        if (!g.setdict) {
            have += got;
            if (have > g.block) {
                fresh = 1;
                have = got;
            }
        }

        if (g.level <= 9) {
            /* clear history if requested */
            if (fresh)
                (void)deflateReset(strm);

            /* compress MAXP2-size chunks in case unsigned type is small */
            while (got > MAXP2) {
                strm->avail_in = MAXP2;
                check = CHECK(check, strm->next_in, strm->avail_in);
                DEFLATE_WRITE(Z_NO_FLUSH);
                got -= MAXP2;
            }

            /* compress the remainder, emit a block, finish if end of input */
            strm->avail_in = (unsigned)got;
            got = left;
            check = CHECK(check, strm->next_in, strm->avail_in);
            if (more || got) {
#if ZLIB_VERNUM >= 0x1260
                int bits;

                DEFLATE_WRITE(Z_BLOCK);
                (void)deflatePending(strm, Z_NULL, &bits);
                if (bits & 1)
                    DEFLATE_WRITE(Z_SYNC_FLUSH);
                else if (bits & 7) {
                    do {
                        bits = deflatePrime(strm, 10, 2);
                        assert(bits == Z_OK);
                        (void)deflatePending(strm, Z_NULL, &bits);
                    } while (bits & 7);
                    DEFLATE_WRITE(Z_NO_FLUSH);
                }
#else
                DEFLATE_WRITE(Z_SYNC_FLUSH);
#endif
            }
            else
                DEFLATE_WRITE(Z_FINISH);
        }
        else {
            /* compress got bytes using zopfli, bring to byte boundary */
            unsigned char bits, *out;
            size_t outsize, off;

            /* discard history if requested */
            off = strm->next_in - in;
            if (fresh)
                hist = off;

            out = NULL;
            outsize = 0;
            bits = 0;
            ZopfliDeflatePart(&g.zopts, 2, !(more || left),
                              in + hist, off - hist, (off - hist) + got,
                              &bits, &out, &outsize);
            bits &= 7;
            if ((more || left) && bits) {
                if (bits & 1) {
                    writen(g.outd, out, outsize);
                    if (bits == 7)
                        writen(g.outd, (unsigned char *)"\0", 1);
                    writen(g.outd, (unsigned char *)"\0\0\xff\xff", 4);
                }
                else {
                    assert(outsize > 0);
                    writen(g.outd, out, outsize - 1);
                    do {
                        out[outsize - 1] += 2 << bits;
                        writen(g.outd, out + outsize - 1, 1);
                        out[outsize - 1] = 0;
                        bits += 2;
                    } while (bits < 8);
                    writen(g.outd, out + outsize - 1, 1);
                }
            }
            else
                writen(g.outd, out, outsize);
            free(out);
            while (got > MAXP2) {
                check = CHECK(check, strm->next_in, MAXP2);
                strm->next_in += MAXP2;
                got -= MAXP2;
            }
            check = CHECK(check, strm->next_in, (unsigned)got);
            strm->next_in += got;
            got = left;
        }

        /* do until no more input */
    } while (more || got);

    /* write trailer */
    put_trailer(ulen, clen, check, head);
}

/* --- decompression --- */

#ifndef NOTHREAD
/* parallel read thread */
local void load_read(void *dummy)
{
    size_t len;

    (void)dummy;

    Trace(("-- launched decompress read thread"));
    do {
        possess(g.load_state);
        wait_for(g.load_state, TO_BE, 1);
        g.in_len = len = readn(g.ind, g.in_which ? g.in_buf : g.in_buf2, BUF);
        Trace(("-- decompress read thread read %lu bytes", len));
        twist(g.load_state, TO, 0);
    } while (len == BUF);
    Trace(("-- exited decompress read thread"));
}
#endif

/* load() is called when the input has been consumed in order to provide more
   input data: load the input buffer with BUF or fewer bytes (fewer if at end
   of file) from the file g.ind, set g.in_next to point to the g.in_left bytes
   read, update g.in_tot, and return g.in_left -- g.in_eof is set to true when
   g.in_left has gone to zero and there is no more data left to read */
local size_t load(void)
{
    /* if already detected end of file, do nothing */
    if (g.in_short) {
        g.in_eof = 1;
        g.in_left = 0;
        return 0;
    }

#ifndef NOTHREAD
    /* if first time in or procs == 1, read a buffer to have something to
       return, otherwise wait for the previous read job to complete */
    if (g.procs > 1) {
        /* if first time, fire up the read thread, ask for a read */
        if (g.in_which == -1) {
            g.in_which = 1;
            g.load_state = new_lock(1);
            g.load_thread = launch(load_read, NULL);
        }

        /* wait for the previously requested read to complete */
        possess(g.load_state);
        wait_for(g.load_state, TO_BE, 0);
        release(g.load_state);

        /* set up input buffer with the data just read */
        g.in_next = g.in_which ? g.in_buf : g.in_buf2;
        g.in_left = g.in_len;

        /* if not at end of file, alert read thread to load next buffer,
           alternate between g.in_buf and g.in_buf2 */
        if (g.in_len == BUF) {
            g.in_which = 1 - g.in_which;
            possess(g.load_state);
            twist(g.load_state, TO, 1);
        }

        /* at end of file -- join read thread (already exited), clean up */
        else {
            join(g.load_thread);
            free_lock(g.load_state);
            g.in_which = -1;
        }
    }
    else
#endif
    {
        /* don't use threads -- simply read a buffer into g.in_buf */
        g.in_left = readn(g.ind, g.in_next = g.in_buf, BUF);
    }

    /* note end of file */
    if (g.in_left < BUF) {
        g.in_short = 1;

        /* if we got bupkis, now is the time to mark eof */
        if (g.in_left == 0)
            g.in_eof = 1;
    }

    /* update the total and return the available bytes */
    g.in_tot += g.in_left;
    return g.in_left;
}

/* initialize for reading new input */
local void in_init(void)
{
    g.in_left = 0;
    g.in_eof = 0;
    g.in_short = 0;
    g.in_tot = 0;
#ifndef NOTHREAD
    g.in_which = -1;
#endif
}

/* buffered reading macros for decompression and listing */
#define GET() (g.in_eof || (g.in_left == 0 && load() == 0) ? EOF : \
               (g.in_left--, *g.in_next++))
#define GET2() (tmp2 = GET(), tmp2 + ((unsigned)(GET()) << 8))
#define GET4() (tmp4 = GET2(), tmp4 + ((unsigned long)(GET2()) << 16))
#define SKIP(dist) \
    do { \
        size_t togo = (dist); \
        while (togo > g.in_left) { \
            togo -= g.in_left; \
            if (load() == 0) \
                return -1; \
        } \
        g.in_left -= togo; \
        g.in_next += togo; \
    } while (0)

/* pull LSB order or MSB order integers from an unsigned char buffer */
#define PULL2L(p) ((p)[0] + ((unsigned)((p)[1]) << 8))
#define PULL4L(p) (PULL2L(p) + ((unsigned long)(PULL2L((p) + 2)) << 16))
#define PULL2M(p) (((unsigned)((p)[0]) << 8) + (p)[1])
#define PULL4M(p) (((unsigned long)(PULL2M(p)) << 16) + PULL2M((p) + 2))

/* convert MS-DOS date and time to a Unix time, assuming current timezone
   (you got a better idea?) */
local time_t dos2time(unsigned long dos)
{
    struct tm tm;

    if (dos == 0)
        return time(NULL);
    tm.tm_year = ((int)(dos >> 25) & 0x7f) + 80;
    tm.tm_mon  = ((int)(dos >> 21) & 0xf) - 1;
    tm.tm_mday = (int)(dos >> 16) & 0x1f;
    tm.tm_hour = (int)(dos >> 11) & 0x1f;
    tm.tm_min  = (int)(dos >> 5) & 0x3f;
    tm.tm_sec  = (int)(dos << 1) & 0x3e;
    tm.tm_isdst = -1;           /* figure out if DST or not */
    return mktime(&tm);
}

/* convert an unsigned 32-bit integer to signed, even if long > 32 bits */
local long tolong(unsigned long val)
{
    return (long)(val & 0x7fffffffUL) - (long)(val & 0x80000000UL);
}

#define LOW32 0xffffffffUL

/* process zip extra field to extract zip64 lengths and Unix mod time */
local int read_extra(unsigned len, int save)
{
    unsigned id, size, tmp2;
    unsigned long tmp4;

    /* process extra blocks */
    while (len >= 4) {
        id = GET2();
        size = GET2();
        if (g.in_eof)
            return -1;
        len -= 4;
        if (size > len)
            break;
        len -= size;
        if (id == 0x0001) {
            /* Zip64 Extended Information Extra Field */
            if (g.zip_ulen == LOW32 && size >= 8) {
                g.zip_ulen = GET4();
                SKIP(4);
                size -= 8;
            }
            if (g.zip_clen == LOW32 && size >= 8) {
                g.zip_clen = GET4();
                SKIP(4);
                size -= 8;
            }
        }
        if (save) {
            if ((id == 0x000d || id == 0x5855) && size >= 8) {
                /* PKWare Unix or Info-ZIP Type 1 Unix block */
                SKIP(4);
                g.stamp = tolong(GET4());
                size -= 8;
            }
            if (id == 0x5455 && size >= 5) {
                /* Extended Timestamp block */
                size--;
                if (GET() & 1) {
                    g.stamp = tolong(GET4());
                    size -= 4;
                }
            }
        }
        SKIP(size);
    }
    SKIP(len);
    return 0;
}

/* read a gzip, zip, zlib, or lzw header from ind and return the method in the
   range 0..256 (256 implies a zip method greater than 255), or on error return
   negative: -1 is immediate EOF, -2 is not a recognized compressed format, -3
   is premature EOF within the header, -4 is unexpected header flag values, -5
   is the zip central directory; a method of 257 is lzw -- if the return value
   is not negative, then get_header() sets g.form to indicate gzip (0), zlib
   (1), or zip (2, or 3 if the entry is followed by a data descriptor) */
local int get_header(int save)
{
    unsigned magic;             /* magic header */
    int method;                 /* compression method */
    int flags;                  /* header flags */
    unsigned fname, extra;      /* name and extra field lengths */
    unsigned tmp2;              /* for macro */
    unsigned long tmp4;         /* for macro */

    /* clear return information */
    if (save) {
        g.stamp = 0;
        RELEASE(g.hname);
    }

    /* see if it's a gzip, zlib, or lzw file */
    g.form = -1;
    g.magic1 = GET();
    if (g.in_eof)
        return -1;
    magic = g.magic1 << 8;
    magic += GET();
    if (g.in_eof)
        return -2;
    if (magic % 31 == 0) {          /* it's zlib */
        g.form = 1;
        return (int)((magic >> 8) & 0xf);
    }
    if (magic == 0x1f9d)            /* it's lzw */
        return 257;
    if (magic == 0x504b) {          /* it's zip */
        magic = GET2();             /* the rest of the signature */
        if (g.in_eof)
            return -3;
        if (magic == 0x0201 || magic == 0x0806)
            return -5;              /* central header or archive extra */
        if (magic != 0x0403)
            return -4;              /* not a local header */
        SKIP(2);
        flags = GET2();
        if (g.in_eof)
            return -3;
        if (flags & 0xfff0)
            return -4;
        method = GET();             /* return low byte of method or 256 */
        if (GET() != 0 || flags & 1)
            method = 256;           /* unknown or encrypted */
        if (g.in_eof)
            return -3;
        if (save)
            g.stamp = dos2time(GET4());
        else
            SKIP(4);
        g.zip_crc = GET4();
        g.zip_clen = GET4();
        g.zip_ulen = GET4();
        fname = GET2();
        extra = GET2();
        if (save) {
            char *next = g.hname = MALLOC(fname + 1);
            if (g.hname == NULL)
                bail("not enough memory", "");
            while (fname > g.in_left) {
                memcpy(next, g.in_next, g.in_left);
                fname -= g.in_left;
                next += g.in_left;
                if (load() == 0)
                    return -3;
            }
            memcpy(next, g.in_next, fname);
            g.in_left -= fname;
            g.in_next += fname;
            next += fname;
            *next = 0;
        }
        else
            SKIP(fname);
        read_extra(extra, save);
        g.form = 2 + ((flags & 8) >> 3);
        return g.in_eof ? -3 : method;
    }
    if (magic != 0x1f8b) {          /* not gzip */
        g.in_left++;    /* unget second magic byte */
        g.in_next--;
        return -2;
    }

    /* it's gzip -- get method and flags */
    method = GET();
    flags = GET();
    if (g.in_eof)
        return -1;
    if (flags & 0xe0)
        return -4;

    /* get time stamp */
    if (save)
        g.stamp = tolong(GET4());
    else
        SKIP(4);

    /* skip extra field and OS */
    SKIP(2);

    /* skip extra field, if present */
    if (flags & 4) {
        extra = GET2();
        if (g.in_eof)
            return -3;
        SKIP(extra);
    }

    /* read file name, if present, into allocated memory */
    if ((flags & 8) && save) {
        unsigned char *end;
        size_t copy, have, size = 128;
        g.hname = MALLOC(size);
        if (g.hname == NULL)
            bail("not enough memory", "");
        have = 0;
        do {
            if (g.in_left == 0 && load() == 0)
                return -3;
            end = memchr(g.in_next, 0, g.in_left);
            copy = end == NULL ? g.in_left : (size_t)(end - g.in_next) + 1;
            if (have + copy > size) {
                while (have + copy > (size <<= 1))
                    ;
                g.hname = REALLOC(g.hname, size);
                if (g.hname == NULL)
                    bail("not enough memory", "");
            }
            memcpy(g.hname + have, g.in_next, copy);
            have += copy;
            g.in_left -= copy;
            g.in_next += copy;
        } while (end == NULL);
    }
    else if (flags & 8)
        while (GET() != 0)
            if (g.in_eof)
                return -3;

    /* skip comment */
    if (flags & 16)
        while (GET() != 0)
            if (g.in_eof)
                return -3;

    /* skip header crc */
    if (flags & 2)
        SKIP(2);

    /* return gzip compression method */
    g.form = 0;
    return method;
}

/* --- list contents of compressed input (gzip, zlib, or lzw) */

/* find standard compressed file suffix, return length of suffix */
local size_t compressed_suffix(char *nm)
{
    size_t len;

    len = strlen(nm);
    if (len > 4) {
        nm += len - 4;
        len = 4;
        if (strcmp(nm, ".zip") == 0 || strcmp(nm, ".ZIP") == 0 ||
            strcmp(nm, ".tgz") == 0)
            return 4;
    }
    if (len > 3) {
        nm += len - 3;
        len = 3;
        if (strcmp(nm, ".gz") == 0 || strcmp(nm, "-gz") == 0 ||
            strcmp(nm, ".zz") == 0 || strcmp(nm, "-zz") == 0)
            return 3;
    }
    if (len > 2) {
        nm += len - 2;
        if (strcmp(nm, ".z") == 0 || strcmp(nm, "-z") == 0 ||
            strcmp(nm, "_z") == 0 || strcmp(nm, ".Z") == 0)
            return 2;
    }
    return 0;
}

/* listing file name lengths for -l and -lv */
#define NAMEMAX1 48     /* name display limit at verbosity 1 */
#define NAMEMAX2 16     /* name display limit at verbosity 2 */

/* print gzip or lzw file information */
local void show_info(int method, unsigned long check, off_t len, int cont)
{
    size_t max;             /* maximum name length for current verbosity */
    size_t n;               /* name length without suffix */
    time_t now;             /* for getting current year */
    char mod[26];           /* modification time in text */
    char tag[NAMEMAX1+1];   /* header or file name, possibly truncated */

    /* create abbreviated name from header file name or actual file name */
    max = g.verbosity > 1 ? NAMEMAX2 : NAMEMAX1;
    memset(tag, 0, max + 1);
    if (cont)
        strncpy(tag, "<...>", max + 1);
    else if (g.hname == NULL) {
        n = strlen(g.inf) - compressed_suffix(g.inf);
        strncpy(tag, g.inf, n > max + 1 ? max + 1 : n);
        if (strcmp(g.inf + n, ".tgz") == 0 && n < max + 1)
            strncpy(tag + n, ".tar", max + 1 - n);
    }
    else
        strncpy(tag, g.hname, max + 1);
    if (tag[max])
        strcpy(tag + max - 3, "...");

    /* convert time stamp to text */
    if (g.stamp) {
        strcpy(mod, ctime(&g.stamp));
        now = time(NULL);
        if (strcmp(mod + 20, ctime(&now) + 20) != 0)
            strcpy(mod + 11, mod + 19);
    }
    else
        strcpy(mod + 4, "------ -----");
    mod[16] = 0;

    /* if first time, print header */
    if (g.first) {
        if (g.verbosity > 1)
            fputs("method    check    timestamp    ", stdout);
        if (g.verbosity > 0)
            puts("compressed   original reduced  name");
        g.first = 0;
    }

    /* print information */
    if (g.verbosity > 1) {
        if (g.form == 3 && !g.decode)
            printf("zip%3d  --------  %s  ", method, mod + 4);
        else if (g.form > 1)
            printf("zip%3d  %08lx  %s  ", method, check, mod + 4);
        else if (g.form == 1)
            printf("zlib%2d  %08lx  %s  ", method, check, mod + 4);
        else if (method == 257)
            printf("lzw     --------  %s  ", mod + 4);
        else
            printf("gzip%2d  %08lx  %s  ", method, check, mod + 4);
    }
    if (g.verbosity > 0) {
        if ((g.form == 3 && !g.decode) ||
            (method == 8 && g.in_tot > (len + (len >> 10) + 12)) ||
            (method == 257 && g.in_tot > len + (len >> 1) + 3))
#if __STDC_VERSION__-0 >= 199901L || __GNUC__-0 >= 3
            printf("%10jd %10jd?  unk    %s\n",
                   (intmax_t)g.in_tot, (intmax_t)len, tag);
        else
            printf("%10jd %10jd %6.1f%%  %s\n",
                   (intmax_t)g.in_tot, (intmax_t)len,
                   len == 0 ? 0 : 100 * (len - g.in_tot)/(double)len,
                   tag);
#else
            printf(sizeof(off_t) == sizeof(long) ?
                   "%10ld %10ld?  unk    %s\n" : "%10lld %10lld?  unk    %s\n",
                   g.in_tot, len, tag);
        else
            printf(sizeof(off_t) == sizeof(long) ?
                   "%10ld %10ld %6.1f%%  %s\n" : "%10lld %10lld %6.1f%%  %s\n",
                   g.in_tot, len,
                   len == 0 ? 0 : 100 * (len - g.in_tot)/(double)len,
                   tag);
#endif
    }
}

/* list content information about the gzip file at ind (only works if the gzip
   file contains a single gzip stream with no junk at the end, and only works
   well if the uncompressed length is less than 4 GB) */
local void list_info(void)
{
    int method;             /* get_header() return value */
    size_t n;               /* available trailer bytes */
    off_t at;               /* used to calculate compressed length */
    unsigned char tail[8];  /* trailer containing check and length */
    unsigned long check, len;   /* check value and length from trailer */

    /* initialize input buffer */
    in_init();

    /* read header information and position input after header */
    method = get_header(1);
    if (method < 0) {
        RELEASE(g.hname);
        if (method != -1 && g.verbosity > 1)
            complain("%s not a compressed file -- skipping", g.inf);
        return;
    }

    /* list zip file */
    if (g.form > 1) {
        g.in_tot = g.zip_clen;
        show_info(method, g.zip_crc, g.zip_ulen, 0);
        return;
    }

    /* list zlib file */
    if (g.form == 1) {
        at = lseek(g.ind, 0, SEEK_END);
        if (at == -1) {
            check = 0;
            do {
                len = g.in_left < 4 ? g.in_left : 4;
                g.in_next += g.in_left - len;
                while (len--)
                    check = (check << 8) + *g.in_next++;
            } while (load() != 0);
            check &= LOW32;
        }
        else {
            g.in_tot = at;
            lseek(g.ind, -4, SEEK_END);
            readn(g.ind, tail, 4);
            check = PULL4M(tail);
        }
        g.in_tot -= 6;
        show_info(method, check, 0, 0);
        return;
    }

    /* list lzw file */
    if (method == 257) {
        at = lseek(g.ind, 0, SEEK_END);
        if (at == -1)
            while (load() != 0)
                ;
        else
            g.in_tot = at;
        g.in_tot -= 3;
        show_info(method, 0, 0, 0);
        return;
    }

    /* skip to end to get trailer (8 bytes), compute compressed length */
    if (g.in_short) {                   /* whole thing already read */
        if (g.in_left < 8) {
            complain("%s not a valid gzip file -- skipping", g.inf);
            return;
        }
        g.in_tot = g.in_left - 8;       /* compressed size */
        memcpy(tail, g.in_next + (g.in_left - 8), 8);
    }
    else if ((at = lseek(g.ind, -8, SEEK_END)) != -1) {
        g.in_tot = at - g.in_tot + g.in_left;   /* compressed size */
        readn(g.ind, tail, 8);          /* get trailer */
    }
    else {                              /* can't seek */
        at = g.in_tot - g.in_left;      /* save header size */
        do {
            n = g.in_left < 8 ? g.in_left : 8;
            memcpy(tail, g.in_next + (g.in_left - n), n);
            load();
        } while (g.in_left == BUF);     /* read until end */
        if (g.in_left < 8) {
            if (n + g.in_left < 8) {
                complain("%s not a valid gzip file -- skipping", g.inf);
                return;
            }
            if (g.in_left) {
                if (n + g.in_left > 8)
                    memcpy(tail, tail + n - (8 - g.in_left), 8 - g.in_left);
                memcpy(tail + 8 - g.in_left, g.in_next, g.in_left);
            }
        }
        else
            memcpy(tail, g.in_next + (g.in_left - 8), 8);
        g.in_tot -= at + 8;
    }
    if (g.in_tot < 2) {
        complain("%s not a valid gzip file -- skipping", g.inf);
        return;
    }

    /* convert trailer to check and uncompressed length (modulo 2^32) */
    check = PULL4L(tail);
    len = PULL4L(tail + 4);

    /* list information about contents */
    show_info(method, check, len, 0);
    RELEASE(g.hname);
}

/* --- copy input to output (when acting like cat) --- */

local void cat(void)
{
    /* write first magic byte (if we're here, there's at least one byte) */
    writen(g.outd, &g.magic1, 1);
    g.out_tot = 1;

    /* copy the remainder of the input to the output (if there were any more
       bytes of input, then g.in_left is non-zero and g.in_next is pointing to
       the second magic byte) */
    while (g.in_left) {
        writen(g.outd, g.in_next, g.in_left);
        g.out_tot += g.in_left;
        g.in_left = 0;
        load();
    }
}

/* --- decompress deflate input --- */

/* call-back input function for inflateBack() */
local unsigned inb(void *desc, unsigned char **buf)
{
    (void)desc;
    load();
    *buf = g.in_next;
    return g.in_left;
}

/* output buffers and window for infchk() and unlzw() */
#define OUTSIZE 32768U      /* must be at least 32K for inflateBack() window */
local unsigned char out_buf[OUTSIZE];

#ifndef NOTHREAD
/* output data for parallel write and check */
local unsigned char out_copy[OUTSIZE];
local size_t out_len;

/* outb threads states */
local lock *outb_write_more = NULL;
local lock *outb_check_more;

/* output write thread */
local void outb_write(void *dummy)
{
    size_t len;

    (void)dummy;

    Trace(("-- launched decompress write thread"));
    do {
        possess(outb_write_more);
        wait_for(outb_write_more, TO_BE, 1);
        len = out_len;
        if (len && g.decode == 1)
            writen(g.outd, out_copy, len);
        Trace(("-- decompress wrote %lu bytes", len));
        twist(outb_write_more, TO, 0);
    } while (len);
    Trace(("-- exited decompress write thread"));
}

/* output check thread */
local void outb_check(void *dummy)
{
    size_t len;

    (void)dummy;

    Trace(("-- launched decompress check thread"));
    do {
        possess(outb_check_more);
        wait_for(outb_check_more, TO_BE, 1);
        len = out_len;
        g.out_check = CHECK(g.out_check, out_copy, len);
        Trace(("-- decompress checked %lu bytes", len));
        twist(outb_check_more, TO, 0);
    } while (len);
    Trace(("-- exited decompress check thread"));
}
#endif

/* call-back output function for inflateBack() -- wait for the last write and
   check calculation to complete, copy the write buffer, and then alert the
   write and check threads and return for more decompression while that's
   going on (or just write and check if no threads or if proc == 1) */
local int outb(void *desc, unsigned char *buf, unsigned len)
{
#ifndef NOTHREAD
    static thread *wr, *ch;

    if (g.procs > 1) {
        /* if first time, initialize state and launch threads */
        if (outb_write_more == NULL) {
            outb_write_more = new_lock(0);
            outb_check_more = new_lock(0);
            wr = launch(outb_write, NULL);
            ch = launch(outb_check, NULL);
        }

        /* wait for previous write and check threads to complete */
        possess(outb_check_more);
        wait_for(outb_check_more, TO_BE, 0);
        possess(outb_write_more);
        wait_for(outb_write_more, TO_BE, 0);

        /* copy the output and alert the worker bees */
        out_len = len;
        g.out_tot += len;
        memcpy(out_copy, buf, len);
        twist(outb_write_more, TO, 1);
        twist(outb_check_more, TO, 1);

        /* if requested with len == 0, clean up -- terminate and join write and
           check threads, free lock */
        if (len == 0) {
            join(ch);
            join(wr);
            free_lock(outb_check_more);
            free_lock(outb_write_more);
            outb_write_more = NULL;
        }

        /* return for more decompression while last buffer is being written
           and having its check value calculated -- we wait for those to finish
           the next time this function is called */
        return 0;
    }
#endif

    (void)desc;

    /* if just one process or no threads, then do it without threads */
    if (len) {
        if (g.decode == 1)
            writen(g.outd, buf, len);
        g.out_check = CHECK(g.out_check, buf, len);
        g.out_tot += len;
    }
    return 0;
}

/* inflate for decompression or testing -- decompress from ind to outd unless
   decode != 1, in which case just test ind, and then also list if list != 0;
   look for and decode multiple, concatenated gzip and/or zlib streams;
   read and check the gzip, zlib, or zip trailer */
local void infchk(void)
{
    int ret, cont, was;
    unsigned long check, len;
    z_stream strm;
    unsigned tmp2;
    unsigned long tmp4;
    off_t clen;

    cont = 0;
    do {
        /* header already read -- set up for decompression */
        g.in_tot = g.in_left;       /* track compressed data length */
        g.out_tot = 0;
        g.out_check = CHECK(0L, Z_NULL, 0);
        strm.zalloc = ZALLOC;
        strm.zfree = ZFREE;
        strm.opaque = OPAQUE;
        ret = inflateBackInit(&strm, 15, out_buf);
        if (ret != Z_OK)
            bail("not enough memory", "");

        /* decompress, compute lengths and check value */
        strm.avail_in = g.in_left;
        strm.next_in = g.in_next;
        ret = inflateBack(&strm, inb, NULL, outb, NULL);
        if (ret != Z_STREAM_END)
            bail("corrupted input -- invalid deflate data: ", g.inf);
        g.in_left = strm.avail_in;
        g.in_next = strm.next_in;
        inflateBackEnd(&strm);
        outb(NULL, NULL, 0);        /* finish off final write and check */

        /* compute compressed data length */
        clen = g.in_tot - g.in_left;

        /* read and check trailer */
        if (g.form > 1) {           /* zip local trailer (if any) */
            if (g.form == 3) {      /* data descriptor follows */
                /* read original version of data descriptor */
                g.zip_crc = GET4();
                g.zip_clen = GET4();
                g.zip_ulen = GET4();
                if (g.in_eof)
                    bail("corrupted zip entry -- missing trailer: ", g.inf);

                /* if crc doesn't match, try info-zip variant with sig */
                if (g.zip_crc != g.out_check) {
                    if (g.zip_crc != 0x08074b50UL || g.zip_clen != g.out_check)
                        bail("corrupted zip entry -- crc32 mismatch: ", g.inf);
                    g.zip_crc = g.zip_clen;
                    g.zip_clen = g.zip_ulen;
                    g.zip_ulen = GET4();
                }

                /* handle incredibly rare cases where crc equals signature */
                else if (g.zip_crc == 0x08074b50UL &&
                         g.zip_clen == g.zip_crc &&
                         ((clen & LOW32) != g.zip_crc ||
                          g.zip_ulen == g.zip_crc)) {
                    g.zip_crc = g.zip_clen;
                    g.zip_clen = g.zip_ulen;
                    g.zip_ulen = GET4();
                }

                /* if second length doesn't match, try 64-bit lengths */
                if (g.zip_ulen != (g.out_tot & LOW32)) {
                    g.zip_ulen = GET4();
                    (void)GET4();
                }
                if (g.in_eof)
                    bail("corrupted zip entry -- missing trailer: ", g.inf);
            }
            if (g.zip_clen != (clen & LOW32) ||
                g.zip_ulen != (g.out_tot & LOW32))
                bail("corrupted zip entry -- length mismatch: ", g.inf);
            check = g.zip_crc;
        }
        else if (g.form == 1) {     /* zlib (big-endian) trailer */
            check = (unsigned long)(GET()) << 24;
            check += (unsigned long)(GET()) << 16;
            check += (unsigned)(GET()) << 8;
            check += GET();
            if (g.in_eof)
                bail("corrupted zlib stream -- missing trailer: ", g.inf);
            if (check != g.out_check)
                bail("corrupted zlib stream -- adler32 mismatch: ", g.inf);
        }
        else {                      /* gzip trailer */
            check = GET4();
            len = GET4();
            if (g.in_eof)
                bail("corrupted gzip stream -- missing trailer: ", g.inf);
            if (check != g.out_check)
                bail("corrupted gzip stream -- crc32 mismatch: ", g.inf);
            if (len != (g.out_tot & LOW32))
                bail("corrupted gzip stream -- length mismatch: ", g.inf);
        }

        /* show file information if requested */
        if (g.list) {
            g.in_tot = clen;
            show_info(8, check, g.out_tot, cont);
            cont = 1;
        }

        /* if a gzip entry follows a gzip entry, decompress it (don't replace
           saved header information from first entry) */
        was = g.form;
    } while (was == 0 && (ret = get_header(0)) == 8 && g.form == 0);

    /* gzip -cdf copies junk after gzip stream directly to output */
    if (was == 0 && ret == -2 && g.force && g.pipeout && g.decode != 2 &&
        !g.list)
        cat();
    else if (was > 1 && get_header(0) != -5)
        complain("entries after the first in %s were ignored", g.inf);
    else if ((was == 0 && ret != -1) || (was == 1 && GET() != EOF))
        complain("%s OK, has trailing junk which was ignored", g.inf);
}

/* --- decompress Unix compress (LZW) input --- */

/* memory for unlzw() --
   the first 256 entries of prefix[] and suffix[] are never used, could
   have offset the index, but it's faster to waste the memory */
unsigned short prefix[65536];           /* index to LZW prefix string */
unsigned char suffix[65536];            /* one-character LZW suffix */
unsigned char match[65280 + 2];         /* buffer for reversed match */

/* throw out what's left in the current bits byte buffer (this is a vestigial
   aspect of the compressed data format derived from an implementation that
   made use of a special VAX machine instruction!) */
#define FLUSHCODE() \
    do { \
        left = 0; \
        rem = 0; \
        if (chunk > g.in_left) { \
            chunk -= g.in_left; \
            if (load() == 0) \
                break; \
            if (chunk > g.in_left) { \
                chunk = g.in_left = 0; \
                break; \
            } \
        } \
        g.in_left -= chunk; \
        g.in_next += chunk; \
        chunk = 0; \
    } while (0)

/* Decompress a compress (LZW) file from ind to outd.  The compress magic
   header (two bytes) has already been read and verified. */
local void unlzw(void)
{
    int got;                    /* byte just read by GET() */
    unsigned chunk;             /* bytes left in current chunk */
    int left;                   /* bits left in rem */
    unsigned rem;               /* unused bits from input */
    int bits;                   /* current bits per code */
    unsigned code;              /* code, table traversal index */
    unsigned mask;              /* mask for current bits codes */
    int max;                    /* maximum bits per code for this stream */
    int flags;                  /* compress flags, then block compress flag */
    unsigned end;               /* last valid entry in prefix/suffix tables */
    unsigned temp;              /* current code */
    unsigned prev;              /* previous code */
    unsigned final;             /* last character written for previous code */
    unsigned stack;             /* next position for reversed string */
    unsigned outcnt;            /* bytes in output buffer */
    unsigned char *p;

    /* process remainder of compress header -- a flags byte */
    g.out_tot = 0;
    flags = GET();
    if (g.in_eof)
        bail("missing lzw data: ", g.inf);
    if (flags & 0x60)
        bail("unknown lzw flags set: ", g.inf);
    max = flags & 0x1f;
    if (max < 9 || max > 16)
        bail("lzw bits out of range: ", g.inf);
    if (max == 9)                           /* 9 doesn't really mean 9 */
        max = 10;
    flags &= 0x80;                          /* true if block compress */

    /* clear table */
    bits = 9;
    mask = 0x1ff;
    end = flags ? 256 : 255;

    /* set up: get first 9-bit code, which is the first decompressed byte, but
       don't create a table entry until the next code */
    got = GET();
    if (g.in_eof)                           /* no compressed data is ok */
        return;
    final = prev = (unsigned)got;           /* low 8 bits of code */
    got = GET();
    if (g.in_eof || (got & 1) != 0)         /* missing a bit or code >= 256 */
        bail("invalid lzw code: ", g.inf);
    rem = (unsigned)got >> 1;               /* remaining 7 bits */
    left = 7;
    chunk = bits - 2;                       /* 7 bytes left in this chunk */
    out_buf[0] = (unsigned char)final;      /* write first decompressed byte */
    outcnt = 1;

    /* decode codes */
    stack = 0;
    for (;;) {
        /* if the table will be full after this, increment the code size */
        if (end >= mask && bits < max) {
            FLUSHCODE();
            bits++;
            mask <<= 1;
            mask++;
        }

        /* get a code of length bits */
        if (chunk == 0)                     /* decrement chunk modulo bits */
            chunk = bits;
        code = rem;                         /* low bits of code */
        got = GET();
        if (g.in_eof) {                     /* EOF is end of compressed data */
            /* write remaining buffered output */
            g.out_tot += outcnt;
            if (outcnt && g.decode == 1)
                writen(g.outd, out_buf, outcnt);
            return;
        }
        code += (unsigned)got << left;      /* middle (or high) bits of code */
        left += 8;
        chunk--;
        if (bits > left) {                  /* need more bits */
            got = GET();
            if (g.in_eof)                   /* can't end in middle of code */
                bail("invalid lzw code: ", g.inf);
            code += (unsigned)got << left;  /* high bits of code */
            left += 8;
            chunk--;
        }
        code &= mask;                       /* mask to current code length */
        left -= bits;                       /* number of unused bits */
        rem = (unsigned)got >> (8 - left);  /* unused bits from last byte */

        /* process clear code (256) */
        if (code == 256 && flags) {
            FLUSHCODE();
            bits = 9;                       /* initialize bits and mask */
            mask = 0x1ff;
            end = 255;                      /* empty table */
            continue;                       /* get next code */
        }

        /* special code to reuse last match */
        temp = code;                        /* save the current code */
        if (code > end) {
            /* Be picky on the allowed code here, and make sure that the code
               we drop through (prev) will be a valid index so that random
               input does not cause an exception.  The code != end + 1 check is
               empirically derived, and not checked in the original uncompress
               code.  If this ever causes a problem, that check could be safely
               removed.  Leaving this check in greatly improves pigz's ability
               to detect random or corrupted input after a compress header.
               In any case, the prev > end check must be retained. */
            if (code != end + 1 || prev > end)
                bail("invalid lzw code: ", g.inf);
            match[stack++] = (unsigned char)final;
            code = prev;
        }

        /* walk through linked list to generate output in reverse order */
        p = match + stack;
        while (code >= 256) {
            *p++ = suffix[code];
            code = prefix[code];
        }
        stack = p - match;
        match[stack++] = (unsigned char)code;
        final = code;

        /* link new table entry */
        if (end < mask) {
            end++;
            prefix[end] = (unsigned short)prev;
            suffix[end] = (unsigned char)final;
        }

        /* set previous code for next iteration */
        prev = temp;

        /* write output in forward order */
        while (stack > OUTSIZE - outcnt) {
            while (outcnt < OUTSIZE)
                out_buf[outcnt++] = match[--stack];
            g.out_tot += outcnt;
            if (g.decode == 1)
                writen(g.outd, out_buf, outcnt);
            outcnt = 0;
        }
        p = match + stack;
        do {
            out_buf[outcnt++] = *--p;
        } while (p > match);
        stack = 0;

        /* loop for next code with final and prev as the last match, rem and
           left provide the first 0..7 bits of the next code, end is the last
           valid table entry */
    }
}

/* --- file processing --- */

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

/* Copy file attributes, from -> to, as best we can.  This is best effort, so
   no errors are reported.  The mode bits, including suid, sgid, and the sticky
   bit are copied (if allowed), the owner's user id and group id are copied
   (again if allowed), and the access and modify times are copied. */
local void copymeta(char *from, char *to)
{
    struct stat st;
    struct timeval times[2];

    /* get all of from's Unix meta data, return if not a regular file */
    if (stat(from, &st) != 0 || (st.st_mode & S_IFMT) != S_IFREG)
        return;

    /* set to's mode bits, ignore errors */
    (void)chmod(to, st.st_mode & 07777);

    /* copy owner's user and group, ignore errors */
    (void)chown(to, st.st_uid, st.st_gid);

    /* copy access and modify times, ignore errors */
    times[0].tv_sec = st.st_atime;
    times[0].tv_usec = 0;
    times[1].tv_sec = st.st_mtime;
    times[1].tv_usec = 0;
    (void)utimes(to, times);
}

/* set the access and modify times of fd to t */
local void touch(char *path, time_t t)
{
    struct timeval times[2];

    times[0].tv_sec = t;
    times[0].tv_usec = 0;
    times[1].tv_sec = t;
    times[1].tv_usec = 0;
    (void)utimes(path, times);
}

/* process provided input file, or stdin if path is NULL -- process() can
   call itself for recursive directory processing */
local void process(char *path)
{
    int method = -1;                /* get_header() return value */
    size_t len;                     /* length of base name (minus suffix) */
    struct stat st;                 /* to get file type and mod time */
    /* all compressed suffixes for decoding search, in length order */
    static char *sufs[] = {".z", "-z", "_z", ".Z", ".gz", "-gz", ".zz", "-zz",
                           ".zip", ".ZIP", ".tgz", NULL};

    /* open input file with name in, descriptor ind -- set name and mtime */
    if (path == NULL) {
        strcpy(g.inf, "<stdin>");
        g.ind = 0;
        g.name = NULL;
        g.mtime = g.headis & 2 ?
                  (fstat(g.ind, &st) ? time(NULL) : st.st_mtime) : 0;
        len = 0;
    }
    else {
        /* set input file name (already set if recursed here) */
        if (path != g.inf) {
            strncpy(g.inf, path, sizeof(g.inf));
            if (g.inf[sizeof(g.inf) - 1])
                bail("name too long: ", path);
        }
        len = strlen(g.inf);

        /* try to stat input file -- if not there and decoding, look for that
           name with compressed suffixes */
        if (lstat(g.inf, &st)) {
            if (errno == ENOENT && (g.list || g.decode)) {
                char **try = sufs;
                do {
                    if (*try == NULL || len + strlen(*try) >= sizeof(g.inf))
                        break;
                    strcpy(g.inf + len, *try++);
                    errno = 0;
                } while (lstat(g.inf, &st) && errno == ENOENT);
            }
#ifdef EOVERFLOW
            if (errno == EOVERFLOW || errno == EFBIG)
                bail(g.inf,
                    " too large -- not compiled with large file support");
#endif
            if (errno) {
                g.inf[len] = 0;
                complain("%s does not exist -- skipping", g.inf);
                return;
            }
            len = strlen(g.inf);
        }

        /* only process regular files, but allow symbolic links if -f,
           recurse into directory if -r */
        if ((st.st_mode & S_IFMT) != S_IFREG &&
            (st.st_mode & S_IFMT) != S_IFLNK &&
            (st.st_mode & S_IFMT) != S_IFDIR) {
            complain("%s is a special file or device -- skipping", g.inf);
            return;
        }
        if ((st.st_mode & S_IFMT) == S_IFLNK && !g.force && !g.pipeout) {
            complain("%s is a symbolic link -- skipping", g.inf);
            return;
        }
        if ((st.st_mode & S_IFMT) == S_IFDIR && !g.recurse) {
            complain("%s is a directory -- skipping", g.inf);
            return;
        }

        /* recurse into directory (assumes Unix) */
        if ((st.st_mode & S_IFMT) == S_IFDIR) {
            char *roll, *item, *cut, *base, *bigger;
            size_t len, hold;
            DIR *here;
            struct dirent *next;

            /* accumulate list of entries (need to do this, since readdir()
               behavior not defined if directory modified between calls) */
            here = opendir(g.inf);
            if (here == NULL)
                return;
            hold = 512;
            roll = MALLOC(hold);
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
                    bigger = REALLOC(roll, hold);
                    if (bigger == NULL) {
                        FREE(roll);
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

            /* run process() for each entry in the directory */
            cut = base = g.inf + strlen(g.inf);
            if (base > g.inf && base[-1] != (unsigned char)'/') {
                if ((size_t)(base - g.inf) >= sizeof(g.inf))
                    bail("path too long", g.inf);
                *base++ = '/';
            }
            item = roll;
            while (*item) {
                strncpy(base, item, sizeof(g.inf) - (base - g.inf));
                if (g.inf[sizeof(g.inf) - 1]) {
                    strcpy(g.inf + (sizeof(g.inf) - 4), "...");
                    bail("path too long: ", g.inf);
                }
                process(g.inf);
                item += strlen(item) + 1;
            }
            *cut = 0;

            /* release list of entries */
            FREE(roll);
            return;
        }

        /* don't compress .gz (or provided suffix) files, unless -f */
        if (!(g.force || g.list || g.decode) && len >= strlen(g.sufx) &&
                strcmp(g.inf + len - strlen(g.sufx), g.sufx) == 0) {
            complain("%s ends with %s -- skipping", g.inf, g.sufx);
            return;
        }

        /* create output file only if input file has compressed suffix */
        if (g.decode == 1 && !g.pipeout && !g.list) {
            int suf = compressed_suffix(g.inf);
            if (suf == 0) {
                complain("%s does not have compressed suffix -- skipping",
                         g.inf);
                return;
            }
            len -= suf;
        }

        /* open input file */
        g.ind = open(g.inf, O_RDONLY, 0);
        if (g.ind < 0)
            bail("read error on ", g.inf);

        /* prepare gzip header information for compression */
        g.name = g.headis & 1 ? justname(g.inf) : NULL;
        g.mtime = g.headis & 2 ? st.st_mtime : 0;
    }
    SET_BINARY_MODE(g.ind);

    /* if decoding or testing, try to read gzip header */
    g.hname = NULL;
    if (g.decode) {
        in_init();
        method = get_header(1);
        if (method != 8 && method != 257 &&
                /* gzip -cdf acts like cat on uncompressed input */
                !(method == -2 && g.force && g.pipeout && g.decode != 2 &&
                  !g.list)) {
            RELEASE(g.hname);
            if (g.ind != 0)
                close(g.ind);
            if (method != -1)
                complain(method < 0 ? "%s is not compressed -- skipping" :
                         "%s has unknown compression method -- skipping",
                         g.inf);
            return;
        }

        /* if requested, test input file (possibly a special list) */
        if (g.decode == 2) {
            if (method == 8)
                infchk();
            else {
                unlzw();
                if (g.list) {
                    g.in_tot -= 3;
                    show_info(method, 0, g.out_tot, 0);
                }
            }
            RELEASE(g.hname);
            if (g.ind != 0)
                close(g.ind);
            return;
        }
    }

    /* if requested, just list information about input file */
    if (g.list) {
        list_info();
        RELEASE(g.hname);
        if (g.ind != 0)
            close(g.ind);
        return;
    }

    /* create output file out, descriptor outd */
    if (path == NULL || g.pipeout) {
        /* write to stdout */
        g.outf = MALLOC(strlen("<stdout>") + 1);
        if (g.outf == NULL)
            bail("not enough memory", "");
        strcpy(g.outf, "<stdout>");
        g.outd = 1;
        if (!g.decode && !g.force && isatty(g.outd))
            bail("trying to write compressed data to a terminal",
                 " (use -f to force)");
    }
    else {
        char *to, *repl;

        /* use header name for output when decompressing with -N */
        to = g.inf;
        if (g.decode && (g.headis & 1) != 0 && g.hname != NULL) {
            to = g.hname;
            len = strlen(g.hname);
        }

        /* replace .tgx with .tar when decoding */
        repl = g.decode && strcmp(to + len, ".tgz") ? "" : ".tar";

        /* create output file and open to write */
        g.outf = MALLOC(len + (g.decode ? strlen(repl) : strlen(g.sufx)) + 1);
        if (g.outf == NULL)
            bail("not enough memory", "");
        memcpy(g.outf, to, len);
        strcpy(g.outf + len, g.decode ? repl : g.sufx);
        g.outd = open(g.outf, O_CREAT | O_TRUNC | O_WRONLY |
                             (g.force ? 0 : O_EXCL), 0600);

        /* if exists and not -f, give user a chance to overwrite */
        if (g.outd < 0 && errno == EEXIST && isatty(0) && g.verbosity) {
            int ch, reply;

            fprintf(stderr, "%s exists -- overwrite (y/n)? ", g.outf);
            fflush(stderr);
            reply = -1;
            do {
                ch = getchar();
                if (reply < 0 && ch != ' ' && ch != '\t')
                    reply = ch == 'y' || ch == 'Y' ? 1 : 0;
            } while (ch != EOF && ch != '\n' && ch != '\r');
            if (reply == 1)
                g.outd = open(g.outf, O_CREAT | O_TRUNC | O_WRONLY,
                              0600);
        }

        /* if exists and no overwrite, report and go on to next */
        if (g.outd < 0 && errno == EEXIST) {
            complain("%s exists -- skipping", g.outf);
            RELEASE(g.outf);
            RELEASE(g.hname);
            if (g.ind != 0)
                close(g.ind);
            return;
        }

        /* if some other error, give up */
        if (g.outd < 0)
            bail("write error on ", g.outf);
    }
    SET_BINARY_MODE(g.outd);
    RELEASE(g.hname);

    /* process ind to outd */
    if (g.verbosity > 1)
        fprintf(stderr, "%s to %s ", g.inf, g.outf);
    if (g.decode) {
        if (method == 8)
            infchk();
        else if (method == 257)
            unlzw();
        else
            cat();
    }
#ifndef NOTHREAD
    else if (g.procs > 1)
        parallel_compress();
#endif
    else
        single_compress(0);
    if (g.verbosity > 1) {
        putc('\n', stderr);
        fflush(stderr);
    }

    /* finish up, copy attributes, set times, delete original */
    if (g.ind != 0)
        close(g.ind);
    if (g.outd != 1) {
        if (close(g.outd))
            bail("write error on ", g.outf);
        g.outd = -1;            /* now prevent deletion on interrupt */
        if (g.ind != 0) {
            copymeta(g.inf, g.outf);
            if (!g.keep)
                unlink(g.inf);
        }
        if (g.decode && (g.headis & 2) != 0 && g.stamp)
            touch(g.outf, g.stamp);
    }
    RELEASE(g.outf);
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
"  -0 to -9, -11        Compression level (11 is much slower, a few % better)",
"  --fast, --best       Compression levels 1 and 9 respectively",
"  -b, --blocksize mmm  Set compression block size to mmmK (default 128K)",
"  -c, --stdout         Write all processed output to stdout (won't delete)",
"  -d, --decompress     Decompress the compressed input",
"  -f, --force          Force overwrite, compress .gz, links, and to terminal",
"  -F  --first          Do iterations first, before block split for -11",
"  -h, --help           Display a help screen and quit",
"  -i, --independent    Compress blocks independently for damage recovery",
"  -I, --iterations n   Number of iterations for -11 optimization",
"  -k, --keep           Do not delete original file after processing",
"  -K, --zip            Compress to PKWare zip (.zip) single entry format",
"  -l, --list           List the contents of the compressed input",
"  -L, --license        Display the pigz license and quit",
"  -M, --maxsplits n    Maximum number of split blocks for -11",
"  -n, --no-name        Do not store or restore file name in/from header",
"  -N, --name           Store/restore file name and mod time in/from header",
"  -O  --oneblock       Do not split into smaller blocks for -11",
#ifndef NOTHREAD
"  -p, --processes n    Allow up to n compression threads (default is the",
"                       number of online processors, or 8 if unknown)",
#endif
"  -q, --quiet          Print no messages, even on error",
"  -r, --recursive      Process the contents of all subdirectories",
"  -R, --rsyncable      Input-determined block locations for rsync",
"  -S, --suffix .sss    Use suffix .sss instead of .gz (for compression)",
"  -t, --test           Test the integrity of the compressed input",
"  -T, --no-time        Do not store or restore mod time in/from header",
#ifdef DEBUG
"  -v, --verbose        Provide more verbose output (-vv to debug)",
#else
"  -v, --verbose        Provide more verbose output",
#endif
"  -V  --version        Show the version of pigz",
"  -z, --zlib           Compress to zlib (.zz) instead of gzip format",
"  --                   All arguments after \"--\" are treated as files"
};

/* display the help text above */
local void help(void)
{
    int n;

    if (g.verbosity == 0)
        return;
    for (n = 0; n < (int)(sizeof(helptext) / sizeof(char *)); n++)
        fprintf(stderr, "%s\n", helptext[n]);
    fflush(stderr);
    exit(0);
}

#ifndef NOTHREAD

/* try to determine the number of processors */
local int nprocs(int n)
{
#  ifdef _SC_NPROCESSORS_ONLN
    n = (int)sysconf(_SC_NPROCESSORS_ONLN);
#  else
#    ifdef _SC_NPROC_ONLN
    n = (int)sysconf(_SC_NPROC_ONLN);
#    else
#      ifdef __hpux
    struct pst_dynamic psd;

    if (pstat_getdynamic(&psd, sizeof(psd), (size_t)1, 0) != -1)
        n = psd.psd_proc_cnt;
#      endif
#    endif
#  endif
    return n;
}

#endif

/* set option defaults */
local void defaults(void)
{
    g.level = Z_DEFAULT_COMPRESSION;
    /* default zopfli options as set by ZopfliInitOptions():
        verbose = 0
        numiterations = 15
        blocksplitting = 1
        blocksplittinglast = 0
        blocksplittingmax = 15
     */
    ZopfliInitOptions(&g.zopts);
#ifdef NOTHREAD
    g.procs = 1;
#else
    g.procs = nprocs(8);
#endif
    g.block = 131072UL;             /* 128K */
    g.rsync = 0;                    /* don't do rsync blocking */
    g.setdict = 1;                  /* initialize dictionary each thread */
    g.verbosity = 1;                /* normal message level */
    g.headis = 3;                   /* store/restore name and timestamp */
    g.pipeout = 0;                  /* don't force output to stdout */
    g.sufx = ".gz";                 /* compressed file suffix */
    g.decode = 0;                   /* compress */
    g.list = 0;                     /* compress */
    g.keep = 0;                     /* delete input file once compressed */
    g.force = 0;                    /* don't overwrite, don't compress links */
    g.recurse = 0;                  /* don't go into directories */
    g.form = 0;                     /* use gzip format */
}

/* long options conversion to short options */
local char *longopts[][2] = {
    {"LZW", "Z"}, {"ascii", "a"}, {"best", "9"}, {"bits", "Z"},
    {"blocksize", "b"}, {"decompress", "d"}, {"fast", "1"}, {"first", "F"},
    {"force", "f"}, {"help", "h"}, {"independent", "i"}, {"iterations", "I"},
    {"keep", "k"}, {"license", "L"}, {"list", "l"}, {"maxsplits", "M"},
    {"name", "N"}, {"no-name", "n"}, {"no-time", "T"}, {"oneblock", "O"},
    {"processes", "p"}, {"quiet", "q"}, {"recursive", "r"}, {"rsyncable", "R"},
    {"silent", "q"}, {"stdout", "c"}, {"suffix", "S"}, {"test", "t"},
    {"to-stdout", "c"}, {"uncompress", "d"}, {"verbose", "v"},
    {"version", "V"}, {"zip", "K"}, {"zlib", "z"}};
#define NLOPTS (sizeof(longopts) / (sizeof(char *) << 1))

/* either new buffer size, new compression level, or new number of processes --
   get rid of old buffers and threads to force the creation of new ones with
   the new settings */
local void new_opts(void)
{
    single_compress(1);
#ifndef NOTHREAD
    finish_jobs();
#endif
}

/* verify that arg is only digits, and if so, return the decimal value */
local size_t num(char *arg)
{
    char *str = arg;
    size_t val = 0;

    if (*str == 0)
        bail("internal error: empty parameter", "");
    do {
        if (*str < '0' || *str > '9' ||
            (val && ((~(size_t)0) - (*str - '0')) / val < 10))
            bail("invalid numeric parameter: ", arg);
        val = val * 10 + (*str - '0');
    } while (*++str);
    return val;
}

/* process an option, return true if a file name and not an option */
local int option(char *arg)
{
    static int get = 0;     /* if not zero, look for option parameter */
    char bad[3] = "-X";     /* for error messages (X is replaced) */

    /* if no argument or dash option, check status of get */
    if (get && (arg == NULL || *arg == '-')) {
        bad[1] = "bpSIM"[get - 1];
        bail("missing parameter after ", bad);
    }
    if (arg == NULL)
        return 0;

    /* process long option or short options */
    if (*arg == '-') {
        /* a single dash will be interpreted as stdin */
        if (*++arg == 0)
            return 1;

        /* process long option (fall through with equivalent short option) */
        if (*arg == '-') {
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

        /* process short options (more than one allowed after dash) */
        do {
            /* if looking for a parameter, don't process more single character
               options until we have the parameter */
            if (get) {
                if (get == 3)
                    bail("invalid usage: -s must be followed by space", "");
                break;      /* allow -pnnn and -bnnn, fall to parameter code */
            }

            /* process next single character option or compression level */
            bad[1] = *arg;
            switch (*arg) {
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                g.level = *arg - '0';
                while (arg[1] >= '0' && arg[1] <= '9') {
                    if (g.level && (INT_MAX - (arg[1] - '0')) / g.level < 10)
                        bail("only levels 0..9 and 11 are allowed", "");
                    g.level = g.level * 10 + *++arg - '0';
                }
                if (g.level == 10 || g.level > 11)
                    bail("only levels 0..9 and 11 are allowed", "");
                new_opts();
                break;
            case 'F':  g.zopts.blocksplittinglast = 1;  break;
            case 'I':  get = 4;  break;
            case 'K':  g.form = 2;  g.sufx = ".zip";  break;
            case 'L':
                fputs(VERSION, stderr);
                fputs("Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012, 2013"
                      " Mark Adler\n",
                      stderr);
                fputs("Subject to the terms of the zlib license.\n",
                      stderr);
                fputs("No warranty is provided or implied.\n", stderr);
                exit(0);
            case 'M':  get = 5;  break;
            case 'N':  g.headis = 3;  break;
            case 'O':  g.zopts.blocksplitting = 0;  break;
            case 'R':  g.rsync = 1;  break;
            case 'S':  get = 3;  break;
            case 'T':  g.headis &= ~2;  break;
            case 'V':  fputs(VERSION, stderr);  exit(0);
            case 'Z':
                bail("invalid option: LZW output not supported: ", bad);
            case 'a':
                bail("invalid option: ascii conversion not supported: ", bad);
            case 'b':  get = 1;  break;
            case 'c':  g.pipeout = 1;  break;
            case 'd':  g.decode = 1;  g.headis = 0;  break;
            case 'f':  g.force = 1;  break;
            case 'h':  help();  break;
            case 'i':  g.setdict = 0;  break;
            case 'k':  g.keep = 1;  break;
            case 'l':  g.list = 1;  break;
            case 'n':  g.headis &= ~1;  break;
            case 'p':  get = 2;  break;
            case 'q':  g.verbosity = 0;  break;
            case 'r':  g.recurse = 1;  break;
            case 't':  g.decode = 2;  break;
            case 'v':  g.verbosity++;  break;
            case 'z':  g.form = 1;  g.sufx = ".zz";  break;
            default:
                bail("invalid option: ", bad);
            }
        } while (*++arg);
        if (*arg == 0)
            return 0;
    }

    /* process option parameter for -b, -p, -S, -I, or -M */
    if (get) {
        size_t n;

        if (get == 1) {
            n = num(arg);
            g.block = n << 10;                  /* chunk size */
            if (g.block < DICT)
                bail("block size too small (must be >= 32K)", "");
            if (n != g.block >> 10 ||
                OUTPOOL(g.block) < g.block ||
                (ssize_t)OUTPOOL(g.block) < 0 ||
                g.block > (1UL << 22))
                bail("block size too large: ", arg);
            new_opts();
        }
        else if (get == 2) {
            n = num(arg);
            g.procs = (int)n;                   /* # processes */
            if (g.procs < 1)
                bail("invalid number of processes: ", arg);
            if ((size_t)g.procs != n || INBUFS(g.procs) < 1)
                bail("too many processes: ", arg);
#ifdef NOTHREAD
            if (g.procs > 1)
                bail("compiled without threads", "");
#endif
            new_opts();
        }
        else if (get == 3)
            g.sufx = arg;                       /* gz suffix */
        else if (get == 4)
            g.zopts.numiterations = num(arg);   /* optimization iterations */
        else if (get == 5)
            g.zopts.blocksplittingmax = num(arg);   /* max block splits */
        get = 0;
        return 0;
    }

    /* neither an option nor parameter */
    return 1;
}

/* catch termination signal */
local void cut_short(int sig)
{
    (void)sig;
    Trace(("termination by user"));
    if (g.outd != -1 && g.outf != NULL)
        unlink(g.outf);
    log_dump();
    _exit(1);
}

/* Process arguments, compress in the gzip format.  Note that procs must be at
   least two in order to provide a dictionary in one work unit for the other
   work unit, and that size must be at least 32K to store a full dictionary. */
int main(int argc, char **argv)
{
    int n;                          /* general index */
    int noop;                       /* true to suppress option decoding */
    unsigned long done;             /* number of named files processed */
    char *opts, *p;                 /* environment default options, marker */

    /* initialize globals */
    g.outf = NULL;
    g.first = 1;
    g.warned = 0;
    g.hname = NULL;

    /* save pointer to program name for error messages */
    p = strrchr(argv[0], '/');
    p = p == NULL ? argv[0] : p + 1;
    g.prog = *p ? p : "pigz";

    /* prepare for interrupts and logging */
    signal(SIGINT, cut_short);
#ifndef NOTHREAD
    yarn_prefix = g.prog;           /* prefix for yarn error messages */
    yarn_abort = cut_short;         /* call on thread error */
#endif
#ifdef DEBUG
    gettimeofday(&start, NULL);     /* starting time for log entries */
    log_init();                     /* initialize logging */
#endif

    /* set all options to defaults */
    defaults();

    /* process user environment variable defaults in GZIP */
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

    /* process user environment variable defaults in PIGZ as well */
    opts = getenv("PIGZ");
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
                bail("cannot provide files in PIGZ environment variable", "");
            opts = p + (n ? 1 : 0);
        }
        option(NULL);
    }

    /* decompress if named "unpigz" or "gunzip", to stdout if "*cat" */
    if (strcmp(g.prog, "unpigz") == 0 || strcmp(g.prog, "gunzip") == 0)
        g.decode = 1, g.headis = 0;
    if ((n = strlen(g.prog)) > 2 && strcmp(g.prog + n - 3, "cat") == 0)
        g.decode = 1, g.headis = 0, g.pipeout = 1;

    /* if no arguments and compressed data to or from a terminal, show help */
    if (argc < 2 && isatty(g.decode ? 0 : 1))
        help();

    /* process command-line arguments, no options after "--" */
    done = noop = 0;
    for (n = 1; n < argc; n++)
        if (noop == 0 && strcmp(argv[n], "--") == 0) {
            noop = 1;
            option(NULL);
        }
        else if (noop || option(argv[n])) { /* true if file name, process it */
            if (done == 1 && g.pipeout && !g.decode && !g.list && g.form > 1)
                complain("warning: output will be concatenated zip files -- "
                         "will not be able to extract");
            process(strcmp(argv[n], "-") ? argv[n] : NULL);
            done++;
        }
    option(NULL);

    /* list stdin or compress stdin to stdout if no file names provided */
    if (done == 0)
        process(NULL);

    /* done -- release resources, show log */
    new_opts();
    log_dump();
    return g.warned ? 2 : 0;
}
