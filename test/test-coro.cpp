#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>

#include "coro.hpp"

class Hello : public CoroFunc {
public:

	Hello(const std::string &hello) 
		: _coro(this, 0)
		, _next(Coro::getMainCoro())
		, _msg(hello) { }
	
	Hello(const std::string &hello, const Hello &next)
		: _coro(this, 0)
		, _next(next._coro)
		, _msg(hello) { }
	
	void bind(const Hello &next) { _next = next._coro; }
	
	Coro getCoro() { return _coro; }

	void operator()(const Coro &coro) {
		std::cout << "first hello:" << _msg << std::endl;
		coro.yield(_next);
		std::cout << "second hello:" << _msg << std::endl;
		coro.stop(_next);
		std::cout << "??? :" << _msg << std::endl;
	}
	
private:
	Coro		_coro;
	Coro 		_next;
	std::string _msg;
};


class Hi : public CoroFunc {
public:
	Hi(const std::string msg)
		: _msg(msg) { }

	void operator()(const Coro &coro) {
		std::cout << "first hello:" << _msg << std::endl;
		coro.yield(Coro::getMainCoro());
		std::cout << "second hello:" << _msg << std::endl;
		coro.stop(Coro::getMainCoro());
		std::cout << "??? :" << _msg << std::endl;
	}
private:
	std::string _msg;
};


class NonRun : public CoroFunc {
public:
	void operator()(const Coro &coro) {
		std::cout << "??? : Test Fault" << std::endl;
	}
};

int main() {
	if (1) {
		Hello world("world");
		Hello china("china", world);
		Hello sichuan("sichuan", china);
		Hello deyang("deyang", sichuan);
		Hello zhoukai("zhoukai", deyang);

		std::cout << "main start" << std::endl;

		Coro::getMainCoro().yield(zhoukai.getCoro());

		std::cout << "main restore" << std::endl;

		Coro::getMainCoro().yield(zhoukai.getCoro());

		std::cout << "main end" << std::endl;
	}

	if (1) {
		
		Hi china("China");

		Coro chinaCoro(&china);

		std::cout << "main start" << std::endl;
		Coro::getMainCoro().yield(chinaCoro);
		std::cout << "main restore" << std::endl;
		chinaCoro.stop();
		std::cout << "main end" << std::endl;
	}

	if (1) {
		NonRun non;
		Coro nonCoro(&non);
	}
	
#ifdef _WIN32
	getchar();
#endif
	return EXIT_SUCCESS;
}
