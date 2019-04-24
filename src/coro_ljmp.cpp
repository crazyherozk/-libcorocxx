#include "config.h"
#include "coro.hpp"
#include "coro_stack.hpp"
#include "coro_impl.hpp"

#include <stddef.h>
#include <sys/types.h>

#if HAVE_SETJMP_H
  #include <setjmp.h>
#else
  #error unsupported setjmp/longjmp
#endif

#ifndef _WIN32
  #include <errno.h>
  #include <unistd.h>
#endif

#if defined(__GLIBC__) && (__GLIBC__ <= 2 && __GLIBC_MINOR__ <= 1)
  /*Old GNU/Linux systems (<= glibc-2.1)*/
  #define CORO_LINUX 1
  #include <features.h>
  #if defined(__GNU_LIBRARY__) && !defined(_GNU_SOURCE)
    #define _GNU_SOURCE
  #endif
#elif HAVE_SIGALTSTACK
  /*This flavour uses SUSv2's setjmp/longjmp and sigaltstack functions*/
  #define CORO_SJLJ 1
  #include <signal.h>
  #if defined(CORO_MUTEX)
    #include <pthread.h>
  #endif
#elif defined(_WIN32)
  #define CORO_WINDOWS 1
#endif

#if !defined(STACK_ADJUST_PTR)
  #if (__i386__ && CORO_LINUX) || (_M_IX86 && CORO_WINDOWS)
    #define STACK_ADJUST_PTR(sp, ss)  ((char *)(sp) + (ss))
    #define STACK_ADJUST_SIZE(sp, ss) (ss)
  #elif (__amd64__ && CORO_LINUX) || ((_M_AMD64 || _M_IA64) && CORO_WINDOWS)
    #define STACK_ADJUST_PTR(sp, ss)  ((char *)(sp) + (ss) - 8)
    #define STACK_ADJUST_SIZE(sp, ss) (ss)
  #else
    #define STACK_ADJUST_PTR(sp, ss)  (sp)
    #define STACK_ADJUST_SIZE(sp, ss) (ss)
  #endif
#endif


#if __sun
  #undef  _XOPEN_UNIX
  #define _XOPEN_UNIX 1
#endif

#if HAVE_SIGSETJMP
  #define coro_jmpbuf			sigjmp_buf
  #define coro_setjmp(env)		sigsetjmp(env, 1)
  #define coro_longjmp(env)		siglongjmp((env), 1)
#elif _XOPEN_UNIX > 0 || defined(_setjmp)
  #define coro_jmpbuf 			jmp_buf
  #define coro_setjmp(env)		_setjmp(env)
  #define coro_longjmp(env)		_longjmp((env), 1)
#else
  #define coro_jmpbuf 			jmp_buf
  #define coro_setjmp(env)		setjmp(env)
  #define coro_longjmp(env)		longjmp((env), 1)
#endif

#if CORO_SJLJ
  #define THREAD_LOCAL
#else
  #if HAS_CXX11_THREAD_LOCAL
    #define THREAD_LOCAL  thread_local
  #elif GCC_VERSION
    #define THREAD_LOCAL  __thread
  #elif _MSC_VER
    #define THREAD_LOCAL  __declspec(thread)
  #else	// !C++11 && !__GNUC__ && !_MSC_VER
    #error "define a thread local storage qualifier for your compiler/platform!"
  #endif
#endif

#ifdef _WIN32
__pragma(pack(push, _CRT_PACKING))
#endif
extern "C" {
	struct CoroAssistor
	{
		CoroImpl*	_new_coro;
		CoroImpl*	_creator_coro;
	};
	typedef struct CoroAssistor CoroAssistor;
	static THREAD_LOCAL CoroAssistor * volatile p_coro_assistor = (CoroAssistor*)0;
	
	static void	coroInit(void);
	static void trampoline(int);

	#if CORO_SJLJ
	  #ifdef CORO_MUTEX
	static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	    #define _sigprocmask(opt, n, o)	pthread_sigmask((opt), (n), (o))
		#define _kill(sig)				pthread_kill(pthread_self(), (sig))
		#define _lock()					pthread_mutex_lock(&mutex)
		#define _unlock()				pthread_mutex_unlock(&mutex)
		struct LockGuard { LockGuard() { _lock(); } ~LockGuard() { _unlock(); } };
	  #else
	    #define _sigprocmask(opt, n, o)	sigprocmask((opt), (n), (o))
		#define _kill(sig)				kill(getpid(), (sig))
		#define _lock()					((void)0)
		#define _unlock()				((void)0)
		struct LockGuard { char _; };
	  #endif
	static volatile int trampoline_done;
	
	static void trampoline(int sig);
	#endif
}
#ifdef _WIN32
__pragma(pack(pop))
#endif

class CoroLjmp : public CoroImpl {
public:
				CoroLjmp()
					: CoroImpl()
				{ create(); }
	explicit 	CoroLjmp(CoroFunc *func, long ssize = 0)
					: CoroImpl(func,  ssize)
				{ create(); }
	int 		setContext()
				{ return coro_setjmp(_ctx); }
protected:
	virtual void create();
	virtual void destroy() { }
	virtual void transfer(CoroImpl *coro);

private:
	coro_jmpbuf _ctx;
};

void CoroLjmp::create()
{
	memset(&_ctx, 0, sizeof(_ctx));
	if (unlikely(!_sstack)) {
		// default context
		return;
	}

	if(unlikely(!_func)) {
		throw std::bad_alloc();
	}

	void*		sptr = addr();
	long		ssize = size();
	CoroLjmp	cctx;
	
	CoroAssistor coro_assistor;
  #if CORO_SJLJ
	int                     flag;
	stack_t                 ostk, nstk;
	struct sigaction        osa, nsa;
	sigset_t                nsig, osig;
  #endif

	coro_assistor._new_coro = this;
	coro_assistor._creator_coro = &cctx;

  #if CORO_SJLJ
	/* we use SIGUSR2. first block it, then fiddle with it. */
	sigemptyset(&nsig);
	sigaddset(&nsig, SIGUSR2);
	_sigprocmask(SIG_BLOCK, &nsig, &osig);

	nsa.sa_handler = trampoline;
	sigemptyset(&nsa.sa_mask);
	nsa.sa_flags = SA_ONSTACK | SA_RESTART;

	flag = sigaction(SIGUSR2, &nsa, &osa);
	checkSysError(flag == 0);

	/* set the new stack */
	/* yes, some platforms (IRIX) get this wrong. */
	nstk.ss_sp = STACK_ADJUST_PTR(sptr, ssize);
	nstk.ss_size = STACK_ADJUST_SIZE(sptr, ssize);
	nstk.ss_flags = 0;

	flag = sigaltstack(&nstk, &ostk);
	checkSysError(flag == 0);

    {
		LockGuard();

		p_coro_assistor = &coro_assistor;
		trampoline_done = 0;
	
		_kill(SIGUSR2);
	
		sigfillset(&nsig);
		sigdelset(&nsig, SIGUSR2);
	
		while (!trampoline_done) {
			sigsuspend(&nsig);
		}
	}

	sigaltstack(0, &nstk);
	nstk.ss_flags = SS_DISABLE;

	flag = sigaltstack(&nstk, 0);
	checkSysError(flag == 0);

	sigaltstack(0, &nstk);
	checkSysError(!(~nstk.ss_flags & SS_DISABLE));
	
	if (~ostk.ss_flags & SS_DISABLE) { sigaltstack(&ostk, 0); }

	sigaction(SIGUSR2, &osa, 0);
	_sigprocmask(SIG_SETMASK, &osig, 0);

  #elif CORO_WINDOWS
	p_coro_assistor = &coro_assistor;
	setContext();
	
    #if __CYGWIN__ && __i386
	_ctx[8] = (long)coroInit;
	_ctx[7] = (long)((char *)sptr + ssize) - sizeof(long);
    #elif __CYGWIN__ && __x86_64
	_ctx[7] = (long)coroInit;
	_ctx[6] = (long)((char *)sptr + ssize) - sizeof(long);
    #elif defined __MINGW32__
	_ctx[5] = (long)coroInit;
	_ctx[4] = (long)((char *)sptr + ssize) - sizeof(long);
    #elif defined _M_IX86
	((_JUMP_BUFFER *)&_ctx)->Eip = (long)coroInit;
	((_JUMP_BUFFER *)&_ctx)->Esp = (long)STACK_ADJUST_PTR(sptr, ssize) - sizeof(long);
    #elif defined _M_AMD64
	((_JUMP_BUFFER *)&_ctx)->Rip = (__int64)coroInit;
	((_JUMP_BUFFER *)&_ctx)->Rsp = (__int64)STACK_ADJUST_PTR(sptr, ssize) - sizeof(__int64);
    #elif defined _M_IA64
	((_JUMP_BUFFER *)&_ctx)->StIIP = (__int64)coroInit;
	((_JUMP_BUFFER *)&_ctx)->IntSp = (__int64)STACK_ADJUST_PTR(sptr, ssize) - sizeof(__int64);
    #else
      #error "microsoft libc or architecture not supported"
    #endif

  #elif CORO_LINUX
	p_coro_assistor = &coro_assistor;
	setContext();
	
    #if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 0 && defined(JB_PC) && defined(JB_SP)
	_ctx[0].__jmpbuf[JB_PC] = (long)coroInit;
	_ctx[0].__jmpbuf[JB_SP] = (long)STACK_ADJUST_PTR(sptr, ssize) - sizeof(long);
    #elif __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 0 && defined(__mc68000__)
	_ctx[0].__jmpbuf[0].__aregs[0] = (long int)coroInit;
	_ctx[0].__jmpbuf[0].__sp = (int *)((char *)sptr + ssize) - sizeof(long);
    #elif defined(__GNU_LIBRARY__) && defined(__i386__)
	_ctx[0].__jmpbuf[0].__pc = (char *)coroInit;
	_ctx[0].__jmpbuf[0].__sp = (void *)((char *)sptr + ssize) - sizeof(long);
    #elif defined(__GNU_LIBRARY__) && defined(__amd64__)
	_ctx[0].__jmpbuf[JB_PC] = (long)coroInit;
	_ctx[0].__jmpbuf[0].__sp = (void *)((char *)sptr + ssize) - sizeof(long);
    #else
      #error "linux libc or architecture not supported"
    #endif

  #else
	#error unknown or unsupported architecture
  #endif
	
	// schedule corotine
	cctx.yield(this);
}

void CoroLjmp::transfer(CoroImpl *coro)
{
	if (!coro_setjmp(_ctx)) {
		coro_longjmp(reinterpret_cast<CoroLjmp*>(coro)->_ctx);
	}
}

void coroInit(void)
{
	CoroImpl* volatile new_coro = p_coro_assistor->_new_coro;
	CoroImpl* volatile creator_coro = p_coro_assistor->_creator_coro;

	new_coro->run(creator_coro);
}

#if CORO_SJLJ
static void trampoline(int sig)
{
	if (unlikely(!p_coro_assistor)) {
		return;
	}
	CoroLjmp *coro = reinterpret_cast<CoroLjmp*>(p_coro_assistor->_new_coro);
	if (coro->setContext()) {
		coroInit();		/* second run, launch it */
	}
	else {
		trampoline_done = 1;	/* first run*/
	}
}
#endif

CoroImpl *CoroImplFactory::create()
{
	return new CoroLjmp();
}
CoroImpl *CoroImplFactory::create(CoroFunc *func, long ssize)
{
	return new CoroLjmp(func, ssize);
}
