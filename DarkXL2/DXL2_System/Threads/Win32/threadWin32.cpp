#include "threadWin32.h"
#include <DXL2_System/system.h>

void setThreadName(HANDLE threadHandle, const char* threadName);

ThreadWin32::ThreadWin32(const char* name, ThreadFunc func, void* userData) : Thread(name, func, userData)
{
	m_win32Handle = 0;
}

ThreadWin32::~ThreadWin32()
{
	if (m_win32Handle)
	{
		TerminateThread(m_win32Handle, 0); // Dangerous source of errors! TO-DO: non-crappy solution. :D-
		CloseHandle(m_win32Handle);
	}
}

bool ThreadWin32::run()
{
	m_win32Handle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)m_func, m_userData, 0, NULL);
	if (m_win32Handle)
	{
		setThreadName(m_win32Handle, m_name);
		m_isRunning = true;
		m_isPaused = false;
	}
	else
	{
		DXL2_System::logWrite(LOG_ERROR, "Thread", "Thread \"%s\" cannot be run", m_name);
	}
	
	return m_win32Handle!=0;
}

void ThreadWin32::pause()
{
	if (!m_win32Handle) { return; }
	SuspendThread(m_win32Handle);
	m_isPaused = true;
}

void ThreadWin32::resume()
{
	if (!m_win32Handle) { return; }
	ResumeThread(m_win32Handle);
	m_isPaused = false;
}

void ThreadWin32::waitOnExit()
{
	WaitForSingleObject(m_win32Handle, INFINITE);
	m_isRunning = false;
}

//factory
Thread* Thread::create(const char* name, ThreadFunc func, void* userData)
{
	return new ThreadWin32(name, func, userData);
}

/////////////////////////////////////////////////////////////
// Wonky Windows specific method for setting the thread name.
/////////////////////////////////////////////////////////////
#define MS_VC_EXCEPTION 0x406D1388

#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
    DWORD dwType; // Must be 0x1000.
    LPCSTR szName; // Pointer to name (in user addr space).
    DWORD dwThreadID; // Thread ID (-1=caller thread).
    DWORD dwFlags; // Reserved for future use, must be zero.
 } THREADNAME_INFO;
#pragma pack(pop)

void setThreadName(HANDLE threadHandle, const char* threadName)
{
	u32 threadID = GetThreadId( threadHandle );

	THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = threadName;
    info.dwThreadID = threadID;
    info.dwFlags = 0;

	//Wonky Windows stuff... (see https://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx)
#pragma warning(push)
#pragma warning(disable: 6320 6322)
    __try
	{
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
	{
    }
#pragma warning(pop)
}
