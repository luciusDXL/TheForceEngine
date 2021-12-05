#include <pthread.h>

#include <TFE_System/system.h>

#include "threadLinux.h"

ThreadLinux::ThreadLinux(const char* name, ThreadFunc func, void* userData) : Thread(name, func, userData)
{
	have_handle = false;
}

ThreadLinux::~ThreadLinux()
{
	if (have_handle) {
		pthread_cancel(m_handle);
		have_handle = false;
	}
}

bool ThreadLinux::run()
{
	s32 res = pthread_create(&m_handle, NULL, m_func, m_userData);
	//create the thread.
	if (res == 0)
	{
		have_handle = true;
	}
	else
	{
		have_handle = false;
		TFE_System::logWrite( LOG_ERROR, "Thread \"%s\" cannot be run", m_name );
	}

	return (res == 0);
}

void ThreadLinux::pause()
{
	if (!have_handle) { return; }
	//to-do.
}

void ThreadLinux::resume()
{
	if (!have_handle) { return; }
	//to-do.
}

void ThreadLinux::waitOnExit()
{
	pthread_cancel(m_handle);
	m_isRunning = false;
	have_handle = false;
}

//factory
Thread* Thread::create(const char* name, ThreadFunc func, void* userData)
{
	return new ThreadLinux(name, func, userData);
}
