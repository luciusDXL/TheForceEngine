#pragma once
#include <DXL2_System/types.h>
#include <string.h>

#ifdef _WIN32
    #define DXL2_THREADRET u32
#else
    #define DXL2_THREADRET void*
#endif

typedef DXL2_THREADRET (DXL2_STDCALL* ThreadFunc)(void*);

class Thread
{
public:
	virtual ~Thread() {};

	virtual bool run()    = 0;
	virtual void pause()  = 0;
	virtual void resume() = 0;
	virtual void waitOnExit() = 0;

	bool isRunning() { return m_isRunning; }
	bool isPaused() { return m_isPaused; }

protected:
	Thread(const char* name, ThreadFunc func, void* userData)
	{
		m_func = func;
		m_userData = userData;
		strcpy(m_name, name);
		m_isRunning = false;
		m_isPaused = false;
	}

public:
	//static factory function - creates the correct platform dependent version
	static Thread* create(const char* name, ThreadFunc func, void* userData);

protected:
	ThreadFunc m_func;
	void* m_userData;
	bool  m_isRunning;
	bool  m_isPaused;
	char  m_name[256];
};
