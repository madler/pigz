/* util.c -- utilities for pigz support
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

/* Included headers and expected functions, vars, types, etc */
#include <stdio.h>      /* fflush(), fprintf(), fputs(), getchar(), putc(),
                         * puts(), printf(), vasprintf(), stderr, EOF, NULL,
                         * SEEK_END, size_t, off_t 
                         */
#include <errno.h>      /* errno, EEXIST */
#include <signal.h>     /* signal(), SIGINT */

#if __STDC_VERSION__-0 >= 199901L || __GNUC__-0 >= 3
#include <inttypes.h>           // intmax_t, uintmax_t
typedef uintmax_t length_t;
typedef uint32_t crc_t;
#else
typedef unsigned long length_t;
typedef unsigned long crc_t;
#endif

#include "zlib.h"       /* deflateInit2(), deflateReset(), deflate(),
                         * deflateEnd(), deflateSetDictionary(), crc32(),
                         * adler32(), inflateBackInit(), inflateBack(),
                         * inflateBackEnd(), Z_DEFAULT_COMPRESSION,
                         * Z_DEFAULT_STRATEGY, Z_DEFLATED, Z_NO_FLUSH, Z_NULL,
                         * Z_OK, Z_SYNC_FLUSH, z_stream
                         */

#ifndef NOTHREAD
#include "lib/yarn.h"   /* thread, launch(), join(), join_all(), lock,
                         * new_lock(), possess(), twist(), wait_for(),
                         * release(), peek_lock(), free_lock(), yarn_name
                         */
#endif

#ifndef NOZOPFLI
#include "lib/zopfli/deflate.h"    /* ZopfliDeflatePart(),
                                    * ZopfliInitOptions(),
                                    * ZopfliOptions
                                    */
#endif

#include "lib/try.h"            /* try, catch, always, throw, drop, punt, ball_t */
      
// Prevent end-of-line conversions on MSDOSish operating systems.
#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#include <io.h>                 // setmode(), O_BINARY
#define SET_BINARY_MODE(fd) setmode(fd, O_BINARY)
#else
#define SET_BINARY_MODE(fd)
#endif

// Release an allocated pointer, if allocated, and mark as unallocated.
#define RELEASE(ptr) \
    do { \
        if ((ptr) != NULL) { \
            FREE(ptr); \
            ptr = NULL; \
        } \
    } while (0)

// Sliding dictionary size for deflate.
#define DICT 32768U

// Largest power of 2 that fits in an unsigned int. Used to limit requests to
// zlib functions that use unsigned int lengths.
#define MAXP2 (UINT_MAX - (UINT_MAX >> 1))

/* rsyncable constants -- RSYNCBITS is the number of bits in the mask for
   comparison. For random input data, there will be a hit on average every
   1<<RSYNCBITS bytes. So for an RSYNCBITS of 12, there will be an average of
   one hit every 4096 bytes, resulting in a mean block size of 4096. RSYNCMASK
   is the resulting bit mask. RSYNCHIT is what the hash value is compared to
   after applying the mask.

   The choice of 12 for RSYNCBITS is consistent with the original rsyncable
   patch for gzip which also uses a 12-bit mask. This results in a relatively
   small hit to compression, on the order of 1.5% to 3%. A mask of 13 bits can
   be used instead if a hit of less than 1% to the compression is desired, at
   the expense of more blocks transmitted for rsync updates. (Your mileage may
   vary.)

   This implementation of rsyncable uses a different hash algorithm than what
   the gzip rsyncable patch uses in order to provide better performance in
   several regards. The algorithm is simply to shift the hash value left one
   bit and exclusive-or that with the next byte. This is masked to the number
   of hash bits (RSYNCMASK) and compared to all ones except for a zero in the
   top bit (RSYNCHIT). This rolling hash has a very small window of 19 bytes
   (RSYNCBITS+7). The small window provides the benefit of much more rapid
   resynchronization after a change, than does the 4096-byte window of the gzip
   rsyncable patch.

   The comparison value is chosen to avoid matching any repeated bytes or short
   sequences. The gzip rsyncable patch on the other hand uses a sum and zero
   for comparison, which results in certain bad behaviors, such as always
   matching everywhere in a long sequence of zeros. Such sequences occur
   frequently in tar files.

   This hash efficiently discards history older than 19 bytes simply by
   shifting that data past the top of the mask -- no history needs to be
   retained to undo its impact on the hash value, as is needed for a sum.

   The choice of the comparison value (RSYNCHIT) has the virtue of avoiding
   extremely short blocks. The shortest block is five bytes (RSYNCBITS-7) from
   hit to hit, and is unlikely. Whereas with the gzip rsyncable algorithm,
   blocks of one byte are not only possible, but in fact are the most likely
   block size.

   Thanks and acknowledgement to Kevin Day for his experimentation and insights
   on rsyncable hash characteristics that led to some of the choices here.
 */

#define RSYNCBITS 12
#define RSYNCMASK ((1U << RSYNCBITS) - 1)
#define RSYNCHIT (RSYNCMASK >> 1)

// Initial pool counts and sizes -- INBUFS is the limit on the number of input
// spaces as a function of the number of processors (used to throttle the
// creation of compression jobs), OUTPOOL is the initial size of the output
// data buffer, chosen to make resizing of the buffer very unlikely and to
// allow prepending with a dictionary for use as an input buffer for zopfli.
#define INBUFS(p) (((p)<<1)+3)
#define OUTPOOL(s) ((s)+((s)>>4)+DICT)

// Input buffer size, and augmentation for re-inserting a central header.
#define BUF 32768
#define CEN 42
#define EXT (BUF + CEN)         // provide enough room to unget a header

// Globals (modified by main thread only when it's the only thread).
static  struct
{
  int volatile ret;             // pigz return code
  char *prog;                   // name by which pigz was invoked
  int ind;                      // input file descriptor
  int outd;                     // output file descriptor
  char *inf;                    // input file name (allocated)
  size_t inz;                   // input file name allocated size
  char *outf;                   // output file name (allocated)
  int verbosity;                // 0 = quiet, 1 = normal, 2 = verbose, 3 = trace
  int headis;                   // 1 to store name, 2 to store date, 3 both
  int pipeout;                  // write output to stdout even if file
  int keep;                     // true to prevent deletion of input file
  int force;                    // true to overwrite, compress links, cat
  int sync;                     // true to flush output file
  int form;                     // gzip = 0, zlib = 1, zip = 2 or 3
  int magic1;                   // first byte of possible header when decoding
  int recurse;                  // true to dive down into directory structure
  char *sufx;                   // suffix to use (".gz" or user supplied)
  char *name;                   // name for gzip or zip header
  char *alias;                  // name for zip header when input is stdin
  char *comment;                // comment for gzip or zip header.
  time_t mtime;                 // time stamp from input file for gzip header
  int list;                     // true to list files instead of compress
  int first;                    // true if we need to print listing header
  int decode;                   // 0 to compress, 1 to decompress, 2 to test
  int level;                    // compression level
#ifndef NOZOPFLI
  ZopfliOptions zopts;          // zopfli compression options
#endif
  int rsync;                    // true for rsync blocking
  int procs;                    // maximum number of compression threads (>= 1)
  int setdict;                  // true to initialize dictionary in each thread
  size_t block;                 // uncompressed input size per thread (>= 32K)
  crc_t shift;                  // pre-calculated CRC-32 shift for length block

  // saved gzip/zip header data for decompression, testing, and listing
  time_t stamp;                 // time stamp from gzip header
  char *hname;                  // name from header (allocated)
  char *hcomm;                  // comment from header (allocated)
  unsigned long zip_crc;        // local header crc
  length_t zip_clen;            // local header compressed length
  length_t zip_ulen;            // local header uncompressed length
  int zip64;                    // true if has zip64 extended information

  // globals for decompression and listing buffered reading
  unsigned char in_buf[EXT];    // input buffer
  unsigned char *in_next;       // next unused byte in buffer
  size_t in_left;               // number of unused bytes in buffer
  int in_eof;                   // true if reached end of file on input
  int in_short;                 // true if last read didn't fill buffer
  length_t in_tot;              // total bytes read from input
  length_t out_tot;             // total bytes written to output
  unsigned long out_check;      // check value of output

#ifndef NOTHREAD
  // globals for decompression parallel reading
  unsigned char in_buf2[EXT];   // second buffer for parallel reads
  size_t in_len;                // data waiting in next buffer
  int in_which;                 // -1: start, 0: in_buf2, 1: in_buf
  lock *load_state;             // value = 0 to wait, 1 to read a buffer
  thread *load_thread;          // load_read() thread for joining
#endif
} g;

// Display a complaint with the program name on stderr.
static  int
complain (char *fmt, ...)
{
  va_list ap;

  if (g.verbosity > 0)
    {
      fprintf (stderr, "%s: ", g.prog);
      va_start (ap, fmt);
      vfprintf (stderr, fmt, ap);
      va_end (ap);
      putc ('\n', stderr);
      fflush (stderr);
    }
  g.ret = 1;
  return 0;
}

#ifdef PIGZ_DEBUG

// Memory tracking.

#define MAXMEM 131072           // maximum number of tracked pointers

static  struct mem_track_s
{
  size_t num;                   // current number of allocations
  size_t size;                  // total size of current allocations
  size_t tot;                   // maximum number of allocations
  size_t max;                   // maximum size of allocations
#ifndef NOTHREAD
  lock *lock;                   // lock for access across threads
#endif
  size_t have;                  // number in array (possibly != num)
  void *mem[MAXMEM];            // sorted array of allocated pointers
} mem_track;

#ifndef NOTHREAD
#define mem_track_grab(m) possess((m)->lock)
#define mem_track_drop(m) release((m)->lock)
#else
#define mem_track_grab(m)
#define mem_track_drop(m)
#endif

// Return the leftmost insert location of ptr in the sorted list mem->mem[],
// which currently has mem->have elements. If ptr is already in the list, the
// returned value will point to its first occurrence. The return location will
// be one after the last element if ptr is greater than all of the elements.
static  size_t
search_track (struct mem_track_s *mem, void *ptr)
{
  ptrdiff_t left = 0;
  ptrdiff_t right = mem->have - 1;
  while (left <= right)
    {
      ptrdiff_t mid = (left + right) >> 1;
      if (mem->mem[mid] < ptr)
        left = mid + 1;
      else
        right = mid - 1;
    }
  return left;
}

// Insert ptr in the sorted list mem->mem[] and update the memory allocation
// statistics.
static  void
insert_track (struct mem_track_s *mem, void *ptr)
{
  mem_track_grab (mem);
  assert (mem->have < MAXMEM && "increase MAXMEM in source and try again");
  size_t i = search_track (mem, ptr);
  if (i < mem->have && mem->mem[i] == ptr)
    complain ("mem_track: duplicate pointer %p\n", ptr);
  memmove (&mem->mem[i + 1], &mem->mem[i], (mem->have - i) * sizeof (void *));
  mem->mem[i] = ptr;
  mem->have++;
  mem->num++;
  mem->size += MALLOC_SIZE (ptr);
  if (mem->num > mem->tot)
    mem->tot = mem->num;
  if (mem->size > mem->max)
    mem->max = mem->size;
  mem_track_drop (mem);
}

// Find and delete ptr from the sorted list mem->mem[] and update the memory
// allocation statistics.
static  void
delete_track (struct mem_track_s *mem, void *ptr)
{
  mem_track_grab (mem);
  size_t i = search_track (mem, ptr);
  if (i < mem->num && mem->mem[i] == ptr)
    {
      memmove (&mem->mem[i], &mem->mem[i + 1],
               (mem->have - (i + 1)) * sizeof (void *));
      mem->have--;
    }
  else
    complain ("mem_track: missing pointer %p\n", ptr);
  mem->num--;
  mem->size -= MALLOC_SIZE (ptr);
  mem_track_drop (mem);
}

static  void *
malloc_track (struct mem_track_s *mem, size_t size)
{
  void *ptr = malloc (size);
  if (ptr != NULL)
    insert_track (mem, ptr);
  return ptr;
}

static  void *
realloc_track (struct mem_track_s *mem, void *ptr, size_t size)
{
  if (ptr == NULL)
    return malloc_track (mem, size);
  delete_track (mem, ptr);
  void *got = realloc (ptr, size);
  insert_track (mem, got == NULL ? ptr : got);
  return got;
}

static  void
free_track (struct mem_track_s *mem, void *ptr)
{
  if (ptr != NULL)
    {
      delete_track (mem, ptr);
      free (ptr);
    }
}

#ifndef NOTHREAD
static  void *
yarn_malloc (size_t size)
{
  return malloc_track (&mem_track, size);
}

static  void
yarn_free (void *ptr)
{
  free_track (&mem_track, ptr);
}
#endif

static  voidpf
zlib_alloc (voidpf opaque, uInt items, uInt size)
{
  return malloc_track (opaque, items * (size_t) size);
}

static  void
zlib_free (voidpf opaque, voidpf address)
{
  free_track (opaque, address);
}

#define REALLOC(p, s) realloc_track(&mem_track, p, s)
#define FREE(p) free_track(&mem_track, p)
#define OPAQUE (&mem_track)
#define ZALLOC zlib_alloc
#define ZFREE zlib_free

#else // !PIGZ_DEBUG

#define REALLOC realloc
#define FREE free
#define OPAQUE Z_NULL
#define ZALLOC Z_NULL
#define ZFREE Z_NULL

#endif

// Assured memory allocation.
static  void *
alloc (void *ptr, size_t size)
{
  ptr = REALLOC (ptr, size);
  if (ptr == NULL)
    throw (ENOMEM, "not enough memory");
  return ptr;
}

#ifdef PIGZ_DEBUG

// Logging. 

// Starting time of day for tracing.
static  struct timeval start;

// Trace log.
static  struct log
{
  struct timeval when;          // time of entry
  char *msg;                    // message
  struct log *next;             // next entry
} *log_head, **log_tail = NULL;
#ifndef NOTHREAD
static  lock *log_lock = NULL;
#endif

// Maximum log entry length.
#define MAXMSG 256

// Set up log (call from main thread before other threads launched).
static  void
log_init (void)
{
  if (log_tail == NULL)
    {
      mem_track.num = 0;
      mem_track.size = 0;
      mem_track.num = 0;
      mem_track.max = 0;
      mem_track.have = 0;
#ifndef NOTHREAD
      mem_track.lock = new_lock (0);
      yarn_mem (yarn_malloc, yarn_free);
      log_lock = new_lock (0);
#endif
      log_head = NULL;
      log_tail = &log_head;
    }
}

// Add entry to trace log.
static  void
log_add (char *fmt, ...)
{
  struct timeval now;
  struct log *me;
  va_list ap;
  char msg[MAXMSG];

  gettimeofday (&now, NULL);
  me = alloc (NULL, sizeof (struct log));
  me->when = now;
  va_start (ap, fmt);
  vsnprintf (msg, MAXMSG, fmt, ap);
  va_end (ap);
  me->msg = alloc (NULL, strlen (msg) + 1);
  strcpy (me->msg, msg);
  me->next = NULL;
#ifndef NOTHREAD
  assert (log_lock != NULL);
  possess (log_lock);
#endif
  *log_tail = me;
  log_tail = &(me->next);
#ifndef NOTHREAD
  twist (log_lock, BY, +1);
#endif
}

// Pull entry from trace log and print it, return false if empty.
static  int
log_show (void)
{
  struct log *me;
  struct timeval diff;

  if (log_tail == NULL)
    return 0;
#ifndef NOTHREAD
  possess (log_lock);
#endif
  me = log_head;
  if (me == NULL)
    {
#ifndef NOTHREAD
      release (log_lock);
#endif
      return 0;
    }
  log_head = me->next;
  if (me->next == NULL)
    log_tail = &log_head;
#ifndef NOTHREAD
  twist (log_lock, BY, -1);
#endif
  diff.tv_usec = me->when.tv_usec - start.tv_usec;
  diff.tv_sec = me->when.tv_sec - start.tv_sec;
  if (diff.tv_usec < 0)
    {
      diff.tv_usec += 1000000L;
      diff.tv_sec--;
    }
  fprintf (stderr, "trace %ld.%06ld %s\n",
           (long) diff.tv_sec, (long) diff.tv_usec, me->msg);
  fflush (stderr);
  FREE (me->msg);
  FREE (me);
  return 1;
}

// Release log resources (need to do log_init() to use again).
static  void
log_free (void)
{
  struct log *me;

  if (log_tail != NULL)
    {
#ifndef NOTHREAD
      possess (log_lock);
#endif
      while ((me = log_head) != NULL)
        {
          log_head = me->next;
          FREE (me->msg);
          FREE (me);
        }
#ifndef NOTHREAD
      twist (log_lock, TO, 0);
      free_lock (log_lock);
      log_lock = NULL;
      yarn_mem (malloc, free);
      free_lock (mem_track.lock);
#endif
      log_tail = NULL;
    }
}

// Show entries until no more, free log.
static  void
log_dump (void)
{
  if (log_tail == NULL)
    return;
  while (log_show ())
    ;
  log_free ();
  if (mem_track.num || mem_track.size)
    complain ("memory leak: %lu allocs of %lu bytes total",
              mem_track.num, mem_track.size);
  if (mem_track.max)
    fprintf (stderr, "%lu bytes of memory used in %lu allocs\n",
             mem_track.max, mem_track.tot);
}

// Debugging macro.
#define Trace(x) \
    do { \
        if (g.verbosity > 2) { \
            log_add x; \
        } \
    } while (0)

#else // !PIGZ_DEBUG

#define log_dump()
#define Trace(x)

#endif

// Abort or catch termination signal.
static  void
cut_short (int sig)
{
  if (sig == SIGINT)
    {
      Trace (("termination by user"));
    }
  if (g.outd != -1 && g.outd != 1)
    {
      unlink (g.outf);
      RELEASE (g.outf);
      g.outd = -1;
    }
  log_dump ();
  _exit (sig < 0 ? -sig : EINTR);
}

// Common code for catch block of top routine in the thread.
#define THREADABORT(ball) \
    do { \
        complain("%s", (ball).why); \
        drop(ball); \
        cut_short(-(ball).code); \
    } while (0)

// Compute next size up by multiplying by about 2**(1/3) and rounding to the
// next power of 2 if close (three applications results in doubling). If small,
// go up to at least 16, if overflow, go to max size_t value.
static  inline size_t
grow (size_t size)
{
  size_t was, top;
  int shift;

  was = size;
  size += size >> 2;
  top = size;
  for (shift = 0; top > 7; shift++)
    top >>= 1;
  if (top == 7)
    size = (size_t) 1 << (shift + 3);
  if (size < 16)
    size = 16;
  if (size <= was)
    size = (size_t) 0 - 1;
  return size;
}

// Copy cpy[0..len-1] to *mem + off, growing *mem if necessary, where *size is
// the allocated size of *mem. Return the number of bytes in the result.
static  inline size_t
vmemcpy (char **mem, size_t * size, size_t off, void *cpy, size_t len)
{
  size_t need;

  need = off + len;
  if (need < off)
    throw (ERANGE, "overflow");
  if (need > *size)
    {
      need = grow (need);
      if (off == 0)
        {
          RELEASE (*mem);
          *size = 0;
        }
      *mem = alloc (*mem, need);
      *size = need;
    }
  memcpy (*mem + off, cpy, len);
  return off + len;
}

// Copy the zero-terminated string cpy to *str + off, growing *str if
// necessary, where *size is the allocated size of *str. Return the length of
// the string plus one.
static  inline size_t
vstrcpy (char **str, size_t * size, size_t off, void *cpy)
{
  return vmemcpy (str, size, off, cpy, strlen (cpy) + 1);
}

// Read up to len bytes into buf, repeating read() calls as needed.
static  size_t
readn (int desc, unsigned char *buf, size_t len)
{
  ssize_t ret;
  size_t got;

  got = 0;
  while (len)
    {
      ret = read (desc, buf, len);
      if (ret < 0)
        throw (errno, "read error on %s (%s)", g.inf, strerror (errno));
      if (ret == 0)
        break;
      buf += ret;
      len -= (size_t) ret;
      got += (size_t) ret;
    }
  return got;
}

// Convert Unix time to MS-DOS date and time, assuming the current timezone.
// (You got a better idea?)
static  unsigned long
time2dos (time_t t)
{
  struct tm *tm;
  unsigned long dos;

  if (t == 0)
    t = time (NULL);
  tm = localtime (&t);
  if (tm->tm_year < 80 || tm->tm_year > 207)
    return 0;
  dos = (unsigned long) (tm->tm_year - 80) << 25;
  dos += (unsigned long) (tm->tm_mon + 1) << 21;
  dos += (unsigned long) tm->tm_mday << 16;
  dos += (unsigned long) tm->tm_hour << 11;
  dos += (unsigned long) tm->tm_min << 5;
  dos += (unsigned long) (tm->tm_sec + 1) >> 1; // round to even seconds
  return dos;
}



// Write len bytes, repeating write() calls as needed. Return len.
static  size_t
writen (int desc, void const *buf, size_t len)
{
  char const *next = buf;
  size_t left = len;

  while (left)
    {
      size_t const max = SIZE_MAX >> 1; // max ssize_t
      ssize_t ret = write (desc, next, left > max ? max : left);
      if (ret < 1)
        throw (errno, "write error on %s (%s)", g.outf, strerror (errno));
      next += ret;
      left -= (size_t) ret;
    }
  return len;
}


// Value type for put() value arguments. All value arguments for put() must be
// cast to this type in order for va_arg() to pull the correct type from the
// argument list.
typedef length_t val_t;

// Write a set of header or trailer values to out, which is a file descriptor.
// The values are specified by a series of arguments in pairs, where the first
// argument in each pair is the number of bytes, and the second argument in
// each pair is the unsigned integer value to write. The first argument in each
// pair must be an int, and the second argument in each pair must be a val_t.
// The arguments are terminated by a single zero (an int). If the number of
// bytes is positive, then the value is written in little-endian order. If the
// number of bytes is negative, then the value is written in big-endian order.
// The total number of bytes written is returned. This makes the long and
// tiresome zip format headers and trailers more readable, maintainable, and
// verifiable.
static  unsigned
put (int out, ...)
{
  // compute the total number of bytes
  unsigned count = 0;
  int n;
  va_list ap;
  va_start (ap, out);
  while ((n = va_arg (ap, int)) != 0)
    {
      va_arg (ap, val_t);
      count += (unsigned) abs (n);
    }
  va_end (ap);

  // allocate memory for the data
  unsigned char *wrap = alloc (NULL, count);
  unsigned char *next = wrap;

  // write the requested data to wrap[]
  va_start (ap, out);
  while ((n = va_arg (ap, int)) != 0)
    {
      val_t val = va_arg (ap, val_t);
      if (n < 0)
        {                       // big endian
          n = -n << 3;
          do
            {
              n -= 8;
              *next++ = (unsigned char) (val >> n);
            }
          while (n);
        }
      else                      // little endian
        do
          {
            *next++ = (unsigned char) val;
            val >>= 8;
          }
        while (--n);
    }
  va_end (ap);

  // write wrap[] to out and return the number of bytes written
  writen (out, wrap, count);
  FREE (wrap);
  return count;
}

// Low 32-bits set to all ones.
#define LOW32 0xffffffff

// Write a gzip, zlib, or zip header using the information in the globals.
static  length_t
put_header (void)
{
  length_t len;

  if (g.form > 1)
    {                           // zip
      // write local header -- we don't know yet whether the lengths will fit
      // in 32 bits or not, so we have to assume that they might not and put
      // in a Zip64 extra field so that the data descriptor that appears
      // after the compressed data is interpreted with 64-bit lengths
      len = put (g.outd, 4, (val_t) 0x04034b50, // local header signature
                 2, (val_t) 45, // version needed to extract (4.5)
                 2, (val_t) 8,  // flags: data descriptor follows data
                 2, (val_t) 8,  // deflate
                 4, (val_t) time2dos (g.mtime), 4, (val_t) 0,   // crc (not here)
                 4, (val_t) LOW32,      // compressed length (not here)
                 4, (val_t) LOW32,      // uncompressed length (not here)
                 2, (val_t) (strlen (g.name == NULL ? g.alias : g.name)),       // name len
                 2, (val_t) 29, // length of extra field (see below)
                 0);

      // write file name (use g.alias for stdin)
      len += writen (g.outd, g.name == NULL ? g.alias : g.name,
                     strlen (g.name == NULL ? g.alias : g.name));

      // write Zip64 and extended timestamp extra field blocks (29 bytes)
      len += put (g.outd, 2, (val_t) 0x0001,    // Zip64 extended information ID
                  2, (val_t) 16,        // number of data bytes in this block
                  8, (val_t) 0, // uncompressed length (not here)
                  8, (val_t) 0, // compressed length (not here)
                  2, (val_t) 0x5455,    // extended timestamp ID
                  2, (val_t) 5, // number of data bytes in this block
                  1, (val_t) 1, // flag presence of mod time
                  4, (val_t) g.mtime,   // mod time
                  0);
    }
  else if (g.form)
    {                           // zlib
      if (g.comment != NULL)
        complain ("can't store comment in zlib format -- ignoring");
      unsigned head;
      head = (0x78 << 8) +      // deflate, 32K window
        (g.level >= 9 ? 3 << 6 : g.level == 1 ? 0 << 6 : g.level >= 6 || g.level == Z_DEFAULT_COMPRESSION ? 1 << 6 : 2 << 6);   // optional compression level clue
      head += 31 - (head % 31); // make it a multiple of 31
      len = put (g.outd, -2, (val_t) head,      // zlib format uses big-endian order
                 0);
    }
  else
    {                           // gzip
      len = put (g.outd, 1, (val_t) 31, 1, (val_t) 139, 1, (val_t) 8,   // deflate
                 1, (val_t) ((g.name != NULL ? 8 : 0) + (g.comment != NULL ? 16 : 0)), 4, (val_t) g.mtime, 1, (val_t) (g.level >= 9 ? 2 : g.level == 1 ? 4 : 0), 1, (val_t) 3,  // unix
                 0);
      if (g.name != NULL)
        len += writen (g.outd, g.name, strlen (g.name) + 1);
      if (g.comment != NULL)
        len += writen (g.outd, g.comment, strlen (g.comment) + 1);
    }
  return len;
}

// Write a gzip, zlib, or zip trailer.
static  void
put_trailer (length_t ulen, length_t clen, unsigned long check, length_t head)
{
  if (g.form > 1)
    {                           // zip
      // write Zip64 data descriptor, as promised in the local header
      length_t desc = put (g.outd,
                           4, (val_t) 0x08074b50,
                           4, (val_t) check,
                           8, (val_t) clen,
                           8, (val_t) ulen,
                           0);

      // zip64 is true if either the compressed or the uncompressed length
      // does not fit in 32 bits, in which case there needs to be a Zip64
      // extra block in the central directory entry
      int zip64 = ulen >= LOW32 || clen >= LOW32;

      // write central file header
      length_t cent = put (g.outd,
                           4, (val_t) 0x02014b50,       // central header signature
                           1, (val_t) 45,       // made by 4.5 for Zip64 V1 end record
                           1, (val_t) 255,      // ignore external attributes
                           2, (val_t) 45,       // version needed to extract (4.5)
                           2, (val_t) 8,        // data descriptor is present
                           2, (val_t) 8,        // deflate
                           4, (val_t) time2dos (g.mtime),
                           4, (val_t) check,    // crc
                           4, (val_t) (zip64 ? LOW32 : clen),   // compressed length
                           4, (val_t) (zip64 ? LOW32 : ulen),   // uncompressed length
                           2, (val_t) (strlen (g.name == NULL ? g.alias : g.name)),     // name len
                           2, (val_t) (zip64 ? 29 : 9), // extra field size (see below)
                           2, (val_t) (g.comment == NULL ? 0 : strlen (g.comment)),     // comment
                           2, (val_t) 0,        // disk number 0
                           2, (val_t) 0,        // internal file attributes
                           4, (val_t) 0,        // external file attributes (ignored)
                           4, (val_t) 0,        // offset of local header
                           0);

      // write file name (use g.alias for stdin)
      cent += writen (g.outd, g.name == NULL ? g.alias : g.name,
                      strlen (g.name == NULL ? g.alias : g.name));

      // write Zip64 extra field block (20 bytes)
      if (zip64)
        cent += put (g.outd, 2, (val_t) 0x0001, // Zip64 extended information ID
                     2, (val_t) 16,     // number of data bytes in this block
                     8, (val_t) ulen,   // uncompressed length
                     8, (val_t) clen,   // compressed length
                     0);

      // write extended timestamp extra field block (9 bytes)
      cent += put (g.outd, 2, (val_t) 0x5455,   // extended timestamp signature
                   2, (val_t) 5,        // number of data bytes in this block
                   1, (val_t) 1,        // flag presence of mod time
                   4, (val_t) g.mtime,  // mod time
                   0);

      // write comment, if requested
      if (g.comment != NULL)
        cent += writen (g.outd, g.comment, strlen (g.comment));

      // here zip64 is true if the offset of the central directory does not
      // fit in 32 bits, in which case insert the Zip64 end records to
      // provide a 64-bit offset
      zip64 = head + clen + desc >= LOW32;
      if (zip64)
        {
          // write Zip64 end of central directory record and locator
          put (g.outd, 4, (val_t) 0x06064b50,   // Zip64 end of central dir sig
               8, (val_t) 44,   // size of the remainder of this record
               2, (val_t) 45,   // version made by
               2, (val_t) 45,   // version needed to extract
               4, (val_t) 0,    // number of this disk
               4, (val_t) 0,    // disk with start of central directory
               8, (val_t) 1,    // number of entries on this disk
               8, (val_t) 1,    // total number of entries
               8, (val_t) cent, // size of central directory
               8, (val_t) (head + clen + desc), // central dir offset
               4, (val_t) 0x07064b50,   // Zip64 end locator signature
               4, (val_t) 0,    // disk with Zip64 end of central dir
               8, (val_t) (head + clen + desc + cent),  // location
               4, (val_t) 1,    // total number of disks
               0);
        }

      // write end of central directory record
      put (g.outd, 4, (val_t) 0x06054b50,       // end of central directory signature
           2, (val_t) 0,        // number of this disk
           2, (val_t) 0,        // disk with start of central directory
           2, (val_t) (zip64 ? 0xffff : 1),     // entries on this disk
           2, (val_t) (zip64 ? 0xffff : 1),     // total number of entries
           4, (val_t) (zip64 ? LOW32 : cent),   // size of central directory
           4, (val_t) (zip64 ? LOW32 : head + clen + desc),     // offset
           2, (val_t) 0,        // no zip file comment
           0);
    }
  else if (g.form)              // zlib
    put (g.outd, -4, (val_t) check,     // zlib format uses big-endian order
         0);
  else                          // gzip
    put (g.outd, 4, (val_t) check, 4, (val_t) ulen, 0);
}

// Compute an Adler-32, allowing a size_t length.
static  unsigned long
adler32z (unsigned long adler, unsigned char const *buf, size_t len)
{
  while (len > UINT_MAX && buf != NULL)
    {
      adler = adler32 (adler, buf, UINT_MAX);
      buf += UINT_MAX;
      len -= UINT_MAX;
    }
  return adler32 (adler, buf, (unsigned) len);
}

// Compute a CRC-32, allowing a size_t length.
static  unsigned long
crc32z (unsigned long crc, unsigned char const *buf, size_t len)
{
  while (len > UINT_MAX && buf != NULL)
    {
      crc = crc32 (crc, buf, UINT_MAX);
      buf += UINT_MAX;
      len -= UINT_MAX;
    }
  return crc32 (crc, buf, (unsigned) len);
}

// Compute check value depending on format.
#define CHECK(a,b,c) (g.form == 1 ? adler32z(a,b,c) : crc32z(a,b,c))

// Return the zlib version as an integer, where each component is interpreted
// as a decimal number and converted to four hexadecimal digits. E.g.
// '1.2.11.1' -> 0x12b1, or return -1 if the string is not a valid version.
static  long
zlib_vernum (void)
{
  char const *ver = zlibVersion ();
  long num = 0;
  int left = 4;
  int comp = 0;
  do
    {
      if (*ver >= '0' && *ver <= '9')
        comp = 10 * comp + *ver - '0';
      else
        {
          num = (num << 4) + (comp > 0xf ? 0xf : comp);
          left--;
          if (*ver != '.')
            break;
          comp = 0;
        }
      ver++;
    }
  while (left);
  return left < 2 ? num << (left << 2) : -1;
}

// -- threaded portions of pigz --
// Notable that some of these functions have found use in other parts of pigz,
// so the #ifndef has been moved downward.
// -- check value combination routines for parallel calculation --

#define COMB(a,b,c) (g.form == 1 ? adler32_comb(a,b,c) : crc32_comb(a,b,c))
// Combine two crc-32's or two adler-32's (copied from zlib 1.2.3 so that pigz
// can be compatible with older versions of zlib).

// We copy the combination routines from zlib here, in order to avoid linkage
// issues with the zlib 1.2.3 builds on Sun, Ubuntu, and others.

// CRC-32 polynomial, reflected.
#define POLY 0xedb88320

// Return a(x) multiplied by b(x) modulo p(x), where p(x) is the CRC
// polynomial, reflected. For speed, this requires that a not be zero.
static  crc_t
multmodp (crc_t a, crc_t b)
{
  crc_t m = (crc_t) 1 << 31;
  crc_t p = 0;
  for (;;)
    {
      if (a & m)
        {
          p ^= b;
          if ((a & (m - 1)) == 0)
            break;
        }
      m >>= 1;
      b = b & 1 ? (b >> 1) ^ POLY : b >> 1;
    }
  return p;
}

// Table of x^2^n modulo p(x).
static  const crc_t x2n_table[] = {
  0x40000000, 0x20000000, 0x08000000, 0x00800000, 0x00008000,
  0xedb88320, 0xb1e6b092, 0xa06a2517, 0xed627dae, 0x88d14467,
  0xd7bbfe6a, 0xec447f11, 0x8e7ea170, 0x6427800e, 0x4d47bae0,
  0x09fe548f, 0x83852d0f, 0x30362f1a, 0x7b5a9cc3, 0x31fec169,
  0x9fec022a, 0x6c8dedc4, 0x15d6874d, 0x5fde7a4e, 0xbad90e37,
  0x2e4e5eef, 0x4eaba214, 0xa8a472c0, 0x429a969e, 0x148d302a,
  0xc40ba6d0, 0xc4e22c3c
};

// Return x^(n*2^k) modulo p(x).
static crc_t
x2nmodp (size_t n, unsigned k)
{
  crc_t p = (crc_t) 1 << 31;    // x^0 == 1
  while (n)
    {
      if (n & 1)
        p = multmodp (x2n_table[k & 31], p);
      n >>= 1;
      k++;
    }
  return p;
}

// This uses the pre-computed g.shift value most of the time. Only the last
// combination requires a new x2nmodp() calculation.
static  unsigned long
crc32_comb (unsigned long crc1, unsigned long crc2, size_t len2)
{
  return multmodp (len2 == g.block ? g.shift : x2nmodp (len2, 3),
                   crc1) ^ crc2;
}

#define BASE 65521U             // largest prime smaller than 65536
#define LOW16 0xffff            // mask lower 16 bits

static  unsigned long
adler32_comb (unsigned long adler1, unsigned long adler2, size_t len2)
{
  unsigned long sum1;
  unsigned long sum2;
  unsigned rem;

  // the derivation of this formula is left as an exercise for the reader
  rem = (unsigned) (len2 % BASE);
  sum1 = adler1 & LOW16;
  sum2 = (rem * sum1) % BASE;
  sum1 += (adler2 & LOW16) + BASE - 1;
  sum2 += ((adler1 >> 16) & LOW16) + ((adler2 >> 16) & LOW16) + BASE - rem;
  if (sum1 >= BASE)
    sum1 -= BASE;
  if (sum1 >= BASE)
    sum1 -= BASE;
  if (sum2 >= (BASE << 1))
    sum2 -= (BASE << 1);
  if (sum2 >= BASE)
    sum2 -= BASE;
  return sum1 | (sum2 << 16);
}
