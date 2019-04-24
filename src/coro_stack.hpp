#ifndef _LIBCOROCXX_STACK_
#define _LIBCOROCXX_STACK_
#include "config.h"
class CoroImpl;
class CoroStack {
	friend class CoroImpl;

private:
	explicit	CoroStack(long ssize);
				CoroStack(void *ptr, unsigned ssize);
				~CoroStack();

	void*		_sbuffer;
	long		_ssize;
#if CORO_USE_VALGRIND
	int			_valgrind_id;
#endif
};

#endif