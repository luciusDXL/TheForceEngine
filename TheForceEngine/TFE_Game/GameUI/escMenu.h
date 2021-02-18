#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Game Level
// Manages the game while playing levels in first person.
// This holds one or more players, handles input updates, etc.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Asset/levelAsset.h>

namespace TFE_EscapeMenu
{
	bool load();
	void unload();
	void open(TFE_Renderer* renderer);
	void close();
	void draw(TFE_Renderer* renderer, s32 offset, s32 scaleX, s32 scaleY, s32 buttonPressed, bool buttonHover, bool nextMission);
	GameUiResult update(const Vec2i* cursor, bool nextMission, s32* buttonPressed, bool* buttonHover);
}
