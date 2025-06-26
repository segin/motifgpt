AC_INIT([motifgpt], [1.0], [bug-report@example.com])
AC_CONFIG_SRCDIR([motifgpt.c])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AC_PROG_CC

# Checks for libraries.
AX_PTHREAD # This sets PTHREAD_LIBS and PTHREAD_CFLAGS

AC_PATH_XTRA
if test "x$no_x" = xyes; then
   AC_MSG_ERROR([MotifGPT requires the X Window System libraries and headers.])
fi
AC_CHECK_LIB([Xm], [XmCreateLabel], [], [AC_MSG_ERROR([Motif library (libXm) not found.])])
AC_CHECK_LIB([Xt], [XtAppInitialize], [], [AC_MSG_ERROR([X Toolkit library (libXt) not found.])])

PKG_CHECK_MODULES([LIBCURL], [libcurl],, [AC_MSG_ERROR([libcurl not found])])
PKG_CHECK_MODULES([LIBCJSON], [libcjson],, [AC_MSG_ERROR([libcjson not found])])

AC_ARG_WITH([disasterparty],
  [AS_HELP_STRING([--with-disasterparty=PREFIX], [prefix for libdisasterparty installation (e.g., /usr/local)])],
  [disasterparty_prefix="$withval"],
  [disasterparty_prefix=""])

DISASTERPARTY_CPPFLAGS=""
DISASTERPARTY_LDFLAGS=""
if test -n "$disasterparty_prefix"; then
  DISASTERPARTY_CPPFLAGS="-I$disasterparty_prefix/include"
  DISASTERPARTY_LDFLAGS="-L$disasterparty_prefix/lib"
fi

# Save original flags
SAVED_CPPFLAGS="$CPPFLAGS"
SAVED_LDFLAGS="$LDFLAGS"
# Add custom flags for check
CPPFLAGS="$CPPFLAGS $DISASTERPARTY_CPPFLAGS"
LDFLAGS="$LDFLAGS $DISASTERPARTY_LDFLAGS"

AC_CHECK_HEADER([disasterparty.h], [], [AC_MSG_ERROR([disasterparty.h not found. Use --with-disasterparty to specify the location.])])
AC_CHECK_LIB([disasterparty], [dp_init_context], [], [AC_MSG_ERROR([libdisasterparty not found. Use --with-disasterparty to specify the location.])])

# Restore original flags
CPPFLAGS="$SAVED_CPPFLAGS"
LDFLAGS="$SAVED_LDFLAGS"

# Combine all flags and libs for Makefile.am
MOTIFGPT_CPPFLAGS="$PTHREAD_CFLAGS $X_CFLAGS $LIBCURL_CFLAGS $LIBCJSON_CFLAGS $DISASTERPARTY_CPPFLAGS"
MOTIFGPT_LDFLAGS="$DISASTERPARTY_LDFLAGS"
MOTIFGPT_LIBS="$PTHREAD_LIBS $X_LIBS -lXm -lXt $LIBCURL_LIBS $LIBCJSON_LIBS -ldisasterparty"

AC_SUBST(MOTIFGPT_CPPFLAGS)
AC_SUBST(MOTIFGPT_LDFLAGS)
AC_SUBST(MOTIFGPT_LIBS)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT