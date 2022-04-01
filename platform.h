/*
 * Depends on libmicrohttpd-0.9.43
 */


#ifndef MHD_PLATFORM_H
#define MHD_PLATFORM_H

#define MHD_W32LIB	1	// static

#ifdef _MSC_VER
#define FMT_TIME	"%lld"
#include "strptime.h"
#else
#define FMT_TIME	"%ld"
#endif


#ifndef _MHD_EXTERN 
#if defined(_WIN32) && defined(MHD_W32LIB)
#define _MHD_EXTERN 
#elif defined (_WIN32) && defined(MHD_W32DLL)
#define _MHD_EXTERN extern __declspec(dllexport) 
#else
#define _MHD_EXTERN 
#endif
#endif 

#define _XOPEN_SOURCE_EXTENDED  1
#if OS390
#define _OPEN_THREADS
#define _OPEN_SYS_SOCK_IPV6
#define _OPEN_MSGQ_EXT
#define _LP64
#endif

#if defined(_WIN32)
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#else /* _WIN32_WINNT */
#if _WIN32_WINNT < 0x0501
#error "Headers for Windows XP or later are required"
#endif /* _WIN32_WINNT < 0x0501 */
#endif /* _WIN32_WINNT */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif /* !WIN32_LEAN_AND_MEAN */
#endif /* _WIN32 */

#if LINUX+0 && (defined(HAVE_SENDFILE64) || defined(HAVE_LSEEK64)) && ! defined(_LARGEFILE64_SOURCE)
#define _LARGEFILE64_SOURCE 1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#ifdef MHD_USE_POSIX_THREADS
#undef HAVE_CONFIG_H
#include <pthread.h>
#define HAVE_CONFIG_H 1
#endif /* MHD_USE_POSIX_THREADS */

/* different OSes have fd_set in
   a broad range of header files;
   we just include most of them (if they
   are available) */


#ifdef OS_VXWORKS
#include <sockLib.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <sys/mman.h>
#define RESTRICT __restrict__
#endif
#if HAVE_MEMORY_H
#include <memory.h>
#endif

#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#if HAVE_SYS_MSG_H
#include <sys/msg.h>
#endif
#if HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#if HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#if HAVE_TIME_H
#include <time.h>
#endif
#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif


#if defined(_WIN32) && !defined(__CYGWIN__)
#include <ws2tcpip.h>
#define sleep(seconds) ((SleepEx((seconds)*1000, 1)==0)?0:(seconds))
#define usleep(useconds) ((SleepEx((useconds)/1000, 1)==0)?0:-1)
#endif

#if !defined(SHUT_WR) && defined(SD_SEND)
#define SHUT_WR SD_SEND
#endif
#if !defined(SHUT_RD) && defined(SD_RECEIVE)
#define SHUT_RD SD_RECEIVE
#endif
#if !defined(SHUT_RDWR) && defined(SD_BOTH)
#define SHUT_RDWR SD_BOTH
#endif

#if defined(_MSC_FULL_VER) && !defined (_SSIZE_T_DEFINED)
#define _SSIZE_T_DEFINED
typedef intptr_t ssize_t;
#endif /* !_SSIZE_T_DEFINED */
#ifndef MHD_SOCKET_DEFINED
/**
 * MHD_socket is type for socket FDs
 */
#if !defined(_WIN32) || defined(_SYS_TYPES_FD_SET)
#define MHD_POSIX_SOCKETS 1
typedef int MHD_socket;
#define MHD_INVALID_SOCKET (-1)
#else /* !defined(_WIN32) || defined(_SYS_TYPES_FD_SET) */
#define MHD_WINSOCK_SOCKETS 1
#include <winsock2.h>
typedef SOCKET MHD_socket;
#define MHD_INVALID_SOCKET (INVALID_SOCKET)
#endif /* !defined(_WIN32) || defined(_SYS_TYPES_FD_SET) */
#define MHD_SOCKET_DEFINED 1
#endif /* MHD_SOCKET_DEFINED */

#ifndef _WIN32
typedef time_t _MHD_TIMEVAL_TV_SEC_TYPE;
#else  /* _WIN32 */
typedef long _MHD_TIMEVAL_TV_SEC_TYPE;
#endif /* _WIN32 */

#ifdef _MSC_VER
int setenv(const char *name, const char *value, int overwrite);
#endif

/* Force don't use pipes on W32 */
#if defined(_WIN32) && !defined(MHD_DONT_USE_PIPES)
#define MHD_DONT_USE_PIPES 1
#endif /* defined(_WIN32) && !defined(MHD_DONT_USE_PIPES) */

/* MHD_pipe is type for pipe FDs*/
#ifndef MHD_DONT_USE_PIPES
typedef int MHD_pipe;
#else /* ! MHD_DONT_USE_PIPES */
typedef MHD_socket MHD_pipe;
#endif /* ! MHD_DONT_USE_PIPES */

#if !defined(IPPROTO_IPV6) && defined(_MSC_FULL_VER) && _WIN32_WINNT >= 0x0501
/* VC use IPPROTO_IPV6 as part of enum */
#define IPPROTO_IPV6 IPPROTO_IPV6
#endif

#endif

#ifdef _MSC_VER
#include <Windows.h>
#include <stdint.h> // portable: uint64_t   MSVC: __int64 
int gettimeofday(struct timeval * tp, struct timezone * tzp);
#endif