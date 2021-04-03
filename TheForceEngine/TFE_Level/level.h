#pragma once
//////////////////////////////////////////////////////////////////////
// Sector
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/memoryPool.h>
#include "core_math.h"
#include "fixedPoint.h"
#include "rsector.h"

namespace TFE_Level
{
	void initMission();
	bool loadGeometry(const char* levelName);
	bool loadObjects(const char* levelName);

	extern u32 s_sectorCount;
	extern RSector* s_sectors;
}
