/* pigz.c -- parallel implementation of gzip
 * Copyright (C) 2007 Mark Adler
 * Version 1.0  17 January 2007  Mark Adler
 */

/* Version history:
   1.0  17 Jan 2007  First version
 */

/*
   pigz compresses from stdin to stdout using threads to make use of multiple
   processors and cores.  The input is broken up into 1 MB chunks, and each
   is compressed separately.  The CRC for each chunk is also calculated
   separately.  The compressed chunks are written in order to the output,
   and the overall CRC is calculated from the CRC's of the chunks.

   The compressed data format is the gzip format using the deflate compression
   method.  First a gzip header is written, followed by independent raw deflate
   partial streams.  They are partial, since they do not have a terminating
   block.  They are independent, since they are not initialized with any
   history for the LZ77 sliding window.  At the end, the deflate stream is
   terminated with a final empty static block, and lastly a gzip trailer is
   written with the CRC and the number of input bytes.

   Since each deflate partial stream is independent, the compression of that
   chunk does not have the advantage of the history from the previous chunk.
   This slightly increases the size of the compressed output as compared to
   a serial compression, but only by about 0.1% for the default chunk size of
   1 MB.  It is possible to initialize deflate() with the last 32K of the
   previous chunk in order to regain that small loss of compression.  However
   it would slow down pigz for negligible gain.

   Each raw deflate partial stream is terminated by an empty stored block
   (using the Z_SYNC_FLUSH option of zlib), in order to end that partial
   bit stream at a byte boundary.  That allows the partial streams to be
   concantenated simply as sequences of bytes.

   zlib's crc32_combine() routine allows the calcuation of CRC of the entire
   input usong the independent CRC's of the chunks.

   The POSIX pthread library is used to initiate and wait for the completion
   of the threads.  No other thread operations are needed, so it should be
   easy to adapt to other thread libraries.
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include "zlib.h"

#define local static

/* command line help */
local void help(void)
{
    fputs("usage: pigz [-1..9] [-b blocksizeinK]", stderr);
    fputs(" [-p processors] < foo > foo.gz\n", stderr);
}

/* exit with error */
local void bail(char *msg)
{
    fprintf(stderr, "pigz abort: %s\n", msg);
    exit(1);
}

/* read up to len bytes into buf, repeating read() calls as needed */
local size_t readn(int desc, void *buf, size_t len)
{
    ssize_t ret;
    size_t got;

    got = 0;
    while (len) {
        ret = read(desc, buf, len);
        if (ret < 0)
            bail("read error");
        if (ret == 0)
            break;
        buf += ret;
        len -= ret;
        got += ret;
    }
    return got;
}

/* write len bytes, repeating write() calls as needed */
local void writen(int desc, void *buf, size_t len)
{
    ssize_t ret;

    while (len) {
        ret = write(desc, buf, len);
        if (ret < 1)
            bail("write error");
        buf += ret;
        len -= ret;
    }
}

/* a unit of work to feed to chunk() -- it is assumed that the out buffer is
   large enough to hold the maximum size len bytes could deflate to, plus
   five bytes for the final sync marker */
struct work {
    size_t len;                 /* length of input */
    unsigned long crc;          /* crc of input */
    unsigned char *buf;         /* input */
    unsigned char *out;         /* space for output (assumed big enough) */
    z_stream strm;              /* pre-initialized z_stream */
    pthread_t me;               /* this thread */
};

/* largest power of 2 that fits in an unsigned int */
#define MAX ((((unsigned)-1) >> 1) + 1)

/* thread task: compress the provided input and compute its crc */
local void *chunk(void *arg)
{
    int ret;
    size_t len;
    unsigned long crc;
    struct work *job = arg;
    z_stream *strm = &(job->strm);

    /* reset state for a new compressed stream */
    ret = deflateReset(strm);
    assert(ret != Z_STREAM_ERROR);

    /* initialize input, output, and crc */
    strm->next_in = job->buf;
    strm->next_out = job->out;
    len = job->len;
    crc = crc32(0L, Z_NULL, 0);

    /* run MAX-sized amounts of input through deflate and crc32 */
    while (len > MAX) {
        strm->avail_in = MAX;
        strm->avail_out = (unsigned)-1;
        crc = crc32(crc, strm->next_in, strm->avail_in);
        (void)deflate(strm, Z_NO_FLUSH);
        len -= MAX;
    }

    /* run last piece through deflate and crc32, follow with a sync marker */
    strm->avail_in = len;
    strm->avail_out = (unsigned)-1;
    crc = crc32(crc, strm->next_in, strm->avail_in);
    (void)deflate(strm, Z_SYNC_FLUSH);

    /* do not need to do a Z_FINISH, since we'd delete last two bytes anyway */

    /* return result */
    job->crc = crc;
    return NULL;
}

/* compress stdin to stdout in the gzip format, using multiple threads for the
   compression and crc calculation */
int main(int argc, char **argv)
{
    int ind;                    /* input file descriptor */
    int outd;                   /* output file descriptor */
    int level;                  /* compression level */
    int cores;                  /* number of compression threads */
    int n;                      /* general index */
    int last;                   /* last option, last activated thread */
    size_t size;                /* uncompressed size (per thread) */
    unsigned long len;          /* total uncompressed size (overflow ok) */
    unsigned long crc;          /* CRC-32 of uncompressed data */
    char *arg;                  /* command line argument */
    struct work *jobs;          /* list of work units for the cores threads */
    pthread_attr_t attr;        /* thread attributes (left at defaults) */
    unsigned char trail[10];    /* gzip trailer */

    /* set defaults */
    ind = 0;
    outd = 1;
    level = Z_DEFAULT_COMPRESSION;
    cores = 4;
    size = 1048576UL;
    pthread_attr_init(&attr);

    /* process command-line arguments */
    last = 0;
    for (n = 1; n < argc; n++) {
        arg = argv[n];
        if (*arg == '-')
            while (*++arg)
                switch (*arg) {
                case '1':  level = 1;  break;
                case '2':  level = 2;  break;
                case '3':  level = 3;  break;
                case '4':  level = 4;  break;
                case '5':  level = 5;  break;
                case '6':  level = 6;  break;
                case '7':  level = 7;  break;
                case '8':  level = 8;  break;
                case '9':  level = 9;  break;
                case 'b':  last |= 1;  break;       /* block size */
                case 'p':  last |= 2;  break;       /* number of processors */
                case 'h':  help();  return 0;
                default:
                    bail("invalid option");
                }
        else if (last & 1) {
            if (last & 2)
                bail("you need to separate the -b and -p options");
            size = (size_t)(atoi(arg)) << 10;
            if (size < 1024)
                bail("invalid option");
            last = 0;
        }
        else if (last & 2) {
            cores = atoi(arg);
            if (cores < 1)
                bail("invalid option");
            last = 0;
        }
        else
            bail("invalid option (you need to pipe input and output)");
    }
    if (last)
        bail("missing option argument");

    /* allocate and initialize work list */
    if (((size_t)0 - 1) / cores <= sizeof(struct work) ||
        (jobs = malloc(cores * sizeof(struct work))) == NULL)
        bail("not enough memory");
    for (n = 0; n < cores; n++) {
        jobs[n].buf = malloc(size);
        jobs[n].out = malloc(size + (size >> 11) + 10);
        jobs[n].strm.zfree = Z_NULL;
        jobs[n].strm.zalloc = Z_NULL;
        jobs[n].strm.opaque = Z_NULL;
        last = deflateInit2(&(jobs[n].strm), level, Z_DEFLATED, -15, 8,
                            Z_DEFAULT_STRATEGY);
        if (jobs[n].buf == NULL || jobs[n].out == NULL || last != Z_OK)
            bail("not enough memory");
    }

    /* write simple gzip header */
    writen(outd, level == 9 ? "\037\213\10\0\0\0\0\0\2\3" :
                (level == 1 ? "\037\213\10\0\0\0\0\0\4\3" :
                              "\037\213\10\0\0\0\0\0\0\3"), 10);

    /* deflate the input to output, using cores independent threads */
    len = 0;
    crc = crc32(0L, Z_NULL, 0);
    do {
        /* load buffers and start up threads */
        for (n = 0; n < cores; n++) {
            jobs[n].len = readn(ind, jobs[n].buf, size);
            if (jobs[n].len == 0)
                break;
            pthread_create(&(jobs[n].me), &attr, chunk, jobs + n);
        }
        last = n;

        /* wait for those threads to complete, write output, accumulate crc */
        for (n = 0; n < last; n++) {
            pthread_join(jobs[n].me, NULL);
            writen(outd, jobs[n].out, jobs[n].strm.next_out - jobs[n].out);
            len += jobs[n].len;
            crc = crc32_combine(crc, jobs[n].crc, jobs[n].len);
        }
    } while (last == cores);

    /* write final static block and gzip trailer (crc and len mod 2^32) */
    trail[0] = 3;
    trail[1] = 0;
    trail[2] = (unsigned char)crc;
    trail[3] = (unsigned char)(crc >> 8);
    trail[4] = (unsigned char)(crc >> 16);
    trail[5] = (unsigned char)(crc >> 24);
    trail[6] = (unsigned char)len;
    trail[7] = (unsigned char)(len >> 8);
    trail[8] = (unsigned char)(len >> 16);
    trail[9] = (unsigned char)(len >> 24);
    writen(outd, trail, 10);

    /* clean up */
    for (n = cores - 1; n >= 0; n--) {
        (void)deflateEnd(&(jobs[n].strm));
        free(jobs[n].out);
        free(jobs[n].buf);
    }
    free(jobs);

    /* done */
    return 0;
}
