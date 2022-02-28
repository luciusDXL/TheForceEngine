#include "imuse.h"
#include <TFE_Audio/midi.h>

namespace TFE_Jedi
{
	/////////////////////////////////////////////////////////// 
	// Main API
	// -----------------------------------------------------
	// This includes both high level functions and low level
	// Note:
	// The original DOS code wrapped calls to an internal
	// dispatch function which is removed for TFE, calling
	// the functions directly instead.
	/////////////////////////////////////////////////////////// 

	////////////////////////////////////////////////////
	// High level functions
	////////////////////////////////////////////////////
	s32 ImSetMasterVol(s32 vol)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImGetMasterVol(void)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImSetMusicVol(s32 vol)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImGetMusicVol(void)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImSetSfxVol(s32 vol)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImGetSfxVol(void)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImSetVoiceVol(s32 vol)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImGetVoiceVol(void)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImStartSfx(s32 sound, s32 priority)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImStartVoice(s32 sound, s32 priority)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImStartMusic(s32 sound, s32 priority)
	{
		// Stub
		return imNotImplemented;
	}

	////////////////////////////////////////////////////
	// Low level functions
	////////////////////////////////////////////////////
	s32 ImInitialize()
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImTerminate(void)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImPause(void)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImResume(void)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImSetGroupVol(s32 group, s32 vol)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImStartSound(s32 sound, s32 priority)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImStopSound(s32 sound)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImStopAllSounds(void)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImGetNextSound(s32 sound)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImSetParam(s32 sound, s32 param, s32 val)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImGetParam(s32 sound, s32 param)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImFadeParam(s32 sound, s32 param, s32 val, s32 time)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImSetHook(s32 sound, s32 val)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImGetHook(s32 sound)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImSetTrigger(s32 sound, s32 marker, s32 opcode)  // Modified based on actual usage to simplify
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImCheckTrigger(s32 sound, s32 marker, s32 opcode)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImClearTrigger(s32 sound, s32 marker, s32 opcode)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImDeferCommand(s32 time, s32 opcode, s32 sound)  // Modified based on actual usage to simplify
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImJumpMidi(s32 sound, s32 chunk, s32 measure, s32 beat, s32 tick, s32 sustain)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImSendMidiMsg(s32 sound, s32 arg1, s32 arg2, s32 arg3)
	{
		// Stub
		return imNotImplemented;
	}

	s32 ImShareParts(s32 sound1, s32 sound2)
	{
		// Stub
		return imNotImplemented;
	}

	/////////////////////////////////////////////////////////// 
	// Internal Implementation
	/////////////////////////////////////////////////////////// 
}
