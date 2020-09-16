#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine UI Library
// This library handles the Force Engine UI using imGUI.
// This allows for debugging, performance analysis, game setup and
// launching.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <vector>
#include <string>

typedef std::vector<std::string> FileResult;

namespace TFE_Ui
{
	bool init(void* window, void* context, s32 uiScale = 100);
	void shutdown();

	void setUiInput(const void* inputEvent);
	void begin();
	void render();

	void setUiScale(s32 scale);
	s32  getUiScale();

	// File dialogs use OS based dialogs in order to provide all of the features that users expect.
	// These do NOT use imGUI so they may clash visually with the rest of the UI but that is a small price to pay for robustness.
	FileResult  openFileDialog(const char* title, const char* initPath, const char** filters = nullptr, bool multiSelect = false);
	FileResult  directorySelectDialog(const char* title, const char* initPath, bool forceInitPath = false);
	const char* saveFileDialog(const char* title, const char* initPath, const char** filters = nullptr, bool forceOverwrite = false);
}
