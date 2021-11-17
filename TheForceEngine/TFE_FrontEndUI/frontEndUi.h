#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>

enum AppState
{
	APP_STATE_MENU = 0,
	APP_STATE_EDITOR,
	APP_STATE_GAME,
	APP_STATE_QUIT,
	APP_STATE_NO_GAME_DATA,
	APP_STATE_CANNOT_RUN,
	APP_STATE_EXIT_TO_MENU,
	APP_STATE_COUNT,
	APP_STATE_UNINIT = APP_STATE_COUNT
};

namespace TFE_FrontEndUI
{
	void init();
	void initConsole();
	void shutdown();

	AppState update();
	void draw(bool drawFrontEnd = true, bool noGameData = false);

	void setAppState(AppState state);
	void enableConfigMenu();
	AppState menuReturn();
	void setMenuReturnState(AppState state);
	bool toggleConsole();

	bool shouldClearScreen();

	bool isConfigMenuOpen();
	bool isConsoleOpen();
	void logToConsole(const char* str);
	bool uiControlsEnabled();

	char* getSelectedMod();

	void toggleProfilerView();
}
