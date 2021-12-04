#include <cstring>

#include "audioSystem.h"
#include "audioDevice.h"
#include <TFE_System/system.h>
#include <TFE_System/math.h>
#include <TFE_Settings/settings.h>
#include <TFE_FrontEndUI/console.h>
#include <assert.h>
#include <algorithm>

// TODO: Add a command queue to avoid locking on the audio thread.
// Commands for adding/removing sources, changing volume, etc.
/*
#include <atomic>
typedef std::atomic_int  s32_atomic;
typedef std::atomic_uint u32_atomic;
#define atomicFetchAdd(a,b)   std::atomic_fetch_add(a,b)
#define atomicWrite(a, value) std::atomic_store(a, value)
#define atomicRead(a)         std::atomic_load(a)

u32 writeBuffer;
u32_atomic readBuffer;	// just keep incrementing and fixup after read.

writeBuffer = 0;
atomicWrite(&readBuffer, BUFFER_COUNT - 1);

// Write data to [writeBuffer]
// ...
// Mark the readBuffer = writeBuffer
atomicWrite(&readBuffer, writeBuffer);
// Increment writeBuffer for next frame.
writeBuffer = (writeBuffer + 1) % BUFFER_COUNT;

// Read current buffer index in the audio thread.
u32 index = atomicRead(&readBuffer);
*/

// Comment out the desired sigmoid function and comment all of the others.
//#define AUDIO_SIGMOID_CLIP 1
#define AUDIO_SIGMOID_TANH 1
//#define AUDIO_SIGMOID_RCP_SQRT 1

enum SoundSourceFlags
{
	SND_FLAG_ONE_SHOT = (1 << 0),
	SND_FLAG_ACTIVE   = (1 << 1),
	SND_FLAG_LOOPING  = (1 << 2),
	SND_FLAG_PLAYING  = (1 << 3),
	SND_FLAG_FINISHED = (1 << 4),
};

struct SoundSource
{
	SoundType type;
	f32 baseVolume;
	f32 volume;
	f32 seperation;		//stereo seperation.
	u32 sampleIndex;
	u32 flags;
	s32 slot;

	// Sound data.
	const SoundBuffer* buffer;

	// positional data.
	const Vec3f* pos;
	// local position if a pointer isn't used - this is for transient effects.
	Vec3f localPos;

	// Callback.
	SoundFinishedCallback finishedCallback = nullptr;
	void* finishedUserData = nullptr;
	s32 finishedArg = 0;
};

namespace TFE_Audio
{
	// This assumes 2 channel support only. This will do for the initial release.
	// TODO: Support surround sound setups (5.1, 7.1, etc).
	// TODO: Support proper HRTF data (optional).
	static const f32 c_stereoSwing   = 0.45f;	// 0.0 = mono positional audio (sound equal in both speakers), 0.5 = full swing (i.e. sound to the left is ONLY heard in the left speaker).
	static const f32 c_channelLimit  = 1.0f;
	static const f32 c_soundHeadroom = 0.35f;	// approximately 1 / sqrt(8); assuming 8 uncorrelated sounds playing at full volume.

	// Client volume controls, ranging from [0, 1]
	static f32 s_soundFxVolume = 1.0f;
	// Internal sound scale based on the client volume and headroom.
	static f32 s_soundFxScale = s_soundFxVolume * c_soundHeadroom;	// actual volume scale based on client set volume and headroom.

	static u32 s_sourceCount;
	static Vec3f s_listener;
	// TODO: This will need to be buffered to avoid locks.
	static SoundSource s_sources[MAX_SOUND_SOURCES];
	static Mutex s_mutex;
	static bool s_paused = false;

	s32 audioCallback(void *outputBuffer, void* inputBuffer, u32 bufferSize, f64 streamTime, u32 status, void* userData);
	void setSoundVolumeConsole(const ConsoleArgList& args);
	void getSoundVolumeConsole(const ConsoleArgList& args);

	bool init()
	{
		TFE_System::logWrite(LOG_MSG, "Startup", "TFE_AudioSystem::init");
		s_sourceCount = 0u;
		s_listener = { 0 };

		MUTEX_INITIALIZE(&s_mutex);

		CCMD("setSoundVolume", setSoundVolumeConsole, 1, "Sets the sound volume, range is 0.0 to 1.0");
		CCMD("getSoundVolume", getSoundVolumeConsole, 0, "Get the current sound volume.");

		TFE_Settings_Sound* soundSettings = TFE_Settings::getSoundSettings();
		setVolume(soundSettings->soundFxVolume);

		memset(s_sources, 0, sizeof(SoundSource) * MAX_SOUND_SOURCES);
		for (s32 i = 0; i < MAX_SOUND_SOURCES; i++)
		{
			s_sources[i].slot = i;
		}

		bool res = TFE_AudioDevice::init();
		res |= TFE_AudioDevice::startOutput(audioCallback, nullptr, 2u, 11025u);
		return res;
	}

	void shutdown()
	{
		TFE_System::logWrite(LOG_MSG, "Audio", "Shutdown");
		stopAllSounds();

		TFE_AudioDevice::destroy();
		MUTEX_DESTROY(&s_mutex);
	}

	void stopAllSounds()
	{
		MUTEX_LOCK(&s_mutex);
		s_sourceCount = 0u;
		memset(s_sources, 0, sizeof(SoundSource) * MAX_SOUND_SOURCES);
		for (s32 i = 0; i < MAX_SOUND_SOURCES; i++)
		{
			s_sources[i].slot = i;
		}
		MUTEX_UNLOCK(&s_mutex);
	}

	void setVolume(f32 volume)
	{
		s_soundFxVolume = volume;
		s_soundFxScale = s_soundFxVolume * c_soundHeadroom;
	}

	f32 getVolume()
	{
		return s_soundFxVolume;
	}

	void pause()
	{
		s_paused = true;
	}

	void resume()
	{
		s_paused = false;
	}

	// Issue - too many locks, wasted time looping over inactive sources.
	// TODO: Reduce audio thread cost and remove locks:
	// 1. SoundSource base array.
	// 2. SoundSource active array, only contains active sounds, sounds with data, etc.
	// 3. Add small queue with finished sounds; do cleanup during main thread update().
	//
	// Better:
	// Keep threads data seperate.
	// Add command buffer.
	// Add/Remove from main thread (add sound when play is called, set volume, etc.)
	// Setup finished queue to handle sounds finishing.

	void update(const Vec3f* listenerPos, const Vec3f* listenerDir)
	{
		// Currently positional audio only accounts for the "horizontal plane"
		// TODO: Support proper HRTF as an option, though we want to keep the "old school" handling in for the classic mode.
		Vec2f listDirXZ = { listenerDir->x, listenerDir->z };
		listDirXZ = TFE_Math::normalize(&listDirXZ);

		SoundSource* snd = s_sources;
		for (u32 s = 0; s < s_sourceCount; s++, snd++)
		{
			if (!(snd->flags & SND_FLAG_ACTIVE)) { continue; }

			if (snd->type == SOUND_2D)
			{
				snd->volume = snd->baseVolume;
			}
			else
			{
				// Compute attentuation and channel seperation.
				const Vec3f offset = { snd->pos->x - listenerPos->x, snd->pos->y - listenerPos->y, snd->pos->z - listenerPos->z };
				const f32 distSq = TFE_Math::dot(&offset, &offset);

				if (distSq >= c_clipDistance * c_clipDistance)
				{
					snd->volume = 0.0f;
				}
				else if (distSq < c_closeDistance * c_closeDistance)
				{
					snd->volume = snd->baseVolume;
				}
				else
				{
					const f32 dist = sqrtf(distSq);
					const f32 atten = 1.0f - (dist - c_closeDistance) / (c_clipDistance - c_closeDistance);
					snd->volume = snd->baseVolume * atten * atten;
				}

				snd->seperation = 0.5f;
				if (snd->volume > FLT_EPSILON)
				{
					// Spatialization is done on the XZ plane.
					const Vec2f offsetXZ = { offset.x, offset.z };
					const Vec2f dir = TFE_Math::normalize(&offsetXZ);
					// Sin of the angle between the listener direction and direction from listener to sound effect.
					const f32 sinAngle = dir.z*listDirXZ.x - dir.x*listDirXZ.z;
					// Stereo Seperation ranges from 0.0 (left), through 0.5 (middle) to 1.0 (right).
					snd->seperation -= c_stereoSwing * sinAngle;
				}
			}
		}
	}

	// One shot, play and forget. Only do this if the client needs no control until stopAllSounds() is called.
	// Note that looping one shots are valid.
	bool playOneShot(SoundType type, f32 volume, f32 stereoSeperation, const SoundBuffer* buffer, bool looping, const Vec3f* pos, bool copyPosition, SoundFinishedCallback finishedCallback, void* cbUserData, s32 cbArg)
	{
		if (!buffer) { return false; }

		MUTEX_LOCK(&s_mutex);
		// Find the first inactive source.
		SoundSource* snd = s_sources;
		SoundSource* newSource = nullptr;
		for (u32 s = 0; s < s_sourceCount; s++, snd++)
		{
			if (!(snd->flags&SND_FLAG_ACTIVE))
			{
				newSource = snd;
				break;
			}
		}
		if (!newSource && s_sourceCount < MAX_SOUND_SOURCES)
		{
			newSource = &s_sources[s_sourceCount];
			s_sourceCount++;
		}

		if (newSource)
		{
			newSource->type = type;
			newSource->flags = SND_FLAG_ACTIVE | SND_FLAG_PLAYING | SND_FLAG_ONE_SHOT;
			if (looping)
			{
				newSource->flags |= SND_FLAG_LOOPING;
			}
			newSource->volume = type == SOUND_3D ? 0.0f : volume;
			newSource->baseVolume = volume;
			newSource->buffer = buffer;
			newSource->sampleIndex = 0u;
			if (copyPosition)
			{
				newSource->localPos = *pos;
				newSource->pos = &newSource->localPos;
			}
			else
			{
				newSource->pos = pos;
			}
			newSource->seperation = stereoSeperation;
			newSource->finishedCallback = finishedCallback;
			newSource->finishedUserData = cbUserData;
			newSource->finishedArg = cbArg;
		}
		MUTEX_UNLOCK(&s_mutex);

		return newSource != nullptr;
	}

	// Sound source that the client holds onto.
	SoundSource* createSoundSource(SoundType type, f32 volume, f32 stereoSeperation, const SoundBuffer* buffer, const Vec3f* pos, bool copyPosition, SoundFinishedCallback callback, void* userData)
	{
		if (!buffer) { return nullptr; }
		assert(volume >= 0.0f && volume <= 1.0f);
		assert(stereoSeperation >= 0.0f && stereoSeperation <= 1.0f);

		MUTEX_LOCK(&s_mutex);
		// Find the first inactive source.
		SoundSource* snd = s_sources;
		SoundSource* newSource = nullptr;
		for (u32 s = 0; s < s_sourceCount; s++, snd++)
		{
			if (!(snd->flags&SND_FLAG_ACTIVE))
			{
				newSource = snd;
				break;
			}
		}
		if (!newSource && s_sourceCount < MAX_SOUND_SOURCES)
		{
			newSource = &s_sources[s_sourceCount];
			s_sourceCount++;
		}

		if (newSource)
		{
			newSource->type = type;
			newSource->flags = SND_FLAG_ACTIVE;
			newSource->volume = type == SOUND_2D ? volume : 0.0f;
			newSource->baseVolume = volume;
			newSource->buffer = buffer;
			newSource->sampleIndex = 0u;
			if (copyPosition && pos)
			{
				newSource->localPos = *pos;
				newSource->pos = &newSource->localPos;
			}
			else
			{
				newSource->pos = pos;
			}
			newSource->seperation = stereoSeperation;
			newSource->finishedCallback = callback;
			newSource->finishedUserData = userData;
		}
		MUTEX_UNLOCK(&s_mutex);

		return newSource;
	}

	s32 getSourceSlot(SoundSource* source)
	{
		if (!source) { return -1; }
		return source->slot;
	}

	SoundSource* getSourceFromSlot(s32 slot)
	{
		if (slot < 0 || slot >= MAX_SOUND_SOURCES)
		{
			return nullptr;
		}
		if (!(s_sources[slot].flags & SND_FLAG_ACTIVE))
		{
			return nullptr;
		}

		return &s_sources[slot];
	}

	void playSource(SoundSource* source, bool looping)
	{
		if (!source || (source->flags & SND_FLAG_PLAYING))
		{
			return;
		}
		
		MUTEX_LOCK(&s_mutex);
			source->flags |= SND_FLAG_PLAYING;
			if (looping) { source->flags |= SND_FLAG_LOOPING; }
			source->sampleIndex = 0u;
		MUTEX_UNLOCK(&s_mutex);
	}

	void stopSource(SoundSource* source)
	{
		if (!source) { return; }
		MUTEX_LOCK(&s_mutex);
			source->flags &= ~SND_FLAG_PLAYING;
		MUTEX_UNLOCK(&s_mutex);
	}
	
	void freeSource(SoundSource* source)
	{
		if (!source) { return; }
		MUTEX_LOCK(&s_mutex);
			source->flags &= ~SND_FLAG_PLAYING;
			source->flags &= ~SND_FLAG_ACTIVE;
			source->buffer = nullptr;
		MUTEX_UNLOCK(&s_mutex);
	}

	void setSourceVolume(SoundSource* source, f32 volume)
	{
		volume = std::max(0.0f, std::min(1.0f, volume));
		source->baseVolume = volume;
		if (source->type == SOUND_2D)
		{
			source->volume = source->baseVolume;
		}
	}

	void setSourceStereoSeperation(SoundSource* source, f32 stereoSeperation)
	{
		source->seperation = std::max(0.0f, std::min(stereoSeperation, 1.0f));
	}

	void setSourcePosition(SoundSource* source, const Vec3f* pos)
	{
		source->localPos = *pos;
	}

	// This will restart the sound and change the buffer.
	void setSourceBuffer(SoundSource* source, const SoundBuffer* buffer)
	{
		MUTEX_LOCK(&s_mutex);
			source->sampleIndex = 0u;
			source->buffer = buffer;
		MUTEX_UNLOCK(&s_mutex);
	}

	bool isSourcePlaying(SoundSource* source)
	{
		return (source->flags & SND_FLAG_PLAYING) != 0u;
	}

	f32 getSourceVolume(SoundSource* source)
	{
		return source->volume;
	}

	// Internal
	static const f32 c_scale[] = { 2.0f / 255.0f, 2.0f / 65535.0f, 1.0f };
	static const f32 c_offset[] = { -1.0f, -1.0f, 0.0f };

	void cleanupSources()
	{
		// call any finished callbacks.
		for (u32 s = 0; s < s_sourceCount; s++)
		{
			if (s_sources[s].flags&SND_FLAG_FINISHED)
			{
				s_sources[s].flags = 0;
				s_sources[s].buffer = nullptr;
				if (s_sources[s].finishedCallback)
				{
					s_sources[s].finishedCallback(s_sources[s].finishedUserData, s_sources[s].finishedArg);
				}
			}
		}

		const s32 end = (s32)s_sourceCount - 1;
		//shrink the number of sources until an active source is found.
		for (s32 s = end; s >= 0; s--)
		{
			if (s_sources[s].flags&SND_FLAG_ACTIVE)
			{
				break;
			}
			s_sourceCount--;
		}
	}
		
	f32 sampleBuffer(u32 index, SoundDataType type, const u8* data)
	{
		f32 sampleValue;
		switch (type)
		{
			case SOUND_DATA_8BIT:  { sampleValue = (f32)data[index]; } break;
			case SOUND_DATA_16BIT: { sampleValue = (f32)(*((u16*)data + index)); } break;
			case SOUND_DATA_FLOAT: { sampleValue = *((f32*)data + index); } break;
		};

		return sampleValue * c_scale[type] + c_offset[type];
	}
		
	// Audio callback
	s32 audioCallback(void *outputBuffer, void* inputBuffer, u32 bufferSize, f64 streamTime, u32 status, void* userData)
	{
		f32* buffer = (f32*)outputBuffer;
	
		MUTEX_LOCK(&s_mutex);
		for (u32 i = 0; i < bufferSize; i++, buffer += 2)
		{
			f32 valueLeft  = 0.0f;
			f32 valueRight = 0.0f;
			
			SoundSource* snd = s_sources;
			for (u32 s = 0; s < s_sourceCount && !s_paused; s++, snd++)
			{
				if (!(snd->flags&SND_FLAG_PLAYING)) { continue; }
				assert(snd->buffer->data);

				// Compute the sample value.
				const f32 sampleValue = sampleBuffer(snd->sampleIndex, snd->buffer->type, snd->buffer->data) * s_soundFxScale;
				// Stereo Seperation.
				const f32 sepSq = snd->seperation*snd->seperation;
				const f32 invSepSq = (1.0f - snd->seperation) * (1.0f - snd->seperation);
				valueLeft  += std::max(snd->volume - sepSq, 0.0f) * sampleValue;
				valueRight += std::max(snd->volume - invSepSq, 0.0f) * sampleValue;

				snd->sampleIndex++;
				if (snd->sampleIndex >= snd->buffer->size)
				{
					if (snd->flags&SND_FLAG_LOOPING)
					{
						snd->sampleIndex = snd->buffer->loopStart;
					}
					else
					{
						snd->flags &= ~SND_FLAG_PLAYING;
						snd->flags |= SND_FLAG_FINISHED;
						snd->sampleIndex = 0u;
					}
				}
			}

			// Audio outside of the [-1, 1] range will cause overflow, which is a major artifact.
			// Instead the audio needs to be limited in range, which can be done in several ways.
			// Sigmoid functions map an arbitrary range into [-1, 1] generall along an S-Curve, allowing us to avoid overflow.
		#if defined(AUDIO_SIGMOID_CLIP)		// Not really a Sigmoid function but acts in a similar way, naively mapping to the required range.
			valueLeft  = std::max(-c_channelLimit, std::min(valueLeft,  c_channelLimit));
			valueRight = std::max(-c_channelLimit, std::min(valueRight, c_channelLimit));
		#elif defined(AUDIO_SIGMOID_TANH)	// Considered one of the most "musical sounding" sigmoid functions, it avoids hard clipping.
			// Note the usable range is approximately -4.8 to 4.8 so the volumes should be adjusted to stay within those ranges when possible.
			// Still much better than the effect -1 to 1 range with hard clipping and cheaper than the more accurate library tanh(). :)
			valueLeft  = TFE_Math::tanhf_series(valueLeft);
			valueRight = TFE_Math::tanhf_series(valueRight);
		#elif defined(AUDIO_SIGMOID_RCP_SQRT)
			valueLeft  = valueLeft  / sqrtf(1.0f + valueLeft  * valueLeft);
			valueRight = valueRight / sqrtf(1.0f + valueRight * valueRight);
		#endif

			//stereo output.
			buffer[0] = valueLeft;
			buffer[1] = valueRight;
		}
		cleanupSources();
		MUTEX_UNLOCK(&s_mutex);

		return 0;
	}
		
	// Console functions.
	void setSoundVolumeConsole(const ConsoleArgList& args)
	{
		if (args.size() < 2) { return; }

		s_soundFxVolume = TFE_Console::getFloatArg(args[1]);
		s_soundFxScale = s_soundFxVolume * c_soundHeadroom;

		TFE_Settings_Sound* soundSettings = TFE_Settings::getSoundSettings();
		soundSettings->soundFxVolume = s_soundFxVolume;
		TFE_Settings::writeToDisk();
	}

	void getSoundVolumeConsole(const ConsoleArgList& args)
	{
		char res[256];
		sprintf(res, "Sound Volume: %2.3f", s_soundFxVolume);
		TFE_Console::addToHistory(res);
	}
}
