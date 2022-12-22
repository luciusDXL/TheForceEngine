#include <TFE_System/system.h>
#include <pthread.h>
#include "threadLinux.h"

ThreadLinux::ThreadLinux(const char* name, ThreadFunc func, void* userData) : Thread(name, func, userData)
{
	m_handle = 0;
}

ThreadLinux::~ThreadLinux()
{
	if (m_handle)
	{
		pthread_cancel(m_handle);
		m_handle = 0;
	}
}

bool ThreadLinux::run(void)
{
	int res = pthread_create(&m_handle, NULL, m_func, m_userData);
	//create the thread.
	if (res != 0)
	{
		m_handle = 0;
		TFE_System::logWrite( LOG_ERROR, "Thread \"%s\" cannot be run", m_name );
	}

	return (res == 0);
}

void ThreadLinux::pause(void)
{
	if (!m_handle) { return; }
	//to-do.
}

void ThreadLinux::resume(void)
{
	if (!m_handle) { return; }
	//to-do.
}

void ThreadLinux::waitOnExit(void)
{
	pthread_cancel(m_handle);
	m_handle = 0;
	m_isRunning = false;
}

//factory
Thread* Thread::create(const char* name, ThreadFunc func, void* userData)
{
	return new ThreadLinux(name, func, userData);
}
