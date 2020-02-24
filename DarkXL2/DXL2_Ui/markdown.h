#pragma once
//////////////////////////////////////////////////////////////////////
// DarkXL 2 UI Library
// This library handles DarkXL 2 UI using imGUI.
// This allows for debugging, performance analysis, game setup and
// launching.
//////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>

typedef void(*LocalLinkCallback)(const char* link);

namespace DXL2_Markdown
{
	bool init(f32 baseFontSize);
	void shutdown();

	void draw(const char* text);
	void registerLocalLinkCB(LocalLinkCallback cb);
}
