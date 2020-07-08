#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Game Level
// Manages the game while playing levels in first person.
// This holds one or more players, handles input updates, etc.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Asset/levelAsset.h>
#include "gameState.h"
#include "view.h"

// Control bindings.
enum GameControlType
{
	GCTRL_KEY = 0,
	GCTRL_CONTROLLER_BUTTON,
	GCTRL_MOUSE_BUTTON,
	GCTRL_CONTROLLER_AXIS,
	GCTRL_MOUSE_AXIS_X,
	GCTRL_MOUSE_AXIS_Y,
	GCTRL_MOUSEWHEEL_X,
	GCTRL_MOUSEWHEEL_Y,
	GCTRL_COUNT
};

enum GameControlTrigger
{
	GTRIGGER_PRESSED = 0,	// Trigger when pressed.
	GTRIGGER_DOWN,			// Trigger when held down.
	GTRIGGER_UPDATE,		// Trigger when updated (useful for analog controls).
};

namespace TFE_GameControlMapping
{
	void clearActions();

	f32  getAction(u32 id);
	void bindAction(u32 id, GameControlType type, GameControlTrigger trigger, u32 primaryId, u32 secondaryId = 0, f32 scale = 0.0f);
}
