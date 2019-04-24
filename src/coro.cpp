#include "coro_impl.hpp"
#include "coro.hpp"


Coro::~Coro()
{
	if (--_coimpl->_used == 0) {
		delete _coimpl;
		_coimpl = (CoroImpl*)0;
	}
}

Coro::Coro()
	: _coimpl(CoroImplFactory::create())
{
	
}

Coro::Coro(CoroFunc *func, int size)
	: _coimpl(CoroImplFactory::create(func, size))
{

}

Coro::Coro(const Coro & coro)
	: _coimpl(coro._coimpl)
{
	_coimpl->_used++;
}

Coro::Coro(CoroImpl* coimp)
	: _coimpl(coimp)
{
	_coimpl->_used++;
}

Coro Coro::mainCoro;

Coro & Coro::operator = (const Coro & coro)
{
	coro._coimpl->_used++; 
	if(--_coimpl->_used == 0) {
		delete _coimpl;
	}
	_coimpl = coro._coimpl;
	
	return *this;
}

void Coro::yield(const Coro &coro) const
{
	const_cast<CoroImpl*>(_coimpl)->yield(const_cast<CoroImpl*>(coro._coimpl));
}

void Coro::stop(const Coro &coro) const
{
	const_cast<CoroImpl*>(_coimpl)->stop(const_cast<CoroImpl*>(coro._coimpl));
}

void Coro::stop() const
{
	const_cast<CoroImpl*>(_coimpl)->stop();
}