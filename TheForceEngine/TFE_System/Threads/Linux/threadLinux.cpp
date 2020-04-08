#include "threadLinux.h"
#include "../../log.h"

ThreadLinux::ThreadLinux(const char* name, ThreadFunc func, void* userData) : Thread(name, func, userData)
{
	m_handle = 0;
}

ThreadLinux::~ThreadLinux()
{
	if (m_handle)
	{
        //terminate the thread.
		pthread_cancel(m_handle);
	}
}

bool ThreadLinux::run()
{
	s32 res = pthread_create(&m_handle, NULL, m_func, m_userData);
	//create the thread.
	if (res == 0)
	{
		//setThreadName(m_win32Handle, m_name);
	}
	else
	{
        m_handle = 0;
		LOG( LOG_ERROR, "Thread \"%s\" cannot be run", m_name );
	}

	return (res == 0);
}

void ThreadLinux::pause()
{
	if (!m_handle) { return; }
	//to-do.
}

void ThreadLinux::resume()
{
	if (!m_handle) { return; }
	//to-do.
}

//factory
Thread* Thread::create(const char* name, ThreadFunc func, void* userData)
{
	return new ThreadLinux(name, func, userData);
}
