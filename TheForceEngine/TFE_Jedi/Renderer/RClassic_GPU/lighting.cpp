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

#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderBackend/indexBuffer.h>
#include <TFE_RenderBackend/shader.h>
#include <TFE_RenderBackend/shaderBuffer.h>

#include "lighting.h"
#include "rclassicGPU.h"
#include "rsectorGPU.h"
#include "../rcommon.h"

#define PTR_OFFSET(ptr, base) size_t((u8*)ptr - (u8*)base)
using namespace TFE_RenderBackend;

namespace TFE_Jedi
{
	enum
	{
		MaxLightCount = 8192,
	};

	static ShaderBuffer s_lightPos;
	static ShaderBuffer s_lightData;
	static Vec4f*  s_lightPosCpu = nullptr;
	static Vec4ui* s_lightDataCpu = nullptr;
	static bool s_enable = false;
	static u32  s_dynlightCount = 0;

	u32 packColor(Vec3f color);
	u32 packFixed10_6(Vec2f v2);
	u32 packFixed4_12(Vec2f v2);

	void lighting_enable(bool enable)
	{
		if (s_enable == enable) { return; }

		s_enable = enable;
		if (s_enable)
		{
			const ShaderBufferDef lightPosDef  = { 4, sizeof(f32), BUF_CHANNEL_FLOAT };
			const ShaderBufferDef lightDataDef = { 4, sizeof(u32), BUF_CHANNEL_UINT };
			s_lightPos.create( MaxLightCount, lightPosDef,  true);
			s_lightData.create(MaxLightCount, lightDataDef, true);

			s_lightPosCpu = (Vec4f*)malloc(sizeof(Vec4f) * MaxLightCount);
			s_lightDataCpu = (Vec4ui*)malloc(sizeof(Vec4ui) * MaxLightCount);

			memset(s_lightPosCpu, 0, sizeof(Vec4f) * MaxLightCount);
			memset(s_lightDataCpu, 0, sizeof(Vec4ui) * MaxLightCount);
		}
		else
		{
			lighting_destroy();
		}
	}

	void lighting_destroy()
	{
		s_lightPos.destroy();
		s_lightData.destroy();
		free(s_lightPosCpu);
		free(s_lightDataCpu);
		s_lightPosCpu = nullptr;
		s_lightDataCpu = nullptr;
		s_enable = false;
	}

	void lighting_clear()
	{
		if (!s_enable) { return; }
		s_dynlightCount = 0;
	}

	void lighting_add(const Light& light)
	{
		if (s_dynlightCount >= MaxLightCount) { return; }

		const u32 index = s_dynlightCount;
		s_dynlightCount++;

		s_lightPosCpu[index] = { light.pos.x, light.pos.y, light.pos.z, 0.0f };
		s_lightDataCpu[index].x = packColor(light.color[0]);
		s_lightDataCpu[index].y = packColor(light.color[1]);
		s_lightDataCpu[index].z = packFixed10_6(light.radii);
		s_lightDataCpu[index].w = packFixed4_12({ light.decay, light.amp });
	}

	void lighting_submit()
	{
		if (s_dynlightCount)
		{
			s_lightPos.update( s_lightPosCpu,  sizeof(Vec4f)  * s_dynlightCount);
			s_lightData.update(s_lightDataCpu, sizeof(Vec4ui) * s_dynlightCount);
		}
	}

	void lighting_bind(s32 index)
	{
		s_lightPos.bind(index);
		s_lightData.bind(index + 1);
	}

	void lighting_unbind(s32 index)
	{
		s_lightPos.unbind(index);
		s_lightData.unbind(index+1);
	}
		
	u32 packColor(Vec3f color)
	{
		const f32 scale = 255.0f;
		const u32 r = u32(clamp(color.x * scale, 0.0f, 255.0f));
		const u32 g = u32(clamp(color.y * scale, 0.0f, 255.0f)) << 8u;
		const u32 b = u32(clamp(color.z * scale, 0.0f, 255.0f)) << 16u;
		return r | g | b;
	}

	u32 packFixed10_6(Vec2f v2)
	{
		const f32 scale = 63.0f;
		const u32 x = u32(clamp(v2.x * scale, 0.0f, 65535.0f));
		const u32 y = u32(clamp(v2.z * scale, 0.0f, 65535.0f)) << 16u;
		return x | y;
	}

	u32 packFixed4_12(Vec2f v2)
	{
		const f32 scale = 4095.0f;
		const u32 x = u32(clamp(v2.x * scale, 0.0f, 65535.0f));
		const u32 y = u32(clamp(v2.z * scale, 0.0f, 65535.0f)) << 16u;
		return x | y;
	}
}