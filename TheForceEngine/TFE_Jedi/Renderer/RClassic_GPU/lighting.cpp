#include "lighting.h"
#include "sbuffer.h"
#include <TFE_System/math.h>
#include <TFE_System/system.h>
#include <TFE_Jedi/Level/levelData.h>
#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Jedi/Level/rwall.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Memory/chunkedArray.h>

#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderBackend/shader.h>
#include <TFE_RenderBackend/shaderBuffer.h>

#include <cstring>

using namespace TFE_Memory;
#define MAX_LIGHT_SECTORS 8

// Scene
struct SceneLight
{
	Light light;
	s32 id;
	s32 sectorCount;
	s32 sectorId[MAX_LIGHT_SECTORS];
	u32 frameIndex;
};
struct SectorBucket
{
	std::vector<SceneLight*> lights;
};

namespace TFE_Jedi
{
	extern Vec3f s_cameraPos;

	enum
	{
		MaxVisLightCount = 1024,
		ShadowWidth = 1024,
		LightPoolStep = 1024,
		LightPoolMax  = 65536,
		MaxClusterCount = 1024,
	};

	static s32 s_sectorCount = 0;

	static ShaderBuffer s_lightPos;
	static ShaderBuffer s_lightData;
	static ShaderBuffer s_clusters;
	static TextureGpu*  s_shadowBuffer = nullptr;
	static Vec4f*  s_lightPosCpu = nullptr;
	static Vec4ui* s_lightDataCpu = nullptr;
	static Vec4ui* s_clustersCpu = nullptr;
	static u16* s_shadowBufferCpu = nullptr;
	static bool s_enable = false;
	static u32  s_dynlightCount = 0;
	static u32  s_lightPoolUsed = 0;
	static u32  s_lightPoolCapacity = 0;
	static u32  s_maxClusterId = 0;
	static u32  s_frameIndex = 0;

	static bool s_shadowBufferDirty = false;
		
	std::vector<SceneLight*> s_freeSceneLights;
	std::vector<SectorBucket> s_sectorBuckets;
	SceneLight* s_lightPool = nullptr;
	//

	u32 packColor(Vec3f color);
	u32 packFixed10_6(Vec2f v2);
	u32 packFixed4_12(Vec2f v2);

	void lighting_destroy()
	{
		s_lightPos.destroy();
		s_lightData.destroy();
		s_clusters.destroy();
		TFE_RenderBackend::freeTexture(s_shadowBuffer);
		free(s_lightPosCpu);
		free(s_lightDataCpu);
		free(s_clustersCpu);
		free(s_lightPool);
		free(s_shadowBufferCpu);
		s_lightPosCpu  = nullptr;
		s_lightDataCpu = nullptr;
		s_clustersCpu  = nullptr;
		s_lightPool = nullptr;
		s_shadowBuffer = nullptr;
		s_shadowBufferCpu = nullptr;
		s_enable = false;
		s_lightPoolCapacity = 0;
	}

	void lighting_clear()
	{
		if (!s_enable) { return; }
		s_dynlightCount = 0;
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
		Vec2i clusterAddr = { s32((posWS.x - s_cameraPos.x + clusterOffset) / scale), s32((posWS.z - s_cameraPos.z + clusterOffset) / scale) };
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
		assert(freeSlotFound);
	}

	void processMip(s32 mipIndex, f32 scale, Vec3f posWS, s32 lightId, const Light& light)
	{
		f32 eps = 0.0001f;
		f32 w = eps + light.radii.z;
		Vec2f mn = { light.pos.x - w, light.pos.z - w };
		Vec2f mx = { light.pos.x + w, light.pos.z + w };

		// This generates a clusterAddr such that x : [0, 7] and z : [0, 7]
		f32 clusterOffset = scale * 4.0f;
		Vec2i clusterAddr0 = { s32(floorf((mn.x - s_cameraPos.x + clusterOffset) / scale)), s32(floorf((mn.z - s_cameraPos.z + clusterOffset) / scale)) };
		Vec2i clusterAddr1 = { s32(ceilf(( mx.x - s_cameraPos.x + clusterOffset) / scale)), s32(ceilf(( mx.z - s_cameraPos.z + clusterOffset) / scale)) };

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

	void lighting_add(const Light& light, s32 id)
	{
		if (s_dynlightCount >= MaxVisLightCount) { return; }

		// Add the light to the overall list.
		const u32 index = s_dynlightCount;
		s_dynlightCount++;

		s_lightPosCpu[index] = { light.pos.x, light.pos.y, light.pos.z, f32(id) };
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
		Vec2i clusterAddr0 = { s32(floorf((mn.x - s_cameraPos.x + clusterOffset) / scale)), s32(floorf((mn.z - s_cameraPos.z + clusterOffset) / scale)) };
		Vec2i clusterAddr1 = { s32(ceilf(( mx.x - s_cameraPos.x + clusterOffset) / scale)), s32(ceilf(( mx.z - s_cameraPos.z + clusterOffset) / scale)) };

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
		if (s_dynlightCount)
		{
			s_lightPos.update( s_lightPosCpu,  sizeof(Vec4f)  * s_dynlightCount);
			s_lightData.update(s_lightDataCpu, sizeof(Vec4ui) * s_dynlightCount);
		}

		// For now update the whole shadow buffer.
		// TODO: Proper updates.
		if (s_shadowBufferDirty)
		{
			s_shadowBuffer->update(s_shadowBufferCpu, sizeof(u16) * ShadowWidth * MaxVisLightCount);
			s_shadowBufferDirty = false;
		}

		// Clusters
		s_clusters.update(s_clustersCpu, sizeof(Vec4ui) * (s_maxClusterId + 1));
		s_frameIndex++;
	}

	void lighting_bind(s32 index)
	{
		s_lightPos.bind(index);
		s_lightData.bind(index + 1);
		s_clusters.bind(index + 2);
		s_shadowBuffer->bind(index + 3);
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

	////////////////////////////////////////
	// Scene
	////////////////////////////////////////
		
	void lighting_addToBucket(SectorBucket* bucket, SceneLight* light)
	{
		bucket->lights.push_back(light);
	}

	void lighting_removeFromBucket(SectorBucket* bucket, SceneLight* sceneLight)
	{
		std::vector<SceneLight*>::iterator iter = bucket->lights.begin();
		for (; iter != bucket->lights.end(); ++iter)
		{
			SceneLight* light = *iter;
			if (light == sceneLight)
			{
				bucket->lights.erase(iter);
				break;
			}
		}
	}

	void lighting_clearScene()
	{
		s_freeSceneLights.clear();
		s_frameIndex = 0;
		s_lightPoolUsed = 0;
	}

	void lighting_growLightPool(u32 newCount)
	{
		if (newCount <= s_lightPoolCapacity) { return; }
		s_lightPoolCapacity += LightPoolStep;
		if (s_lightPoolCapacity >= LightPoolMax)
		{
			assert(0);
			TFE_System::logWrite(LOG_ERROR, "Lighting", "Light pool is too large, requested %d, maximum %d.", newCount, LightPoolMax);
			s_lightPoolCapacity = LightPoolMax;
		}
		s_lightPool = (SceneLight*)realloc(s_lightPool, sizeof(SceneLight) * s_lightPoolCapacity);
	}
	
	void lighting_initScene(s32 sectorCount)
	{
		bool allocRequired = !s_enable;
		s_enable = true;
		lighting_clearScene();
		s_sectorBuckets.resize(sectorCount);
		for (size_t i = 0; i < sectorCount; i++)
		{
			s_sectorBuckets[i].lights.clear();
		}

		// GPU data -- allocate once, even if changing games.
		if (allocRequired)
		{
			lighting_growLightPool(LightPoolStep);

			const ShaderBufferDef lightPosDef  = { 4, sizeof(f32), BUF_CHANNEL_FLOAT };
			const ShaderBufferDef lightDataDef = { 4, sizeof(u32), BUF_CHANNEL_UINT };
			s_lightPos.create(MaxVisLightCount,   lightPosDef, true);
			s_lightData.create(MaxVisLightCount,  lightDataDef, true);
			s_clusters.create(MaxClusterCount, lightDataDef, true);

			s_lightPosCpu  = (Vec4f*)malloc(sizeof(Vec4f)   * MaxVisLightCount);
			s_lightDataCpu = (Vec4ui*)malloc(sizeof(Vec4ui) * MaxVisLightCount);
			s_clustersCpu  = (Vec4ui*)malloc(sizeof(Vec4ui) * MaxClusterCount);

			// Limited shadow map cache, regenerate as needed.
			s_shadowBuffer = TFE_RenderBackend::createTexture(MaxVisLightCount, ShadowWidth, 1/*channelCount*/, 2/*bytesPerChannel*/);	// 16-bit, single channel
			s_shadowBufferCpu = (u16*)malloc(sizeof(u16) * MaxVisLightCount * ShadowWidth);
			s_shadowBufferDirty = false;

			memset(s_lightPosCpu,  0, sizeof(Vec4f)  * MaxVisLightCount);
			memset(s_lightDataCpu, 0, sizeof(Vec4ui) * MaxVisLightCount);
			memset(s_clustersCpu,  0, sizeof(Vec4ui) * MaxClusterCount);
		}
		s_sectorCount = sectorCount;
	}

	Vec2f closestPointOnLine(const Vec2f& a, const Vec2f& b, const Vec2f& p)
	{
		const Vec2f ap  = { p.x - a.x, p.z - a.z };
		const Vec2f dir = { b.x - a.x, b.z - a.z };
		const f32 u = ap.x * dir.x + ap.z * dir.z;
		if (u <= 0.0f)
		{
			return a;
		}

		const f32 lenSq = dir.x*dir.x + dir.z*dir.z;
		if (u >= lenSq)
		{
			return b;
		}

		const f32 scale = u / lenSq;
		return { a.x + dir.x*scale, a.z + dir.z*scale };
	}

	bool lighting_addLightSector(SceneLight* sceneLight, s32 sectorId)
	{
		// Make sure there is room.
		if (sceneLight->sectorCount >= MAX_LIGHT_SECTORS) { return false; }
		// Make sure it isn't already there...
		for (s32 i = 0; i < sceneLight->sectorCount; i++)
		{
			if (sceneLight->sectorId[i] == sectorId) { return false; }
		}
		// Finally add it.
		sceneLight->sectorId[sceneLight->sectorCount++] = sectorId;
		return true;
	}

	// TODO: Move segment rendering to the GPU.
	struct LightWallSeg
	{
		Vec2f proj;
		Vec2f pos[2];
	};

	Vec2f lighting_clip(Vec2f v0, Vec2f v1, Vec2f pointOnPlane)
	{
		Vec2f p1 = { pointOnPlane.x, pointOnPlane.z };
		Vec2f planeNormal = { -p1.z, p1.x };
		f32 lenSq = p1.x*p1.x + p1.z*p1.z;
		f32 scale = (lenSq > FLT_EPSILON) ? 1.0f / sqrtf(lenSq) : 0.0f;

		f32 d0 = (v0.x*planeNormal.x + v0.z*planeNormal.z) * scale;
		f32 d1 = (v1.x*planeNormal.x + v1.z*planeNormal.z) * scale;
		f32 t = -d0 / (d1 - d0);

		Vec2f intersection;
		intersection.x = (1.0f - t)*v0.x + t * v1.x;
		intersection.z = (1.0f - t)*v0.z + t * v1.z;
		return intersection;
	}

	void lighting_rasterizeSubSegment(const LightWallSeg& seg, u16* depth, f32 radius, Vec2f p0, Vec2f p1, Vec2f dir)
	{
		f32 z0 = seg.pos[0].x*dir.x + seg.pos[0].z*dir.z;
		f32 z1 = seg.pos[1].x*dir.x + seg.pos[1].z*dir.z;
		assert(z0 > 0.0f && z1 > 0.0f);
		z0 = 1.0f / z0;
		z1 = 1.0f / z1;
		
		// Project
		Vec2f left  = { seg.pos[0].x * z0, seg.pos[0].z * z0 };
		Vec2f right = { seg.pos[1].x * z1, seg.pos[1].z * z1 };

		f32 px = seg.proj.x * 256.0f;
		f32 pz = seg.proj.z * 256.0f;
		f32 delta = 1.0f / (pz - px);
		Vec2f dPos = { (right.x - left.x) * delta, (right.z - left.z) * delta };
		f32 dZ = (z1 - z0) * delta;
		Vec2f sPos = left;
		f32 z = z0;
		
		f32 u = px;
		f32 uEnd = pz;
		if (pz > 1023.0f)
		{
			uEnd = 1023.0f;
		}

		// Next step to the previous whole number.
		f32 uWhole = floorf(u);
		if (uWhole != u)
		{
			u = uWhole;
			f32 step = (uWhole - px) * delta;
			sPos.x += dPos.x * step;
			sPos.z += dPos.z * step;
			z += dZ * step;
		}

		// Finally loop.
		f32 scale = 65536.0f / radius;
		for (s32 i = s32(u); i <= s32(uEnd); i++)
		{
			f32 Z = 1.0f / z;
			Vec2f pos = { sPos.x*Z, sPos.z*Z };

			f32 dist = sqrtf(pos.x*pos.x + pos.z*pos.z);
			dist = min(65535.0f, ceilf(dist * scale));
			depth[i] = min(depth[i], u16(dist));

			sPos.x += dPos.x;
			sPos.z += dPos.z;
			z += dZ;
		}
	}

	void lighting_rasterizeSegment(const LightWallSeg& seg, u16* depth, f32 radius)
	{
		/*
		(-1,  1) 2               (1,  1) 3
		(-1, -1) 1				 (1, -1) 0, 4
		*/
		const Vec2f planePos[4] = { {1, -1}, {-1, -1}, {-1, 1}, {1, 1} };
		const Vec2f planeDir[4] = { {0, -1}, {-1, 0}, {0, 1}, {1, 0} };

		// Split into linear parts.
		if (seg.proj.x < 1.0f && seg.proj.z > 0.0f)
		{
			LightWallSeg subSeg = seg;
			if (seg.proj.x < 0.0f)
			{
				// split along {1,-1}
				subSeg.pos[0] = lighting_clip(seg.pos[0], seg.pos[1], planePos[0]);
				subSeg.proj.x = 0.0f;
			}
			if (seg.proj.z > 1.0f)
			{
				// split along {1,-1}
				subSeg.pos[1] = lighting_clip(seg.pos[0], seg.pos[1], planePos[1]);
				subSeg.proj.z = 1.0f;
			}
			lighting_rasterizeSubSegment(subSeg, depth, radius, planePos[0], planePos[1], planeDir[0]);
		}
		if (seg.proj.x < 2.0f && seg.proj.z > 1.0f)
		{
			LightWallSeg subSeg = seg;
			if (seg.proj.x < 1.0f)
			{
				// split along {1,-1}
				subSeg.pos[0] = lighting_clip(seg.pos[0], seg.pos[1], planePos[1]);
				subSeg.proj.x = 1.0f;
			}
			if (seg.proj.z > 2.0f)
			{
				// split along {1,-1}
				subSeg.pos[1] = lighting_clip(seg.pos[0], seg.pos[1], planePos[2]);
				subSeg.proj.z = 2.0f;
			}
			lighting_rasterizeSubSegment(subSeg, depth, radius, planePos[1], planePos[2], planeDir[1]);
		}
		if (seg.proj.x < 3.0f && seg.proj.z > 2.0f)
		{
			LightWallSeg subSeg = seg;
			if (seg.proj.x < 2.0f)
			{
				// split along {1,-1}
				subSeg.pos[0] = lighting_clip(seg.pos[0], seg.pos[1], planePos[2]);
				subSeg.proj.x = 2.0f;
			}
			if (seg.proj.z > 3.0f)
			{
				// split along {1,-1}
				subSeg.pos[1] = lighting_clip(seg.pos[0], seg.pos[1], planePos[3]);
				subSeg.proj.z = 3.0f;
			}
			lighting_rasterizeSubSegment(subSeg, depth, radius, planePos[2], planePos[3], planeDir[2]);
		}
		if (seg.proj.z > 3.0f)
		{
			LightWallSeg subSeg = seg;
			if (seg.proj.x < 3.0f)
			{
				// split along {1,-1}
				subSeg.pos[0] = lighting_clip(seg.pos[0], seg.pos[1], planePos[3]);
				subSeg.proj.x = 3.0f;
			}
			if (seg.proj.z > 4.0f)
			{
				// split along {1,-1}
				subSeg.pos[1] = lighting_clip(seg.pos[0], seg.pos[1], planePos[0]);
				subSeg.proj.z = 4.0f;
			}
			lighting_rasterizeSubSegment(subSeg, depth, radius, planePos[3], planePos[0], planeDir[3]);
		}
	}

	f32 lighting_projectToUnitSquare(Vec2f coord)
	{
		// Handle the singularity.
		if (fabsf(coord.x) < FLT_EPSILON && fabsf(coord.z) < FLT_EPSILON)
		{
			return 0.0f;
		}

		// Find the largest axis.
		s32 axis = 0;
		if (fabsf(coord.x) > fabsf(coord.z))
		{
			axis = coord.x < 0.0f ? 2 : 0;
		}
		else
		{
			axis = coord.z < 0.0f ? 3 : 1;
		}
		const f32 axisScale = coord.m[axis & 1] != 0.0f ? 1.0f / fabsf(coord.m[axis & 1]) : 0.0f;
		f32 value = coord.m[(1 + axis) & 1] * axisScale * 0.5f + 0.5f;
		if (axis == 1 || axis == 2) { value = 1.0f - value; }

		// Switch the direction to match the wall direction.
		f32 projection = 4.0f - fmodf(f32(axis) + value, 4.0f);	// this wraps around to 0.0 at 4.0
		// This zero checks above should catch singularities, but since this causes crashes I want to be extra sure. :)
		if (!std::isfinite(projection)) { projection = 0.0f; }
		return projection;
	}

	void lighting_rasterizeWalls(SceneLight* sceneLight, s32 sectorId, u16* depth)
	{
		const RSector* sector = &s_levelState.sectors[sectorId];
		const RWall* wall     = sector->walls;
		const s32 wallCount   = sector->wallCount;
		const f32 lightHeight = sceneLight->light.pos.y;
		const f32 r = sceneLight->light.radii.z;
		const f32 rSq = r * r;
		const Vec2f lightPos = { sceneLight->light.pos.x, sceneLight->light.pos.z };
		for (s32 w = 0; w < wallCount; w++, wall++)
		{
			const RSector* next = wall->nextSector;
			if (next)
			{
				// Now check to see if the sector edge will still block the light...
				const f32 maxDiff = max(lightHeight - fixed16ToFloat(next->floorHeight), fixed16ToFloat(next->ceilingHeight) - lightHeight);
				if (maxDiff < r) { continue; }
			}

			// Relative position.
			const Vec2f w0 = { fixed16ToFloat(wall->w0->x) - lightPos.x, fixed16ToFloat(wall->w0->z) - lightPos.z };
			const Vec2f w1 = { fixed16ToFloat(wall->w1->x) - lightPos.x, fixed16ToFloat(wall->w1->z) - lightPos.z };

			// Sidedness test.
			const Vec2f wallNormal = { -(w1.z - w0.z), w1.x - w0.x };
			if (wallNormal.x*w0.x + wallNormal.z*w0.z < 0.0f) { continue; }

			// Minimum distance from light to wall check (2D).
			const Vec2f offset = closestPointOnLine(w0, w1, { 0.0f, 0.0f });
			const f32 minDistSq = offset.x*offset.x + offset.z*offset.z;
			if (minDistSq > rSq) { continue; }

			// Projection and post-projection backface culling.
			LightWallSeg seg;
			seg.proj.x = lighting_projectToUnitSquare(w0);
			seg.proj.z = lighting_projectToUnitSquare(w1);
			sbuffer_handleEdgeWrapping(seg.proj.x, seg.proj.z);
			if (seg.proj.x >= seg.proj.z) { continue; }

			// Rasterize the segment.
			seg.pos[0] = w0;
			seg.pos[1] = w1;
			lighting_rasterizeSegment(seg, depth, r);

			// The segment wraps around the edge of the projection, offset to the other edge.
			if (seg.proj.z > 4.0f)
			{
				seg.proj.x -= 4.0f;
				seg.proj.z -= 4.0f;
				lighting_rasterizeSegment(seg, depth, r);
			}
		}
	}

	void lighting_traversePortals(SceneLight* sceneLight, s32 sectorId, u16* depth)
	{
		lighting_rasterizeWalls(sceneLight, sectorId, depth);

		const RSector* sector = &s_levelState.sectors[sectorId];
		const RWall* wall = sector->walls;
		const s32 wallCount = sector->wallCount;
		const f32 r = sceneLight->light.radii.z;
		const f32 rSq = r * r;
		const Vec2f lightPos = { sceneLight->light.pos.x, sceneLight->light.pos.z };
		const f32 lightHeight = sceneLight->light.pos.y;
		const Vec2f zero = { 0 };
		for (s32 w = 0; w < wallCount; w++, wall++)
		{
			RSector* next = wall->nextSector;
			if (!next) { continue; }
			Vec2f w0 = { fixed16ToFloat(wall->w0->x) - lightPos.x, fixed16ToFloat(wall->w0->z) - lightPos.z };
			Vec2f w1 = { fixed16ToFloat(wall->w1->x) - lightPos.x, fixed16ToFloat(wall->w1->z) - lightPos.z };

			// Sidedness test.
			const Vec2f wallNormal = { -(w1.z - w0.z), w1.x - w0.x };
			if (wallNormal.x*w0.x + wallNormal.z*w0.z < 0.0f) { continue; }

			// Approximate height check.
			const f32 nextFloor = fixed16ToFloat(min(next->floorHeight, sector->floorHeight));
			const f32 nextCeil  = fixed16ToFloat(max(next->ceilingHeight, sector->ceilingHeight));
			if (lightHeight - nextFloor > r || nextCeil - lightHeight > r) { continue; }

			// Minimum distance from light to portal line check (2D).
			const Vec2f offset = closestPointOnLine(w0, w1, zero);
			if (offset.x*offset.x + offset.z*offset.z < rSq)
			{
				if (lighting_addLightSector(sceneLight, next->index))
				{
					lighting_traversePortals(sceneLight, next->index, depth);
				}
			}
		}
	}
		
	void lighting_handleVisibility(SceneLight* sceneLight, s32 sectorId)
	{
		sceneLight->sectorCount = 0;
		// Assume that the light is actually in the specified sector.
		sceneLight->sectorId[sceneLight->sectorCount++] = sectorId;

		// test..
		u16* depth = &s_shadowBufferCpu[ShadowWidth * sceneLight->id];
		for (s32 i = 0; i < 1024; i++) { depth[i] = 0xffff; }
		s_shadowBufferDirty = true;
		
		// Next go through all of the potential portals and see if the light overlaps.
		lighting_traversePortals(sceneLight, sectorId, depth);

		// Finally add it to all of the sector buckets.
		for (s32 i = 0; i < sceneLight->sectorCount; i++)
		{
			lighting_addToBucket(&s_sectorBuckets[sceneLight->sectorId[i]], sceneLight);
		}
	}
	
	SceneLight* lighting_addToScene(const Light& light, s32 sectorId)
	{
		if (sectorId < 0) { return nullptr; }

		SceneLight* sceneLight = nullptr;
		if (!s_freeSceneLights.empty())
		{
			sceneLight = s_freeSceneLights.back();
			s_freeSceneLights.pop_back();
		}
		else if (s_lightPoolUsed < LightPoolMax)
		{
			lighting_growLightPool(s_lightPoolUsed + 1);

			sceneLight = (SceneLight*)&s_lightPool[s_lightPoolUsed];
			sceneLight->id = s_lightPoolUsed;
			s_lightPoolUsed++;
		}
		
		if (sceneLight)
		{
			sceneLight->light = light;
			lighting_handleVisibility(sceneLight, sectorId);
		}
		return sceneLight;
	}

	void lighting_updateLight(SceneLight* sceneLight, const Light& light, s32 newSector)
	{
		const bool posRadChanged = (light.pos.x != sceneLight->light.pos.x || light.pos.y != sceneLight->light.pos.y || light.pos.z != sceneLight->light.pos.z ||
		                            light.radii.z != sceneLight->light.radii.z);

		sceneLight->light = light;
		// Only change sectors if the light position or radius has changed.
		if (posRadChanged)
		{
			for (s32 i = 0; i < sceneLight->sectorCount; i++)
			{
				lighting_removeFromBucket(&s_sectorBuckets[sceneLight->sectorId[i]], sceneLight);
			}
			lighting_handleVisibility(sceneLight, newSector);
		}
	}

	void lighting_freeLight(SceneLight* sceneLight)
	{
		s_freeSceneLights.push_back(sceneLight);

		for (s32 i = 0; i < sceneLight->sectorCount; i++)
		{
			lighting_removeFromBucket(&s_sectorBuckets[sceneLight->sectorId[i]], sceneLight);
		}
	}

	void lighting_addSectorLights(s32 sectorId, Frustum* viewFrustum)
	{
		SectorBucket* bucket = &s_sectorBuckets[sectorId];
		const size_t count   = bucket->lights.size();
		SceneLight** lights  = bucket->lights.data();
		for (size_t i = 0; i < count; i++)
		{
			SceneLight* sceneLight = lights[i];
			// Make sure this light hasn't already been added from a different view.
			if (sceneLight->frameIndex == s_frameIndex) { continue; }

			// Cull the light against the current frustum.
			if (!frustum_sphereInside(sceneLight->light.pos, sceneLight->light.radii.z)) { continue; }

			// Finally add the light.
			sceneLight->frameIndex = s_frameIndex;
			lighting_add(sceneLight->light, sceneLight->id);
		}
	}
}