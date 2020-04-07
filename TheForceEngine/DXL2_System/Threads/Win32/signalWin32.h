#pragma once
#include <Windows.h>
#include "../signal.h"

class SignalWin32 : public Signal
{
public:
	SignalWin32();
	virtual ~SignalWin32();

	virtual void fire();
	virtual bool wait(u32 timeOutInMS=TIMEOUT_INFINITE, bool reset=true);

protected:
	HANDLE m_win32Handle;
};
