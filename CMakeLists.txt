cmake_minimum_required(VERSION 3.9)
set(PROJECT_NAME "lorawan-ws")
project(${PROJECT_NAME} VERSION 1.0)

set(CMAKE_CXX_STANDARD 11)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

option(ENABLE_JWT "Enable JWT" OFF)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	set(LIB_OS_SPECIFIC_SOURCE usleep.cpp)
	find_package(unofficial-sqlite3 CONFIG REQUIRED)
	set(DB_LIB unofficial::sqlite3::sqlite3)
else ()
	find_package(SQLite3 REQUIRED)
	set(DB_LIB ${SQLite3_LIBRARIES})
endif ()


set(HTTPD_LIB microhttpd)

include_directories("third_party" ${CURL_INCLUDE_DIR})

set(WS_SQLITE_SOURCES
	ws-sqlite.cpp lorawan-ws.cpp
	third_party/argtable3/argtable3.c
	third_party/daemonize/daemonize.cpp
	db-helper.cpp
	third_party/lorawan-network-server/db-sqlite.cpp
)

set(LIB_LORAWAN_WS_SRC
	lorawan-ws.cpp
)

if(ENABLE_JWT)
	set(WS_SQLITE_SOURCES ${WS_SQLITE_SOURCES} auth-jwt.cpp)
	set(LIB_LORAWAN_WS_SRC ${LIB_LORAWAN_WS_SRC} auth-jwt.cpp)
# OpenSSL
	set(OPENSSL_USE_STATIC_LIBS TRUE)
	find_package(OpenSSL REQUIRED)
	set(EXTRA_LIBS OpenSSL::Crypto OpenSSL::SSL)
	set(EXTRA_DEF ENABLE_JWT)
endif()

add_executable(ws_sqlite ${WS_SQLITE_SOURCES})
target_link_libraries(ws_sqlite PRIVATE Threads::Threads
	${DB_LIB} ${HTTPD_LIB} ${EXTRA_LIBS}
)
target_compile_definitions(ws_sqlite PUBLIC ${EXTRA_DEF})

add_library(ws-sqlite STATIC
	${LIB_LORAWAN_WS_SRC}
)
target_compile_definitions(ws-sqlite PUBLIC ${EXTRA_DEF})
target_link_libraries(ws-sqlite PRIVATE Threads::Threads
	${EXTRA_LIBS}
)
INSTALL(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/ws-sqlite DESTINATION cmake)
INSTALL(FILES README.md LICENSE HISTORY COPYING TODO CODE_OF_CONDUCT.md CONTRIBUTING.md DESTINATION doc)

include(InstallRequiredSystemLibraries)
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_VERSION_MAJOR "${semtech_udp_packet_emitter_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${semtech_udp_packet_emitter_VERSION_MINOR}")
include(CPack)
