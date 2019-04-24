#include "config.h"
#include "coro.hpp"
#include "coro_impl.hpp"
#include "coro_stack.hpp"

#if !HAVE_UCONTEXT_H
  #error unknown or unsupported architecture
#endif

#include <unistd.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <iostream>
#include <stdexcept>
#include <ucontext.h>

extern "C" {
	struct CoroAssistor
	{
		CoroImpl*	_new_coro;
		CoroImpl*	_creator_coro;
	};
	typedef struct CoroAssistor CoroAssistor;
	
	static void coroInit(uint32_t, uint32_t);
}

class CoroUctx : public CoroImpl {
public:
				CoroUctx()
					: CoroImpl()
				{ create(); }
	explicit	CoroUctx(CoroFunc *func, long ssize = 0)
					: CoroImpl(func, ssize)
				{ create(); }
				~CoroUctx()
				{ destroy(); }
protected:
	virtual void create();
	virtual void destroy() { }
	virtual void transfer(CoroImpl *coro);

private:
	ucontext_t	_ctx;
};

void CoroUctx::create()
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
	CoroUctx	cctx;
	
	CoroAssistor coro_assistor;
	coro_assistor._new_coro = this;
	coro_assistor._creator_coro = &cctx;
	
	getcontext(&_ctx);

	_ctx.uc_link = 0;
	_ctx.uc_stack.ss_sp = sptr;
	_ctx.uc_stack.ss_size = (size_t)ssize;
	_ctx.uc_stack.ss_flags = 0;
	
	makecontext(&_ctx,
		(void (*)())coroInit,
		2,
		(uint32_t)(((uintptr_t)&coro_assistor) >> 32),
		(uint32_t)(uintptr_t)&coro_assistor);
		
	cctx.yield(this);
}

void CoroUctx::transfer(CoroImpl* coro)
{
	swapcontext(&_ctx, &reinterpret_cast<CoroUctx*>(coro)->_ctx);
}

void coroInit(uint32_t h, uint32_t l)
{
	CoroAssistor* p_coro_assistor = (CoroAssistor*)0;
	
	uint64_t bits = h;
	bits <<= 32;
	bits |= l;
	p_coro_assistor = reinterpret_cast<CoroAssistor*>(static_cast<uintptr_t>(bits));
	
	CoroImpl* volatile new_coro = p_coro_assistor->_new_coro;
	CoroImpl* volatile creator_coro = p_coro_assistor->_creator_coro;
	
	new_coro->run(creator_coro);
}

CoroImpl *CoroImplFactory::create()
{
	return new CoroUctx();
}
CoroImpl *CoroImplFactory::create(CoroFunc *func, long ssize)
{
	return new CoroUctx(func, ssize);
}
	