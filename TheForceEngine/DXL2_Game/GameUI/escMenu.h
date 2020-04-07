#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Game Level
// Manages the game while playing levels in first person.
// This holds one or more players, handles input updates, etc.
//////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>
#include <DXL2_Asset/levelAsset.h>
#include <DXL2_Game/view.h>

namespace DXL2_EscapeMenu
{
	bool load();
	void unload();
	void open(DXL2_Renderer* renderer);
	void close();
	void draw(DXL2_Renderer* renderer, s32 scaleX, s32 scaleY, s32 buttonPressed, bool buttonHover, bool nextMission);
	GameUiResult update(const Vec2i* cursor, bool nextMission, s32* buttonPressed, bool* buttonHover);
}
