#pragma once
#include <TFE_System/types.h>
namespace LevelEditor
{
	typedef void(*SwitchTabCallback)(void);

	s32 handleTabs(s32 curTab, s32 offsetX, s32 offsetY, s32 count, const char** names, const char** tooltips, SwitchTabCallback switchCallback = nullptr);
}