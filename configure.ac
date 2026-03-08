AC_PREREQ([2.69])
AC_INIT([motifgpt], [1.0], [bug-report@example.com])
AC_CONFIG_SRCDIR([motifgpt.c])
AC_CONFIG_MACRO_DIRS([m4])
AM_INIT_AUTOMAKE([foreign -Wall -Werror subdir-objects])
AM_SILENT_RULES([yes])
AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
AC_PROG_INSTALL
AC_LANG([C])

# Checks for libraries.
AX_PTHREAD

AC_PATH_XTRA
if test "x$no_x" = xyes; then
   AC_MSG_ERROR([MotifGPT requires the X Window System libraries and headers.])
fi
AC_CHECK_LIB([Xm], [XmCreateLabel], [], [AC_MSG_ERROR([Motif library (libXm) not found.])])
AC_CHECK_LIB([Xt], [XtAppInitialize], [], [AC_MSG_ERROR([X Toolkit library (libXt) not found.])])

PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([LIBCURL], [libcurl],, [AC_MSG_ERROR([libcurl not found.])])
dnl Try libcjson first, then fallback to cjson if needed
PKG_CHECK_MODULES([LIBCJSON], [libcjson],
  ,
  [PKG_CHECK_MODULES([LIBCJSON], [cjson],
     ,
     [AC_MSG_ERROR([cJSON not found. Try installing libcjson-dev or cjson-devel.])])])
PKG_CHECK_MODULES([DISASTERPARTY], [libdisasterparty],, [AC_MSG_ERROR([libdisasterparty not found. Make sure disasterparty.pc is in your PKG_CONFIG_PATH.])])

# Combine all flags and libs for Makefile.am
MOTIFGPT_CPPFLAGS="$PTHREAD_CFLAGS $X_CFLAGS $LIBCURL_CFLAGS $LIBCJSON_CFLAGS $DISASTERPARTY_CFLAGS"
dnl Include X linkage bits discovered by AC_PATH_XTRA
MOTIFGPT_LIBS="$PTHREAD_LIBS $X_LIBS $X_PRE_LIBS -lXm -lXt -lX11 $X_EXTRA_LIBS $LIBCURL_LIBS $LIBCJSON_LIBS $DISASTERPARTY_LIBS"

AC_SUBST(MOTIFGPT_CPPFLAGS)
AC_SUBST(MOTIFGPT_LIBS)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
