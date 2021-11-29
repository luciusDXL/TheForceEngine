#include <cstring>

#include "soundSystem.h"
#include <TFE_Asset/vocAsset.h>
#include <TFE_Audio/audioSystem.h>

using namespace TFE_Audio;

namespace TFE_Jedi
{
	enum SoundConstants
	{
		JSND_SLOT_MASK = 255,
		JSND_UID_SHIFT = 8,
		JSND_UID_MASK  = (1 << 24) - 1
	};
	#define BUILD_EFFECT_ID(slot) SoundEffectID(slot | (s_slotID[slot] << JSND_UID_SHIFT))

	static const SoundSource* s_slotMapping[MAX_SOUND_SOURCES];
	static s32 s_slotID[MAX_SOUND_SOURCES] = { 0 };

	void sound_stopAll()
	{
		stopAllSounds();
		memset(s_slotID, 0, sizeof(s32)*MAX_SOUND_SOURCES);
		memset(s_slotMapping, 0, sizeof(SoundSource*)*MAX_SOUND_SOURCES);
	}

	void sound_freeAll()
	{
		sound_stopAll();
		TFE_VocAsset::freeAll();
	}
	
	SoundEffectID playSound2D(SoundSourceID sourceId)
	{
		if (sourceId == NULL_SOUND) { return NULL_SOUND; }
		SoundBuffer* buffer = TFE_VocAsset::getFromIndex(sourceId - 1);
		if (!buffer) { return NULL_SOUND; }

		SoundSource* source = createSoundSource(SOUND_2D, 1.0f, 0.5f, buffer);
		if (!source)
		{
			return NULL_SOUND;
		}
		playSource(source);

		s32 slot = getSourceSlot(source);
		assert(slot >= 0);

		s_slotMapping[slot] = source;
		s_slotID[slot] = (s_slotID[slot] + 1) & JSND_UID_MASK;
		if (s_slotID[slot] == 0) { s_slotID[slot]++; }
		
		return BUILD_EFFECT_ID(slot);
	}

	SoundEffectID playSound2D_looping(SoundSourceID sourceId)
	{
		if (sourceId == NULL_SOUND) { return NULL_SOUND; }
		SoundBuffer* buffer = TFE_VocAsset::getFromIndex(sourceId - 1);
		if (!buffer) { return NULL_SOUND; }

		SoundSource* source = createSoundSource(SOUND_2D, 1.0f, 0.5f, buffer);
		if (!source) { return NULL_SOUND; }

		playSource(source, true);
		s32 slot = getSourceSlot(source);
		assert(slot >= 0);

		s_slotMapping[slot] = source;
		s_slotID[slot] = (s_slotID[slot] + 1) & JSND_UID_MASK;
		if (s_slotID[slot] == 0) { s_slotID[slot]++; }

		return BUILD_EFFECT_ID(slot);
	}

	SoundEffectID playSound3D_oneshot(SoundSourceID sourceId, vec3_fixed pos)
	{
		if (sourceId == NULL_SOUND) { return NULL_SOUND; }

		SoundBuffer* buffer = TFE_VocAsset::getFromIndex(sourceId - 1);
		if (!buffer) { return NULL_SOUND; }

		Vec3f posFloat = { fixed16ToFloat(pos.x), fixed16ToFloat(pos.y), fixed16ToFloat(pos.z) };
		SoundSource* source = createSoundSource(SOUND_3D, 1.0f, 0.5f, buffer, &posFloat, true);
		if (!source) { return NULL_SOUND; }
		playSource(source);

		s32 slot = getSourceSlot(source);
		assert(slot >= 0);

		s_slotMapping[slot] = source;
		s_slotID[slot] = (s_slotID[slot] + 1) & JSND_UID_MASK;
		if (s_slotID[slot] == 0) { s_slotID[slot]++; }

		return BUILD_EFFECT_ID(slot);
	}

	SoundEffectID playSound3D_looping(SoundSourceID sourceId, SoundEffectID soundId, vec3_fixed pos)
	{
		if (sourceId == NULL_SOUND) { return NULL_SOUND; }

		Vec3f posFloat = { fixed16ToFloat(pos.x), fixed16ToFloat(pos.y), fixed16ToFloat(pos.z) };
		u32 slot = soundId & JSND_SLOT_MASK;
		u32 uid  = soundId >> JSND_UID_SHIFT;

		SoundSource* source = getSourceFromSlot(slot);
		if (soundId && source && source == s_slotMapping[slot] && uid == s_slotID[slot])
		{
			setSourcePosition(source, &posFloat);
			return soundId;
		}
		else
		{
			SoundBuffer* buffer = TFE_VocAsset::getFromIndex(sourceId - 1);
			if (!buffer) { return NULL_SOUND; }

			source = createSoundSource(SOUND_3D, 1.0f, 0.5f, buffer, &posFloat, true);
			playSource(source, true);
		}

		slot = getSourceSlot(source);
		assert(slot >= 0);

		s_slotMapping[slot] = source;
		s_slotID[slot] = (s_slotID[slot] + 1) & JSND_UID_MASK;
		if (s_slotID[slot] == 0) { s_slotID[slot]++; }

		return BUILD_EFFECT_ID(slot);
	}

	void stopSound(SoundEffectID sourceId)
	{
		if (sourceId == NULL_SOUND) { return; }

		u32 slot = sourceId & JSND_SLOT_MASK;
		u32 uid = sourceId >> JSND_UID_SHIFT;
		SoundSource* curSoundSource = getSourceFromSlot(slot);
		
		if (curSoundSource == s_slotMapping[slot] && uid == s_slotID[slot])
		{
			freeSource(curSoundSource);
			s_slotMapping[slot] = nullptr;
			s_slotID[slot] = 0;
		}
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