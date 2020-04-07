#pragma once
#include <DXL2_System/types.h>
#include <string.h>

#define TIMEOUT_INFINITE 0xFFFFFFFF

class Signal
{
public:
	~Signal() {};

	virtual void fire() = 0;
	//returns true if signaled, false if the timeout was hit instead.
	virtual bool wait(u32 timeOutInMS=TIMEOUT_INFINITE, bool reset=true) = 0;

public:
	//static factory function - creates the correct platform dependent version
	static Signal* create();

protected:
};
