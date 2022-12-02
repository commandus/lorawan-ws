SUBDIRS = .
AM_CPPFLAGS = -O2
ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS=ws-sqlite
lib_LIBRARIES = libws-sqlite.a

AUTH_CPP =
AUTH_DEF =
AUTH_LIB =
if ENABLE_JWT
AUTH_CPP += auth-jwt.cpp
AUTH_DEF += -DENABLE_JWT
AUTH_LIB += -lcrypto -lssl
endif

nobase_dist_include_HEADERS = \
	platform.h lorawan-ws.h db-helper.h auth-jwt.h \
	third_party/argtable3/argtable3.h \
	third_party/daemonize/daemonize.h \
	third_party/db-intf.h log-intf.h \
	third_party/lorawan-network-server/db-sqlite.h \
	third_party/jwt-cpp/jwt.h third_party/jwt-cpp/base.h third_party/picojson/picojson.h

LIB_WS_SQLITE_SRC = \
	lorawan-ws.cpp \
	$(AUTH_CPP)

ws_sqlite_SOURCES = \
	ws-sqlite.cpp lorawan-ws.cpp db-helper.cpp \
	third_party/argtable3/argtable3.c \
	third_party/lorawan-network-server/db-sqlite.cpp \
	third_party/daemonize/daemonize.cpp \
	$(AUTH_CPP)
ws_sqlite_LDADD = -lsqlite3 -lmicrohttpd $(AUTH_LIB)
ws_sqlite_CPPFLAGS = -I. -Ithird_party -I../../third_party $(AUTH_DEF)

libws_sqlite_a_SOURCES = $(LIB_WS_SQLITE_SRC)
libws_sqlite_a_CXXFLAGS = -I. -Ithird_party -I../../third_party -fPIC $(AUTH_DEF)

configdir = $(datadir)
dist_config_DATA = \
	CMakeLists.txt CODE_OF_CONDUCT.md CONTRIBUTING.md COPYING HISTORY LICENSE README.md TODO \
	third_party/argtable3/README
