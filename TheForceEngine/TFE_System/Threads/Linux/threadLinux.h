#pragma once
#include <pthread.h>
#include "../thread.h"

class ThreadLinux : public Thread
{
public:
	ThreadLinux(const char* name, ThreadFunc func, void* userData);
	virtual ~ThreadLinux();

	virtual bool run();
	virtual void pause();
	virtual void resume();

protected:
	pthread_t m_handle;
};
