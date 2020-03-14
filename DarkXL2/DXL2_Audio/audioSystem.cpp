#include "audioSystem.h"
#include "audioDevice.h"
#include <assert.h>
#include <algorithm>

enum SoundSourceFlags
{
	SND_FLAG_ONE_SHOT = (1 << 0),
	SND_FLAG_ACTIVE   = (1 << 1),
	SND_FLAG_LOOPING  = (1 << 2),
	SND_FLAG_PLAYING  = (1 << 3),
};

struct SoundSource
{
	SoundType type;
	f32 volume;
	f32 seperation;		//stereo seperation.
	u32 sampleIndex;
	u32 flags;

	// Sound data.
	const SoundBuffer* buffer;

	// positional data.
	const Vec3f* pos;
};

namespace DXL2_Audio
{
	#define MAX_SOUND_SOURCES 256
	static u32 s_sourceCount;
	static Vec3f s_listener;
	static SoundSource s_sources[MAX_SOUND_SOURCES];
	static Mutex s_mutex;

	s32 audioCallback(void *outputBuffer, void* inputBuffer, u32 bufferSize, f64 streamTime, u32 status, void* userData);

	bool init()
	{
		s_sourceCount = 0u;
		s_listener = { 0 };

		MUTEX_INITIALIZE(&s_mutex);

		bool res = DXL2_AudioDevice::init();
		res |= DXL2_AudioDevice::startOutput(audioCallback, nullptr, 2u, 11025u);
		return res;
	}

	void shutdown()
	{
		stopAllSounds();
		DXL2_AudioDevice::destroy();
		MUTEX_DESTROY(&s_mutex);
	}

	void stopAllSounds()
	{
		s_sourceCount = 0u;
		memset(s_sources, 0, sizeof(SoundSource) * MAX_SOUND_SOURCES);
	}

	// For positional audio, a listener is required.
	void setListenerPosition(const Vec3f* pos)
	{
		s_listener = *pos;
	}

	// One shot, play and forget. Only do this if the client needs no control until stopAllSounds() and freeAllSounds() is called.
	// Note that looping one shots are valid.
	bool playOneShot(SoundType type, f32 volume, f32 stereoSeperation, const SoundBuffer* buffer, bool looping, const Vec3f* pos)
	{
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
			newSource->volume = volume;
			newSource->buffer = buffer;
			newSource->sampleIndex = 0u;
			newSource->pos = pos;
			newSource->seperation = stereoSeperation;
		}
		MUTEX_UNLOCK(&s_mutex);

		return newSource != nullptr;
	}

	// Sound source that the client holds onto.
	SoundSource* createSoundSource(SoundType type, f32 volume, f32 stereoSeperation, const SoundBuffer* buffer, const Vec3f* pos)
	{
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
			newSource->volume = volume;
			newSource->buffer = buffer;
			newSource->sampleIndex = 0u;
			newSource->pos = pos;
			newSource->seperation = stereoSeperation;
		}
		MUTEX_UNLOCK(&s_mutex);

		return newSource;
	}

	void playSource(SoundSource* source, bool looping)
	{
		if (source->flags & SND_FLAG_PLAYING)
		{
			return;
		}
		
		source->flags = SND_FLAG_PLAYING;
		if (looping) { source->flags |= SND_FLAG_LOOPING; }
		source->sampleIndex = 0u;
	}

	void stopSource(SoundSource* source)
	{
		source->flags &= ~SND_FLAG_PLAYING;
	}

	void freeSource(SoundSource* source)
	{
		stopSource(source);
		source->flags &= ~SND_FLAG_ACTIVE;
	}

	void setSourceVolume(SoundSource* source, f32 volume)
	{
		source->volume = volume;
	}

	void setSourceStereoSeperation(SoundSource* source, f32 stereoSeperation)
	{
		source->seperation = std::max(0.0f, std::min(stereoSeperation, 1.0f));
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
		s32 end = (s32)s_sourceCount - 1;
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
			for (u32 s = 0; s < s_sourceCount; s++, snd++)
			{
				if (!(snd->flags&SND_FLAG_PLAYING)) { continue; }

				// Compute the sample value.
				const f32 sampleValue = sampleBuffer(snd->sampleIndex, snd->buffer->type, snd->buffer->data);
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
						snd->sampleIndex = 0u;
						// If this is a one shot, flag for later removal.
						if (snd->flags&SND_FLAG_ONE_SHOT)
						{
							snd->flags &= ~SND_FLAG_ACTIVE;
						}
					}
				}
			}

			//clamp.
			valueLeft  = std::min(valueLeft  * 0.5f, 0.98f);
			valueRight = std::min(valueRight * 0.5f, 0.98f);

			//stereo output.
			buffer[0] = valueLeft;
			buffer[1] = valueRight;
		}
		cleanupSources();
		MUTEX_UNLOCK(&s_mutex);

		return 0;
	}
}
