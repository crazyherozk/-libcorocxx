#ifndef _LIBCOROCXX_
#define _LIBCOROCXX_

class CoroImpl;
class Coro;


class CoroFunc {
public:
	virtual ~CoroFunc() { }
	virtual void operator()(const Coro &) = 0;
};

class Coro {
	friend class CoroImpl;
public:
	static const Coro& getMainCoro() { return mainCoro; }
	
public:
	explicit 	Coro(CoroFunc *func, int ssize = 0);
				Coro(const Coro & coro);
				Coro();// only for creating new main corotine, maybe you could use it in multi thread.
	virtual		~Coro();
	
	void		yield(const Coro &coro) const;
	
	/*manual stop this coro, and switch to other coro, call this function in corotine only*/
	void		stop(const Coro &coro) const;
	/*manual stop this coro, call this function out of corotine only*/
	void		stop() const;
	
	Coro &		operator = (const Coro & coro);

private:
				Coro(CoroImpl* coimp);
	CoroImpl*	_coimpl;
	static Coro mainCoro;
};

#endif
