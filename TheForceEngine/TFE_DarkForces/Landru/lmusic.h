#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Landru Sound - the Landru system manages sounds differently than
// the main game.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Asset/vocAsset.h>
#include <TFE_Audio/audioSystem.h>
#include "lsystem.h"

namespace TFE_DarkForces
{
	void lmusic_init();
	void lmusic_destroy();

	s32 lmusic_startCutscene(s32 newSeq);
	s32 lmusic_setCuePoint(s32 newCuePoint);
	void lmusic_stop();
	void lmusic_reset();
}  // namespace TFE_DarkForces