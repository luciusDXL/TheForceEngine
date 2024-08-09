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

	void setUiScale(s32 scale);
	s32  getUiScale();

	void invalidateFontAtlas();

	void openFileDialog(const char* title, const char* initPath, const char* filters = ".*", bool multiSelect = false);
	void saveFileDialog(const char* title, const char* initPath, const char* filters = ".*", bool forceOverwrite = false);
	void directorySelectDialog(const char* title, const char* initPath, bool forceInitPath = false);
	bool renderFileDialog(FileResult& inOutPath);
}
