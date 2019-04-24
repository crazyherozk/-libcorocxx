#include "coro_stack.hpp"

#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
  #include <unistd.h>
#endif

#if CORO_USE_VALGRIND
  #include <valgrind/valgrind.h>
#endif

#if _POSIX_MAPPED_FILES
  #include <sys/mman.h>
  #define CORO_MMAP           1
  #ifndef MAP_ANONYMOUS
    #ifdef MAP_ANON
	  #define MAP_ANONYMOUS   MAP_ANON
    #else
	  #undef CORO_MMAP
    #endif
  #endif
  #include <limits.h>
#else
  #undef CORO_MMAP
#endif

#if _POSIX_MEMORY_PROTECTION
  #ifndef CORO_GUARDPAGES
    #define CORO_GUARDPAGES 4
  #endif
#else
  #undef CORO_GUARDPAGES
#endif

#if !CORO_MMAP
  #undef CORO_GUARDPAGES
#endif

#if !__i386 && !__x86_64 && !__powerpc && !__m68k && !__alpha && !__mips && !__sparc64
  #undef CORO_GUARDPAGES
#endif

#ifndef CORO_GUARDPAGES
  #define CORO_GUARDPAGES 0
#endif

#if !PAGESIZE
  #if !CORO_MMAP
    #define PAGESIZE 4096
  #else
static inline
long coro_pagesize(void)
{
	static long pagesize = 0;

	if (unlikely(!pagesize)) {
		pagesize = sysconf(_SC_PAGESIZE);
		if (unlikely(pagesize < 1)) {
			pagesize = 4096;
		}
	}

	return pagesize;
}

    #define PAGESIZE coro_pagesize()
  #endif
#endif

#ifndef CORO_SSIZE
  #define CORO_SSIZE  (256 * 1024)
#endif

#if CORO_SSIZE < 1
  #error "invalid size of stack"
#endif

#include <new>

CoroStack::CoroStack(long ssize)
	: _sbuffer((void*)0), _ssize(0)
{
	if (unlikely(ssize < 1)) {
		ssize = CORO_SSIZE;
	}

	_ssize = (ssize * (long)sizeof(void *) + PAGESIZE - 1) / PAGESIZE * PAGESIZE;
	
	long	ssze = _ssize + CORO_GUARDPAGES * PAGESIZE;
	void	*base = (void*)0;
#if CORO_FIBER
	/* nop */
#else
  #if CORO_MMAP
  	/* mmap supposedly does allocate-on-write for us */
  	base = mmap(0, ssze, PROT_READ | PROT_WRITE | PROT_EXEC,
  			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  
  	if (unlikely(base == (void *)-1)) {
  		/* some systems don't let us have executable heap */
  		/* we assume they won't need executable stack in that case */
  		base = mmap(0, ssze, PROT_READ | PROT_WRITE,
  				MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  
  		if (unlikely(base == (void *)-1)) {
  			throw std::bad_alloc();
  		}
  	}
  
    #if CORO_GUARDPAGES
  	mprotect(base, CORO_GUARDPAGES * PAGESIZE, PROT_NONE);
    #endif
  
  	base = (void *)((char *)base + CORO_GUARDPAGES * PAGESIZE);
  #else
  	base = malloc(ssze);
  
  	if (unlikely(!base)) {
  		throw std::bad_alloc();
  	}
  #endif	/* if CORO_MMAP */
  
  #if CORO_USE_VALGRIND
  	_valgrind_id = VALGRIND_STACK_REGISTER((char *)base, ((char *)base) + ssze - CORO_GUARDPAGES * PAGESIZE);
  #endif
	_sbuffer = base;
#endif
}

CoroStack::CoroStack(void *ptr, unsigned ssize) 
	: _sbuffer(ptr), _ssize(0 - (long)ssize)
{
#if CORO_FIBER
	/* nop */
#else
	if (unlikely(!ptr || !ssize)) {
		throw std::bad_alloc();
	}
#endif
}

CoroStack::~CoroStack()
{
#if CORO_FIBER
	/* nop */
#else
	
	if (unlikely(_ssize < 1 || !_sbuffer)) {
		return;
	}

  #if CORO_USE_VALGRIND
  	VALGRIND_STACK_DEREGISTER(stack->valgrind_id);
  #endif
  #if CORO_MMAP
  	munmap((void *)((char *)_sbuffer - CORO_GUARDPAGES * PAGESIZE),
  		_ssize + CORO_GUARDPAGES * PAGESIZE);
  #else
  	free(_sbuffer);
  #endif
#endif
}
