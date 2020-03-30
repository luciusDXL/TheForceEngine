#include "signalWin32.h"
#include <DXL2_System/system.h>

SignalWin32::SignalWin32()
{
	m_win32Handle = CreateEvent(0, TRUE, FALSE, 0);
}

SignalWin32::~SignalWin32()
{
	if (m_win32Handle)
	{
		CloseHandle(m_win32Handle);
	}
}

void SignalWin32::fire()
{
	SetEvent(m_win32Handle);
}

bool SignalWin32::wait(u32 timeOutInMS, bool reset)
{
	DWORD res = WaitForSingleObject(m_win32Handle, timeOutInMS);

	//reset the event so it can be used again but only if the event was signaled.
	if (res == WAIT_OBJECT_0 && reset)
	{
		ResetEvent(m_win32Handle);
	}

	return (res!=WAIT_TIMEOUT);
}

//factory
Signal* Signal::create()
{
	return new SignalWin32();
}
