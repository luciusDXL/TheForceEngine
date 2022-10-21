#pragma once
//////////////////////////////////////////////////////////////////////
// Display list for rendering sector geometry
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/memoryPool.h>
#include "sectorDisplayList.h"

namespace TFE_Jedi
{
	void objectPortalPlanes_init();
	void objectPortalPlanes_destroy();

	void objectPortalPlanes_clear();
	void objectPortalPlanes_finish();

	void objectPortalPlanes_bind(s32 index);
	void objectPortalPlanes_unbind(s32 index);

	u32  objectPortalPlanes_add(u32 count, const Vec4f* planes);
}  // TFE_Jedi
