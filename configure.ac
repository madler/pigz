dnl AC_PREREQ([2.69])
AC_INIT([pigz], m4_esyscmd_s(git describe))
AC_CONFIG_SRCDIR(src/pigz.c)
dnl AC_DEFINE([NOZOPFLI]) dnl for now we can't build it with zopfli
dnl m4_syscmd([git submodule update --init])
AC_CONFIG_LIBOBJ_DIR([lib])

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
