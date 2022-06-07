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

#include "sectorDisplayList.h"
#include "frustum.h"
#include "../rcommon.h"

using namespace TFE_RenderBackend;

namespace TFE_Jedi
{
	// Warning: these IDs must match the PartId used in the vertex shader.
	enum SegmentPartID
	{
		SPARTID_WALL_MID = 0,
		SPARTID_WALL_TOP,
		SPARTID_WALL_BOT,
		SPARTID_FLOOR,
		SPARTID_CEILING,
		SPARTID_FLOOR_CAP,
		SPARTID_CEIL_CAP,
		SPARTID_COUNT
	};

	static s32 s_displayListCount;
	static s32 s_displayPlaneCount;
	static s32 s_displayCurrentPortalId;
	static Vec4f  s_displayListPos[1024];
	static Vec4ui s_displayListData[1024];
	static Vec4f  s_displayListPlanes[2048];
	static ShaderBuffer s_displayListPosGPU;
	static ShaderBuffer s_displayListDataGPU;
	static ShaderBuffer s_displayListPlanesGPU;
	static s32 s_posIndex;
	static s32 s_dataIndex;
	static s32 s_planesIndex;

	void sdisplayList_init(s32 posIndex, s32 dataIndex, s32 planesIndex)
	{
		ShaderBufferDef bufferDefDisplayListPos =
		{
			4,				// 1, 2, 4 channels (R, RG, RGBA)
			sizeof(f32),	// 1, 2, 4 bytes (u8; s16,u16; s32,u32,f32)
			BUF_CHANNEL_FLOAT
		};
		s_displayListPosGPU.create(1024, bufferDefDisplayListPos, true);

		ShaderBufferDef bufferDefDisplayListData =
		{
			4,				// 1, 2, 4 channels (R, RG, RGBA)
			sizeof(u32),	// 1, 2, 4 bytes (u8; s16,u16; s32,u32,f32)
			BUF_CHANNEL_UINT
		};
		s_displayListDataGPU.create(1024, bufferDefDisplayListData, true);

		ShaderBufferDef bufferDefDisplayListPlanes =
		{
			4,				// 1, 2, 4 channels (R, RG, RGBA)
			sizeof(f32),	// 1, 2, 4 bytes (u8; s16,u16; s32,u32,f32)
			BUF_CHANNEL_FLOAT
		};
		s_displayListPlanesGPU.create(2048, bufferDefDisplayListPlanes, true);

		s_posIndex = posIndex;
		s_dataIndex = dataIndex;
		s_planesIndex = planesIndex;

		sdisplayList_clear();
	}

	void sdisplayList_destroy()
	{
		s_displayListPosGPU.destroy();
		s_displayListDataGPU.destroy();
		s_displayListPlanesGPU.destroy();
	}

	void sdisplayList_clear()
	{
		s_displayListCount = 0;
		s_displayPlaneCount = 0;
		s_displayCurrentPortalId = 0;
	}
		
	void sdisplayList_finish()
	{
		if (!s_displayListCount) { return; }
		s_displayListPosGPU.update(s_displayListPos, sizeof(Vec4f) * s_displayListCount);
		s_displayListDataGPU.update(s_displayListData, sizeof(Vec4ui) * s_displayListCount);
		s_displayListPlanesGPU.update(s_displayListPlanes, sizeof(Vec4f) * s_displayPlaneCount);
	}

	void sdisplayList_addCaps(RSector* curSector)
	{
		// TODO: Constrain to portal frustum.
		Vec4f pos = { fixed16ToFloat(curSector->boundsMin.x), fixed16ToFloat(curSector->boundsMin.z), fixed16ToFloat(curSector->boundsMax.x), fixed16ToFloat(curSector->boundsMax.z) };
		Vec4ui data = { 0, (u32)curSector->index/*sectorId*/, 0u, 0u/*textureId*/ };

		s_displayListPos[s_displayListCount] = pos;
		s_displayListData[s_displayListCount] = data;
		s_displayListData[s_displayListCount].x = SPARTID_FLOOR_CAP;
		s_displayListData[s_displayListCount].w = curSector->floorTex && *curSector->floorTex ? (*curSector->floorTex)->textureId : 0;
		s_displayListCount++;

		s_displayListPos[s_displayListCount] = pos;
		s_displayListData[s_displayListCount] = data;
		s_displayListData[s_displayListCount].x = SPARTID_CEIL_CAP;
		s_displayListData[s_displayListCount].w = curSector->ceilTex && *curSector->ceilTex ? (*curSector->ceilTex)->textureId : 0;
		s_displayListCount++;
	}

	void sdisplayList_addPortal(Vec3f p0, Vec3f p1)
	{
		Vec4f* botPlane = &s_displayListPlanes[s_displayPlaneCount + 0];
		Vec4f* topPlane = &s_displayListPlanes[s_displayPlaneCount + 1];
		s_displayCurrentPortalId = 1 + (s_displayPlaneCount >> 1);
		s_displayPlaneCount += 2;

		const Vec3f botEdge[] =
		{
			{ p0.x, p1.y, p0.z },
			{ p1.x, p1.y, p1.z },
		};
		const Vec3f topEdge[] =
		{
			{ p1.x, p0.y, p1.z },
			{ p0.x, p0.y, p0.z },
		};

		*botPlane = frustum_calculatePlaneFromEdge(botEdge);
		*topPlane = frustum_calculatePlaneFromEdge(topEdge);
	}

	void sdisplayList_addSegment(RSector* curSector, GPUCachedSector* cached, SegmentClipped* wallSeg)
	{
		s32 wallId = wallSeg->seg->id;
		RWall* srcWall = &curSector->walls[wallId];
		u32 portalId  = u32(s_displayCurrentPortalId) << 6u;	// pre-shift.
		u32 wallGpuId = u32(cached->wallStart + wallId) << 16u;
		// Add 32 so the value is unsigned and easy to decode in the shader (just subtract 32).
		// Values should never to larger than [-31,31] but clamp just in case (larger values would have no effect anyway).
		u32 wallLight = u32(32 + clamp(floor16(srcWall->wallLight), -31, 31));
		
		Vec4f pos = { wallSeg->v0.x, wallSeg->v0.z, wallSeg->v1.x, wallSeg->v1.z };
		Vec4ui data = { (srcWall->nextSector ? u32(srcWall->nextSector->index) << 16u : 0u)/*partId | nextSector*/, (u32)curSector->index/*sectorId*/,
						 wallLight | portalId, 0u/*textureId*/ };

		// Wall Flags.
		if (srcWall->drawFlags == WDF_MIDDLE && !srcWall->nextSector) // TODO: Fix transparent mid textures.
		{
			s_displayListPos[s_displayListCount] = pos;
			s_displayListData[s_displayListCount] = data;
			s_displayListData[s_displayListCount].x |= SPARTID_WALL_MID;
			s_displayListData[s_displayListCount].w = wallGpuId | (srcWall->midTex && *srcWall->midTex ? (*srcWall->midTex)->textureId : 0);
			s_displayListCount++;
		}
		if ((srcWall->drawFlags & WDF_TOP) && srcWall->nextSector)
		{
			s_displayListPos[s_displayListCount] = pos;
			s_displayListData[s_displayListCount] = data;
			s_displayListData[s_displayListCount].x |= SPARTID_WALL_TOP;
			s_displayListData[s_displayListCount].w = wallGpuId | (srcWall->topTex && *srcWall->topTex ? (*srcWall->topTex)->textureId : 0);
			s_displayListCount++;
		}
		if ((srcWall->drawFlags & WDF_BOT) && srcWall->nextSector)
		{
			s_displayListPos[s_displayListCount] = pos;
			s_displayListData[s_displayListCount] = data;
			s_displayListData[s_displayListCount].x |= SPARTID_WALL_BOT;
			s_displayListData[s_displayListCount].w = wallGpuId | (srcWall->botTex && *srcWall->botTex ? (*srcWall->botTex)->textureId : 0);
			s_displayListCount++;
		}
		// Add Floor
		s_displayListPos[s_displayListCount] = pos;
		s_displayListData[s_displayListCount] = data;
		s_displayListData[s_displayListCount].x |= SPARTID_FLOOR;
		s_displayListData[s_displayListCount].w = curSector->floorTex && *curSector->floorTex ? (*curSector->floorTex)->textureId : 0;
		s_displayListCount++;

		// Add Ceiling
		s_displayListPos[s_displayListCount] = pos;
		s_displayListData[s_displayListCount] = data;
		s_displayListData[s_displayListCount].x |= SPARTID_CEILING;
		s_displayListData[s_displayListCount].w = curSector->ceilTex && *curSector->ceilTex ? (*curSector->ceilTex)->textureId : 0;
		s_displayListCount++;
	}

	s32 sdisplayList_getSize()
	{
		return s_displayListCount;
	}

	void sdisplayList_draw()
	{
		s_displayListPosGPU.bind(s_posIndex);
		s_displayListDataGPU.bind(s_dataIndex);
		s_displayListPlanesGPU.bind(s_planesIndex);

		TFE_RenderBackend::drawIndexedTriangles(2 * s_displayListCount, sizeof(u16));

		s_displayListPosGPU.unbind(s_posIndex);
		s_displayListDataGPU.unbind(s_dataIndex);
		s_displayListPlanesGPU.unbind(s_planesIndex);
	}
}
