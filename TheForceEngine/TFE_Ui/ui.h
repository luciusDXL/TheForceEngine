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

#include "imGUI/imgui.h"
#include "imGUI/imgui_internal.h"
#include "imGUI/imgui_tfe_widgets.h"

#if _WIN32
#define DEFAULT_PATH "C:\\"
#else
#define DEFAULT_PATH "/tmp"
#endif

typedef std::vector<std::string> FileResult;

namespace TFE_Ui
{

	bool init(void* window, void* context, s32 uiScale = 100);
	void shutdown();

	void setUiInput(const void* inputEvent);
	void begin();
	void render();
	// Whether we are currently inside an Imgui frame and can safely use the
	// Imgui API. If we try to create UI when this function is returning false,
	// Imgui will throw an assertion error.
	bool isGuiFrameActive();

	void setUiScale(s32 scale);
	s32  getUiScale();

	void invalidateFontAtlas();

	// File dialogs use OS based dialogs in order to provide all of the features that users expect.
	// These do NOT use imGUI so they may clash visually with the rest of the UI but that is a small price to pay for robustness.
	FileResult openFileDialog(const char* title, const char* initPath, std::vector<std::string> const &filters = { "All Files", "*" }, bool multiSelect = false);
	FileResult directorySelectDialog(const char* title, const char* initPath, bool forceInitPath = false);
	FileResult saveFileDialog(const char* title, const char* initPath, std::vector<std::string> filters = { "All Files", "*" }, bool forceOverwrite = false);
}