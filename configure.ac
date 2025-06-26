AC_INIT([motifgpt], [1.0], [bug-report@example.com])
AC_CONFIG_SRCDIR([motifgpt.c])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AC_PROG_CC

# Checks for libraries.
m4_include([m4/ax_pthread.m4])
AX_PTHREAD

AC_PATH_XTRA
if test "x$no_x" = xyes; then
   AC_MSG_ERROR([MotifGPT requires the X Window System libraries and headers.])
fi
AC_CHECK_LIB([Xm], [XmCreateLabel], [], [AC_MSG_ERROR([Motif library (libXm) not found.])])
AC_CHECK_LIB([Xt], [XtAppInitialize], [], [AC_MSG_ERROR([X Toolkit library (libXt) not found.])])

PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([LIBCURL], [libcurl],, [AC_MSG_ERROR([libcurl not found.])])
PKG_CHECK_MODULES([LIBCJSON], [libcjson],, [AC_MSG_ERROR([libcjson not found.])])
PKG_CHECK_MODULES([DISASTERPARTY], [libdisasterparty],, [AC_MSG_ERROR([libdisasterparty not found. Make sure disasterparty.pc is in your PKG_CONFIG_PATH.])])

# Combine all flags and libs for Makefile.am
MOTIFGPT_CPPFLAGS="$PTHREAD_CFLAGS $X_CFLAGS $LIBCURL_CFLAGS $LIBCJSON_CFLAGS $DISASTERPARTY_CFLAGS"
MOTIFGPT_LIBS="$PTHREAD_LIBS $X_LIBS -lXm -lXt $LIBCURL_LIBS $LIBCJSON_LIBS $DISASTERPARTY_LIBS"

AC_SUBST(MOTIFGPT_CPPFLAGS)
AC_SUBST(MOTIFGPT_LIBS)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT