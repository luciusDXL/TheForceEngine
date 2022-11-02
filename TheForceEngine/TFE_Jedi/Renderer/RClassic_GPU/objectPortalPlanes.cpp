#include <cstring>

#include <TFE_System/profiler.h>
#include <TFE_System/math.h>
#include <TFE_Asset/modelAsset_jedi.h>
#include <TFE_Game/igame.h>
#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>

#include <TFE_Input/input.h>

#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderBackend/indexBuffer.h>
#include <TFE_RenderBackend/shader.h>
#include <TFE_RenderBackend/shaderBuffer.h>

#include "objectPortalPlanes.h"
#include "frustum.h"
#include "../rcommon.h"

using namespace TFE_RenderBackend;

namespace TFE_Jedi
{
	static u32 s_objectPlaneCount = 0;
	static u32* s_objectPlaneInfo = nullptr;
	static Vec4f* s_objectPlanes = nullptr;
	static ShaderBuffer s_objectPlanesGPU;
		
	void objectPortalPlanes_init()
	{
		s_objectPlaneInfo = (u32*)malloc(sizeof(u32)*MAX_DISP_ITEMS);
		s_objectPlanes = (Vec4f*)malloc(sizeof(Vec4f)*MAX_BUFFER_SIZE);

		const ShaderBufferDef bufferDefDisplayListPlanes = { 4, sizeof(f32), BUF_CHANNEL_FLOAT };
		s_objectPlanesGPU.create(MAX_BUFFER_SIZE, bufferDefDisplayListPlanes, true);
	}

	void objectPortalPlanes_destroy()
	{
		free(s_objectPlaneInfo);
		free(s_objectPlanes);
		s_objectPlaneInfo = nullptr;
		s_objectPlanes = nullptr;

		s_objectPlaneCount = 0;
		s_objectPlanesGPU.destroy();
	}

	void objectPortalPlanes_clear()
	{
		s_objectPlaneCount = 0;
	}

	void objectPortalPlanes_finish()
	{
		if (s_objectPlaneCount)
		{
			s_objectPlanesGPU.update(s_objectPlanes, sizeof(Vec4f) * s_objectPlaneCount);
		}
	}
		
	void objectPortalPlanes_bind(s32 index)
	{
		s_objectPlanesGPU.bind(index);
	}

	void objectPortalPlanes_unbind(s32 index)
	{
		s_objectPlanesGPU.unbind(index);
	}

	u32 objectPortalPlanes_add(u32 count, const Vec4f* planes)
	{
		if (count < 1 || s_objectPlaneCount >= MAX_BUFFER_SIZE) { return 0; }

		const u32 planeInfo = PACK_PORTAL_INFO(s_objectPlaneCount, count);
		memcpy(&s_objectPlanes[s_objectPlaneCount], planes, sizeof(Vec4f) * count);
		s_objectPlaneCount += count;
		return planeInfo;
	}
}  // TFE_Jedi