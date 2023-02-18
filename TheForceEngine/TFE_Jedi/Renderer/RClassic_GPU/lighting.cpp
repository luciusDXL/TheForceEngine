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
	extern Vec3f s_cameraPos;

	enum
	{
		MaxLightCount = 8192,
		MaxClusterCount = 1024,
	};

	struct SectorLights
	{
		u32 count;
		u16 lightId[256];
	};
	static SectorLights* s_sectorLights = nullptr;
	static s32 s_sectorCount = 0;

	static ShaderBuffer s_lightPos;
	static ShaderBuffer s_lightData;
	static ShaderBuffer s_clusters;
	static Vec4f*  s_lightPosCpu = nullptr;
	static Vec4ui* s_lightDataCpu = nullptr;
	static Vec4ui* s_clustersCpu = nullptr;
	static bool s_enable = false;
	static u32  s_dynlightCount = 0;
	static u32  s_maxClusterId = 0;

	u32 packColor(Vec3f color);
	u32 packFixed10_6(Vec2f v2);
	u32 packFixed4_12(Vec2f v2);

	void lighting_enable(bool enable, s32 sectorCount)
	{
		if (enable)
		{
			if (!s_enable)
			{
				const ShaderBufferDef lightPosDef  = { 4, sizeof(f32), BUF_CHANNEL_FLOAT };
				const ShaderBufferDef lightDataDef = { 4, sizeof(u32), BUF_CHANNEL_UINT };
				s_lightPos.create( MaxLightCount, lightPosDef,  true);
				s_lightData.create(MaxLightCount, lightDataDef, true);
				s_clusters.create(MaxClusterCount, lightDataDef, true);

				s_lightPosCpu = (Vec4f*)malloc(sizeof(Vec4f) * MaxLightCount);
				s_lightDataCpu = (Vec4ui*)malloc(sizeof(Vec4ui) * MaxLightCount);
				s_clustersCpu = (Vec4ui*)malloc(sizeof(Vec4ui) * MaxClusterCount);

				memset(s_lightPosCpu, 0, sizeof(Vec4f) * MaxLightCount);
				memset(s_lightDataCpu, 0, sizeof(Vec4ui) * MaxLightCount);
				memset(s_clustersCpu, 0, sizeof(Vec4ui) * MaxClusterCount);
			}
			if (sectorCount != s_sectorCount)
			{
				s_sectorLights = (SectorLights*)game_realloc(s_sectorLights, sizeof(SectorLights) * sectorCount);
				memset(s_sectorLights, 0, sizeof(SectorLights) * sectorCount);
				s_sectorCount = sectorCount;
			}
		}
		else
		{
			lighting_destroy();
		}
		s_enable = enable;
	}

	void lighting_destroy()
	{
		s_lightPos.destroy();
		s_lightData.destroy();
		s_clusters.destroy();
		free(s_lightPosCpu);
		free(s_lightDataCpu);
		free(s_clustersCpu);
		s_lightPosCpu = nullptr;
		s_lightDataCpu = nullptr;
		s_clustersCpu = nullptr;
		s_enable = false;
	}

	void lighting_clear()
	{
		if (!s_enable) { return; }
		s_dynlightCount = 0;
		for (s32 i = 0; i < s_sectorCount; i++)
		{
			s_sectorLights[i].count = 0;
		}

		s_maxClusterId = 0;
		for (s32 i = 0; i < MaxClusterCount; i++)
		{
			s_clustersCpu[i] = { 0 };
		}
	}

	s32 getClusterId(Vec3f posWS, s32& mipIndex, f32& scale)
	{
		Vec2f offset = { fabsf(posWS.x - s_cameraPos.x), fabsf(posWS.z - s_cameraPos.z) };
		f32 d     = max(offset.x, offset.z) / 16.0f;
		f32 mip   = log2f(max(1.0f, d));
		scale = powf(2.0f, floorf(mip) + 3.0f);

		// This is the minimum corner for the current mip-level.
		f32 clusterOffset = scale * 4.0f;
		// This generates a clusterAddr such that x : [0, 7] and z : [0, 7]
		Vec2i clusterAddr = { (posWS.x - s_cameraPos.x + clusterOffset) / scale, (posWS.z - s_cameraPos.z + clusterOffset) / scale };
		// This generates a final ID ranging from [0, 63]
		s32 clusterId = clusterAddr.x + clusterAddr.z*8;
		
		mipIndex = s32(mip);
		if (mipIndex >= 1)
		{
			// In larger mips, the center 4x4 is the previous mip.
			// So those Ids need to be adjusted.
			if (clusterAddr.z >= 6)
			{
				clusterId = 32 + (clusterAddr.z - 6) * 8 + clusterAddr.x;
			}
			else if (clusterAddr.z >= 2)
			{
				clusterId = 16 + (clusterAddr.z - 2) * 4 + ((clusterAddr.x < 2) ? clusterAddr.x : clusterAddr.x - 4);
			}
			// Finally linearlize the ID factoring in the current mip.
			clusterId += 64 + (mipIndex - 1) * 48;
		}
		return clusterId;
	}

	void addLightToCluster(s32 clusterId, u16 lightId)
	{
		assert(clusterId < MaxClusterCount);

		s_maxClusterId = max(s_maxClusterId, (u32)clusterId);
		lightId++;	// 0 = no light.
		// Look for a free slot to stick it.
		bool freeSlotFound = false;
		for (s32 i = 0; i < 4; i++)
		{
			if ((s_clustersCpu[clusterId].m[i] & 0xffff) == 0u)
			{
				s_clustersCpu[clusterId].m[i] = lightId;
				freeSlotFound = true;
				break;
			}
			else if (((s_clustersCpu[clusterId].m[i] >> 16u) & 0xffff) == 0u)
			{
				s_clustersCpu[clusterId].m[i] |= (lightId << 16u);
				freeSlotFound = true;
				break;
			}
		}
		//assert(freeSlotFound);
	}

	void processMip(s32 mipIndex, f32 scale, Vec3f posWS, s32 lightId, const Light& light)
	{
		f32 eps = 0.0001f;
		f32 w = eps + light.radii.z;
		Vec2f mn = { light.pos.x - w, light.pos.z - w };
		Vec2f mx = { light.pos.x + w, light.pos.z + w };

		// This generates a clusterAddr such that x : [0, 7] and z : [0, 7]
		f32 clusterOffset = scale * 4.0f;
		Vec2i clusterAddr0 = { floorf((mn.x - s_cameraPos.x + clusterOffset) / scale), floorf((mn.z - s_cameraPos.z + clusterOffset) / scale) };
		Vec2i clusterAddr1 = { ceilf((mx.x - s_cameraPos.x + clusterOffset) / scale), ceilf((mx.z - s_cameraPos.z + clusterOffset) / scale) };

		bool processPrevMip = false;
		s32 baseIndex = ((mipIndex > 0) ? 64 : 0) + max(0, mipIndex - 1) * 48;
		for (s32 z = max(0, clusterAddr0.z); z <= min(7, clusterAddr1.z); z++)
		{
			for (s32 x = max(0, clusterAddr0.x); x <= min(7, clusterAddr1.x); x++)
			{
				s32 clusterId = z * 8 + x;
				if (mipIndex > 0 && (z >= 2 && z < 6 && x >= 2 && x < 6))
				{
					processPrevMip = true;
					continue;
				}
				else if (mipIndex > 0)
				{
					if (z >= 6)
					{
						clusterId = 32 + (z - 6) * 8 + x;
					}
					else if (z >= 2)
					{
						clusterId = 16 + (z - 2) * 4 + ((x < 2) ? x : x - 4);
					}
				}
				clusterId += baseIndex;
				addLightToCluster(clusterId, lightId);
			}
		}

		if (processPrevMip)
		{
			processMip(mipIndex - 1, scale * 0.5f, posWS, lightId, light);
		}
	}

	void lighting_add(const Light& light, s32 objIndex, s32 sectorIndex)
	{
		if (s_dynlightCount >= MaxLightCount) { return; }

		// Make sure the light hasn't been added already.
		SectorLights* lights = &s_sectorLights[sectorIndex];
		for (s32 i = 0; i < lights->count; i++)
		{
			if (lights->lightId[i] == objIndex) { return; }
		}
		// Then add it.
		lights->lightId[lights->count++] = objIndex;

		// Add the light to the overall list.
		const u32 index = s_dynlightCount;
		s_dynlightCount++;

		s_lightPosCpu[index] = { light.pos.x, light.pos.y, light.pos.z, 0.0f };
		s_lightDataCpu[index].x = packColor(light.color[0]);
		s_lightDataCpu[index].y = packColor(light.color[1]);
		s_lightDataCpu[index].z = packFixed10_6(light.radii);
		s_lightDataCpu[index].w = packFixed4_12({ light.decay, light.amp });

		// Then add the light to the relevant clusters.
		// For now, add to the center cluster only (this will have artifacts).
		s32 mipIndex;
		f32 scale;
		s32 centerCluster = getClusterId(light.pos, mipIndex, scale);

		// First check to see if we have the correct scale.
		f32 eps = 0.0001f;
		f32 w = eps + light.radii.z;
		Vec2f mn = { light.pos.x - w, light.pos.z - w };
		Vec2f mx = { light.pos.x + w, light.pos.z + w };

		f32 clusterOffset = scale * 4.0f;
		Vec2i clusterAddr0 = { floorf((mn.x - s_cameraPos.x + clusterOffset) / scale), floorf((mn.z - s_cameraPos.z + clusterOffset) / scale) };
		Vec2i clusterAddr1 = { ceilf((mx.x - s_cameraPos.x + clusterOffset) / scale), ceilf((mx.z - s_cameraPos.z + clusterOffset) / scale) };

		// If not, then move up a mip level.
		// We assume that lights are small enough that they can only straddle one mip level.
		if (clusterAddr0.x < 0 || clusterAddr0.z < 0 || clusterAddr1.x > 7 || clusterAddr1.z > 7)
		{
			mipIndex++;
			scale *= 2.0f;
		}
		processMip(mipIndex, scale, light.pos, index, light);
	}

	void lighting_submit()
	{
		// HACK! For now clear out any extra lights.
		s32 start = s_dynlightCount;
		for (s32 i = start; i < 8; i++)
		{
			s_lightPosCpu[i] = { 0 };
			s_lightDataCpu[i] = { 0 };
			s_dynlightCount++;
		}

		if (s_dynlightCount)
		{
			s_lightPos.update( s_lightPosCpu,  sizeof(Vec4f)  * s_dynlightCount);
			s_lightData.update(s_lightDataCpu, sizeof(Vec4ui) * s_dynlightCount);
		}

		// Clusters
		s_clusters.update(s_clustersCpu, sizeof(Vec4ui) * (s_maxClusterId + 1));
	}

	void lighting_bind(s32 index)
	{
		s_lightPos.bind(index);
		s_lightData.bind(index + 1);
		s_clusters.bind(index + 2);
	}

	void lighting_unbind(s32 index)
	{
		s_lightPos.unbind(index);
		s_lightData.unbind(index+1);
		s_clusters.unbind(index+2);
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