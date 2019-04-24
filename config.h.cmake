#ifndef LIBCOROCPP_CONFIG_H_INCLUDED_
#define LIBCOROCPP_CONFIG_H_INCLUDED_

#cmakedefine DEBUG

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef DEBUG
  #ifndef _WIN32
    #define PATH_SPER '/'
  #else
    #define PATH_SPER '\\'
  #endif
  #define logxx(f, ...)										\
	do {													\
		const char *file = strrchr(__FILE__, PATH_SPER);	\
		fprintf(stderr, "%20s:%04d, ", file ? file + 1 : __FILE__, __LINE__); \
		fprintf(stderr, f, ##__VA_ARGS__);					\
		fprintf(stderr, "\n");								\
	} while(0)
#else
  #define logxx(f, ...) ((void)f)
#endif

#ifndef GCC_VERSION
  #define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#endif

#if GCC_VERSION
  #define likely(x)     __builtin_expect(!!(x), 1)
  #define unlikely(x)   __builtin_expect(!!(x), 0)
#else
  #define likely(x)     (!!(x))
  #define unlikely(x)   (!!(x))
#endif

#cmakedefine CORO_MUTEX       1
#cmakedefine CORO_ASM			    1
#cmakedefine HAVE_SIGLONGJMP	1
#cmakedefine HAVE_SETJMP_H		1
#cmakedefine HAVE_UCONTEXT_H	1
#cmakedefine HAVE_SIGALTSTACK	1
#cmakedefine CORO_SSIZE       @CORO_SSIZE@
#endif