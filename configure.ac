#                                               -*- Autoconf -*-
# Process this file with autoconf, et al to produce a configure script.
#
#    autoreconf -v -f
# which is just:
#    aclocal && autoconf && automake -a -f && ./configure && make

AC_PREREQ([2.13])

AC_INIT([duc], [1.0-rc1], [duc@zevv.nl])

LIB_CURRENT=1
LIB_REVISION=0
LIB_AGE=0
AC_SUBST(LIB_CURRENT)
AC_SUBST(LIB_REVISION)
AC_SUBST(LIB_AGE)

AM_INIT_AUTOMAKE([foreign subdir-objects])

AC_CONFIG_HEADER([config.h])
AC_GNU_SOURCE

AC_SYS_LARGEFILE
AC_SYS_LONG_FILE_NAMES

AC_PROG_CC
AC_PROG_LIBTOOL
PKG_PROG_PKG_CONFIG

AM_DISABLE_STATIC
AM_PROG_LIBTOOL

AC_CHECK_LIB([m], [main])

#
# Check --disable options
#

AC_ARG_ENABLE(
        [graph],
        [AS_HELP_STRING([--disable-graph], [disable graph drawing @<:@default=yes@:>@])], ,
        [enable_graph="yes"]
)

AC_ARG_ENABLE(
        [ui],
        [AS_HELP_STRING([--disable-ui], [disable ncurses ui @<:@default=yes@:>@])], ,
        [enable_ui="yes"]
)

AC_ARG_ENABLE(
        [gui],
        [AS_HELP_STRING([--disable-gui], [disable X11 gui @<:@default=yes@:>@])], ,
        [enable_gui="yes"]
)

AC_ARG_WITH(
        [db-backend],
        [AS_HELP_STRING([--with-db-backend], [select database backend (tokyocabinet,leveldb) @<:@default=tokyocabinet@:>@])], ,
        [with_db_backend="tokyocabinet"]
)

AC_MSG_RESULT([Selected backend ${with_db_backend}])

#
# Check for available libraries
#

case "${with_db_backend}" in
	tokyocabinet)
		PKG_CHECK_MODULES([TC], [tokyocabinet])
		AC_DEFINE([ENABLE_TOKYOCABINET], [1], "Enable tokyocabinet db backend")
		;;
	leveldb)
		AC_CHECK_LIB([leveldb], [leveldb_open])
		AC_DEFINE([ENABLE_LEVELDB], [1], "Enable leveldb db backend")
		;;
	*)
		AC_MSG_ERROR([Unsupported db-backend])
esac


if test "${enable_graph}" = "yes"; then

	PKG_CHECK_MODULES([CAIRO], [cairo],, [AC_MSG_ERROR(
[The cairo library was not found, which is needed for graph support. Either install
the cairo development libraries, or compile without graph support (--disable-graph)
	])])

	PKG_CHECK_MODULES([PANGO], [pango],, [AC_MSG_ERROR([
The pango library was not found, which is needed for graph support. Either install
the pango development libraries, or compile without graph support (--disable-graph)
	])])

	PKG_CHECK_MODULES([PANGOCAIRO], [pangocairo],, [AC_MSG_ERROR([
The pangocairo library was not found, which is needed for graph support. Either install
the pangocairo development libraries, or compile without graph support (--disable-graph)
	])])

        AC_DEFINE([ENABLE_GRAPH], [1], [Enable graph])
fi


if test "${enable_ui}" = "yes"; then
        AC_DEFINE([ENABLE_UI], [1], [Enable ui])

	AC_CHECK_LIB([ncurses], [tputs],, [AC_MSG_ERROR([
The ncurses library was not found, which is needed for ui support. Either install
the ncurses development libraries, or compile without ui support (--disable-ui)
	])])
fi


if test "${enable_gui}" = "yes"; then
        test "${enable_graph}" != "yes" && AC_MSG_ERROR([graph must be enabled for gui])

	AC_CHECK_LIB([X11], [XOpenDisplay],, [AC_MSG_ERROR([
The X11 library was not found, which is needed for gui support. Either install
the X11 development libraries, or compile without gui support (--disable-gui)
	])])
        AC_DEFINE([ENABLE_GUI], [1], [Enable gui])
fi


AC_CHECK_HEADERS([fcntl.h limits.h stdint.h stdlib.h string.h sys/ioctl.h unistd.h fnmatch.h glob.h])

AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_CHECK_TYPES([dev_t, ino_t])

AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_CHECK_FUNCS([floor memset strchr strdup strerror gettimeofday lstat])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

AM_CONDITIONAL([ENABLE_GRAPH], [test "$enable_graph" = "true"])
AM_CONDITIONAL([ENABLE_UI], [test "$enable_ui" = "true"])
AM_CONDITIONAL([ENABLE_GUI], [test "$enable_gui" = "true"])

AC_MSG_RESULT([

  configuration summary:
  
   - Package version: $PACKAGE $VERSION
   - Prefix: ${prefix}
   - Database backend: ${with_db_backend}
   - GUI (X11) support: ${enable_gui}
   - UI (ncurses) support: ${enable_ui}
   - Graph drawing support: ${enable_graph}

])

# End

