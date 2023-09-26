#pragma once
#include <Windows.h>
#include "../mutex.h"

class MutexWin32 : public Mutex
{
public:
	MutexWin32();
	virtual ~MutexWin32();

	virtual s32 lock();
	virtual s32 unlock();

private:
	SRWLOCK C;
};
