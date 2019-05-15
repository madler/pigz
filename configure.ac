AC_PREREQ([2.65])
AC_INIT([pigz], m4_esyscmd_s(git describe))
AC_CONFIG_SRCDIR(src/pigz.c)
AC_CONFIG_LIBOBJ_DIR([lib])

AC_ARG_ENABLE([zopfli],
	[AS_HELP_STRING([--enable-zopfli],
		[Enable zopfli, a very slow but very effective algorithm (default yes)])],
	[AS_IF([test "x$enable_zopfli" == "xno"],
	       AC_DEFINE([NOZOPFLI]))])
dnl before you ask, I don't know why they would do the x thing. it's in the
dnl autoconf manual and I don't like to think about the many idiosyncrasies of sh.

AC_ARG_ENABLE([threads],
	[AS_HELP_STRING([--enable-threads],
		[Enable speedup by multithreading (default yes)])],
	[AS_IF([test "x$enable_threads" == "xno"],
	       AC_DEFINE([NOTHREAD]))])

AC_CONFIG_FILES([Makefile lib/Makefile])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.11 foreign subdir-objects -Wall -Werror]) 
dnl TODO: change to 'foreign'->'gnu' after adding requisite infos.
AM_SILENT_RULES([yes])
AC_PROG_CC_C99
gl_EARLY
gl_INIT
AC_PROG_RANLIB
LIBS="-lm -lpthread -lz"
AC_OUTPUT
