#include "mutexLinux.h"

MutexLinux::MutexLinux() : Mutex()
{
	pthread_mutex_init(&m_handle, NULL);
}

MutexLinux::~MutexLinux()
{
	pthread_mutex_destroy(&m_handle);
}

s32 MutexLinux::lock()
{
	pthread_mutex_lock(&m_handle);
	return 0;
}

s32 MutexLinux::unlock()
{
	pthread_mutex_unlock(&m_handle);
	return 0;
}

//factory
Mutex* Mutex::create()
{
	return new MutexLinux();
}
