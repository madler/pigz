#
# Makefile for pigz
# Copyright (C) xxxx-2021 Mark Adler
#
# This Makefile is free software: you have unlimited permission
# to copy, distribute, and modify it.
#
pkgname = pigz
pkgversion = 2.6
progname = pigz
VPATH = .
prefix = /usr/local

# dev
CC = gcc
CFLAGS = -O3 -Wall -Wextra -Wno-unknown-pragmas -Wcast-qual
LFLAGS =
INCLUDES =
LIBS = -lm -lpthread -lz
ZOPFLI = ./zopfli/src/zopfli


# sources
SRCS  = pigz.c try.c yarn.c
SRCS += $(ZOPFLI)/lz77.c $(ZOPFLI)/util.c $(ZOPFLI)/symbols.c $(ZOPFLI)/cache.c \
        $(ZOPFLI)/tree.c $(ZOPFLI)/blocksplitter.c $(ZOPFLI)/deflate.c \
        $(ZOPFLI)/squeeze.c $(ZOPFLI)/hash.c $(ZOPFLI)/katajainen.c
# objects
OBJS := $(SRCS:.c=.o)

# target
$(progname) : $(OBJS)
	$(CC) $(LFLAGS) -o $@ $(OBJS) $(LIBS)

# rules
%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $<

#$(OBJS) : $(progname).d
# pigz.o: pigz.c yarn.h try.h $(ZOPFLI)/*.h
include $(progname).d

# paths
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
datarootdir = $(prefix)/share
infodir = $(datarootdir)/info
mandir = $(datarootdir)/man

# programs
DISTNAME = $(pkgname)-$(pkgversion)
INSTALL = install
INSTALL_PROGRAM = $(INSTALL) -m 755
INSTALL_DATA = $(INSTALL) -m 644
INSTALL_DIR = $(INSTALL) -d -m 755
SHELL = /bin/sh
CAN_RUN_INSTALLINFO = $(SHELL) -c "install-info --version" > /dev/null 2>&1


# targets
.PHONY : all install install-bin install-man \
         install-strip install-compress install-strip-compress \
         install-bin-strip install-man-compress \
         uninstall uninstall-bin uninstall-man \
         doc info man check dist clean distclean

all : $(progname)


depend : $(progname).d
$(progname).d : $(SRCS)
	@rm -f ./$@
	$(CC) -MM $^  >  ./$@

test: pigz
	@ln -fs pigz unpigz
	./pigz -kf pigz.c ; ./pigz -t pigz.c.gz
	./pigz -kfb 32 pigz.c ; ./pigz -t pigz.c.gz
	./pigz -kfp 1 pigz.c ; ./pigz -t pigz.c.gz
	./pigz -kfz pigz.c ; ./pigz -t pigz.c.zz
	./pigz -kfK pigz.c ; ./pigz -t pigz.c.zip
	printf "" | ./pigz -cdf | wc -c | test `cat` -eq 0
	printf "x" | ./pigz -cdf | wc -c | test `cat` -eq 1
	printf "xy" | ./pigz -cdf | wc -c | test `cat` -eq 2
	printf "xyz" | ./pigz -cdf | wc -c | test `cat` -eq 3
	(printf "w" | gzip ; printf "x") | ./pigz -cdf | wc -c | test `cat` -eq 2
	(printf "w" | gzip ; printf "xy") | ./pigz -cdf | wc -c | test `cat` -eq 3
	(printf "w" | gzip ; printf "xyz") | ./pigz -cdf | wc -c | test `cat` -eq 4
	-@if test "`which compress | grep /`" != ""; then \
	  echo 'compress -f < pigz.c | ./unpigz | cmp - pigz.c' ;\
	  compress -f < pigz.c | ./unpigz | cmp - pigz.c ;\
	fi
	@rm -f pigz.c.gz pigz.c.zz pigz.c.zip

tests: dev test
	./pigzn -kf pigz.c ; ./pigz -t pigz.c.gz
	@rm -f pigz.c.gz

doc : man

man : $(VPATH)/doc/$(progname).1

$(VPATH)/doc/$(progname).1 : $(progname)
	help2man -n 'reduces the size of files' -o $@ ./$(progname)

#Makefile : $(VPATH)/configure $(VPATH)/Makefile.in
#	./config.status

install : install-bin install-man
install-strip : install-bin-strip install-man
install-compress : install-bin install-man-compress
install-strip-compress : install-bin-strip install-man-compress

install-bin : all
	if [ ! -d "$(DESTDIR)$(bindir)" ] ; then $(INSTALL_DIR) "$(DESTDIR)$(bindir)" ; fi
	$(INSTALL_PROGRAM) ./$(progname) "$(DESTDIR)$(bindir)/$(progname)"
	@ln -snf "$(DESTDIR)$(bindir)/$(progname)" "$(DESTDIR)$(bindir)/un$(progname)"

install-bin-strip : all
	$(MAKE) INSTALL_PROGRAM='$(INSTALL_PROGRAM) -s' install-bin

install-man :
	if [ ! -d "$(DESTDIR)$(mandir)/man1" ] ; then $(INSTALL_DIR) "$(DESTDIR)$(mandir)/man1" ; fi
	-rm -f "$(DESTDIR)$(mandir)/man1/$(progname).1"*
	$(INSTALL_DATA) $(VPATH)/doc/$(progname).1 "$(DESTDIR)$(mandir)/man1/$(progname).1"

install-man-compress : install-man
	pigz -v -11 "$(DESTDIR)$(mandir)/man1/$(progname).1"

uninstall : uninstall-man uninstall-bin

uninstall-bin :
	-rm -f "$(DESTDIR)$(bindir)/$(progname)"

uninstall-man :
	-rm -f "$(DESTDIR)$(mandir)/man1/$(progname).1"*

dist : doc
	ln -sf $(VPATH) $(DISTNAME)
	tar -Hustar --owner=root --group=root -cvf $(DISTNAME).tar \
	  $(DISTNAME)/AUTHORS \
	  $(DISTNAME)/LICENSE \
	  $(DISTNAME)/ChangeLog \
	  $(DISTNAME)/INSTALL \
	  $(DISTNAME)/Makefile.in \
	  $(DISTNAME)/NEWS \
	  $(DISTNAME)/README \
	  $(DISTNAME)/configure \
	  $(DISTNAME)/doc/$(progname).1 \
	  $(DISTNAME)/*.h \
	  $(DISTNAME)/*.c \
	rm -f $(DISTNAME)
	pigz -9 $(DISTNAME).tar

clean :
	-rm -f $(progname) $(OBJS)

distclean : clean
	-rm -f Makefile config.status *.tar *.tar.gz
