AC_INIT([motifgpt], [1.0], [bug-report@example.com])
AC_CONFIG_SRCDIR([motifgpt.c])
AM_INIT_AUTOMAKE([foreign -Wall -Werror subdir-objects])
AC_PROG_CC

# Checks for libraries.
AC_CHECK_LIB([pthread], [pthread_create], [PTHREAD_LIBS="-lpthread"], [AC_MSG_ERROR([pthread library not found])])
AC_CHECK_HEADER([pthread.h], [], [AC_MSG_ERROR([pthread header not found])])
AC_SUBST(PTHREAD_LIBS)
PTHREAD_CFLAGS=""
AC_SUBST(PTHREAD_CFLAGS)

AC_PATH_XTRA
if test "x$no_x" = xyes; then
   AC_MSG_WARN([X Window System libraries and headers not found. MotifGPT will not be built.])
   have_x=no
else
   have_x=yes
fi

have_motif=no
if test "x$have_x" = xyes; then
    AC_CHECK_LIB([Xm], [XmCreateLabel], [have_motif=yes], [AC_MSG_WARN([Motif library (libXm) not found. MotifGPT will not be built.])])
    if test "x$have_motif" = xyes; then
        AC_CHECK_LIB([Xt], [XtAppInitialize], [], [AC_MSG_WARN([X Toolkit library (libXt) not found.])])
    fi
fi

PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([LIBCURL], [libcurl], [have_libcurl=yes], [have_libcurl=no; AC_MSG_WARN([libcurl not found.])])
PKG_CHECK_MODULES([LIBCJSON], [libcjson], [have_libcjson=yes], [have_libcjson=no; AC_MSG_WARN([libcjson not found.])])
PKG_CHECK_MODULES([DISASTERPARTY], [libdisasterparty], [have_disasterparty=yes], [have_disasterparty=no; AC_MSG_WARN([libdisasterparty not found.])])

AM_CONDITIONAL([BUILD_MOTIFGPT], [test "x$have_motif" = "xyes" && test "x$have_libcurl" = "xyes" && test "x$have_libcjson" = "xyes" && test "x$have_disasterparty" = "xyes"])

# Combine all flags and libs for Makefile.am
MOTIFGPT_CPPFLAGS="$PTHREAD_CFLAGS $X_CFLAGS $LIBCURL_CFLAGS $LIBCJSON_CFLAGS $DISASTERPARTY_CFLAGS"
MOTIFGPT_LIBS="$PTHREAD_LIBS $X_LIBS -lXm -lXt $LIBCURL_LIBS $LIBCJSON_LIBS $DISASTERPARTY_LIBS"

AC_SUBST(MOTIFGPT_CPPFLAGS)
AC_SUBST(MOTIFGPT_LIBS)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
