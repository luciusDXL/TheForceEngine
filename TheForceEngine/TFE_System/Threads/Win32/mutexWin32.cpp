#include "mutexWin32.h"

MutexWin32::MutexWin32() : Mutex()
{ 
	InitializeCriticalSection(&C); 
}

MutexWin32::~MutexWin32()
{ 
	DeleteCriticalSection(&C); 
}

s32 MutexWin32::lock()
{ 
	EnterCriticalSection(&C); 
	return 0; 
}

s32 MutexWin32::unlock()
{ 
	LeaveCriticalSection(&C); 
	return 0; 
}

//factory
Mutex* Mutex::create()
{
	return new MutexWin32();
}
