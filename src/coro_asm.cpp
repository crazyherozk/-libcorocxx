#include "config.h"
#include "coro.hpp"
#include "coro_impl.hpp"
#include "coro_stack.hpp"

#include <stddef.h>
#include <sys/types.h>
#include <iostream>
#include <stdexcept>

#if __x86_64 && __ILP32
  #error unsupported Hand coded assembly
#endif

#if HAS_CXX11_THREAD_LOCAL
  #define THREAD_LOCAL  thread_local
#elif GCC_VERSION
  #define THREAD_LOCAL  __thread
#elif _MSC_VER
  #define THREAD_LOCAL  __declspec(thread)
#else	// !C++11 && !__GNUC__ && !_MSC_VER
  #error "define a thread local storage qualifier for your compiler/platform!"
#endif

extern "C" {
	struct CoroContext {
		void **sp; /*must be at offset 0*/
	};
	typedef struct CoroContext CoroContext;
	
	void __attribute__((__noinline__, __regparm__(2)))
		coroTransfer(CoroContext *prev, CoroContext *next);
	
	static void coroInit(void);
	
	struct CoroAssistor
	{
		CoroImpl*	_new_coro;
		CoroImpl*	_creator_coro;
	};
	typedef struct CoroAssistor CoroAssistor;
	
	static THREAD_LOCAL CoroAssistor * volatile p_coro_assistor = (CoroAssistor*)0;
}

asm (
	"\t.text\n"
	"\t.globl coroTransfer\n"
	"coroTransfer:\n"
#if __amd64
	#define NUM_SAVED 6
	"\tpushq %rbp\n"
	"\tpushq %rbx\n"
	"\tpushq %r12\n"
	"\tpushq %r13\n"
	"\tpushq %r14\n"
	"\tpushq %r15\n"
	"\tmovq %rsp, (%rdi)\n"
	"\tmovq (%rsi), %rsp\n"
	"\tpopq %r15\n"
	"\tpopq %r14\n"
	"\tpopq %r13\n"
	"\tpopq %r12\n"
	"\tpopq %rbx\n"
	"\tpopq %rbp\n"
	"\tpopq %rcx\n"
	"\tjmpq *%rcx\n"

#elif __i386
  #define NUM_SAVED 4
	"\tpushl %ebp\n"
	"\tpushl %ebx\n"
	"\tpushl %esi\n"
	"\tpushl %edi\n"
	"\tmovl %esp, (%eax)\n"
	"\tmovl (%edx), %esp\n"
	"\tpopl %edi\n"
	"\tpopl %esi\n"
	"\tpopl %ebx\n"
	"\tpopl %ebp\n"
	"\tpopl %ecx\n"
	"\tjmpl *%ecx\n"
#else
      #error unsupported architecture
#endif	/* if __amd64 */
	);

class CoroAsm : public CoroImpl {
public:
				CoroAsm()
					: CoroImpl()
				{ create(); }
	explicit 	CoroAsm(CoroFunc *func, long ssize = 0)
					: CoroImpl(func,  ssize)
				{ create(); }

protected:
	virtual void create();
	virtual void destroy() { }
	virtual void transfer(CoroImpl *coro);

private:			
	CoroContext _ctx;
};

void CoroAsm::create()
{
	if (unlikely(!_sstack)) {
		// default context
		return;
	}

	if(unlikely(!_func)) {
		throw std::bad_alloc();
	}
	
	void*		sptr = addr();
	long		ssize = size();
	CoroAsm		cctx;
	CoroAssistor coro_assistor;
	
	coro_assistor._new_coro = this;
	coro_assistor._creator_coro = &cctx;
	
	p_coro_assistor = &coro_assistor;
	_ctx.sp = (void **)(ssize + (char *)sptr);
	*--_ctx.sp = (void *)abort;		/* needed for alignment only */
	*--_ctx.sp = (void *)coroInit;

	_ctx.sp -= NUM_SAVED;
	memset(_ctx.sp, 0, sizeof(*_ctx.sp) * NUM_SAVED);
	
	cctx.yield(this);
}

void CoroAsm::transfer(CoroImpl *coro)
{
	coroTransfer(&_ctx, &reinterpret_cast<CoroAsm*>(coro)->_ctx);
}

void coroInit(void)
{
	CoroImpl* volatile new_coro = p_coro_assistor->_new_coro;
	CoroImpl* volatile creator_coro = p_coro_assistor->_creator_coro;

	new_coro->run(creator_coro);
}

CoroImpl *CoroImplFactory::create()
{
	return new CoroAsm();
}
CoroImpl *CoroImplFactory::create(CoroFunc *func, long ssize)
{
	return new CoroAsm(func, ssize);
}