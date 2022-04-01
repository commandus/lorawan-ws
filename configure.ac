AC_PREREQ([2.63])
AC_INIT([collarws], [0.1], [andrey.ivanov@ikfia.sbras.ru])
AC_CONFIG_FILES([Makefile])
AM_INIT_AUTOMAKE([1.11 foreign no-define subdir-objects])
AC_CONFIG_SRCDIR([collarws.cpp])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

LT_INIT
AC_ENABLE_SHARED
AC_ENABLE_STATIC

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LIBTOOL

# Checks for libraries.
LIBCURL_CHECK_CONFIG

# Checks for header files.
AC_CHECK_HEADERS([stdint.h string.h sys/time.h termios.h unistd.h])
AC_CHECK_HEADERS([gd.h], [], AC_MSG_ERROR([Could not find gd.h. Try ./configure CFLAGS='-Iyour-gd-include-path']))
AC_CHECK_HEADERS([argtable2.h], [], AC_MSG_ERROR([Could not find argtable2.h. Try ./configure CFLAGS='-Iyour-argtable2-include-path']))
#AC_CHECK_HEADERS([sqlite/sqlite3.h], [], AC_MSG_ERROR([Could not find sqlite/sqlite3.h. Try ./configure CFLAGS='-I.']))
#AC_CHECK_HEADERS([iridiummessage.h], [], AC_MSG_ERROR([Could not find iridiummessage.h. Try ./configure CFLAGS='-I../collarie']))

AC_CHECK_LIB(argtable2, arg_parse, [], AC_MSG_ERROR([Could not find libargtable2. Try ./configure LDFLAGS='-Lyour-argtable2-lib-path']))
AC_CHECK_LIB(dl, dlclose, [], AC_MSG_ERROR([Could not find libdl. Try $ ./configure LDFLAGS='-Lyour-lib-path']))
AC_CHECK_LIB(gd, gdImageCreate, [], AC_MSG_ERROR([Could not find libgd. Try $ ./configure LDFLAGS='-Lyour-lib-path']))
AC_CHECK_LIB(microhttpd, MHD_lookup_connection_value, [], AC_MSG_ERROR([Could not find libmicrohttpd. Try ./configure LDFLAGS='-Lyour-lib-path']))
AC_CHECK_LIB(pthread, pthread_mutexattr_settype, [], AC_MSG_ERROR([Could not find libpthread. Try ./configure LDFLAGS='-Lyour-lib-path']))
AC_CHECK_LIB(iridium, time52time_t, [], AC_MSG_ERROR([Could not find libiridium. Try ./configure LDFLAGS='-L../collarie']))

# Checks for typedefs, structures, and compiler characteristics.
# AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_FUNC_FORK
AC_CHECK_FUNCS([memmove memset])

CXXFLAGS='-Wall -std=c++11'

AC_OUTPUT

