#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "levelEditorData.h"
#include "selection.h"
#include "sharedState.h"

namespace LevelEditor
{
	void handleMouseControlSurface(Vec2f worldPos);
	void edit_autoAlign(s32 sectorId, s32 wallIndex, HitPart part);
	void edit_applySurfaceTextures();
	void edit_clearCopiedTextureOffset();
	void edit_applyTextureToSelection(s32 texIndex, Vec2f* offset);
	void edit_applySignToSelection(s32 signIndex);

	// Transforms
	
}
