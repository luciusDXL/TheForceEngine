#pragma once
#include <pthread.h>
#include "../mutex.h"

class MutexLinux : public Mutex
{
public:
	MutexLinux();
	virtual ~MutexLinux();

	virtual s32 lock();
	virtual s32 unlock();

private:
	pthread_mutex_t m_handle;
};
