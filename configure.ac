#                                               -*- Autoconf -*-
# Process this file with autoconf, et al to produce a configure script.
#
#    autoreconf -v -f
# which is just:
#    aclocal && autoconf && automake -a -f && ./configure && make

AC_PREREQ([2.13])

AC_INIT([duc], [1.3.3], [duc@zevv.nl])

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
PKG_PROG_PKG_CONFIG

AC_CHECK_LIB([m], [main])

#
# Check --disable options
#

AC_ARG_ENABLE(
        [cairo],
        [AS_HELP_STRING([--disable-cairo], [disable cairo drawing @<:@default=yes@:>@])], ,
        [enable_cairo="yes"]
)

AC_ARG_ENABLE(
        [opengl],
        [AS_HELP_STRING([--enable-opengl], [enable OpenGL drawing @<:@default=no@:>@])], ,
        [enable_opengl="no"]
)

AC_ARG_ENABLE(
        [ui],
        [AS_HELP_STRING([--disable-ui], [disable ncurses ui @<:@default=yes@:>@])], ,
        [enable_ui="yes"]
)

AC_ARG_ENABLE(
        [x11],
        [AS_HELP_STRING([--disable-x11], [disable X11 @<:@default=yes@:>@])], ,
        [enable_x11="yes"]
)

AC_ARG_WITH(
        [db-backend],
        [AS_HELP_STRING([--with-db-backend], [select database backend (tokyocabinet,leveldb,sqlite3) @<:@default=tokyocabinet@:>@])], ,
        [with_db_backend="tokyocabinet"]
)

AC_MSG_RESULT([Selected backend ${with_db_backend}])

#
# Check for available libraries
#

case "${with_db_backend}" in
	tokyocabinet)
		PKG_CHECK_MODULES([TC], [tokyocabinet])
		AC_DEFINE([ENABLE_TOKYOCABINET], [1], [Enable tokyocabinet db backend])
		;;
	leveldb)
		AC_CHECK_LIB([leveldb], [leveldb_open])
		AC_DEFINE([ENABLE_LEVELDB], [1], [Enable leveldb db backend])
		;;
	sqlite3)
		PKG_CHECK_MODULES([SQLITE3], [sqlite3])
		AC_DEFINE([ENABLE_SQLITE], [1], [Enable sqlite3 db backend])
		;;
	*)
		AC_MSG_ERROR([Unsupported db-backend])
esac

AC_DEFINE_UNQUOTED(DB_BACKEND, ["${with_db_backend}"], [Database backend])

if test "${enable_cairo}" = "yes"; then

	PKG_CHECK_MODULES([CAIRO], [cairo],, [AC_MSG_ERROR([
The cairo library was not found, which is needed for graph support. Either install
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

        AC_DEFINE([ENABLE_CAIRO], [1], [Enable cairo])
fi

if test "${enable_opengl}" = "yes"; then

	PKG_CHECK_MODULES([GLFW3], [glfw3],, [AC_MSG_ERROR([
The glfw3 library was not found, which is needed for opengl support. Either install
the glfw3 development libraries, or compile without opengl support (--disable-opengl)
	 ])])

	PKG_CHECK_MODULES([GL], [gl],, [AC_MSG_ERROR([
The opengl library was not found, which is needed for opengl support. Either install
the opengl development libraries, or compile without opengl support (--disable-opengl)
	 ])])

	if test "${enable_x11}" = "yes"; then
		AC_MSG_ERROR([
Only one graphical user interface can be configured, use --disable-x11 when 
using --enable-opengl])
	fi

	AC_CHECK_HEADERS([GLES2/gl2.h])

        AC_DEFINE([ENABLE_OPENGL], [1], [Enable opengl])
fi


if test "${enable_ui}" = "yes"; then
        AC_DEFINE([ENABLE_UI], [1], [Enable ui])

	AC_CHECK_LIB([ncursesw], [tputs],, [
		AC_CHECK_LIB([ncurses], [tputs],, [
			AC_MSG_ERROR([
The ncurses library was not found, which is needed for ui support. Either install
the ncurses development libraries, or compile without ui support (--disable-ui)
			])
		])
	])
fi


if test "${enable_x11}" = "yes"; then
        test "${enable_cairo}" != "yes" && AC_MSG_ERROR([cairo must be enabled for x11])

	AC_CHECK_LIB([X11], [XOpenDisplay],, [AC_MSG_ERROR([
The X11 library was not found, which is needed for x11 gui support.
	])])
        AC_DEFINE([ENABLE_X11], [1], [Enable X11])
fi


AC_CHECK_HEADERS([fcntl.h limits.h stdint.h stdlib.h string.h sys/ioctl.h unistd.h fnmatch.h termios.h])
AC_CHECK_HEADERS([ncurses.h ncurses/ncurses.h ncursesw/ncurses.h])

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

AM_CONDITIONAL([ENABLE_CAIRO], [test "$enable_cairo" = "true"])
AM_CONDITIONAL([ENABLE_OPENGL], [test "$enable_opengl" = "true"])
AM_CONDITIONAL([ENABLE_UI], [test "$enable_ui" = "true"])
AM_CONDITIONAL([ENABLE_X11], [test "$enable_x11" = "true"])

AC_MSG_RESULT([

  configuration summary:
  
   - Package version: $PACKAGE $VERSION
   - Prefix: ${prefix}
   - Database backend: ${with_db_backend}
   - X11 support: ${enable_x11}
   - OpenGL support: ${enable_opengl}
   - UI (ncurses) support: ${enable_ui}
   - Graph cairo support: ${enable_cairo}

])

# End

