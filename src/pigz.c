/* pigz.c -- parallel implementation of gzip
 * Copyright (C) 2007-2017 Mark Adler
 * Version 2.4.1x  xx Dec 2017  Mark Adler
 */

/*
  This software is provided 'as-is', without any express or implied
  warranty. In no event will the author be held liable for any damages
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
   2.3.2  24 Jan 2015  Change whereis to which in Makefile for portability
                       Return zero exit code when only warnings are issued
                       Increase speed of unlzw (Unix compress decompression)
                       Update zopfli to current google state
                       Allow larger maximum blocksize (-b), now 512 MiB
                       Do not require that -d precede -N, -n, -T options
                       Strip any path from header name for -dN or -dNT
                       Remove use of PATH_MAX (PATH_MAX is not reliable)
                       Do not abort on inflate data error, do remaining files
                       Check gzip header CRC if present
                       Improve decompression error detection and reporting
   2.3.3  24 Jan 2015  Portability improvements
                       Update copyright years in documentation
   2.3.4   1 Oct 2016  Fix an out of bounds access due to invalid LZW input
                       Add an extra sync marker between independent blocks
                       Add zlib version for verbose version option (-vV)
                       Permit named pipes as input (e.g. made by mkfifo())
                       Fix a bug in -r directory traversal
                       Add warning for a zip file entry 4 GiB or larger
   2.4    26 Dec 2017  Portability improvements
                       Produce Zip64 format when needed for --zip (>= 4 GiB)
                       Make -no-name compatible with gzip, add --time option
                       Add -m as a short option for --no-time
                       Check run-time zlib version to handle weak linking
                       Fix a concurrent read bug in --list operation
                       Process options first, for gzip compatibility
                       Add --synchronous (-Y) option to force device write
                       Disallow an empty suffix (e.g. --suffix '')
                       Return an exit code of 1 if any issues are encountered
                       Fix sign error in compression reduction percentage
 */

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
   zip format using the deflate compression method. The compression produces
   partial raw deflate streams which are concatenated by a single write thread
   and wrapped with the appropriate header and trailer, where the trailer
   contains the combined check value.

   Each partial raw deflate stream is terminated by an empty stored block
   (using the Z_SYNC_FLUSH option of zlib), in order to end that partial bit
   stream at a byte boundary, unless that partial stream happens to already end
   at a byte boundary (the latter requires zlib 1.2.6 or later). Ending on a
   byte boundary allows the partial streams to be concatenated simply as
   sequences of bytes. This adds a very small four to five byte overhead
   (average 3.75 bytes) to the output for each input chunk.

   The default input block size is 128K, but can be changed with the -b option.
   The number of compress threads is set by default to 8, which can be changed
   using the -p option. Specifying -p 1 avoids the use of threads entirely.
   pigz will try to determine the number of processors in the machine, in which
   case if that number is two or greater, pigz will use that as the default for
   -p instead of 8.

   The input blocks, while compressed independently, have the last 32K of the
   previous block loaded as a preset dictionary to preserve the compression
   effectiveness of deflating in a single thread. This can be turned off using
   the --independent or -i option, so that the blocks can be decompressed
   independently for partial error recovery or for random access.

   Decompression can't be parallelized over an arbitrary number of processors
   like compression can be, at least not without specially prepared deflate
   streams for that purpose. As a result, pigz uses a single thread (the main
   thread) for decompression, but will create three other threads for reading,
   writing, and check calculation, which can speed up decompression under some
   circumstances. Parallel decompression can be turned off by specifying one
   process (-dp 1 or -tp 1).

   pigz requires zlib 1.2.1 or later to allow setting the dictionary when doing
   raw deflate. Since zlib 1.2.3 corrects security vulnerabilities in zlib
   version 1.2.1 and 1.2.2, conditionals check for zlib 1.2.3 or later during
   the compilation of pigz.c. zlib 1.2.4 includes some improvements to
   Z_FULL_FLUSH and deflateSetDictionary() that permit identical output for
   pigz with and without threads, which is not possible with zlib 1.2.3. This
   may be important for uses of pigz -R where small changes in the contents
   should result in small changes in the archive for rsync. Note that due to
   the details of how the lower levels of compression result in greater speed,
   compression level 3 and below does not permit identical pigz output with and
   without threads.

   pigz uses the POSIX pthread library for thread control and communication,
   through the yarn.h interface to yarn.c. yarn.c can be replaced with
   equivalent implementations using other thread libraries. pigz can be
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
   buffer that grows in size if necessary to hold the compressed data.  The job
   is then put into the write job list, sorted by the sequence number.  The
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
   drop the output buffer, and then wait for the check value to be unlocked by
   the compress thread.  Then the write thread combines the check value for this
   chunk with the total check value for eventual use in the trailer.  If this is
   not the last chunk, the write thread then goes back to look for the next
   output chunk in sequence.  After the last chunk, the write thread returns and
   joins the main thread.  Unlike the compress threads, a new write thread is
   launched for each input stream.  The write thread writes the appropriate
   header and trailer around the compressed data.

   The input and output buffers are reused through their collection in pools.
   Each buffer has a use count, which when decremented to zero returns the
   buffer to the respective pool.  Each input buffer has up to three parallel
   uses: as the input for compression, as the data for the check value
   calculation, and as a dictionary for compression.  Each output buffer has
   only one use, which is as the output of compression followed serially as
   data to be written.  The input pool is limited in the number of buffers, so
   that reading does not get way ahead of compression and eat up memory with
   more input than can be used.  The limit is approximately two times the number
   of compression threads.  In the case that reading is fast as compared to
   compression, that number allows a second set of buffers to be read while the
   first set of compressions are being performed.  The number of output buffers
   is not directly limited, but is indirectly limited by the release of input
   buffers to about the same number.
 */

/* Use large file functions if available. */
#include <config.h>
#define _FILE_OFFSET_BITS 64

/* Included headers and expected functions, vars, types, etc */
#include <stdlib.h>     /* exit(), malloc(), free(), realloc(), atol(), atoi(),
                         * getenv() */
#include <stdarg.h>     /* va_start(), va_arg(), va_end(), va_list */
#include <string.h>     /* memset(), memchr(), memcpy(), strcmp(), strcpy(),
                         * strncpy(), strlen(), strcat(), strrchr(),
                         * strerror() 
                         */
#include <assert.h>     /* assert() */
#include <time.h>       /* ctime(), time(), time_t, mktime() */
#include <sys/types.h>  /* ssize_t */
#include <sys/stat.h>   /* chmod(), stat(), fstat(), lstat(), struct stat,
                         * S_IFDIR, S_IFLNK, S_IFMT, S_IFREG
                         */
#include <sys/time.h>   /* utimes(), gettimeofday(), struct timeval  */
#include <unistd.h>     /* unlink(), _exit(), read(), write(), close(),
                         * lseek(), isatty(), chown(), fsync()
                         */
#include <fcntl.h>      /* open(), O_CREAT, O_EXCL, O_RDONLY, O_TRUNC,
                         * O_WRONLY, fcntl(), F_FULLFSYNC
                         */
#include <dirent.h>     /* opendir(), readdir(), closedir(), DIR,
                         * struct dirent
                         */
#include <limits.h>     /* UINT_MAX, INT_MAX  */
#include <getopt.h>     /* getopt_long  */

#ifdef PIGZ_DEBUG
#if defined(__APPLE__)
#include <malloc/malloc.h>
#define MALLOC_SIZE(p) malloc_size(p)
#elif defined (__linux)
#include <malloc.h>
#define MALLOC_SIZE(p) malloc_usable_size(p)
#elif defined (_WIN32) || defined(_WIN64)
#include <malloc.h>
#define MALLOC_SIZE(p) _msize(p)
#else
#define MALLOC_SIZE(p) (0)
#endif
#endif

#ifdef __hpux
#include <sys/param.h>
#include <sys/pstat.h>
#endif

#ifndef S_IFLNK
#define S_IFLNK 0
#endif

#ifdef __MINGW32__
#define chown(p,o,g) 0
#define utimes(p,t)  0
#define lstat(p,s)   stat(p,s)
#define _exit(s)     exit(s)
#endif

#include "util.c"
#include "zlib.h"       /* deflateInit2(), deflateReset(), deflate(),
                         * deflateEnd(), deflateSetDictionary(), crc32(),
                         * adler32(), inflateBackInit(), inflateBack(),
                         * inflateBackEnd(), Z_DEFAULT_COMPRESSION,
                         * Z_DEFAULT_STRATEGY, Z_DEFLATED, Z_NO_FLUSH, Z_NULL,
                         * Z_OK, Z_SYNC_FLUSH, z_stream
                         */
#if !defined(ZLIB_VERNUM) || ZLIB_VERNUM < 0x1230
#error "Need zlib version 1.2.3 or later"
#endif

#ifndef NOZOPFLI
#include "lib/zopfli/deflate.h"    /* ZopfliDeflatePart(),
                                    * ZopfliInitOptions(),
                                    * ZopfliOptions
                                    */
#endif

#ifndef NOTHREAD
/* ===========================================================================
 * Pool of spaces for buffer management
 */

/* These routines manage a pool of spaces.  Each pool specifies a fixed size
buffer to be contained in each space.  Each space has a use count, which when
decremented to zero returns the space to the pool.  If a space is requested
from the pool and the pool is empty, a space is immediately created unless a
specified limit on the number of spaces has been reached.  Only if the limit
is reached will it wait for a space to be returned to the pool.  Each space
knows what pool it belongs to, so that it can be returned. */

/* A space (one buffer for each space). */
struct space
{
  lock *use;                    /* use count -- return to pool when zero */
  unsigned char *buf;           /* buffer of size size */
  size_t size;                  /* current size of this buffer */
  size_t len;                   /* for application usage (initially zero) */
  struct pool *pool;            /* pool to return to */
  struct space *next;           /* for pool linked list */
};

/* Pool of spaces (one pool for each type needed). */
struct pool
{
  lock *have;                   /* unused spaces available, lock for list */
  struct space *head;           /* linked list of available buffers */
  size_t size;                  /* size of new buffers in this pool */
  int limit;                    /* number of new spaces allowed, or -1 */
  int made;                     /* number of buffers made */
};

/* Initialize a pool (pool structure itself provided, not allocated).  The limit
is the maximum number of spaces in the pool, or -1 to indicate no limit,
i.e., to never wait for a buffer to return to the pool. */
static  void
new_pool (struct pool *pool, size_t size, int limit)
{
  pool->have = new_lock (0);
  pool->head = NULL;
  pool->size = size;
  pool->limit = limit;
  pool->made = 0;
}

/* Get a space from a pool.  The use count is initially set to one, so there is
no need to call use_space() for the first use. */
static  struct space *
get_space (struct pool *pool)
{
  struct space *space;

  /* If can't create any more, wait for a space to show up */
  possess (pool->have);
  if (pool->limit == 0)
    wait_for (pool->have, NOT_TO_BE, 0);

  /* If a space is available, pull it from the list and return it */
  if (pool->head != NULL)
    {
      space = pool->head;
      pool->head = space->next;
      twist (pool->have, BY, -1);       /* one less in pool */
      possess (space->use);
      twist (space->use, TO, 1);        /* initially one user */
      space->len = 0;
      return space;
    }

  /* Nothing available, don't want to wait, make a new space. */
  assert (pool->limit != 0);
  if (pool->limit > 0)
    pool->limit--;
  pool->made++;
  release (pool->have);
  space = alloc (NULL, sizeof (struct space));
  space->use = new_lock (1);    /* initially one user */
  space->buf = alloc (NULL, pool->size);
  space->size = pool->size;
  space->len = 0;
  space->pool = pool;           /* remember the pool this belongs to */
  return space;
}

/* Increase the size of the buffer in space. */
static  void
grow_space (struct space *space)
{
  size_t more;

  /* compute next size up */
  more = grow (space->size);
  if (more == space->size)
    throw (ERANGE, "overflow");

  /* reallocate the buffer */
  space->buf = alloc (space->buf, more);
  space->size = more;
}

/* Increment the use count to require one more drop before returning this space
to the pool. */
static  void
use_space (struct space *space)
{
  long use;

  possess (space->use);
  use = peek_lock (space->use);
  assert (use != 0);
  twist (space->use, BY, +1);
}

/* Drop a space, returning it to the pool if the use count is zero. */
static  void
drop_space (struct space *space)
{
  long use;
  struct pool *pool;

  if (space == NULL)
    return;
  possess (space->use);
  use = peek_lock (space->use);
  assert (use != 0);
  twist (space->use, BY, -1);
  if (use == 1)
    {
      pool = space->pool;
      possess (pool->have);
      space->next = pool->head;
      pool->head = space;
      twist (pool->have, BY, +1);
    }
}

/* Free the memory and lock resources of a pool.  Return number of spaces for
debugging and resource usage measurement. */
static  int
free_pool (struct pool *pool)
{
  int count;
  struct space *space;

  possess (pool->have);
  count = 0;
  while ((space = pool->head) != NULL)
    {
      pool->head = space->next;
      FREE (space->buf);
      free_lock (space->use);
      FREE (space);
      count++;
    }
  assert (count == pool->made);
  release (pool->have);
  free_lock (pool->have);
  return count;
}

/* Input and output buffer pools. */
static  struct pool in_pool;
static  struct pool out_pool;
static  struct pool dict_pool;
static  struct pool lens_pool;

/* ===========================================================================
 * Parallel compression
 */

/* Compress or write job (passed from compress list to write list).  If seq is
equal to -1, compress_thread is instructed to return; if more is false then
this is the last chunk, which after writing tells write_thread to return. */
struct job
{
  long seq;                     /* sequence number */
  int more;                     /* true if this is not the last chunk */
  struct space *in;             /* input data to compress */
  struct space *out;            /* dictionary or resulting compressed data */
  struct space *lens;           /* coded list of flush block lengths */
  unsigned long check;          /* check value for input data */
  lock *calc;                   /* released when check calculation complete */
  struct job *next;             /* next job in the list (either list) */
};

/* List of compress jobs (with tail for appending to list). */
static  lock *compress_have = NULL;       /* number of compress jobs waiting */
static  struct job *compress_head, **compress_tail;

/* List of write jobs. */
static  lock *write_first;        /* lowest sequence number in list */
static  struct job *write_head;

/* Number of compression threads running. */
static  int cthreads = 0;

/* Write thread if running. */
static  thread *writeth = NULL;

/* Setup job lists (call from main thread). */
static  void
setup_jobs (void)
{
  /* Set up only if not already set up. */
  if (compress_have != NULL)
    return;

  /* Allocate locks and initialize lists. */
  compress_have = new_lock (0);
  compress_head = NULL;
  compress_tail = &compress_head;
  write_first = new_lock (-1);
  write_head = NULL;

  /* Initialize buffer pools (initial size for out_pool not critical, since
  buffers will be grown in size if needed -- the initial size chosen to
  make this unlikely, the same for lens_pool). */
  new_pool (&in_pool, g.block, INBUFS (g.procs));
  new_pool (&out_pool, OUTPOOL (g.block), -1);
  new_pool (&dict_pool, DICT, -1);
  new_pool (&lens_pool, g.block >> (RSYNCBITS - 1), -1);
}

/* Command the compress threads to all return, then join them all (call from
main thread), free all the thread-related resources. */
static  void
finish_jobs (void)
{
  struct job job;
  int caught;

  /* Only do this once */
  if (compress_have == NULL)
    return;

  /* Command all of the extant compress threads to return */
  possess (compress_have);
  job.seq = -1;
  job.next = NULL;
  compress_head = &job;
  compress_tail = &(job.next);
  twist (compress_have, BY, +1);        /* will wake them all up */

  /* Join all of the compress threads, verify they all came back. */
  caught = join_all ();
  Trace (("-- joined %d compress threads", caught));
  assert (caught == cthreads);
  cthreads = 0;

  /* Free the resources. */
  caught = free_pool (&lens_pool);
  Trace (("-- freed %d block lengths buffers", caught));
  caught = free_pool (&dict_pool);
  Trace (("-- freed %d dictionary buffers", caught));
  caught = free_pool (&out_pool);
  Trace (("-- freed %d output buffers", caught));
  caught = free_pool (&in_pool);
  Trace (("-- freed %d input buffers", caught));
  free_lock (write_first);
  free_lock (compress_have);
  compress_have = NULL;
}

/* Compress all strm->avail_in bytes at strm->next_in to out->buf, updating
out->len, grow the size of the buffer (out->size) if necessary.  Respect the
size limitations of the zlib stream data types (size_t may be larger than
unsigned). */
static  void
deflate_engine (z_stream * strm, struct space *out, int flush)
{
  size_t room;

  do
    {
      room = out->size - out->len;
      if (room == 0)
        {
          grow_space (out);
          room = out->size - out->len;
        }
      strm->next_out = out->buf + out->len;
      strm->avail_out = room < UINT_MAX ? (unsigned) room : UINT_MAX;
      (void) deflate (strm, flush);
      out->len = (size_t) (strm->next_out - out->buf);
    }
  while (strm->avail_out == 0);
  assert (strm->avail_in == 0);
}

/* Get the next compression job from the head of the list, compress and compute
the check value on the input, and put a job in the write list with the
results.  Keep looking for more jobs, returning when a job is found with a
sequence number of -1 (leave that job in the list for other incarnations to
find). */
static  void
compress_thread (void *dummy)
{
  struct job *job;               /* job pulled and working on */
  struct job *here, **prior;     /* pointers for inserting in write list */
  unsigned long check;           /* check value of input */
  unsigned char *next;           /* pointer for blocks, check value data */
  size_t left;                   /* input left to process */
  size_t len;                    /* remaining bytes to compress/check */
#if ZLIB_VERNUM >= 0x1260
  int bits;                      /* deflate pending bits */
#endif
#ifndef NOZOPFLI
  struct space *temp = NULL;     /* temporary space for zopfli input */
#endif
  int ret;                       /* zlib return code */
  z_stream strm;                 /* deflate stream */
  ball_t err;                    /* error information from throw() */

  (void) dummy;

  try
  {
    /* Initialize the deflate stream for this thread. */
    strm.zfree = ZFREE;
    strm.zalloc = ZALLOC;
    strm.opaque = OPAQUE;
    ret = deflateInit2 (&strm, 6, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
    if (ret == Z_MEM_ERROR)
      throw (ENOMEM, "not enough memory");
    if (ret != Z_OK)
      throw (EINVAL, "internal error");

    /* Keep looking for work. */
    for (;;)
      {
        /* Get a job (like I tell my son). */
        possess (compress_have);
        wait_for (compress_have, NOT_TO_BE, 0);
        job = compress_head;
        assert (job != NULL);
        if (job->seq == -1)
          break;
        compress_head = job->next;
        if (job->next == NULL)
          compress_tail = &compress_head;
        twist (compress_have, BY, -1);

        /* Got a job -- initialize and set the compression level (note that
        if deflateParams() is called immediately after deflateReset(),
        there is no need to initialize input/output for the stream). */
        Trace (("-- compressing #%ld", job->seq));
#ifndef NOZOPFLI
        if (g.level <= 9)
          {
#endif
            (void) deflateReset (&strm);
            (void) deflateParams (&strm, g.level, Z_DEFAULT_STRATEGY);
#ifndef NOZOPFLI
          }
        else
          {
            if (temp == NULL)
              temp = get_space (&out_pool);
            temp->len = 0;
          }
#endif

        /* Set dictionary if provided, release that input or dictionary
        buffer (not NULL if g.setdict is true and if this is not the
        first work unit). */
        if (job->out != NULL)
          {
            len = job->out->len;
            left = len < DICT ? len : DICT;
#ifndef NOZOPFLI
            if (g.level <= 9)
#endif
              deflateSetDictionary (&strm, job->out->buf + (len - left),
                                    (unsigned) left);
#ifndef NOZOPFLI
            else
              {
                memcpy (temp->buf, job->out->buf + (len - left), left);
                temp->len = left;
              }
#endif
            drop_space (job->out);
          }

        /* Set up input and output. */
        job->out = get_space (&out_pool);
#ifndef NOZOPFLI
        if (g.level <= 9)
          {
#endif
            strm.next_in = job->in->buf;
            strm.next_out = job->out->buf;
#ifndef NOZOPFLI
          }
        else
          memcpy (temp->buf + temp->len, job->in->buf, job->in->len);
#endif

        /* Compress each block, either flushing or finishing. */
        next = job->lens == NULL ? NULL : job->lens->buf;
        left = job->in->len;
        job->out->len = 0;
        do
          {
            /* Decode next block length from blocks list. */
            len = next == NULL ? 128 : *next++;
            if (len < 128)        /* 64..32831 */
              len = (len << 8) + (*next++) + 64;
            else if (len == 128)  /* end of list */
              len = left;
            else if (len < 192)   /* 1..63 */
              len &= 0x3f;
            else if (len < 224)
              {                   /* 32832..2129983 */
                len = ((len & 0x1f) << 16) + ((size_t) * next++ << 8);
                len += *next++ + 32832U;
              }
            else
              {                  /* 2129984..539000895 */
                len = ((len & 0x1f) << 24) + ((size_t) * next++ << 16);
                len += (size_t) * next++ << 8;
                len += (size_t) * next++ + 2129984UL;
              }
            left -= len;

#ifndef NOZOPFLI
            if (g.level <= 9)
              {
#endif
                /* Run MAXP2-sized amounts of input through deflate -- this
                 * loop is needed for those cases where the unsigned type
                 * is smaller than the size_t type, or when len is close to
                 * the limit of the size_t type. 
                 */
                while (len > MAXP2)
                  {
                    strm.avail_in = MAXP2;
                    deflate_engine (&strm, job->out, Z_NO_FLUSH);
                    len -= MAXP2;
                  }

                /* Run the last piece through deflate -- end on a byte
                 * boundary, using a sync marker if necessary, or finish
                 * the deflate stream if this is the last block. 
                 */
                strm.avail_in = (unsigned) len;
                if (left || job->more)
                  {
#if ZLIB_VERNUM >= 0x1260
                    if (zlib_vernum () >= 0x1260)
                      {
                        deflate_engine (&strm, job->out, Z_BLOCK);

                        /* Add enough empty blocks to get to a byte boundary. */
                        (void) deflatePending (&strm, Z_NULL, &bits);
                        if ((bits & 1) || !g.setdict)
                          deflate_engine (&strm, job->out, Z_SYNC_FLUSH);
                        else if (bits & 7)
                          {
                            do
                              { /* add static empty blocks */
                                bits = deflatePrime (&strm, 10, 2);
                                assert (bits == Z_OK);
                                (void) deflatePending (&strm, Z_NULL, &bits);
                              }
                            while (bits & 7);
                            deflate_engine (&strm, job->out, Z_BLOCK);
                          }
                      }
                    else
#endif
                      {
                        deflate_engine (&strm, job->out, Z_SYNC_FLUSH);
                      }
                    if (!g.setdict)     /* two markers when independent */
                      deflate_engine (&strm, job->out, Z_FULL_FLUSH);
                  }
                else
                  deflate_engine (&strm, job->out, Z_FINISH);
#ifndef NOZOPFLI
              }
            else
              {
                /* Compress len bytes using zopfli, end at byte boundary. */
                unsigned char bits, *out;
                size_t outsize;

                out = NULL;
                outsize = 0;
                bits = 0;
                ZopfliDeflatePart (&g.zopts, 2, !(left || job->more),
                                   temp->buf, temp->len, temp->len + len,
                                   &bits, &out, &outsize);
                assert (job->out->len + outsize + 5 <= job->out->size);
                memcpy (job->out->buf + job->out->len, out, outsize);
                free (out);
                job->out->len += outsize;
                if (left || job->more)
                  {
                    bits &= 7;
                    if ((bits & 1) || !g.setdict)
                      {
                        if (bits == 0 || bits > 5)
                          job->out->buf[job->out->len++] = 0;
                        job->out->buf[job->out->len++] = 0;
                        job->out->buf[job->out->len++] = 0;
                        job->out->buf[job->out->len++] = 0xff;
                        job->out->buf[job->out->len++] = 0xff;
                      }
                    else if (bits)
                      {
                        do
                          {
                            job->out->buf[job->out->len - 1] += 2 << bits;
                            job->out->buf[job->out->len++] = 0;
                            bits += 2;
                          }
                        while (bits < 8);
                      }
                    if (!g.setdict)
                      {         /* two markers when independent */
                        job->out->buf[job->out->len++] = 0;
                        job->out->buf[job->out->len++] = 0;
                        job->out->buf[job->out->len++] = 0;
                        job->out->buf[job->out->len++] = 0xff;
                        job->out->buf[job->out->len++] = 0xff;
                      }
                  }
                temp->len += len;
              }
#endif
          }
        while (left);
        drop_space (job->lens);
        job->lens = NULL;
        Trace (("-- compressed #%ld%s", job->seq,
                job->more ? "" : " (last)"));

        /* Reserve input buffer until check value has been calculated. */
        use_space (job->in);

        /* Insert write job in list in sorted order, alert write thread. */
        possess (write_first);
        prior = &write_head;
        while ((here = *prior) != NULL)
          {
            if (here->seq > job->seq)
              break;
            prior = &(here->next);
          }
        job->next = here;
        *prior = job;
        twist (write_first, TO, write_head->seq);

        /* Calculate the check value in parallel with writing, alert the
         * write thread that the calculation is complete, and drop this
         * usage of the input buffer. 
         */
        len = job->in->len;
        next = job->in->buf;
        check = CHECK (0L, Z_NULL, 0);
        while (len > MAXP2)
          {
            check = CHECK (check, next, MAXP2);
            len -= MAXP2;
            next += MAXP2;
          }
        check = CHECK (check, next, (unsigned) len);
        drop_space (job->in);
        job->check = check;
        Trace (("-- checked #%ld%s", job->seq, job->more ? "" : " (last)"));
        possess (job->calc);
        twist (job->calc, TO, 1);

        /* Done with that one -- go find another job. */
      }

    /* Found job with seq == -1 -- return to join. */
#ifndef NOZOPFLI
    drop_space (temp);
#endif
    release (compress_have);
    (void) deflateEnd (&strm);
  }
  catch (err)
  {
    THREADABORT (err);
  }
}

/* Collect the write jobs off of the list in sequence order and write out the
 * compressed data until the last chunk is written.  Also write the header and
 * trailer and combine the individual check values of the input buffers. 
 */
static  void
write_thread (void *dummy)
{
  long seq;                     /* next sequence number looking for */
  struct job *job;              /* job pulled and working on */
  size_t len;                   /* input length */
  int more;                     /* true if more chunks to write */
  length_t head;                /* header length */
  length_t ulen;                /* total uncompressed size (overflow ok) */
  length_t clen;                /* total compressed size (overflow ok) */
  unsigned long check;          /* check value of uncompressed data */
  ball_t err;                   /* error information from throw() */

  (void) dummy;

  try
  {
    /* Build and write header. */
    Trace (("-- write thread running"));
    head = put_header ();

    /* Process output of compress threads until end of input. */
    ulen = clen = 0;
    check = CHECK (0L, Z_NULL, 0);
    seq = 0;
    do
      {
        /* Get next write job in order. */
        possess (write_first);
        wait_for (write_first, TO_BE, seq);
        job = write_head;
        write_head = job->next;
        twist (write_first, TO, write_head == NULL ? -1 : write_head->seq);

        /* Update lengths, save uncompressed length for COMB. */
        more = job->more;
        len = job->in->len;
        drop_space (job->in);
        ulen += len;
        clen += job->out->len;

        /* Write the compressed data and drop the output buffer. */
        Trace (("-- writing #%ld", seq));
        writen (g.outd, job->out->buf, job->out->len);
        drop_space (job->out);
        Trace (("-- wrote #%ld%s", seq, more ? "" : " (last)"));

        /* Wait for check calculation to complete, then combine, once the
         * compress thread is done with the input, release it. 
         */
        possess (job->calc);
        wait_for (job->calc, TO_BE, 1);
        release (job->calc);
        check = COMB (check, job->check, len);
        Trace (("-- combined #%ld%s", seq, more ? "" : " (last)"));

        /* Free the job. */
        free_lock (job->calc);
        FREE (job);

        /* Get the next buffer in sequence. */
        seq++;
      }
    while (more);

    /* Write trailer. */
    put_trailer (ulen, clen, check, head);

    /* Verify no more jobs, prepare for next use. */
    possess (compress_have);
    assert (compress_head == NULL && peek_lock (compress_have) == 0);
    release (compress_have);
    possess (write_first);
    assert (write_head == NULL);
    twist (write_first, TO, -1);
  }
  catch (err)
  {
    THREADABORT (err);
  }
}

/* Encode a hash hit to the block lengths list.  hit == 0 ends the list. */
static  void
append_len (struct job *job, size_t len)
{
  struct space *lens;

  assert (len < 539000896UL);
  if (job->lens == NULL)
    job->lens = get_space (&lens_pool);
  lens = job->lens;
  if (lens->size < lens->len + 3)
    grow_space (lens);
  if (len < 64)
    lens->buf[lens->len++] = (unsigned char) (len + 128);
  else if (len < 32832U)
    {
      len -= 64;
      lens->buf[lens->len++] = (unsigned char) (len >> 8);
      lens->buf[lens->len++] = (unsigned char) len;
    }
  else if (len < 2129984UL)
    {
      len -= 32832U;
      lens->buf[lens->len++] = (unsigned char) ((len >> 16) + 192);
      lens->buf[lens->len++] = (unsigned char) (len >> 8);
      lens->buf[lens->len++] = (unsigned char) len;
    }
  else
    {
      len -= 2129984UL;
      lens->buf[lens->len++] = (unsigned char) ((len >> 24) + 224);
      lens->buf[lens->len++] = (unsigned char) (len >> 16);
      lens->buf[lens->len++] = (unsigned char) (len >> 8);
      lens->buf[lens->len++] = (unsigned char) len;
    }
}

/* Compress ind to outd, using multiple threads for the compression and check
 * value calculations and one other thread for writing the output.  Compress
 * threads will be launched and left running (waiting actually) to support
 * subsequent calls of parallel_compress(). 
 */
static  void
parallel_compress (void)
{
  long seq;                     /* sequence number */
  struct space *curr;           /* input data to compress */
  struct space *next;           /* input data that follows curr */
  struct space *hold;           /* input data that follows next */
  struct space *dict;           /* dictionary for next compression */
  struct job *job;              /* job for compress, then write */
  int more;                     /* true if more input to read */
  unsigned hash;                /* hash for rsyncable */
  unsigned char *scan;          /* next byte to compute hash on */
  unsigned char *end;           /* after end of data to compute hash on */
  unsigned char *last;          /* position after last hit */
  size_t left;                  /* last hit in curr to end of curr */
  size_t len;                   /* for various length computations */

  /* If first time or after an option change, setup the job lists. */
  setup_jobs ();

  /* Start write thread. */
  writeth = launch (write_thread, NULL);

  /* Read from input and start compress threads (write thread will pick up
   * the output of the compress threads). 
   */
  seq = 0;
  next = get_space (&in_pool);
  next->len = readn (g.ind, next->buf, next->size);
  hold = NULL;
  dict = NULL;
  scan = next->buf;
  hash = RSYNCHIT;
  left = 0;
  do
    {
      /* Create a new job. */
      job = alloc (NULL, sizeof (struct job));
      job->calc = new_lock (0);

      /* Update input spaces. */
      curr = next;
      next = hold;
      hold = NULL;

      /* Get more input if we don't already have some. */
      if (next == NULL)
        {
          next = get_space (&in_pool);
          next->len = readn (g.ind, next->buf, next->size);
        }

      /* If rsyncable, generate block lengths and prepare curr for job to
       * likely have less than size bytes (up to the last hash hit).
       */
      job->lens = NULL;
      if (g.rsync && curr->len)
        {
          /* Compute the hash function starting where we last left off to
           * cover either size bytes or to EOF, whichever is less, through
           * the data in curr (and in the next loop, through next) -- save
           * the block lengths resulting from the hash hits in the job->lens
           * list. 
           */
          if (left == 0)
            {
              /* Scan is in curr. */
              last = curr->buf;
              end = curr->buf + curr->len;
              while (scan < end)
                {
                  hash = ((hash << 1) ^ *scan++) & RSYNCMASK;
                  if (hash == RSYNCHIT)
                    {
                      len = (size_t) (scan - last);
                      append_len (job, len);
                      last = scan;
                    }
                }

              /* Continue scan in next. */
              left = (size_t) (scan - last);
              scan = next->buf;
            }

          /* Scan in next for enough bytes to fill curr, or what is available
           * in next, whichever is less (if next isn't full, then we're at
           * the end of the file) -- the bytes in curr since the last hit,
           * stored in left, counts towards the size of the first block. 
           */ 
          last = next->buf;
          len = curr->size - curr->len;
          if (len > next->len)
            len = next->len;
          end = next->buf + len;
          while (scan < end)
            {
              hash = ((hash << 1) ^ *scan++) & RSYNCMASK;
              if (hash == RSYNCHIT)
                {
                  len = (size_t) (scan - last) + left;
                  left = 0;
                  append_len (job, len);
                  last = scan;
                }
            }
          append_len (job, 0);

          /* Create input in curr for job up to last hit or entire buffer if
           * no hits at all -- save remainder in next and possibly hold.
           */
          len = (size_t) ((job->lens->len == 1 ? scan : last) - next->buf);
          if (len)
            {
              /* Got hits in next, or no hits in either -- copy to curr. */
              memcpy (curr->buf + curr->len, next->buf, len);
              curr->len += len;
              memmove (next->buf, next->buf + len, next->len - len);
              next->len -= len;
              scan -= len;
              left = 0;
            }
          else if (job->lens->len != 1 && left && next->len)
            {
              /* Had hits in curr, but none in next, and last hit in curr
               * wasn't right at the end, so we have input there to save --
               * use curr up to the last hit, save the rest, moving next to
               * hold.
               */
              hold = next;
              next = get_space (&in_pool);
              memcpy (next->buf, curr->buf + (curr->len - left), left);
              next->len = left;
              curr->len -= left;
            }
          else
            {
              /* Else, last match happened to be right at the end of curr, or
               * we're at the end of the input compressing the rest. 
               */
              left = 0;
            }
        }

      /* Compress curr->buf to curr->len -- compress thread will drop curr. */
      job->in = curr;

      /* Set job->more if there is more to compress after curr. */
      more = next->len != 0;
      job->more = more;

      /* Provide dictionary for this job, prepare dictionary for next job. */
      job->out = dict;
      if (more && g.setdict)
        {
          if (curr->len >= DICT || job->out == NULL)
            {
              dict = curr;
              use_space (dict);
            }
          else
            {
              dict = get_space (&dict_pool);
              len = DICT - curr->len;
              memcpy (dict->buf, job->out->buf + (job->out->len - len), len);
              memcpy (dict->buf + len, curr->buf, curr->len);
              dict->len = DICT;
            }
        }

      /* Preparation of job is complete. */
      job->seq = seq;
      Trace (("-- read #%ld%s", seq, more ? "" : " (last)"));
      if (++seq < 1)
        throw (ERANGE, "overflow");

      /* Start another compress thread if needed. */
      if (cthreads < seq && cthreads < g.procs)
        {
          (void) launch (compress_thread, NULL);
          cthreads++;
        }

      /* Put job at end of compress list, let all the compressors know. */
      possess (compress_have);
      job->next = NULL;
      *compress_tail = job;
      compress_tail = &(job->next);
      twist (compress_have, BY, +1);
    }
  while (more);
  drop_space (next);

  /* Wait for the write thread to complete (we leave the compress threads out
   * there and waiting in case there is another stream to compress). 
   */
  join (writeth);
  writeth = NULL;
  Trace (("-- write thread joined"));
}

#endif

/* Repeated code in single_compress to compress available input and write it. */
#define DEFLATE_WRITE(flush) \
    do { \
        do { \
            strm->avail_out = out_size; \
            strm->next_out = out; \
            (void)deflate(strm, flush); \
            clen += writen(g.outd, out, out_size - strm->avail_out); \
        } while (strm->avail_out == 0); \
        assert(strm->avail_in == 0); \
    } while (0)

/* Do a simple compression in a single thread from ind to outd.  If reset is
 * true, instead free the memory that was allocated and retained for input,
 * output, and deflate. 
 */
static  void
single_compress (int reset)
{
  size_t got;                   /* amount of data in in[] */
  size_t more;                  /* amount of data in next[] (0 if eof) */
  size_t start;                 /* start of data in next[] */
  size_t have;                  /* bytes in current block for -i */
  size_t hist;                  /* offset of permitted history */
  int fresh;                    /* if true, reset compression history */
  unsigned hash;                /* hash for rsyncable */
  unsigned char *scan;          /* pointer for hash computation */
  size_t left;                  /* bytes left to compress after hash hit */
  unsigned long head;           /* header length */
  length_t ulen;                /* total uncompressed size */
  length_t clen;                /* total compressed size */
  unsigned long check;          /* check value of uncompressed data */
  static unsigned out_size;     /* size of output buffer */
  static unsigned char *in, *next, *out;        /* reused i/o buffers */
  static z_stream *strm = NULL; /* reused deflate structure */

  /* If requested, just release the allocations and return. */
  if (reset)
    {
      if (strm != NULL)
        {
          (void) deflateEnd (strm);
          FREE (strm);
          FREE (out);
          FREE (next);
          FREE (in);
          strm = NULL;
        }
      return;
    }

  /* Initialize the deflate structure if this is the first time. */
  if (strm == NULL)
    {
      int ret;                  /* zlib return code */

      out_size = g.block > MAXP2 ? MAXP2 : (unsigned) g.block;
      in = alloc (NULL, g.block + DICT);
      next = alloc (NULL, g.block + DICT);
      out = alloc (NULL, out_size);
      strm = alloc (NULL, sizeof (z_stream));
      strm->zfree = ZFREE;
      strm->zalloc = ZALLOC;
      strm->opaque = OPAQUE;
      ret = deflateInit2 (strm, 6, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
      if (ret == Z_MEM_ERROR)
        throw (ENOMEM, "not enough memory");
      if (ret != Z_OK)
        throw (EINVAL, "internal error");
    }

  /* Write header. */
  head = put_header ();

  /* Set compression level in case it changed. */
#ifndef NOZOPFLI
  if (g.level <= 9)
    {
#endif
      (void) deflateReset (strm);
      (void) deflateParams (strm, g.level, Z_DEFAULT_STRATEGY);
#ifndef NOZOPFLI
    }
#endif

  /* Do raw deflate and calculate check value. */
  got = 0;
  more = readn (g.ind, next, g.block);
  ulen = more;
  start = 0;
  hist = 0;
  clen = 0;
  have = 0;
  check = CHECK (0L, Z_NULL, 0);
  hash = RSYNCHIT;
  do
    {
      /* Get data to compress, see if there is any more input. */
      if (got == 0)
        {
          scan = in;
          in = next;
          next = scan;
          strm->next_in = in + start;
          got = more;
          if (g.level > 9)
            {
              left = start + more - hist;
              if (left > DICT)
                left = DICT;
              memcpy (next, in + ((start + more) - left), left);
              start = left;
              hist = 0;
            }
          else
            start = 0;
          more = readn (g.ind, next + start, g.block);
          ulen += more;
        }

      /* If rsyncable, compute hash until a hit or the end of the block. */
      left = 0;
      if (g.rsync && got)
        {
          scan = strm->next_in;
          left = got;
          do
            {
              if (left == 0)
                {
                  /* Went to the end -- if no more or no hit in size bytes,
                   * then proceed to do a flush or finish with got bytes
                   */
                  if (more == 0 || got == g.block)
                    break;

                  /* fill in[] with what's left there and as much as possible
                   * from next[] -- set up to continue hash hit search 
                   */
                  if (g.level > 9)
                    {
                      left = (size_t) (strm->next_in - in) - hist;
                      if (left > DICT)
                        left = DICT;
                    }
                  memmove (in, strm->next_in - left, left + got);
                  hist = 0;
                  strm->next_in = in + left;
                  scan = in + left + got;
                  left = more > g.block - got ? g.block - got : more;
                  memcpy (scan, next + start, left);
                  got += left;
                  more -= left;
                  start += left;

                  /* If that emptied the next buffer, try to refill it. */
                  if (more == 0)
                    {
                      more = readn (g.ind, next, g.block);
                      ulen += more;
                      start = 0;
                    }
                }
              left--;
              hash = ((hash << 1) ^ *scan++) & RSYNCMASK;
            }
          while (hash != RSYNCHIT);
          got -= left;
        }

      /* Clear history for --independent option. */
      fresh = 0;
      if (!g.setdict)
        {
          have += got;
          if (have > g.block)
            {
              fresh = 1;
              have = got;
            }
        }

#ifndef NOZOPFLI
      if (g.level <= 9)
        {
#endif
          /* Clear history if requested. */
          if (fresh)
            (void) deflateReset (strm);

          /* Compress MAXP2-size chunks in case unsigned type is small. */
          while (got > MAXP2)
            {
              strm->avail_in = MAXP2;
              check = CHECK (check, strm->next_in, strm->avail_in);
              DEFLATE_WRITE (Z_NO_FLUSH);
              got -= MAXP2;
            }

          /* Compress the remainder, emit a block, finish if end of input. */
          strm->avail_in = (unsigned) got;
          got = left;
          check = CHECK (check, strm->next_in, strm->avail_in);
          if (more || got)
            {
#if ZLIB_VERNUM >= 0x1260
              if (zlib_vernum () >= 0x1260)
                {
                  int bits;

                  DEFLATE_WRITE (Z_BLOCK);
                  (void) deflatePending (strm, Z_NULL, &bits);
                  if ((bits & 1) || !g.setdict)
                    DEFLATE_WRITE (Z_SYNC_FLUSH);
                  else if (bits & 7)
                    {
                      do
                        {
                          bits = deflatePrime (strm, 10, 2);
                          assert (bits == Z_OK);
                          (void) deflatePending (strm, Z_NULL, &bits);
                        }
                      while (bits & 7);
                      DEFLATE_WRITE (Z_NO_FLUSH);
                    }
                }
              else
#else
              {
                DEFLATE_WRITE (Z_SYNC_FLUSH);
              }
#endif
              if (!g.setdict)   /* two markers when independent */
                DEFLATE_WRITE (Z_FULL_FLUSH);
            }
          else
            DEFLATE_WRITE (Z_FINISH);
#ifndef NOZOPFLI
        }
      else
        {
          /* Compress got bytes using zopfli, bring to byte boundary. */
          unsigned char bits, *def;
          size_t size, off;

          /* Discard history if requested. */
          off = (size_t) (strm->next_in - in);
          if (fresh)
            hist = off;

          def = NULL;
          size = 0;
          bits = 0;
          ZopfliDeflatePart (&g.zopts, 2, !(more || left),
                             in + hist, off - hist, (off - hist) + got,
                             &bits, &def, &size);
          bits &= 7;
          if (more || left)
            {
              if ((bits & 1) || !g.setdict)
                {
                  writen (g.outd, def, size);
                  if (bits == 0 || bits > 5)
                    writen (g.outd, (unsigned char *) "\0", 1);
                  writen (g.outd, (unsigned char *) "\0\0\xff\xff", 4);
                }
              else
                {
                  assert (size > 0);
                  writen (g.outd, def, size - 1);
                  if (bits)
                    do
                      {
                        def[size - 1] += 2 << bits;
                        writen (g.outd, def + size - 1, 1);
                        def[size - 1] = 0;
                        bits += 2;
                      }
                    while (bits < 8);
                  writen (g.outd, def + size - 1, 1);
                }
              if (!g.setdict)   /* two markers when independent */
                writen (g.outd, (unsigned char *) "\0\0\0\xff\xff", 5);
            }
          else
            writen (g.outd, def, size);
          free (def);
          while (got > MAXP2)
            {
              check = CHECK (check, strm->next_in, MAXP2);
              strm->next_in += MAXP2;
              got -= MAXP2;
            }
          check = CHECK (check, strm->next_in, (unsigned) got);
          strm->next_in += got;
          got = left;
        }
#endif

      /* Do until no more input. */
    }
  while (more || got);

  /* Write trailer. */
  put_trailer (ulen, clen, check, head);
}

/* ===========================================================================
 * Decompression 
 */

#ifndef NOTHREAD
/* Parallel read thread.  If the state is 1, then read a buffer and set the
 * state to 0 when done, if the state is > 1, then end this thread.
 */
static  void
load_read (void *dummy)
{
  size_t len;
  ball_t err;                   /* error information from throw() */

  (void) dummy;

  Trace (("-- launched decompress read thread"));
  try
  {
    do
      {
        possess (g.load_state);
        wait_for (g.load_state, NOT_TO_BE, 0);
        if (peek_lock (g.load_state) > 1)
          {
            release (g.load_state);
            break;
          }
        g.in_len = len = readn (g.ind, g.in_which ? g.in_buf : g.in_buf2,
                                BUF);
        Trace (("-- decompress read thread read %lu bytes", len));
        twist (g.load_state, TO, 0);
      }
    while (len == BUF);
  }
  catch (err)
  {
    THREADABORT (err);
  }
  Trace (("-- exited decompress read thread"));
}

/* Wait for load_read() to complete the current read operation.  If the
 * load_read() thread is not active, then return immediately. 
 */
static  void
load_wait (void)
{
  if (g.in_which == -1)
    return;
  possess (g.load_state);
  wait_for (g.load_state, TO_BE, 0);
  release (g.load_state);
}
#endif

/* load() is called when the input has been consumed in order to provide more
 * input data: load the input buffer with BUF or fewer bytes (fewer if at end
 * of file) from the file g.ind, set g.in_next to point to the g.in_left bytes
 * read, update g.in_tot, and return g.in_left.  g.in_eof is set to true when
 * g.in_left has gone to zero and there is no more data left to read.
 */
static  size_t
load (void)
{
  /* If already detected end of file, do nothing. */
  if (g.in_short)
    {
      g.in_eof = 1;
      g.in_left = 0;
      return 0;
    }

#ifndef NOTHREAD
  /* If first time in or procs == 1, read a buffer to have something to
   * return, otherwise wait for the previous read job to complete.
   */
  if (g.procs > 1)
    {
      /* If first time, fire up the read thread, ask for a read. */
      if (g.in_which == -1)
        {
          g.in_which = 1;
          g.load_state = new_lock (1);
          g.load_thread = launch (load_read, NULL);
        }

      /* Wait for the previously requested read to complete. */
      load_wait ();

      /* Set up input buffer with the data just read. */
      g.in_next = g.in_which ? g.in_buf : g.in_buf2;
      g.in_left = g.in_len;

      /* If not at end of file, alert read thread to load next buffer,
       * alternate between g.in_buf and g.in_buf2. 
       */
      if (g.in_len == BUF)
        {
          g.in_which = 1 - g.in_which;
          possess (g.load_state);
          twist (g.load_state, TO, 1);
        }

      /* At end of file -- join read thread (already exited), clean up. */
      else
        {
          join (g.load_thread);
          free_lock (g.load_state);
          g.in_which = -1;
        }
    }
  else
#endif
    {
      /* Don't use threads -- simply read a buffer into g.in_buf. */
      g.in_left = readn (g.ind, g.in_next = g.in_buf, BUF);
    }

  /* Note end of file. */
  if (g.in_left < BUF)
    {
      g.in_short = 1;

      /* If we got bupkis, now is the time to mark eof. */
      if (g.in_left == 0)
        g.in_eof = 1;
    }

  /* Update the total and return the available bytes. */
  g.in_tot += g.in_left;
  return g.in_left;
}

/* Terminate the load() operation.  Empty buffer, mark end, close file (if not
 * stdin), and free the name and comment obtained from the header, if present.
 */
static  void
load_end (void)
{
#ifndef NOTHREAD
  /* If the read thread is running, then end it. */
  if (g.in_which != -1)
    {
      /* Wait for the previously requested read to complete and send the
       * thread a message to exit. 
       */
      possess (g.load_state);
      wait_for (g.load_state, TO_BE, 0);
      twist (g.load_state, TO, 2);

      /* Join the thread (which has exited or will very shortly) and clean up. 
       */
      join (g.load_thread);
      free_lock (g.load_state);
      g.in_which = -1;
    }
#endif
  g.in_left = 0;
  g.in_short = 1;
  g.in_eof = 1;
  if (g.ind != 0)
    close (g.ind);
  RELEASE (g.hname);
  RELEASE (g.hcomm);
}

/* Initialize for reading new input. */
static  void
in_init (void)
{
  g.in_left = 0;
  g.in_eof = 0;
  g.in_short = 0;
  g.in_tot = 0;
#ifndef NOTHREAD
  g.in_which = -1;
#endif
}

/* Buffered reading macros for decompression and listing. */
#define GET() (g.in_left == 0 && (g.in_eof || load() == 0) ? 0 : \
               (g.in_left--, *g.in_next++))
#define GET2() (tmp2 = GET(), tmp2 + ((unsigned)(GET()) << 8))
#define GET4() (tmp4 = GET2(), tmp4 + ((unsigned long)(GET2()) << 16))
#define SKIP(dist) \
    do { \
        size_t togo = (dist); \
        while (togo > g.in_left) { \
            togo -= g.in_left; \
            if (load() == 0) \
                return -3; \
        } \
        g.in_left -= togo; \
        g.in_next += togo; \
    } while (0)

/* GET(), GET2(), GET4() and SKIP() equivalents, with crc update. */
#define GETC() (g.in_left == 0 && (g.in_eof || load() == 0) ? 0 : \
                (g.in_left--, crc = crc32z(crc, g.in_next, 1), *g.in_next++))
#define GET2C() (tmp2 = GETC(), tmp2 + ((unsigned)(GETC()) << 8))
#define GET4C() (tmp4 = GET2C(), tmp4 + ((unsigned long)(GET2C()) << 16))
#define SKIPC(dist) \
    do { \
        size_t togo = (dist); \
        while (togo > g.in_left) { \
            crc = crc32z(crc, g.in_next, g.in_left); \
            togo -= g.in_left; \
            if (load() == 0) \
                return -3; \
        } \
        crc = crc32z(crc, g.in_next, togo); \
        g.in_left -= togo; \
        g.in_next += togo; \
    } while (0)

/* Get a zero-terminated string into allocated memory, with crc update. */
#define GETZC(str) \
    do { \
        unsigned char *end; \
        size_t copy, have, size = 0; \
        have = 0; \
        do { \
            if (g.in_left == 0 && load() == 0) \
                return -3; \
            end = memchr(g.in_next, 0, g.in_left); \
            copy = end == NULL ? g.in_left : (size_t)(end - g.in_next) + 1; \
            have = vmemcpy(&str, &size, have, g.in_next, copy); \
            g.in_left -= copy; \
            g.in_next += copy; \
        } while (end == NULL); \
        crc = crc32z(crc, (unsigned char *)str, have); \
    } while (0)

/* Pull LSB order or MSB order integers from an unsigned char buffer. */
#define PULL2L(p) ((p)[0] + ((unsigned)((p)[1]) << 8))
#define PULL4L(p) (PULL2L(p) + ((unsigned long)(PULL2L((p) + 2)) << 16))
#define PULL2M(p) (((unsigned)((p)[0]) << 8) + (p)[1])
#define PULL4M(p) (((unsigned long)(PULL2M(p)) << 16) + PULL2M((p) + 2))

/* Convert MS-DOS date and time to a Unix time, assuming current timezone.
 * (You got a better idea?)
 */
static  time_t
dos2time (unsigned long dos)
{
  struct tm tm;

  if (dos == 0)
    return time (NULL);
  tm.tm_year = ((int) (dos >> 25) & 0x7f) + 80;
  tm.tm_mon = ((int) (dos >> 21) & 0xf) - 1;
  tm.tm_mday = (int) (dos >> 16) & 0x1f;
  tm.tm_hour = (int) (dos >> 11) & 0x1f;
  tm.tm_min = (int) (dos >> 5) & 0x3f;
  tm.tm_sec = (int) (dos << 1) & 0x3e;
  tm.tm_isdst = -1;             /* figure out if DST or not */
  return mktime (&tm);
}

/* Convert an unsigned 32-bit integer to signed, even if long > 32 bits. */
static  long
tolong (unsigned long val)
{
  return (long) (val & 0x7fffffffUL) - (long) (val & 0x80000000UL);
}

/* Process zip extra field to extract zip64 lengths and Unix mod time. */
static  int
read_extra (unsigned len, int save)
{
  unsigned id, size, tmp2;
  unsigned long tmp4;

  /* Process extra blocks. */
  while (len >= 4)
    {
      id = GET2 ();
      size = GET2 ();
      if (g.in_eof)
        return -1;
      len -= 4;
      if (size > len)
        break;
      len -= size;
      if (id == 0x0001)
        {
          /* Zip64 Extended Information Extra Field */
          g.zip64 = 1;
          if (g.zip_ulen == LOW32 && size >= 8)
            {
              g.zip_ulen = GET4 ();
              SKIP (4);
              size -= 8;
            }
          if (g.zip_clen == LOW32 && size >= 8)
            {
              g.zip_clen = GET4 ();
              SKIP (4);
              size -= 8;
            }
        }
      if (save)
        {
          if ((id == 0x000d || id == 0x5855) && size >= 8)
            {
              /* PKWare Unix or Info-ZIP Type 1 Unix block. */
              SKIP (4);
              g.stamp = tolong (GET4 ());
              size -= 8;
            }
          if (id == 0x5455 && size >= 5)
            {
              /* Extended Timestamp block */
              size--;
              if (GET () & 1)
                {
                  g.stamp = tolong (GET4 ());
                  size -= 4;
                }
            }
        }
      SKIP (size);
    }
  SKIP (len);
  return 0;
}

/* Read a gzip, zip, zlib, or Unix compress header from ind and return the
 * compression method in the range 0..257.  8 is deflate, 256 is a zip method
 * greater than 255, and 257 is LZW (compress).  The only methods decompressed
 * by pigz are 8 and 257.  On error, return negative: -1 is immediate EOF, -2 is
 * not a recognized compressed format (considering only the first two bytes of
 * input), -3 is premature EOF within the header, -4 is unexpected header flag
 * values, -5 is the zip central directory, and -6 is a failed gzip header crc
 * check.  If -2 is returned, the input pointer has been reset to the beginning.
 * If the return value is not negative, then get_header() sets g.form to
 * indicate gzip (0), zlib (1), or zip (2, or 3 if the entry is followed by a
 * data descriptor), and the input points to the first byte of compressed data. 
 */
static  int
get_header (int save)
{
  unsigned magic;               /* magic header */
  unsigned method;              /* compression method */
  unsigned flags;               /* header flags */
  unsigned fname, extra;        /* name and extra field lengths */
  unsigned tmp2;                /* for macro */
  unsigned long tmp4;           /* for macro */
  unsigned long crc;            /* gzip header crc */

  /* Clear return information. */
  if (save)
    {
      g.stamp = 0;
      RELEASE (g.hname);
      RELEASE (g.hcomm);
    }

  /* See if it's a gzip, zlib, or lzw file. */
  g.magic1 = GET ();
  if (g.in_eof)
    {
      g.magic1 = -1;
      return -1;
    }
  magic = (unsigned) g.magic1 << 8;
  magic += GET ();
  if (g.in_eof)
    return -2;
  if (magic % 31 == 0 && (magic & 0x8f20) == 0x0800)
    {
      /* It's zlib */
      g.form = 1;
      return 8;
    }
  if (magic == 0x1f9d)
    {                           /* it's lzw */
      g.form = -1;
      return 257;
    }
  if (magic == 0x504b)
    {                           /* it's zip */
      magic = GET2 ();          /* the rest of the signature */
      if (g.in_eof)
        return -3;
      if (magic == 0x0201 || magic == 0x0806)
        return -5;              /* central header or archive extra */
      if (magic != 0x0403)
        return -4;              /* not a local header */
      g.zip64 = 0;
      SKIP (2);
      flags = GET2 ();
      if (flags & 0xf7f0)
        return -4;
      method = GET ();          /* return low byte of method or 256 */
      if (GET () != 0 || flags & 1)
        method = 256;           /* unknown or encrypted */
      if (save)
        g.stamp = dos2time (GET4 ());
      else
        SKIP (4);
      g.zip_crc = GET4 ();
      g.zip_clen = GET4 ();
      g.zip_ulen = GET4 ();
      fname = GET2 ();
      extra = GET2 ();
      if (save)
        {
          char *next;

          if (g.in_eof)
            return -3;
          next = g.hname = alloc (NULL, fname + 1);
          while (fname > g.in_left)
            {
              memcpy (next, g.in_next, g.in_left);
              fname -= g.in_left;
              next += g.in_left;
              if (load () == 0)
                return -3;
            }
          memcpy (next, g.in_next, fname);
          g.in_left -= fname;
          g.in_next += fname;
          next += fname;
          *next = 0;
        }
      else
        SKIP (fname);
      read_extra (extra, save);
      g.form = 2 + ((flags & 8) >> 3);
      return g.in_eof ? -3 : (int) method;
    }
  if (magic != 0x1f8b)
    {                           /* not gzip */
      g.in_left++;              /* return the second byte */
      g.in_next--;
      return -2;
    }

  /* It's gzip -- get method and flags. */
  crc = 0xf6e946c9;             /* crc of 0x1f 0x8b */
  method = GETC ();
  flags = GETC ();
  if (flags & 0xe0)
    return -4;

  /* Get time stamp. */
  if (save)
    g.stamp = tolong (GET4C ());
  else
    SKIPC (4);

  /* skip extra field and OS. */
  SKIPC (2);

  /* Skip extra field, if present. */
  if (flags & 4)
    SKIPC (GET2C ());

  /* Read file name, if present, into allocated memory. */
  if (flags & 8)
    {
      if (save)
        GETZC (g.hname);
      else
        while (GETC () != 0)
          ;
    }

  /* Read comment, if present, into allocated memory. */
  if (flags & 16)
    {
      if (save)
        GETZC (g.hcomm);
      else
        while (GETC () != 0)
          ;
    }

  /* Check header crc. */
  if ((flags & 2) && GET2 () != (crc & 0xffff))
    return -6;

  /* Return gzip compression method. */
  g.form = 0;
  return g.in_eof ? -3 : (int) method;
}

/* Process the remainder of a zip file after the first entry.  Return true if
 * the next signature is another local file header.  If listing verbosely, then
 * search the remainder of the zip file for the central file header
 * corresponding to the first zip entry, and save the file comment, if any.
 */
static  int
more_zip_entries (void)
{
  unsigned long sig;
  int ret, n;
  unsigned char *first;
  unsigned tmp2;                /* for macro */
  unsigned long tmp4;           /* for macro */
  unsigned char const central[] = { 0x50, 0x4b, 1, 2 };

  sig = GET4 ();
  ret = !g.in_eof && sig == 0x04034b50; /* true if another entry follows */
  if (!g.list || g.verbosity < 2)
    return ret;

  /* If it was a central file header signature, then already four bytes
   * into a central directory header -- otherwise search for the next one 
   */
  n = sig == 0x02014b50 ? 4 : 0;       /* number of bytes into central header */
  for (;;)
    {
      /* Assure that more input is available. */
      if (g.in_left == 0 && load () == 0)       /* never found it! */
        return ret;
      if (n == 0)
        {
          /* Look for first byte in central signature. */
          first = memchr (g.in_next, central[0], g.in_left);
          if (first == NULL)
            {
              /* Not found -- go get the next buffer and keep looking. */
              g.in_left = 0;
            }
          else
            {
              /* Found -- continue search at next byte. */
              n++;
              g.in_left -= first - g.in_next + 1;
              g.in_next = first + 1;
            }
        }
      else if (n < 4)
        {
          /* Look for the remaining bytes in the central signature. */
          if (g.in_next[0] == central[n])
            {
              n++;
              g.in_next++;
              g.in_left--;
            }
          else
            n = 0;              /* Mismatch -- restart search with this byte. */
        }
      else
        {
          /* Now in a suspected central file header, just past the signature.
           * Read the rest of the fixed-length portion of the header.
           */
          unsigned char head[CEN];
          size_t need = CEN, part = 0, len, i;

          if (need > g.in_left)
            {                   /* will only need to do this once */
              part = g.in_left;
              memcpy (head + CEN - need, g.in_next, part);
              need -= part;
              g.in_left = 0;
              if (load () == 0) /* never found it! */
                return ret;
            }
          memcpy (head + CEN - need, g.in_next, need);

          /* Determine to sufficient probability that this is the droid we're
           * looking for, by checking the CRC and the local header offset.
           */
          if (PULL4L (head + 12) == g.out_check && PULL4L (head + 38) == 0)
            {
              /* Update the number of bytes consumed from the current buffer. */
              g.in_next += need;
              g.in_left -= need;

              /* Get the comment length. */
              len = PULL2L (head + 28);
              if (len == 0)     /* no comment */
                return ret;

              /* Skip the file name and extra field. */
              SKIP (PULL2L (head + 24) + (unsigned long) PULL2L (head + 26));

              /* Save the comment field. */
              need = len;
              g.hcomm = alloc (NULL, len + 1);
              while (need > g.in_left)
                {
                  memcpy (g.hcomm + len - need, g.in_next, g.in_left);
                  need -= g.in_left;
                  g.in_left = 0;
                  if (load () == 0)
                    {          /* premature EOF */
                      RELEASE (g.hcomm);
                      return ret;
                    }
                }
              memcpy (g.hcomm + len - need, g.in_next, need);
              g.in_next += need;
              g.in_left -= need;
              for (i = 0; i < len; i++)
                if (g.hcomm[i] == 0)
                  g.hcomm[i] = ' ';
              g.hcomm[len] = 0;
              return ret;
            }
          else
            {
              /* Nope, false alarm.  Restart the search at the first byte
               * after what we thought was the central file header signature.
               */
              if (part)
                {
                  /* Move buffer data up and insert the part of the header
                   * data read from the previous buffer.
                   */
                  memmove (g.in_next + part, g.in_next, g.in_left);
                  memcpy (g.in_next, head, part);
                  g.in_left += part;
                }
              n = 0;
            }
        }
    }
}

/* ===========================================================================
 * List contents of compressed input (gzip, zlib, or lzw)
 */

/* Find standard compressed file suffix, return length of suffix. */
static  size_t
compressed_suffix (char *nm)
{
  size_t len;

  len = strlen (nm);
  if (len > 4)
    {
      nm += len - 4;
      len = 4;
      if (strcmp (nm, ".zip") == 0 || strcmp (nm, ".ZIP") == 0 ||
          strcmp (nm, ".tgz") == 0)
        return 4;
    }
  if (len > 3)
    {
      nm += len - 3;
      len = 3;
      if (strcmp (nm, ".gz") == 0 || strcmp (nm, "-gz") == 0 ||
          strcmp (nm, ".zz") == 0 || strcmp (nm, "-zz") == 0)
        return 3;
    }
  if (len > 2)
    {
      nm += len - 2;
      if (strcmp (nm, ".z") == 0 || strcmp (nm, "-z") == 0 ||
          strcmp (nm, "_z") == 0 || strcmp (nm, ".Z") == 0)
        return 2;
    }
  return 0;
}

/* Listing file name lengths for -l and -lv. */
#define NAMEMAX1 48             /* name display limit at verbosity 1 */
#define NAMEMAX2 16             /* name display limit at verbosity 2 */

/* Print gzip, lzw, zlib, or zip file information. */
static  void
show_info (int method, unsigned long check, length_t len, int cont)
{
  size_t max;                   /* maximum name length for current verbosity */
  size_t n;                     /* name length without suffix */
  time_t now;                   /* for getting current year */
  char mod[26];                 /* modification time in text */
  char tag[NAMEMAX1 + 1];       /* header or file name, possibly truncated */

  /* Create abbreviated name from header file name or actual file name. */
  max = g.verbosity > 1 ? NAMEMAX2 : NAMEMAX1;
  memset (tag, 0, max + 1);
  if (cont)
    strncpy (tag, "<...>", max + 1);
  else if (g.hname == NULL)
    {
      n = strnlen (g.inf, g.inz) - compressed_suffix (g.inf);
      strncpy (tag, g.inf, n > max + 1 ? max + 1 : n);
      if (strcmp (g.inf + n, ".tgz") == 0 && n < max + 1)
        strncpy (tag + n, ".tar", max + 1 - n);
    }
  else
    strncpy (tag, g.hname, max + 1);
  if (tag[max])
    strcpy (tag + max - 3, "...");

  /* Convert time stamp to text. */
  if (g.stamp)
    {
      strcpy (mod, ctime (&g.stamp));
      now = time (NULL);
      if (strcmp (mod + 20, ctime (&now) + 20) != 0)
        strcpy (mod + 11, mod + 19);
    }
  else
    strcpy (mod + 4, "------ -----");
  mod[16] = 0;

  /* If first time, print header. */
  if (g.first)
    {
      if (g.verbosity > 1)
        fputs ("method    check    timestamp    ", stdout);
      if (g.verbosity > 0)
        puts ("compressed   original reduced  name");
      g.first = 0;
    }

  /* Print information. */
  if (g.verbosity > 1)
    {
      if (g.form == 3 && !g.decode)
        printf ("zip%3d  --------  %s  ", method, mod + 4);
      else if (g.form > 1)
        printf ("zip%3d  %08lx  %s  ", method, check, mod + 4);
      else if (g.form == 1)
        printf ("zlib%2d  %08lx  %s  ", method, check, mod + 4);
      else if (method == 257)
        printf ("lzw     --------  %s  ", mod + 4);
      else
        printf ("gzip%2d  %08lx  %s  ", method, check, mod + 4);
    }
  if (g.verbosity > 0)
    {
      /* Compute reduction percent -- allow divide-by-zero, displays as -inf%. 
       */
      double red = 100. * (len - (double) g.in_tot) / len;
      if ((g.form == 3 && !g.decode) ||
          (method == 8 && g.in_tot > (len + (len >> 10) + 12)) ||
          (method == 257 && g.in_tot > len + (len >> 1) + 3))
#if __STDC_VERSION__-0 >= 199901L || __GNUC__-0 >= 3
        printf ("%10jd %10jd?  unk    %s\n",
                (intmax_t) g.in_tot, (intmax_t) len, tag);
      else
        printf ("%10jd %10jd %6.1f%%  %s\n",
                (intmax_t) g.in_tot, (intmax_t) len, red, tag);
#else
        printf (sizeof (off_t) == sizeof (long) ?
                "%10ld %10ld?  unk    %s\n" : "%10lld %10lld?  unk    %s\n",
                g.in_tot, len, tag);
      else
        printf (sizeof (off_t) == sizeof (long) ?
                "%10ld %10ld %6.1f%%  %s\n" : "%10lld %10lld %6.1f%%  %s\n",
                g.in_tot, len, red, tag);
#endif
    }
  if (g.verbosity > 1 && g.hcomm != NULL)
    puts (g.hcomm);
}

/* List content information about the gzip file at ind (only works if the gzip
 * file contains a single gzip stream with no junk at the end, and only works
 * well if the uncompressed length is less than 4 GB). 
 */
static  void
list_info (void)
{
  int method;                   /* get_header() return value */
  size_t n;                     /* available trailer bytes */
  off_t at;                     /* used to calculate compressed length */
  unsigned char tail[8];        /* trailer containing check and length */
  unsigned long check;          /* check value */
  length_t len;                 /* length from trailer */

  /* Initialize input buffer. */
  in_init ();

  /* Read header information and position input after header. */
  method = get_header (1);
  if (method < 0)
    {
      complain (method == -6 ? "skipping: %s corrupt: header crc error" :
                method == -1 ? "skipping: %s empty" :
                "skipping: %s unrecognized format", g.inf);
      return;
    }

#ifndef NOTHREAD
  /* Wait for read thread to complete current read() operation, to permit
   * seeking and reading on g.ind here in the main thread. 
   */
  load_wait ();
#endif

  /* List zip file. */
  if (g.form > 1)
    {
      more_zip_entries ();      /* get first entry comment, if any */
      g.in_tot = g.zip_clen;
      show_info (method, g.zip_crc, g.zip_ulen, 0);
      return;
    }

  /* List zlib file. */
  if (g.form == 1)
    {
      at = lseek (g.ind, 0, SEEK_END);
      if (at == -1)
        {
          check = 0;
          do
            {
              len = g.in_left < 4 ? g.in_left : 4;
              g.in_next += g.in_left - len;
              while (len--)
                check = (check << 8) + *g.in_next++;
            }
          while (load () != 0);
          check &= LOW32;
        }
      else
        {
          g.in_tot = (length_t) at;
          lseek (g.ind, -4, SEEK_END);
          readn (g.ind, tail, 4);
          check = PULL4M (tail);
        }
      g.in_tot -= 6;
      show_info (method, check, 0, 0);
      return;
    }

  /* List lzw file. */
  if (method == 257)
    {
      at = lseek (g.ind, 0, SEEK_END);
      if (at == -1)
        while (load () != 0)
          ;
      else
        g.in_tot = (length_t) at;
      g.in_tot -= 3;
      show_info (method, 0, 0, 0);
      return;
    }

  /* Skip to end to get trailer (8 bytes), compute compressed length. */
  if (g.in_short)
    {                           /* whole thing already read */
      if (g.in_left < 8)
        {
          complain ("skipping: %s not a valid gzip file", g.inf);
          return;
        }
      g.in_tot = g.in_left - 8; /* compressed size */
      memcpy (tail, g.in_next + (g.in_left - 8), 8);
    }
  else if ((at = lseek (g.ind, -8, SEEK_END)) != -1)
    {
      g.in_tot = (length_t) at - g.in_tot + g.in_left;  /* compressed size */
      readn (g.ind, tail, 8);   /* get trailer */
    }
  else
    {                           /* can't seek */
      len = g.in_tot - g.in_left;       /* save header size */
      do
        {
          n = g.in_left < 8 ? g.in_left : 8;
          memcpy (tail, g.in_next + (g.in_left - n), n);
          load ();
        }
      while (g.in_left == BUF); /* read until end */
      if (g.in_left < 8)
        {
          if (n + g.in_left < 8)
            {
              complain ("skipping: %s not a valid gzip file", g.inf);
              return;
            }
          if (g.in_left)
            {
              if (n + g.in_left > 8)
                memcpy (tail, tail + n - (8 - g.in_left), 8 - g.in_left);
              memcpy (tail + 8 - g.in_left, g.in_next, g.in_left);
            }
        }
      else
        memcpy (tail, g.in_next + (g.in_left - 8), 8);
      g.in_tot -= len + 8;
    }
  if (g.in_tot < 2)
    {
      complain ("skipping: %s not a valid gzip file", g.inf);
      return;
    }

  /* Convert trailer to check and uncompressed length (modulo 2^32). */
  check = PULL4L (tail);
  len = PULL4L (tail + 4);

  /* List information about contents. */
  show_info (method, check, len, 0);
}

/* ===========================================================================
 * Copy input to output (when acting like cat)
 */

static  void
cat (void)
{
  /* Copy the first header byte read, if any. */
  if (g.magic1 != -1)
    {
      unsigned char buf[1] = { g.magic1 };
      g.out_tot += writen (g.outd, buf, 1);
    }

  /* Copy the remainder of the input to the output. */
  while (g.in_left)
    {
      g.out_tot += writen (g.outd, g.in_next, g.in_left);
      g.in_left = 0;
      load ();
    }
}

/* ===========================================================================
 * Decompress deflate input
 */

/* Call-back input function for inflateBack(). */
static  unsigned
inb (void *desc, unsigned char **buf)
{
  (void) desc;
  if (g.in_left == 0)
    load ();
  *buf = g.in_next;
  unsigned len = g.in_left > UINT_MAX ? UINT_MAX : (unsigned) g.in_left;
  g.in_next += len;
  g.in_left -= len;
  return len;
}

/* Output buffers and window for infchk() and unlzw(). */
#define OUTSIZE 32768U      /* Must be at least 32K for inflateBack() window. */
static  unsigned char out_buf[OUTSIZE];

#ifndef NOTHREAD
/* Output data for parallel write and check. */
static  unsigned char out_copy[OUTSIZE];
static  size_t out_len;

/* outb threads states. */
static  lock *outb_write_more = NULL;
static  lock *outb_check_more;

/* Output write thread. */
static  void
outb_write (void *dummy)
{
  size_t len;
  ball_t err;                   /* error information from throw() */

  (void) dummy;

  Trace (("-- launched decompress write thread"));
  try
  {
    do
      {
        possess (outb_write_more);
        wait_for (outb_write_more, TO_BE, 1);
        len = out_len;
        if (len && g.decode == 1)
          writen (g.outd, out_copy, len);
        Trace (("-- decompress wrote %lu bytes", len));
        twist (outb_write_more, TO, 0);
      }
    while (len);
  }
  catch (err)
  {
    THREADABORT (err);
  }
  Trace (("-- exited decompress write thread"));
}

/* Output check thread. */
static  void
outb_check (void *dummy)
{
  size_t len;
  ball_t err;                   /* error information from throw() */

  (void) dummy;

  Trace (("-- launched decompress check thread"));
  try
  {
    do
      {
        possess (outb_check_more);
        wait_for (outb_check_more, TO_BE, 1);
        len = out_len;
        g.out_check = CHECK (g.out_check, out_copy, len);
        Trace (("-- decompress checked %lu bytes", len));
        twist (outb_check_more, TO, 0);
      }
    while (len);
  }
  catch (err)
  {
    THREADABORT (err);
  }
  Trace (("-- exited decompress check thread"));
}
#endif

/* Call-back output function for inflateBack().  Wait for the last write and
 * check calculation to complete, copy the write buffer, and then alert the
 * write and check threads and return for more decompression while that's going
 * on (or just write and check if no threads or if proc == 1).
 */
static  int
outb (void *desc, unsigned char *buf, unsigned len)
{
  (void) desc;

#ifndef NOTHREAD
  static thread *wr, *ch;

  if (g.procs > 1)
    {
      /* If first time, initialize state and launch threads. */
      if (outb_write_more == NULL)
        {
          outb_write_more = new_lock (0);
          outb_check_more = new_lock (0);
          wr = launch (outb_write, NULL);
          ch = launch (outb_check, NULL);
        }

      /* Wait for previous write and check threads to complete. */
      possess (outb_check_more);
      wait_for (outb_check_more, TO_BE, 0);
      possess (outb_write_more);
      wait_for (outb_write_more, TO_BE, 0);

      /* Copy the output and alert the worker bees. */
      out_len = len;
      g.out_tot += len;
      memcpy (out_copy, buf, len);
      twist (outb_write_more, TO, 1);
      twist (outb_check_more, TO, 1);

      /* If requested with len == 0, clean up -- terminate and join write and
       * check threads, free lock. 
       */
      if (len == 0 && outb_write_more != NULL)
        {
          join (ch);
          join (wr);
          free_lock (outb_check_more);
          free_lock (outb_write_more);
          outb_write_more = NULL;
        }

      /* Return for more decompression while last buffer is being written and
       * having its check value calculated -- we wait for those to finish the
       * next time this function is called. 
       */
      return 0;
    }
#endif

  /* If just one process or no threads, then do it without threads. */
  if (len)
    {
      if (g.decode == 1)
        writen (g.outd, buf, len);
      g.out_check = CHECK (g.out_check, buf, len);
      g.out_tot += len;
    }
  return 0;
}

/* Zip file data descriptor signature.  This signature may or may not precede
 * the CRC and lengths, with either resulting in a valid zip file! There is
 * some odd code below that tries to detect and accommodate both cases. 
 */
#define SIG 0x08074b50

/* Inflate for decompression or testing.  Decompress from ind to outd unless
 * decode != 1, in which case just test ind, and then also list if list != 0;
 * look for and decode multiple, concatenated gzip and/or zlib streams; read
 * and check the gzip, zlib, or zip trailer. 
 */
static  void
infchk (void)
{
  int ret, cont, more;
  unsigned long check, len;
  z_stream strm;
  unsigned tmp2;
  unsigned long tmp4;
  length_t clen;

  cont = more = 0;
  do
    {
      /* Header already read -- set up for decompression. */
      g.in_tot = g.in_left;     /* track compressed data length */
      g.out_tot = 0;
      g.out_check = CHECK (0L, Z_NULL, 0);
      strm.zalloc = ZALLOC;
      strm.zfree = ZFREE;
      strm.opaque = OPAQUE;
      ret = inflateBackInit (&strm, 15, out_buf);
      if (ret == Z_MEM_ERROR)
        throw (ENOMEM, "not enough memory");
      if (ret != Z_OK)
        throw (EINVAL, "internal error");

      /* Decompress, compute lengths and check value. */
      strm.avail_in = 0;
      strm.next_in = Z_NULL;
      ret = inflateBack (&strm, inb, NULL, outb, NULL);
      inflateBackEnd (&strm);
      if (ret == Z_DATA_ERROR)
        throw (EDOM, "%s: corrupted -- invalid deflate data (%s)",
               g.inf, strm.msg);
      if (ret == Z_BUF_ERROR)
        throw (EDOM, "%s: corrupted -- incomplete deflate data", g.inf);
      if (ret != Z_STREAM_END)
        throw (EINVAL, "internal error");
      g.in_left += strm.avail_in;
      g.in_next = strm.next_in;
      outb (NULL, NULL, 0);     /* finish off final write and check */

      /* Compute compressed data length. */
      clen = g.in_tot - g.in_left;

      /* Read and check trailer. */
      if (g.form > 1)
        {                       /* zip local trailer (if any) */
          if (g.form == 3)
            {                   /* data descriptor follows */
              /* Get data descriptor values, assuming no signature. */
              g.zip_crc = GET4 ();
              g.zip_clen = GET4 ();
              g.zip_ulen = GET4 ();     /* ZIP64 -> high clen, not ulen */

             /* Deduce whether or not a signature precedes the values. */
              if (g.zip_crc == SIG &&   /* might be the signature */
                  /* If the expected CRC is not SIG, then it's a signature. */
                  (g.out_check != SIG ||        /* assume signature */
                   /* Now we're in a very rare case where CRC == SIG -- the
                    * first four bytes could be the signature or the CRC. 
                    */
                   (g.zip_clen == SIG &&        /* if not, then no signature */
                    /* Now we have the first two words are SIG and the
                     * expected CRC is SIG, so it could be a signature and
                     * the CRC, or it could be the CRC and a compressed
                     * length that is *also* SIG (!) -- so check the low 32
                     * bits of the expected compressed length for SIG. 
                     */
                    ((clen & LOW32) != SIG ||   /* assume signature and CRC */
                     /* Now the expected CRC *and* the expected low 32 bits
                      * of the compressed length are SIG -- this is so
                      * incredibly unlikely, clearly someone is messing with
                      * us, but we continue...  if the next four bytes are
                      * not SIG, then there is not a signature -- check those
                      * bytes, currently in g.zip_ulen: 
                      */
                     (g.zip_ulen == SIG &&      /* if not, then no signature */
                      /* We have three SIGs in a row in the descriptor, and
                       * both the expected CRC and the expected clen are SIG
                       * -- the first one is a signature if we don't expect
                       * the third word to be SIG, which is either the low 32
                       * bits of ulen, or if ZIP64, the high 32 bits of clen:
                       */
                      (g.zip64 ? clen >> 32 : g.out_tot) != SIG
                       /* If that last compare was equal, then the expected
                        * values for the CRC, the low 32 bits of clen, *and*
                        * the low 32 bits of ulen are all SIG (!!), or in the
                        * case of ZIP64, even crazier, the CRC and *both*
                        * 32-bit halves of clen are all SIG (clen > 500
                        * petabytes!!!)...  we can no longer discriminate the
                        * hypotheses, so we will assume no signature. 
                        */
                     )))))
                {
                  /* First four bytes were actually the descriptor -- shift
                   * the values down and get another four bytes. 
                   */
                  g.zip_crc = g.zip_clen;
                  g.zip_clen = g.zip_ulen;
                  g.zip_ulen = GET4 ();
                }

              /* If ZIP64, then ulen is really the high word of clen -- get
               * the actual ulen and skip its high word as well (we only
               * compare the low 32 bits of the lengths to verify). 
               */
              if (g.zip64)
                {
                  g.zip_ulen = GET4 ();
                  (void) GET4 ();
                }
              if (g.in_eof)
                throw (EDOM, "%s: corrupted entry -- missing trailer", g.inf);
            }
          check = g.zip_crc;
          if (check != g.out_check)
            throw (EDOM, "%s: corrupted entry -- crc32 mismatch", g.inf);
          if (g.zip_clen != (clen & LOW32) ||
              g.zip_ulen != (g.out_tot & LOW32))
            throw (EDOM, "%s: corrupted entry -- length mismatch", g.inf);
          more = more_zip_entries ();   /* see if more entries, get comment */
        }
      else if (g.form == 1)
        {                       /* zlib (big-endian) trailer */
          check = (unsigned long) (GET ()) << 24;
          check += (unsigned long) (GET ()) << 16;
          check += (unsigned) (GET ()) << 8;
          check += GET ();
          if (g.in_eof)
            throw (EDOM, "%s: corrupted -- missing trailer", g.inf);
          if (check != g.out_check)
            throw (EDOM, "%s: corrupted -- adler32 mismatch", g.inf);
        }
      else
        {                       /* gzip trailer */
          check = GET4 ();
          len = GET4 ();
          if (g.in_eof)
            throw (EDOM, "%s: corrupted -- missing trailer", g.inf);
          if (check != g.out_check)
            throw (EDOM, "%s: corrupted -- crc32 mismatch", g.inf);
          if (len != (g.out_tot & LOW32))
            throw (EDOM, "%s: corrupted -- length mismatch", g.inf);
        }

      /* Show file information if requested. */
      if (g.list)
        {
          g.in_tot = clen;
          show_info (8, check, g.out_tot, cont);
          cont = 1;
        }

      /* If a gzip entry follows a gzip entry, decompress it (don't replace
       * saved header information from first entry).
       */
    }
  while (g.form == 0 && (ret = get_header (0)) == 8);

  /* gzip -cdf copies junk after gzip stream directly to output. */
  if (g.form == 0 && ret == -2 && g.force && g.pipeout && g.decode != 2 &&
      !g.list)
    cat ();

  /* Check for more entries in zip file. */
  else if (more)
    complain ("warning: %s: entries after the first were ignored", g.inf);

  /* Check for non-gzip after gzip stream, or anything after zlib stream. */
  else if ((g.form == 0 && ret != -1) || (g.form == 1 && (GET (), !g.in_eof)))
    complain ("warning: %s: trailing junk was ignored", g.inf);
}

/* ===========================================================================
 * Decompress Unix compress (LZW) input.
 */

/* Type for accumulating bits.  23 bits will be used to accumulate up to 16-bit
 * symbols. 
 */
typedef unsigned long bits_t;

#define NOMORE() (g.in_left == 0 && (g.in_eof || load() == 0))
#define NEXT() (g.in_left--, (unsigned)*g.in_next++)

/* Decompress a compress (LZW) file from ind to outd.  The compress magic header
 *(two bytes) has already been read and verified.
 */
static  void
unlzw (void)
{
  unsigned bits;                /* current bits per code (9..16) */
  unsigned mask;                /* mask for current bits codes = (1<<bits)-1 */
  bits_t buf;                   /* bit buffer (need 23 bits) */
  unsigned left;                /* bits left in buf (0..7 after code pulled) */
  length_t mark;                /* offset where last change in bits began */
  unsigned code;                /* code, table traversal index */
  unsigned max;                 /* maximum bits per code for this stream */
  unsigned flags;               /* compress flags, then block compress flag */
  unsigned end;                 /* last valid entry in prefix/suffix tables */
  unsigned prev;                /* previous code */
  unsigned final;               /* last character written for previous code */
  unsigned stack;               /* next position for reversed string */
  unsigned outcnt;              /* bytes in output buffer */
  /* Memory for unlzw() -- the first 256 entries of prefix[] and suffix[] are
   * never used, so could have offset the index but it's faster to waste a
   * little memory. 
   */
  uint_least16_t prefix[65536]; /* index to LZW prefix string */
  unsigned char suffix[65536];  /* one-character LZW suffix */
  unsigned char match[65280 + 2];       /* buffer for reversed match */

  /* Process remainder of compress header -- a flags byte. */
  g.out_tot = 0;
  if (NOMORE ())
    throw (EDOM, "%s: lzw premature end", g.inf);
  flags = NEXT ();
  if (flags & 0x60)
    throw (EDOM, "%s: unknown lzw flags set", g.inf);
  max = flags & 0x1f;
  if (max < 9 || max > 16)
    throw (EDOM, "%s: lzw bits out of range", g.inf);
  if (max == 9)                 /* 9 doesn't really mean 9 */
    max = 10;
  flags &= 0x80;                /* true if block compress */

  /* Mark the start of the compressed data for computing the first flush. */
  mark = g.in_tot - g.in_left;

  /* clear table, start at nine bits per symbol. */
  bits = 9;
  mask = 0x1ff;
  end = flags ? 256 : 255;

  /* set up: get first 9-bit code, which is the first decompressed byte, but
   * don't create a table entry until the next code. 
   */
  if (NOMORE ())                /* no compressed data is ok */
    return;
  buf = NEXT ();
  if (NOMORE ())
    throw (EDOM, "%s: lzw premature end", g.inf);       /* need nine bits */
  buf += NEXT () << 8;
  final = prev = buf & mask;    /* code */
  buf >>= bits;
  left = 16 - bits;
  if (prev > 255)
    throw (EDOM, "%s: invalid lzw code", g.inf);
  out_buf[0] = (unsigned char) final;   /* write first decompressed byte */
  outcnt = 1;

  /* Decode codes. */
  stack = 0;
  for (;;)
    {
      /* If the table will be full after this, increment the code size. */
      if (end >= mask && bits < max)
        {
          /* Flush unused input bits and bytes to next 8*bits bit boundary
           * (this is a vestigial aspect of the compressed data format
           * derived from an implementation that made use of a special VAX
           * machine instruction!). 
           */

          {
            unsigned rem = ((g.in_tot - g.in_left) - mark) % bits;
            if (rem)
              {
                rem = bits - rem;
                if (NOMORE ())
                  break;        /* end of compressed data */
                while (rem > g.in_left)
                  {
                    rem -= g.in_left;
                    if (load () == 0)
                      throw (EDOM, "%s: lzw premature end", g.inf);
                  }
                g.in_left -= rem;
                g.in_next += rem;
              }
          }
          buf = 0;
          left = 0;

          /* Mark this new location for computing the next flush. */
          mark = g.in_tot - g.in_left;

          /* Go to the next number of bits per symbol. */
          bits++;
          mask <<= 1;
          mask++;
        }

      /* Get a code of bits bits. */
      if (NOMORE ())
        break;                  /* end of compressed data */
      buf += (bits_t) (NEXT ()) << left;
      left += 8;
      if (left < bits)
        {
          if (NOMORE ())
            throw (EDOM, "%s: lzw premature end", g.inf);
          buf += (bits_t) (NEXT ()) << left;
          left += 8;
        }
      code = buf & mask;
      buf >>= bits;
      left -= bits;

      /* Process clear code (256). */
      if (code == 256 && flags)
        {
          /* Flush unused input bits and bytes to next 8*bits bit boundary. */
          {
            unsigned rem = ((g.in_tot - g.in_left) - mark) % bits;
            if (rem)
              {
                rem = bits - rem;
                while (rem > g.in_left)
                  {
                    rem -= g.in_left;
                    if (load () == 0)
                      throw (EDOM, "%s: lzw premature end", g.inf);
                  }
                g.in_left -= rem;
                g.in_next += rem;
              }
          }
          buf = 0;
          left = 0;

          /* Mark this new location for computing the next flush. */
          mark = g.in_tot - g.in_left;

          /* Go back to nine bits per symbol. */
          bits = 9;             /* initialize bits and mask */
          mask = 0x1ff;
          end = 255;            /* empty table */
          continue;             /* get next code */
        }

      /* Special code to reuse last match. */
      {
        unsigned temp = code;   /* save the current code */
        if (code > end)
          {
            /* Be picky on the allowed code here, and make sure that the
             * code we drop through (prev) will be a valid index so that
             * random input does not cause an exception. 
             */
            if (code != end + 1 || prev > end)
              throw (EDOM, "%s: invalid lzw code", g.inf);
            match[stack++] = (unsigned char) final;
            code = prev;
          }

        /* Walk through linked list to generate output in reverse order. */
        while (code >= 256)
          {
            match[stack++] = suffix[code];
            code = prefix[code];
          }
        match[stack++] = (unsigned char) code;
        final = code;

        /* Link new table entry. */
        if (end < mask)
          {
            end++;
            prefix[end] = (uint_least16_t) prev;
            suffix[end] = (unsigned char) final;
          }

        /* Set previous code for next iteration. */
        prev = temp;
      }

      /* Write output in forward order. */
      while (stack > OUTSIZE - outcnt)
        {
          while (outcnt < OUTSIZE)
            out_buf[outcnt++] = match[--stack];
          g.out_tot += outcnt;
          if (g.decode == 1)
            writen (g.outd, out_buf, outcnt);
          outcnt = 0;
        }
      do
        {
          out_buf[outcnt++] = match[--stack];
        }
      while (stack);
    }

  /* Write any remaining buffered output. */
  g.out_tot += outcnt;
  if (outcnt && g.decode == 1)
    writen (g.outd, out_buf, outcnt);
}

/* ===========================================================================
 * File processing
 */

/* Extract file name from path. */
static  char *
justname (char *path)
{
  char *p;

  p = strrchr (path, '/');
  return p == NULL ? path : p + 1;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"

/* Copy file attributes, from -> to, as best we can.  This is best effort, so no
 * errors are reported.  The mode bits, including suid, sgid, and the sticky bit
 * are copied (if allowed), the owner's user id and group id are copied (again
 * if allowed), and the access and modify times are copied. 
 */
static  void
copymeta (char *from, char *to)
{
  struct stat st;
  struct timeval times[2];

  /* Get all of from's Unix meta data, return if not a regular file. */
  if (stat (from, &st) != 0 || (st.st_mode & S_IFMT) != S_IFREG)
    return;

  /* Set to's mode bits, ignore errors. */
  (void) chmod (to, st.st_mode & 07777);

  /* Copy owner's user and group, ignore errors. */
  (void) chown (to, st.st_uid, st.st_gid);

  /* Copy access and modify times, ignore errors. */
  times[0].tv_sec = st.st_atime;
  times[0].tv_usec = 0;
  times[1].tv_sec = st.st_mtime;
  times[1].tv_usec = 0;
  (void) utimes (to, times);
}

#pragma GCC diagnostic pop

/* Set the access and modify times of fd to t.. */
static  void
touch (char *path, time_t t)
{
  struct timeval times[2];

  times[0].tv_sec = t;
  times[0].tv_usec = 0;
  times[1].tv_sec = t;
  times[1].tv_usec = 0;
  (void) utimes (path, times);
}

/* Request that all data buffered by the operating system for g.outd be written
 * to the permanent storage device.  If fsync(fd) is used (POSIX), then all of
 * the data is sent to the device, but will likely be buffered in volatile
 * memory on the device itself, leaving open a window of vulnerability.
 * fcntl(fd, F_FULLSYNC) on the other hand, available in macOS only, will
 * request and wait for the device to write out its buffered data to permanent
 * storage.
 * 
 * For a future Windows port of pigz, winbase.h should be included and
 * FlushFileBuffers() used, which also waits for the device write out its
 * buffered data to permanent storage. 
 */
static  void
out_push (void)
{
  if (g.outd == -1)
    return;
#ifdef F_FULLSYNC
  int ret = fcntl (g.outd, F_FULLSYNC);
#else
  int ret = fsync (g.outd);
#endif
  if (ret == -1)
    throw (errno, "sync error on %s (%s)", g.outf, strerror (errno));
}

/* Process provided input file, or stdin if path is NULL.  process() can call
 * itself for recursive directory processing. 
 */
static  void
process (char *path)
{
  volatile int method = -1;     /* get_header() return value */
  size_t len;                   /* length of base name (minus suffix) */
  struct stat st;               /* to get file type and mod time */
  ball_t err;                   /* error information from throw() */
  /* All compressed suffixes for decoding search, in length order. */
  static char *sufs[] = { ".z", "-z", "_z", ".Z", ".gz", "-gz", ".zz", "-zz",
    ".zip", ".ZIP", ".tgz", NULL
  };

  /* Open input file with name in, descriptor ind -- set name and mtime. */
  if (path == NULL)
    {
      vstrcpy (&g.inf, &g.inz, 0, "<stdin>");
      g.ind = 0;
      g.name = NULL;
      g.mtime = g.headis & 2 ?
        (fstat (g.ind, &st) ? time (NULL) : st.st_mtime) : 0;
      len = 0;
    }
  else
    {
      /* Set input file name (already set if recursed here). */
      if (path != g.inf)
        vstrcpy (&g.inf, &g.inz, 0, path);
      len = strnlen (g.inf, g.inz);

      /* Try to stat input file -- if not there and decoding, look for that
       * name with compressed suffixes. 
       */
      if (lstat (g.inf, &st))
        {
          if (errno == ENOENT && (g.list || g.decode))
            {
              char **sufx = sufs;
              do
                {
                  if (*sufx == NULL)
                    break;
                  vstrcpy (&g.inf, &g.inz, len, *sufx++);
                  errno = 0;
                }
              while (lstat (g.inf, &st) && errno == ENOENT);
            }
#if defined(EOVERFLOW) && defined(EFBIG)
          if (errno == EOVERFLOW || errno == EFBIG)
            throw (EDOM, "%s too large -- "
                   "not compiled with large file support", g.inf);
#endif
          if (errno)
            {
              g.inf[len] = 0;
              complain ("%s: No such file or directory", g.inf);
              return;
            }
          len = strnlen (g.inf, g.inz);
        }

      /* Only process regular files or named pipes, but allow symbolic links
       * if -f, recurse into directory if -r. 
       */
      if ((st.st_mode & S_IFMT) != S_IFREG &&
          (st.st_mode & S_IFMT) != S_IFIFO &&
          (st.st_mode & S_IFMT) != S_IFLNK &&
          (st.st_mode & S_IFMT) != S_IFDIR)
        {
          complain ("skipping: %s is a special file or device", g.inf);
          return;
        }
      if ((st.st_mode & S_IFMT) == S_IFLNK && !g.force && !g.pipeout)
        {
          complain ("skipping: %s is a symbolic link", g.inf);
          return;
        }
      if ((st.st_mode & S_IFMT) == S_IFDIR && !g.recurse)
        {
          complain ("skipping: %s is a directory", g.inf);
          return;
        }

      /* Recurse into directory (assumes Unix). */
      if ((st.st_mode & S_IFMT) == S_IFDIR)
        {
          char *roll = NULL;
          size_t size = 0, off = 0, base;
          DIR *here;
          struct dirent *next;

          /* Accumulate list of entries (need to do this, since readdir()
           * behavior not defined if directory modified between calls). 
           */
          here = opendir (g.inf);
          if (here == NULL)
            return;
          while ((next = readdir (here)) != NULL)
            {
              if (next->d_name[0] == 0 ||
                  (next->d_name[0] == '.' && (next->d_name[1] == 0 ||
                                              (next->d_name[1] == '.'
                                               && next->d_name[2] == 0))))
                continue;
              off = vstrcpy (&roll, &size, off, next->d_name);
            }
          closedir (here);
          vstrcpy (&roll, &size, off, "");

          /* Run process() for each entry in the directory. */
          base = len && g.inf[len - 1] != (unsigned char) '/' ?
            vstrcpy (&g.inf, &g.inz, len, "/") - 1 : len;
          for (off = 0; roll[off]; off += strlen (roll + off) + 1)
            {
              vstrcpy (&g.inf, &g.inz, base, roll + off);
              process (g.inf);
            }
          g.inf[len] = 0;

          /* Release list of entries. */
          FREE (roll);
          return;
        }

      /* Don't compress .gz (or provided suffix) files, unless -f. */
      if (!(g.force || g.list || g.decode) && len >= strlen (g.sufx) &&
          strcmp (g.inf + len - strlen (g.sufx), g.sufx) == 0)
        {
          complain ("skipping: %s ends with %s", g.inf, g.sufx);
          return;
        }

      /* Create output file only if input file has compressed suffix. */
      if (g.decode == 1 && !g.pipeout && !g.list)
        {
          size_t suf = compressed_suffix (g.inf);
          if (suf == 0)
            {
              complain ("skipping: %s does not have compressed suffix",
                        g.inf);
              return;
            }
          len -= suf;
        }

      /* Open input file. */
      g.ind = open (g.inf, O_RDONLY, 0);
      if (g.ind < 0)
        throw (errno, "read error on %s (%s)", g.inf, strerror (errno));

      /* Prepare gzip header information for compression. */
      g.name = g.headis & 1 ? justname (g.inf) : NULL;
      g.mtime = g.headis & 2 ? st.st_mtime : 0;
    }
  SET_BINARY_MODE (g.ind);

  /* If decoding or testing, try to read gzip header. */
  if (g.decode)
    {
      in_init ();
      method = get_header (1);
      if (method != 8 && method != 257 &&
          /* gzip -cdf acts like cat on uncompressed input. */
          !((method == -1 || method == -2) && g.force && g.pipeout &&
            g.decode != 2 && !g.list))
        {
          load_end ();
          complain (method == -6 ? "skipping: %s corrupt: header crc error" :
                    method == -1 ? "skipping: %s empty" :
                    method < 0 ? "skipping: %s unrecognized format" :
                    "skipping: %s unknown compression method", g.inf);
          return;
        }

      /* If requested, test input file (possibly a test list). */
      if (g.decode == 2)
        {
          try
          {
            if (method == 8)
              infchk ();
            else
              {
                unlzw ();
                if (g.list)
                  {
                    g.in_tot -= 3;
                    show_info (method, 0, g.out_tot, 0);
                  }
              }
          }
          catch (err)
          {
            if (err.code != EDOM)
              punt (err);
            complain ("skipping: %s", err.why);
            drop (err);
            outb (NULL, NULL, 0);
          }
          load_end ();
          return;
        }
    }

  /* If requested, just list information about input file. */
  if (g.list)
    {
      list_info ();
      load_end ();
      return;
    }

  /* Create output file out, descriptor outd. */
  if (path == NULL || g.pipeout)
    {
      /* Write to stdout. */
      g.outf = alloc (NULL, strlen ("<stdout>") + 1);
      strcpy (g.outf, "<stdout>");
      g.outd = 1;
      if (!g.decode && !g.force && isatty (g.outd))
        throw (EINVAL, "compressed data not written to a terminal. " 
                        "Use -f to force compression.\n"
                        "For help, type: gzip - h");
    }
  else
    {
      char *to = g.inf, *sufx = "";
      size_t pre = 0;

      /* Select parts of the output file name. */
      if (g.decode)
        {
          /* For -dN or -dNT, use the path from the input file and the name
           * from the header, stripping any path in the header name. 
           */
          if ((g.headis & 1) != 0 && g.hname != NULL)
            {
              pre = (size_t) (justname (g.inf) - g.inf);
              to = justname (g.hname);
              len = strlen (to);
            }
          /* For -d or -dNn, replace abbreviated suffixes. */
          else if (strcmp (to + len, ".tgz") == 0)
            sufx = ".tar";
        }
      else
        /* Add appropriate suffix when compressing. */
        sufx = g.sufx;

      /* Create output file and open to write. */
      g.outf = alloc (NULL, pre + len + strlen (sufx) + 1);
      memcpy (g.outf, g.inf, pre);
      memcpy (g.outf + pre, to, len);
      strcpy (g.outf + pre + len, sufx);
      g.outd = open (g.outf, O_CREAT | O_TRUNC | O_WRONLY |
                     (g.force ? 0 : O_EXCL), 0600);

      /* If exists and not -f, give user a chance to overwrite. */
      if (g.outd < 0 && errno == EEXIST && isatty (0) && g.verbosity)
        {
          int ch, reply;

          fprintf (stderr, "gzip: %s already exists; "
                           "do you wish to overwrite (y or n)", g.outf);
          fflush (stderr);
          reply = -1;
          do
            {
              ch = getchar ();
              if (reply < 0 && ch != ' ' && ch != '\t')
                reply = ch == 'y' || ch == 'Y' ? 1 : 0;
            }
          while (ch != EOF && ch != '\n' && ch != '\r');
          if (reply == 1)
            g.outd = open (g.outf, O_CREAT | O_TRUNC | O_WRONLY, 0600);
        }

      /* If exists and no overwrite, report and go on to next. */
      if (g.outd < 0 && errno == EEXIST)
        {
          complain ("skipping: %s exists", g.outf);
          RELEASE (g.outf);
          load_end ();
          return;
        }

      /* If some other error, give up. */
      if (g.outd < 0)
        throw (errno, "write error on %s (%s)", g.outf, strerror (errno));
    }
  SET_BINARY_MODE (g.outd);

  /* Process ind to outd. */
  if (g.verbosity > 1)
    fprintf (stderr, "%s to %s ", g.inf, g.outf);
  if (g.decode)
    {
      try
      {
        if (method == 8)
          infchk ();
        else if (method == 257)
          unlzw ();
        else
          cat ();
      }
      catch (err)
      {
        if (err.code != EDOM)
          punt (err);
        complain ("skipping: %s", err.why);
        drop (err);
        outb (NULL, NULL, 0);
        if (g.outd != -1 && g.outd != 1)
          {
            close (g.outd);
            g.outd = -1;
            unlink (g.outf);
            RELEASE (g.outf);
          }
      }
    }
#ifndef NOTHREAD
  else if (g.procs > 1)
    parallel_compress ();
#endif
  else
    single_compress (0);
  if (g.verbosity > 1)
    {
      putc ('\n', stderr);
      fflush (stderr);
    }

  /* Finish up, copy attributes, set times, delete original. */
  load_end ();
  if (g.outd != -1 && g.outd != 1)
    {
      if (g.sync)
        out_push ();            /* push to permanent storage */
      if (close (g.outd))
        throw (errno, "write error on %s (%s)", g.outf, strerror (errno));
      g.outd = -1;              /* now prevent deletion on interrupt */
      if (g.ind != 0)
        {
          copymeta (g.inf, g.outf);
          if (!g.keep)
            unlink (g.inf);
        }
      if (g.decode && (g.headis & 2) != 0 && g.stamp)
        touch (g.outf, g.stamp);
    }
  RELEASE (g.outf);
}

static  char *helptext[] = {
  "Usage: gzip [OPTION]... [FILE]...",
  "Compress or uncompress FILEs (by default, compress FILES in-place).",
  "",
  "Mandatory arguments to long options are mandatory for short options too.",
  "",
#ifdef NOZOPFLI
  "  -0 to -9             Compression level",
#else
  "  -0 to -9, -11        Compression level (level 11, zopfli, is much slower)",
#endif
  "  -1, --fast           Compress faster",
  "  -9, --best           Compress better",
  "  -A, --alias xxx      Use xxx as the name for any --zip entry from stdin",
  "  -b, --blocksize mmm  Set compression block size to mmmK (default 128K)",
  "  -c, --stdout         Write on standard output, keep original files unchanged",
  "  -C, --comment ccc    Put comment ccc in the gzip or zip header",
  "  -d, --decompress     Decompress",
  "  -f, --force          Force overwrite of output file and compress links",
#ifndef NOZOPFLI
  "  -F  --first          Do iterations first, before block split for -11",
#endif
  "  -h, --help           Give this help",
  "  -i, --independent    Compress blocks independently for damage recovery",
#ifndef NOZOPFLI
  "  -I, --iterations n   Number of iterations for -11 optimization",
#endif
#ifndef NOTHREAD
  "  -j, --jobs n         Allow up to n compression threads (default is the",
  "                       number of online processors, or 8 if unknown)",
#endif
#ifndef NOZOPFLI
  "  -J, --maxsplits n    Maximum number of split blocks for -11",
#endif
  "  -k, --keep           Keep (don't delete) input files",
  "  -K, --zip            Compress to PKWare zip (.zip) single entry format",
  "  -l, --list           List compressed file contents",
  "  -L, --license        Display software license",
  "  -m, --no-time        Do not store or restore mod time",
  "  -M, --time           Store or restore mod time",
  "  -n, --no-name        Do not save or restore the original name and time stamp",
  "  -N, --name           Save or restore the original name and time stamp",
#ifndef NOZOPFLI
  "  -O  --oneblock       Do not split into smaller blocks for -11",
#endif
  "  -q, --quiet          Suppress all warnings",
  "  -r, --recursive      Operate recursively on directories",
  "  -R, --rsyncable      Make rsync-friendly archive",
  "  -S, --suffix=SUF     Use suffix SUF on compressed files",
  "  -t, --test           Test compressed file integrity",
#ifdef PIGZ_DEBUG
  "  -v, --verbose        Provide more verbose output (-vv to debug)",
#else
  "  -v, --verbose        Verbose mode",
#endif
  "  -V  --version        Display version number",
  "  -Y  --synchronous    Force output file write to permanent storage",
  "  -z, --zlib           Compress to zlib (.zz) instead of gzip format",
  "  --                   All arguments after \"--\" are treated as files",
  "",
  "With no FILE, or when FILE is -, read standard input.",
  "Report bugs to <bug-gzip@gnu.org>."
};

/* Display the help text above. */
static  void
help (void)
{
  int n;

  if (g.verbosity == 0)
    return;
  for (n = 0; n < (int) (sizeof (helptext) / sizeof (char *)); n++)
    fprintf (stderr, "%s\n", helptext[n]);
  fflush (stderr);
  exit (0);
}

#ifndef NOTHREAD

/* Try to determine the number of processors. */
static  int
nprocs (int n)
{
#ifdef _SC_NPROCESSORS_ONLN
  n = (int) sysconf (_SC_NPROCESSORS_ONLN);
#else
#ifdef _SC_NPROC_ONLN
  n = (int) sysconf (_SC_NPROC_ONLN);
#else
#ifdef __hpux
  struct pst_dynamic psd;

  if (pstat_getdynamic (&psd, sizeof (psd), (size_t) 1, 0) != -1)
    n = psd.psd_proc_cnt;
#endif
#endif
#endif
  return n;
}

#endif

/* Set option defaults. */
static  void
defaults (void)
{
  g.level = Z_DEFAULT_COMPRESSION;
#ifndef NOZOPFLI
  /* default zopfli options as set by ZopfliInitOptions():
   * verbose = 0
   * numiterations = 15
   * blocksplitting = 1
   * blocksplittinglast = 0
   * blocksplittingmax = 15
   */
  ZopfliInitOptions (&g.zopts);
#endif
#ifdef NOTHREAD
  g.procs = 1;
#else
  g.procs = nprocs (8);
#endif
  g.block = 131072UL;           /* 128K */
  g.shift = x2nmodp (g.block, 3);
  g.rsync = 0;                  /* don't do rsync blocking */
  g.setdict = 1;                /* initialize dictionary each thread */
  g.verbosity = 1;              /* normal message level */
  g.headis = 3;                 /* store name and time (low bits == 11), */
 /* Restore neither (next bits == 00), where 01 is name and 10 is time. */
  g.pipeout = 0;                /* don't force output to stdout */
  g.sufx = ".gz";               /* compressed file suffix */
  g.comment = NULL;             /* no comment */
  g.decode = 0;                 /* compress */
  g.list = 0;                   /* compress */
  g.keep = 0;                   /* delete input file once compressed */
  g.force = 0;                  /* don't overwrite, don't compress links */
  g.sync = 0;                   /* don't force a flush on output */
  g.recurse = 0;                /* don't go into directories */
  g.form = 0;                   /* use gzip format */
}


/* Either new buffer size, new compression level, or new number of processes.
 * Get rid of old buffers and threads to force the creation of new ones with
 * the new settings. 
 */
static  void
new_opts (void)
{
  single_compress (1);
#ifndef NOTHREAD
  finish_jobs ();
#endif
}

/* Verify that arg is only digits, and if so, return the decimal value. */
static  size_t
num (char *arg)
{
  char *str = arg;
  size_t val = 0;

  if (*str == 0)
    throw (EINVAL, "internal error: empty parameter");
  do
    {
      if (*str < '0' || *str > '9' ||
          (val && ((~(size_t) 0) - (size_t) (*str - '0')) / val < 10))
        throw (EINVAL, "'%s' operand is not numeric\n"
                       "Try `gzip --help' for more information.", arg);
      val = val * 10 + (size_t) (*str - '0');
    }
  while (*++str);
  return val;
}

#ifndef NOTHREAD
/* Handle error received from yarn function. */
static  void
cut_yarn (int err)
{
  throw (err, "internal threads error");
}
#endif

/* Process command line arguments. */
int
main (int argc, char **argv)
{
  int n;                        /* general index */
  int done;                     /* number of named files processed */
  int optc;                     /* For Option Handling */
  size_t j;                     /* ^^^^^^^^^^^^^^^^^^^ */
  size_t k;                     /* program name length */
  char *p;                      /* environment default options, marker */
  ball_t err;                   /* error information from throw() */

  g.ret = 0;                    /* return code */

  /* Move these after testing. */
  static char const short_options[] = ":b:cC:dfFhiI:j:J:klLmMnNqrRS:tvVYz0123456789";
  static struct option const long_options[] =
    {
      { "fast",        0, 0, '1' },
      { "best",        0, 0, '9' },
      /* {"ascii",    0, 0, 'a}, */
      { "blocksize",   1, 0, 'b' },
      { "stdout",      0, 0, 'c' },
      { "to-stdout",   0, 0, 'c' },
      { "complevel",   1, 0, 'C' },
      { "decompress",  0, 0, 'd' },
      { "uncompress",  0, 0, 'd' },
      { "force",       0, 0, 'f' },
      { "first",       0, 0, 'F' },
      { "help",        0, 0, 'h' },
      { "independent", 0, 0, 'i' },
      { "iterations",  1, 0, 'I' },
      { "jobs",        1, 0, 'j' },
      { "maxsplits",   1, 0, 'J' },
      { "keep",        0, 0, 'k' },
      { "list",        0, 0, 'l' },
      { "license",     0, 0, 'L' },
      { "no-time",     0, 0, 'm' },
      { "time",        0, 0, 'M' },
      { "no-name",     0, 0, 'n' },
      { "name",        0, 0, 'N' },
      { "oneblock",    0, 0, 'O' },
      { "quiet",       0, 0, 'q' },
      { "recursive",   0, 0, 'r' },
      { "rsyncable",   0, 0, 'R' },
      { "suffix",      1, 0, 'S' },
      { "test",        0, 0, 't' },
      { "verbose",     0, 0, 'v' },
      { "version",     0, 0, 'V' },
      { "synchronous", 0, 0, 'Y' },
      { "zlib",        0, 0, 'z' },
      /* { "LZW",        0, 0, 'Z' }, */
      { NULL, 0, 0, 0 }
    };


  try
    {
      /* Initialize globals. */
      g.inf = NULL;
      g.inz = 0;
#ifndef NOTHREAD
      g.in_which = -1;
#endif
      g.alias = "-"; /* !!! new in develop pigz version !!! */
      g.outf = NULL; 
      g.first = 1;
      g.hname = NULL;
      g.hcomm = NULL; /* !!! new in develop pigz version !!! */

      /* Save pointer to program name for error messages. */
      p = strrchr (argv[0], '/');
      p = p == NULL ? argv[0] : p + 1;
      g.prog = *p ? p : "pigz";

      /* Prepare for interrupts and logging. */
      signal (SIGINT, cut_short);
#ifndef NOTHREAD
      yarn_prefix = g.prog;       /* prefix for yarn error messages */
      yarn_abort = cut_yarn;      /* call on thread error */
#endif
#ifdef PIGZ_DEBUG
      gettimeofday (&start, NULL);    /* starting time for log entries */
      log_init ();                    /* initialize logging */
#endif

      /* Set all options to defaults. */
      defaults ();

      /* Check zlib version. */
      if (zlib_vernum () < 0x1230)
        throw (EINVAL, "zlib version less than 1.2.3");

      /* Create CRC table, in case zlib compiled with dynamic tables. */
      get_crc_table ();  /* !!! new in develop pigz version !!! */

      /* TODO: CHANGE TO ungzip rather than unpigz later on
       * ENSURE WE ACCEPT zcat, pcat, gcat, and gzcat 
       * decompress if named "unpigz" or "gunzip", to stdout if "*cat" 
       */
      if (strcmp (g.prog, "unpigz") == 0 || strcmp (g.prog, "gunzip") == 0)
        {
          if (!g.decode)
            g.headis >>= 2;
          g.decode = 1;
        }
      if ((k = strlen (g.prog)) > 2 && strcmp (g.prog + k - 3, "cat") == 0)
        {
          if (!g.decode)
             g.headis >>= 2;
          g.decode = 1;
          g.pipeout = 1;
        }

      /* Error if user has environment variables  */
      /* This could affect people when they update, so ask eggert */
      if (getenv ("GZIP") != NULL)
        throw (EINVAL, "Environment variable support removed in"
                      "gzip version x.\nRun `unset GZIP' to fix.");

      /* If no arguments and compressed data to/from terminal, show help. */
      if (argc < 2 && isatty (g.decode ? 0 : 1))
        help ();

      /* Process all command-line options first, move to own function later. */
      while ((optc = getopt_long(argc, argv, short_options, long_options, NULL)) != -1)
        {
          switch (optc)
            {
              /* Z and a are NOT supported by pigz! */
              case '0': case '1': case '2': case '3': case '4':
              case '5': case '6': case '7': case '8': case '9':
                        if (g.level == 1 && (optc - '0') == 1)
                          g.level = 11;
                        else
                          g.level = optc - '0';
                        printf("%d ", g.level);
                        break;
              case 'b': j = num (optarg);
                        g.block = j << 10;                  /* chunk size */
                        if (g.block < DICT)
                          throw (EINVAL, "block size too small"
                                        "(must be >= 32K) -- '%s'\n"
                                        "Try `gzip --help' for more"
                                        "information.", optarg);
                        if (j != g.block >> 10 ||
                          OUTPOOL (g.block) < g.block ||
                                 (ssize_t)OUTPOOL (g.block) < 0 ||
                                  g.block > (1UL << 29))  /* limited by append_len() */
                          throw (EINVAL, "block size too large -- '%s'\n"
                                        "Try `gzip --help' for more"
                                        "information.", optarg);
                        break;
              case 'c':  g.pipeout = 1;  break;
              case 'd':  if (!g.decode)
                           g.headis >>= 2;
                         g.decode = 1;  break;
              case 'f':  g.force = 1;  break;
              case 'h':  help ();  break;
              case 'i':  g.setdict = 0;  break;
              case 'j':  j = num (optarg); /* Use of num function to be removed later */
                         g.procs = (int)j;                   /* # processes */
                         if (g.procs < 1)
                           throw (EINVAL, "invalid number of processes -- '%s'"
                               "\nTry `gzip --help' for more "
                               "information", optarg);
                         if ((size_t)g.procs != j || INBUFS (g.procs) < 1)
                           throw (EINVAL, "too many processes: %s", optarg);
#ifdef NOTHREAD
                         if (g.procs > 1)
                           throw(EINVAL, "compiled without threads");
#endif
                         break;
              case 'k':  g.keep = 1;  break;
              case 'K':  g.form = 2;
                         g.sufx = ".zip";  break;
              case 'l':  g.list = 1;  break;
              case 'L':  fputs (VERSION, stderr); /* TODO: Ask professor what we make this */
                         fputs ("Copyright (C) 2007-2017 Mark Adler\n", stderr);
                         fputs ("Subject to the terms of the zlib license.\n", stderr);
                         fputs ("No warranty is provided or implied.\n", stderr);
                         exit (0); break;
              case 'm':  g.headis &= ~0xa;  break;
              case 'M':  g.headis |= 0xa;  break;
              case 'n':  g.headis = 0;  break;
              case 'N':  g.headis = 0xf;  break;
#ifndef NOZOPFLI
              case 'O':  g.zopts.blocksplitting = 0;  break;
              case 'F':  g.zopts.blocksplittinglast = 1;  break;
              case 'I':  g.zopts.numiterations = (int)num (optarg);  break; /* optimize iterations */
              case 'J':  g.zopts.blocksplittingmax = (int)num (optarg);  break; /* max block splits */
#endif
              case 'q':  g.verbosity = 0;  break;
              case 'r':  g.recurse = 1;  break;
              case 'R':  g.rsync = 1;  break;
              case 't':  g.decode = 2;  break;
              case 'S':  g.sufx = optarg; /* gz suffix */
                         if (strlen (g.sufx) == 0 || strlen (g.sufx) > 30) /* TODO: DEFINE MAX_SUFFIX AS 30 AND REPLACE */
                           throw (EINVAL, "invalid suffix '%s'", g.sufx);
                         break;
              case 'v': g.verbosity++;  break;
              case 'V': fputs (VERSION, stderr);
                        if (g.verbosity > 1)
                          fprintf (stderr, "zlib %s\n", zlibVersion());
                        exit (0);
                        break;
              case 'Y':  g.sync = 1;  break; /* Synchronous option, not in pdf should be added to docs */
              case 'z':  g.form = 1;
                         g.sufx = ".zz";  break;
              case ':': throw (EINVAL, "option requires an argument -- '%c'\n"
                                      "Try `gzip --help' for more information",
                                      optopt);
                        break;
              default:  if (optopt) /* invalid short opt */
                          throw (EINVAL, "invalid option -- '%c'\nTry `gzip "
                                    "--help' for more information.", optopt);
                        else /* invalid long opt */
                          throw (EINVAL, "unrecognized option: '%s'\nTry `gzip"
                                        "--help' for more information",
                                        argv[(int)optind - 1]);
                          break;
            }
        }

      /* Process command-line filenames. */
      /* optind is the start of the files. */
      done = 0;
      for (n = (int)optind; n < argc; n++)
        if (argv[n] != NULL)
          { 
            if (done == 1 && g.pipeout && !g.decode && !g.list && g.form > 1)
              complain ("warning: output will be concatenated zip files"
                        " -- %s will not be able to extract", g.prog);
            process (strcmp (argv[n], "-") == 0 ? NULL : argv[n]);
            done++;
          }

      /* List stdin or compress stdin to stdout if no file names provided. */
      if (done == 0)
        process (NULL);
    }
  always
    {
      /* Release resources. */
      RELEASE (g.inf);
      g.inz = 0;
      new_opts ();
    }
  catch (err)
    {
      THREADABORT (err);
    }

  /* Show log (if any). */
  log_dump ();
  return g.ret;
}
