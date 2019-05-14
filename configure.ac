dnl AC_PREREQ([2.69])
AC_INIT([gzippier],
	m4_esyscmd([build-aux/git-version-gen .tarball-version]))
AC_CONFIG_SRCDIR(src/pigz.c)
m4_syscmd([git submodule update --init])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([lib/config.h:lib/config.hin)]

AC_PROG_CC_C99
C_PROG_RANLIB
AC_OUTPUT
