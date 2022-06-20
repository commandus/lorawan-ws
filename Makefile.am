SUBDIRS = .
AM_CPPFLAGS = -O2
ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS=ws-sqlite
lib_LIBRARIES = libws-sqlite.a

nobase_dist_include_HEADERS = \
	platform.h lorawan-ws.h db-helper.h \
	third_party/argtable3/argtable3.h \
	third_party/daemonize/daemonize.h \
	third_party/db-intf.h \
	third_party/lorawan-network-server/db-sqlite.h
	
LIB_WS_SQLITE_SRC = \
	lorawan-ws.cpp

ws_sqlite_SOURCES = \
	ws-sqlite.cpp lorawan-ws.cpp db-helper.cpp \
	third_party/argtable3/argtable3.c \
	third_party/lorawan-network-server/db-sqlite.cpp \
	third_party/daemonize/daemonize.cpp
ws_sqlite_LDADD = -lsqlite3 -lmicrohttpd
ws_sqlite_CPPFLAGS = -I. -Ithird_party -I../../third_party

libws_sqlite_a_SOURCES = $(LIB_WS_SQLITE_SRC)
libws_sqlite_a_CXXFLAGS = -I. -Ithird_party -I../../third_party -fPIC

configdir = $(datadir)
dist_config_DATA = \
	CMakeLists.txt CODE_OF_CONDUCT.md CONTRIBUTING.md COPYING HISTORY LICENSE README.md TODO \
	third_party/argtable3/README
