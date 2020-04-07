#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>
#include <DXL2_Renderer/renderer.h>

enum AppState
{
	APP_STATE_MENU = 0,
	APP_STATE_EDITOR,
	APP_STATE_DARK_FORCES,
	APP_STATE_QUIT,
	APP_STATE_COUNT,
	APP_STATE_UNINIT = APP_STATE_COUNT
};

namespace DXL2_FrontEndUI
{
	void init();
	void shutdown();

	AppState update();
	void draw();

	void setAppState(AppState state);
}
