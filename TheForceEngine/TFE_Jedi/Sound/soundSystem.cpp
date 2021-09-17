#include "soundSystem.h"
#include <TFE_Asset/vocAsset.h>
#include <TFE_Audio/audioSystem.h>

using namespace TFE_Audio;

namespace TFE_Jedi
{
	SoundEffectID playSound2D(SoundSourceID sourceId)
	{
		if (sourceId == NULL_SOUND) { return NULL_SOUND; }
		SoundBuffer* buffer = TFE_VocAsset::getFromIndex(sourceId - 1);
		if (!buffer) { return NULL_SOUND; }

		SoundSource* source = createSoundSource(SOUND_2D, 1.0f, 0.5f, buffer);
		playSource(source);
		
		return SoundSourceID(getSourceSlot(source) + 1);
	}

	SoundEffectID playSound2D_looping(SoundSourceID sourceId)
	{
		if (sourceId == NULL_SOUND) { return NULL_SOUND; }
		SoundBuffer* buffer = TFE_VocAsset::getFromIndex(sourceId - 1);
		if (!buffer) { return NULL_SOUND; }

		SoundSource* source = createSoundSource(SOUND_2D, 1.0f, 0.5f, buffer);
		playSource(source, true);

		return SoundSourceID(getSourceSlot(source) + 1);
	}

	void playSound3D_oneshot(SoundSourceID sourceId, vec3_fixed pos)
	{
		if (sourceId == NULL_SOUND) { return; }

		SoundBuffer* buffer = TFE_VocAsset::getFromIndex(sourceId - 1);
		if (!buffer) { return; }

		Vec3f posFloat = { fixed16ToFloat(pos.x), fixed16ToFloat(pos.y), fixed16ToFloat(pos.z) };
		TFE_Audio::playOneShot(SOUND_3D, 1.0f, 0.5f, buffer, false, &posFloat, true);
	}

	SoundEffectID playSound3D_looping(SoundSourceID sourceId, SoundEffectID soundId, vec3_fixed pos)
	{
		if (sourceId == NULL_SOUND) { return NULL_SOUND; }

		Vec3f posFloat = { fixed16ToFloat(pos.x), fixed16ToFloat(pos.y), fixed16ToFloat(pos.z) };
		SoundSource* source = getSourceFromSlot(s32(soundId) - 1);
		if (source && isSourcePlaying(source))
		{
			setSourcePosition(source, &posFloat);
		}
		else
		{
			SoundBuffer* buffer = TFE_VocAsset::getFromIndex(sourceId - 1);
			if (!buffer) { return NULL_SOUND; }

			source = createSoundSource(SOUND_3D, 1.0f, 0.5f, buffer, &posFloat, true);
			playSource(source, true);
		}
						
		return SoundSourceID(getSourceSlot(source) + 1);
	}

	void stopSound(SoundEffectID sourceId)
	{
		freeSource(getSourceFromSlot(s32(sourceId) - 1));
	}

	SoundSourceID sound_Load(const char* sound)
	{
		return SoundSourceID(TFE_VocAsset::getIndex(sound) + 1);
	}

	void sound_pitchShift(SoundEffectID soundId, s32 shift)
	{
		// STUB
	}

	void setSoundSourceVolume(SoundSourceID soundId, s32 volume)
	{
		// STUB
	}
}  // TFE_JediSound