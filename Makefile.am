SUBDIRS = .
AM_CPPFLAGS = -O2
ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS=ws-sqlite

nobase_dist_include_HEADERS = \
	platform.h \
	lorawan-ws.h \
	third_party/argtable3/argtable3.h \
	third_party/lorawan-network-server/db-intf.h \
	third_party/lorawan-network-server/db-sqlite.h
	
ws_sqlite_SOURCES = \
	ws-sqlite.cpp lorawan-ws.cpp \
	third_party/argtable3/argtable3.c
ws_sqlite_LDADD = \
    -lsqlite3 -lmicrohttpd
ws_sqlite_CPPFLAGS = -I. -Ithird_party

configdir = $(datadir)
dist_config_DATA = \
	CMakeLists.txt CODE_OF_CONDUCT.md CONTRIBUTING.md COPYING HISTORY LICENSE README.md TODO \
	third_party/argtable3/README
