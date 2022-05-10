#include "imuse.h"
#include "imDigitalSound.h"
#include <TFE_System/system.h>
#include <TFE_Audio/midi.h>
#include <assert.h>

namespace TFE_Jedi
{
	////////////////////////////////////////////////////
	// Structures
	////////////////////////////////////////////////////
	struct ImWaveSound
	{
		s32 u00;
		ImWaveSound* next;
		u32 waveStreamFlag;
		iptr soundId;
		s32 marker;
		s32 group;
		s32 priority;
		s32 baseVolume;
		s32 volume;
		s32 pan;
		s32 detune;
		s32 transpose;
		s32 detuneTrans;
		s32 mailbox;
	};

	struct ImWaveData;

	struct ImWavePlayer
	{
		ImWavePlayer* prev;
		ImWavePlayer* next;
		ImWaveData*   data;
		iptr soundId;
	};

	struct ImWaveData
	{
		ImWavePlayer* player;
		s32 offset;
		s32 chunkSize;
		s32 u0c;

		s32 chunkIndex;
		s32 u14;
		s32 u18;
		s32 u1c;

		s32 u20;
		s32 u24;
		s32 u28;
		s32 u2c;

		s32 u30;
	};
	
	/////////////////////////////////////////////////////
	// Internal State
	/////////////////////////////////////////////////////
	static ImWaveSound* s_imWaveSounds;
	static ImWaveSound  s_imWaveSoundStore[16];
	static u8  s_imWaveChunkData[48];
	static s32 s_imWaveMixCount = 8;

	/////////////////////////////////////////////////////////// 
	// API
	/////////////////////////////////////////////////////////// 
	

	////////////////////////////////////
	// Internal
	////////////////////////////////////
	

}  // namespace TFE_Jedi
