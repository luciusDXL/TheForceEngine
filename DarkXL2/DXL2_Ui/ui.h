#pragma once
//////////////////////////////////////////////////////////////////////
// DarkXL 2 UI Library
// This library handles DarkXL 2 UI using imGUI.
// This allows for debugging, performance analysis, game setup and
// launching.
//////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>

namespace DXL2_Ui
{
	bool init(void* window, void* context, s32 uiScale = 100);
	void shutdown();

	void setUiInput(const void* inputEvent);
	void begin();
	void render();

	void setUiScale(s32 scale);
	s32  getUiScale();
}
