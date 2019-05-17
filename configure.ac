AC_PREREQ([2.65])
AC_INIT([pigz], m4_esyscmd_s([git describe --dirty]))
AC_CONFIG_SRCDIR(src/pigz.c)
AC_CONFIG_LIBOBJ_DIR([lib])

AH_TEMPLATE([NOTHREAD],
	    [Disable support for multithreading.])
AH_TEMPLATE([NOZOPFLI],
	    [Disable support for Google zopfli, a very effective but very
	     slow compression algorithm.])
AH_TEMPLATE([PIGZ_DEBUG],
	    [Advanced debugging info such as memory use and very detailed logging.])
AC_ARG_ENABLE([zopfli],
	[AS_HELP_STRING([--disable-zopfli],
		[Disable zopfli, a very slow but very effective algorithm])],
	[AS_IF([test "x$enable_zopfli" == "xno"],
	       AC_DEFINE([NOZOPFLI]))])
dnl before you ask, I don't know why they would do the x thing. it's in the
dnl autoconf manual and I don't like to think about the many idiosyncrasies of sh.

AC_ARG_ENABLE([threads],
	[AS_HELP_STRING([--disable-threads],
		[Disable speedup by multithreading.])],
	[AS_IF([test "x$enable_threads" == "xno"],
	       AC_DEFINE([NOTHREAD]))])
AC_ARG_ENABLE([advanced-debug],
	[AS_HELP_STRING([--enable-advanced-debug],
		[Enable advanced verbose debugging.])],
	[AS_IF([test "xenable_advanced_debug" != "xno"],
	       AC_DEFINE([PIGZ_DEBUG]))])
AC_CONFIG_FILES([Makefile lib/Makefile])


AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([lib/config.h:lib/config.hin])
AM_INIT_AUTOMAKE([1.11 foreign subdir-objects -Wall -Werror]) 
dnl TODO: change to 'foreign'->'gnu' after adding requisite infos.
AM_SILENT_RULES([yes])
AC_PROG_CC_C99
gl_EARLY
gl_INIT
AC_PROG_RANLIB
LIBS="-lm -lpthread -lz"
AC_OUTPUT
