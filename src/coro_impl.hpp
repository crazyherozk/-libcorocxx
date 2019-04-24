#ifndef _LIBCOROXXIMPL_
#define _LIBCOROXXIMPL_

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <iostream>
#include <string>
#include <stdexcept>

#if _WIN32
  #define strerror_r(e, p, s)	strerror_s((p), (s), (e))
#endif

#include "config.h"
#include "coro_stack.hpp"

class Coro;
class CoroFunc;
class CoroStack;
class CoroImpl;

class CoroImplFactory {
public:
	static CoroImpl *create();
	static CoroImpl *create(CoroFunc *func, long ssize);
};

class CoroImpl {
	friend class Coro;
protected:
				CoroImpl() //main coro
					: _flag(INITIAL & (~SUSPEND))
					, _used(1)
					, _sstack((CoroStack*)0)
					, _func((CoroFunc*)0)
				{
					logxx("new default coro : %p", this);
				}

	explicit 	CoroImpl(CoroFunc *func, long ssize = 0) //sub coro
					: _flag(INITIAL | SUSPEND)
					, _used(1)
					, _sstack(new CoroStack(ssize))
					, _func(func)
				{
					logxx("new function coro : %p", this);
				}

	virtual		~CoroImpl()
				{
					logxx("free coro : %p", this);
					if (likely(_sstack)) { delete _sstack; }
				}
public:
	
	inline void yield(CoroImpl *coro);
	
		   void run(CoroImpl *coro);

	inline void stop(CoroImpl *coro);/*call in this corotine*/

	inline void	stop(); /*call out of this corotine*/

	void		bind(CoroFunc *func)
				{ _func = func; }
	
	void*		addr() const
				{ return likely(_sstack) ? _sstack->_sbuffer : (void*)0; }

	long		size() const
				{
					return likely(_sstack) ? 
						(likely(_sstack->_ssize > 0) ? 
							_sstack->_ssize : -_sstack->_ssize) : 0;
				}
	bool		isRunning() { return !!(_flag & RUNNING); }
	bool		isSuspend() { return !!(_flag & SUSPEND); }
	
protected:
	virtual void create() = 0;
	virtual void destroy() = 0;
	virtual void transfer(CoroImpl *coro) = 0;
	
	enum {
		INITIAL = 0x01,
		RUNNING = 0x02,
		SUSPEND = 0x04
	};
	typedef unsigned char ubit;
	ubit		_flag;
	int 		_used;
	CoroStack*	_sstack;
	CoroFunc*	_func;
	
protected:
	static void checkSysError(bool flag)
				{
					if (unlikely(!flag)) {
						char msg[512];
						strerror_r(errno, msg, sizeof(msg));
						throw std::runtime_error(std::string(msg));
					}
				}
};

void CoroImpl::yield(CoroImpl *coro)
{
	if (unlikely(!((_flag | SUSPEND) 
		&& (coro->_flag & SUSPEND)))) {
		throw std::invalid_argument("error status of corotine.");
	}
	
	// switch out
	logxx("corotine switch %p to %p", this, coro);
	_flag |= SUSPEND;
	transfer(coro);
	_flag &= ~SUSPEND;
	// switch in
	logxx("corotine %p has been restored.", this);
}

void CoroImpl::stop(CoroImpl *coro)
{
	if (unlikely(!_sstack)) {
		throw std::invalid_argument("error type of corotine");
	}
	
	if (unlikely(!((_flag | SUSPEND)
		&& (coro->_flag & SUSPEND)))) {
		throw std::invalid_argument("error status of corotine.");
		//logxx("corotine switch %p to %p", this, coro);
	}
	
	if(likely(_flag & RUNNING)) {
		logxx("corotine has been stoped.");
		_flag &= ~RUNNING;
		_used--;
	}
	
	// switch out
	logxx("corotine switch %p to %p", this, coro);
	transfer(coro);
	// nerver switch in
	logxx("Serious error : %p", this);
	/* the new coro returned. bad. just abort() for now */
	abort();
}

void CoroImpl::stop()
{
	if (unlikely(!_sstack)) {
		throw std::invalid_argument("error type of corotine");
	}
	
	if (unlikely(!(_flag & SUSPEND))) {
		throw std::invalid_argument("error status of corotine.");
	}
	
	// nerver been scheduled again.
	_flag |= SUSPEND;
	
	if(likely(_flag & RUNNING)) {
		logxx("corotine has been stoped.");
		_flag &= ~RUNNING;
		_used--;
	}
}

#endif