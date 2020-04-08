#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine UI Library
// This library handles the Force Engine UI using imGUI.
// This allows for debugging, performance analysis, game setup and
// launching.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>

typedef void(*LocalLinkCallback)(const char* link);

namespace TFE_Markdown
{
	bool init(f32 baseFontSize);
	void shutdown();

	void draw(const char* text);
	void registerLocalLinkCB(LocalLinkCallback cb);
}
