#                                               -*- Autoconf -*-
# Process this file with autoconf, et al to produce a configure script.
#
#    autoreconf -v -f
# which is just:
#    aclocal && autoconf && automake -a -f && ./configure && make

AC_PREREQ([2.13])

AC_INIT([duc], [0.1], [duc@zevv.nl])

dnl Library versioning
dnl We begin with 1:0:0
LIB_CURRENT=1
LIB_REVISION=0
LIB_AGE=0
AC_SUBST(LIB_CURRENT)
AC_SUBST(LIB_REVISION)
AC_SUBST(LIB_AGE)

# Turn off GNU standards like README, etc, for now.
# Do not do dependency tracking either.
AM_INIT_AUTOMAKE([foreign no-dependencies])

AC_CONFIG_HEADER([config.h])
AC_GNU_SOURCE

# We need LARGEFILE support turned on by default
AC_SYS_LARGEFILE

# Support long filenames too.
AC_SYS_LONG_FILE_NAMES

# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL

# Initialize libtool, but disable static libs for now
AM_DISABLE_STATIC
AM_PROG_LIBTOOL


# Checks for libraries.
AC_CHECK_LIB([m], [main])
AC_CHECK_LIB([X11], [XOpenDisplay])

# Check for pkg-config, needs fallback for when it doesn't exist...
PKG_PROG_PKG_CONFIG

# Check for tokyocabinet
PKG_CHECK_MODULES([TC],[tokyocabinet],[
   LIBS="$TC_LIBS $LIBS"
   CFLAGS="$TC_CFLAGS $CFLAGS"], [
   AC_CHECK_HEADER(tcadb.h, HAVE_TC_H=1)
   AC_CHECK_LIB(tokyocabinet, tcfree , HAVE_TC=1)
   if test -n "$HAVE_TC_H" -a -n "$HAVE_TC";  then
      LIBS="-ltokyocabinet $LIBS"
   else
      echo -e "Error:  both library and header files for the tokyocabinet library\n"\
      "are required to build this package.  See README file for"\
      "further information."
      exit 1;
   fi
])

# Check for Cairo support
PKG_CHECK_MODULES([CAIRO],[cairo])

# Check for Pango support
PKG_CHECK_MODULES([PANGO],[pango])

# Check for Pangocairo support
PKG_CHECK_MODULES([PANGOCAIRO],[pangocairo])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h stdint.h stdlib.h string.h sys/ioctl.h unistd.h fnmatch.h glob.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_CHECK_TYPES([dev_t, ino_t])

# Checks for library functions.
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_CHECK_FUNCS([floor memset realpath strchr strdup strerror gettimeofday lstat])

AC_CONFIG_FILES([Makefile lib/Makefile duc/Makefile])
AC_OUTPUT

echo ""
echo "Done"
