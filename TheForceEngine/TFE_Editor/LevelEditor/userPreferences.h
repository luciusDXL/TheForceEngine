#pragma once
#include <TFE_System/types.h>
namespace LevelEditor
{
	enum PrefInterfaceFlags : u32
	{
		PIF_NONE = 0,
		PIF_HIDE_NOTES = FLAG_BIT(0),
		PIF_HIDE_GUIDELINES = FLAG_BIT(1),
	};

	bool userPreferences();
}