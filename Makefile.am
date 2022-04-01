SUBDIRS = .
AM_CPPFLAGS = -O2
ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS=ws-sqlite

nobase_dist_include_HEADERS = \
	platform.h \
	lorawan-ws.h \
	argtable3/argtable3.h \
	lorawan-network-server/db-intf.h \
	lorawan-network-server/db-sqlite.h
	
ws_sqlite_SOURCES = \
	ws-sqlite.cpp lorawan-ws.cpp
ws_sqlite_LDADD = \
    -lsqlite3 -lmicrohttpd -lpthread 
ws_sqlite_CPPFLAGS = -I. -Ithird_party

configdir = $(datadir)
dist_config_DATA = \
	README.md \
	third_party/argtable3/README \
	third_party/lorawan-network-server/README
