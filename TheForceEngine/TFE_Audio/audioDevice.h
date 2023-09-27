#pragma once
#include <TFE_System/types.h>
#include <TFE_FileSystem/paths.h>
#include "audioOutput.h"
#include <string>
#include <SDL_audio.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>

#define MUTEX_INITIALIZE(A) InitializeCriticalSection(A)
#define MUTEX_DESTROY(A)    DeleteCriticalSection(A)
#define MUTEX_LOCK(A)       EnterCriticalSection(A)
#define MUTEX_TRYLOCK(A)    TryEnterCriticalSection(A)
#define MUTEX_UNLOCK(A)     LeaveCriticalSection(A)

typedef CRITICAL_SECTION Mutex;
#undef min
#undef max
#else
#include <cstddef>

#include <pthread.h>

#define MUTEX_INITIALIZE(A)	pthread_mutex_init(A, NULL)
#define MUTEX_DESTROY(A)	pthread_mutex_destroy(A)
#define MUTEX_LOCK(A)		pthread_mutex_lock(A)
#define MUTEX_TRYLOCK(A)	pthread_mutex_trylock(A)
#define MUTEX_UNLOCK(A)		pthread_mutex_unlock(A)

typedef pthread_mutex_t Mutex;
#endif

namespace TFE_AudioDevice
{
	bool init(u32 audioFrameSize = 256u, s32 deviceId=-1, bool useNullDevice=false);
	void destroy();

	bool startOutput(SDL_AudioCallback callback, void* userData = 0, u32 channels = 2, u32 sampleRate = 44100);
	void stopOutput();

	s32 getDefaultOutputDevice();
	s32 getOutputDeviceId();
	s32 getOutputDeviceCount();

	const OutputDeviceInfo* getOutputDeviceList(s32& count, s32& curOutput);
};
