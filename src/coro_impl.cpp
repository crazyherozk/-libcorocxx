#include "coro.hpp"
#include "coro_impl.hpp"

void CoroImpl::run(CoroImpl *coro) 
{
	// wait for scheduling by user again
	yield(coro);
	
#if __GCC_HAVE_DWARF2_CFI_ASM && __amd64
	asm(".cfi_undefined rip");
#endif
	logxx("Ready to run corotine : %p.", this);
	if (likely(_func)) {
		logxx("corotine call user's function.");
		_flag |= RUNNING;
		(*_func)(Coro(this));
	}
	logxx("Serious error : %p", this);
	/* the new coro returned. bad. just abort() for now */
	abort();
}