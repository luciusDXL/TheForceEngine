#include "levelEditScript_system.h"
#include <TFE_System/system.h>
#include <TFE_Editor/LevelEditor/infoPanel.h>
#include <TFE_Editor/LevelEditor/sharedState.h>

namespace LevelEditor
{
	s32 level_getSectorCount(void)
	{
		return (s32)s_level.sectors.size();
	}

	s32 level_getObjectCount(void)
	{
		// TODO
		return 0;
	}
}