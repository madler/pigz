SUBDIRS = lib

bin_PROGRAMS = pigz
AM_LDFLAGS = -lm -lpthread -lz
pigz_LDADD = $(LIBOBJS)
pigz_SOURCES = \
	       src/pigz.c \
	       lib/try.c \
	       lib/try.h \
	       lib/yarn.c \
	       lib/yarn.h \
	       lib/zopfli/blocksplitter.c \
	       lib/zopfli/blockspplitter.h \
	       lib/zopfli/cache.c \
	       lib/zopfli/cache.h \
	       lib/zopfli/deflate.c \
	       lib/zopfli/deflate.h \
	       lib/zopfli/hash.c \
	       lib/zopfli/hash.h \
	       lib/zopfli/katajainen.c \
	       lib/zopfli/katajainen.h \
	       lib/zopfli/lz77.c \
	       lib/zopfli/lz77.h \
	       lib/zopfli/squeeze.c \
	       lib/zopfli/squeeze.h \
	       lib/zopfli/tree.c \
	       lib/zopfli/tree.h \
	       lib/zopfli/util.c \
	       lib/zopfli/util.h \
	       lib/zopfli/zopfli.h

EXTRA_DIST = \
	     lib/zopfli/README \
	     lib/zopfli/COPYING \
	     lib/zopfli/CONTRIBUTORS \
	     AUTHORS \
	     ChangeLog \
	     COPYING \
	     NEWS \
	     pigz.pdf \
	     pigz.spec \
	     README \
	     README-hacking

dist_doc_DATA = README
