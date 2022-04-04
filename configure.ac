AC_PREREQ([2.63])
AC_INIT([lorawan-ws], [0.1], [andrey.ivanov@ikfia.ysn.ru])
AC_CONFIG_FILES([Makefile])
AM_INIT_AUTOMAKE([1.11 foreign no-define subdir-objects])
AC_CONFIG_SRCDIR([lorawan-ws.cpp])
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
AC_CHECK_LIB(microhttpd, MHD_lookup_connection_value, [], AC_MSG_ERROR([Could not find libmicrohttpd. Try ./configure LDFLAGS='-Lyour-lib-path']))
AC_CHECK_LIB([sqlite3], [sqlite3_exec], [], AC_MSG_ERROR([Could not find sqlite3 library. Try ./configure LDFLAGS='-Lyour-lib-path]))

# Checks for header files.
AC_CHECK_HEADER([sqlite3.h])
AC_CHECK_HEADERS([stdint.h string.h])

# Checks for library functions.
AC_CHECK_FUNCS([memmove memset])

CXXFLAGS='-Wall -std=c++11'

AC_OUTPUT

