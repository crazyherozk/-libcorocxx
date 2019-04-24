#include "config.h"
#include "coro.hpp"
#include "coro_impl.hpp"
#include "coro_stack.hpp"

#if !_WIN32 || __CYGWIN__
  #error unknown or unsupported architecture
#endif

#define WIN32_LEAN_AND_MEAN
#if _WIN32_WINNT < 0x0400
  #undef _WIN32_WINNT
  #define _WIN32_WINNT 0x0400
#endif
#include <windows.h>

__pragma(pack(push, _CRT_PACKING))
extern "C" {
	struct CoroAssistor
	{
		CoroImpl*	_new_coro;
		CoroImpl*	_creator_coro;
	};
	typedef struct CoroAssistor CoroAssistor;
	static VOID CALLBACK coroInit(PVOID args);
}
__pragma(pack(pop))

class CoroFiber : public CoroImpl {
public:
				CoroFiber()
					: CoroImpl()
					, _fiber((void*)0)
				{ create(); }
	explicit 	CoroFiber(CoroFunc *func, long ssize = 0)
					: CoroImpl(func,  ssize)
					, _fiber((void*)0)
				{ create(); }
				~CoroFiber()
				{ destroy(); }

protected:
	virtual void create();
	virtual void destroy();
	virtual void transfer(CoroImpl *coro);

private:
	void	*_fiber;
};


void CoroFiber::create()
{
	if (unlikely(!_sstack)) {
		// default context
		return;
	}

	if(unlikely(!_func)) {
		throw std::bad_alloc();
	}
	
	long		ssize = size();
	CoroFiber 	cctx;
	
	CoroAssistor coro_assistor;
	coro_assistor._new_coro = this;
	coro_assistor._creator_coro = &cctx;
	
	_fiber = CreateFiber(ssize, coroInit, &coro_assistor);
	checkSysError(!!_fiber);
	
	cctx.yield(this);
}

void CoroFiber::transfer(CoroImpl *coro)
{
	if (unlikely(!_fiber)) {
		_fiber = GetCurrentFiber();
		if ((_fiber == 0) || (_fiber == (void *)0x1e00)) {
			_fiber = ConvertThreadToFiber(0);
			checkSysError(!!_fiber);
		}
	}
	
	SwitchToFiber(reinterpret_cast<CoroFiber*>(coro)->_fiber);
}

void CoroFiber::destroy()
{
	if (unlikely(!_fiber)) {
		return;
	}
	DeleteFiber(_fiber);
}

VOID CALLBACK coroInit(PVOID args)
{
	CoroAssistor *p_coro_assistor = reinterpret_cast<CoroAssistor*>(args);
	CoroImpl* volatile new_coro = p_coro_assistor->_new_coro;
	CoroImpl* volatile creator_coro = p_coro_assistor->_creator_coro;
	
	new_coro->run(creator_coro);
}

CoroImpl *CoroImplFactory::create()
{
	return new CoroFiber();
}
CoroImpl *CoroImplFactory::create(CoroFunc *func, long ssize)
{
	return new CoroFiber(func, ssize);
}