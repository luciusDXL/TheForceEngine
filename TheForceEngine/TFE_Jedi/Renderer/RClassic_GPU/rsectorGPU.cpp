#include <cstring>

#include <TFE_System/profiler.h>
#include <TFE_Asset/modelAsset_jedi.h>
#include <TFE_Game/igame.h>
#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>

#include "rclassicGPU.h"
#include "rsectorGPU.h"
#include "../rcommon.h"

#define PTR_OFFSET(ptr, base) size_t((u8*)ptr - (u8*)base)

namespace TFE_Jedi
{
	void TFE_Sectors_GPU::reset()
	{
	}

	void TFE_Sectors_GPU::prepare()
	{
	}

	void TFE_Sectors_GPU::draw(RSector* sector)
	{
	}

	void TFE_Sectors_GPU::subrendererChanged()
	{
	}
}