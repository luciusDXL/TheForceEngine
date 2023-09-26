#include "mutexWin32.h"

MutexWin32::MutexWin32() : Mutex()
{ 
	InitializeSRWLock(&C);
}

MutexWin32::~MutexWin32()
{ 
 
}

s32 MutexWin32::lock()
{ 
	AcquireSRWLockExclusive(&C);
	return 0; 
}

s32 MutexWin32::unlock()
{ 
	ReleaseSRWLockExclusive(&C);
	return 0; 
}

//factory
Mutex* Mutex::create()
{
	return new MutexWin32();
}
