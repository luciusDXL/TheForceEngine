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
	// Pack portal info into a 16-bit value.
	#define PACK_PORTAL_INFO(offset, count) (u32(offset) | u32(count) << 13u)

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
		SPARTID_WALL_MID_SIGN,
		SPARTID_WALL_TOP_SIGN,
		SPARTID_WALL_BOT_SIGN,
		SPARTID_SKY = 32768,
		SPARTID_COUNT
	};

	enum Constants
	{
		MAX_DISP_ITEMS = 1024,
		MAX_PORTAL_PLANES = 6,
	};

	// TODO: factor out so the sprite, sector, and geometry passes can use it.
	s32 s_displayCurrentPortalId;
	ShaderBuffer s_displayListPlanesGPU;

	static s32 s_displayListCount[SECTOR_PASS_COUNT];
	static s32 s_displayPlaneCount;
	static s32 s_displayPortalCount;
	static Vec4f  s_displayListPos[SECTOR_PASS_COUNT * MAX_DISP_ITEMS];
	static Vec4ui s_displayListData[SECTOR_PASS_COUNT * MAX_DISP_ITEMS];
	static Vec4f  s_displayListPlanes[MAX_PORTAL_PLANES * MAX_DISP_ITEMS];
	static ShaderBuffer s_displayListPosGPU[SECTOR_PASS_COUNT];
	static ShaderBuffer s_displayListDataGPU[SECTOR_PASS_COUNT];
	static s32 s_posIndex[SECTOR_PASS_COUNT];
	static s32 s_dataIndex[SECTOR_PASS_COUNT];
	static s32 s_planesIndex;

	u32 s_portalPlaneInfo[256];
	static Frustum s_portalFrustumVert[256];
	static s32 s_maxPlaneCount = 0;

	void sdisplayList_init(s32* posIndex, s32* dataIndex, s32 planesIndex)
	{
		for (s32 i = 0; i < SECTOR_PASS_COUNT; i++)
		{
			ShaderBufferDef bufferDefDisplayListPos =
			{
				4,				// 1, 2, 4 channels (R, RG, RGBA)
				sizeof(f32),	// 1, 2, 4 bytes (u8; s16,u16; s32,u32,f32)
				BUF_CHANNEL_FLOAT
			};
			s_displayListPosGPU[i].create(MAX_DISP_ITEMS, bufferDefDisplayListPos, true);

			ShaderBufferDef bufferDefDisplayListData =
			{
				4,				// 1, 2, 4 channels (R, RG, RGBA)
				sizeof(u32),	// 1, 2, 4 bytes (u8; s16,u16; s32,u32,f32)
				BUF_CHANNEL_UINT
			};
			s_displayListDataGPU[i].create(MAX_DISP_ITEMS, bufferDefDisplayListData, true);

			s_posIndex[i] = posIndex[i];
			s_dataIndex[i] = dataIndex[i];
		}

		ShaderBufferDef bufferDefDisplayListPlanes =
		{
			4,				// 1, 2, 4 channels (R, RG, RGBA)
			sizeof(f32),	// 1, 2, 4 bytes (u8; s16,u16; s32,u32,f32)
			BUF_CHANNEL_FLOAT
		};
		s_displayListPlanesGPU.create(MAX_PORTAL_PLANES*MAX_DISP_ITEMS, bufferDefDisplayListPlanes, true);
		s_planesIndex = planesIndex;

		sdisplayList_clear();
	}

	void sdisplayList_destroy()
	{
		for (s32 i = 0; i < SECTOR_PASS_COUNT; i++)
		{
			s_displayListPosGPU[i].destroy();
			s_displayListDataGPU[i].destroy();
		}
		s_displayListPlanesGPU.destroy();
	}

	void sdisplayList_clear()
	{
		for (s32 i = 0; i < SECTOR_PASS_COUNT; i++)
		{
			s_displayListCount[i] = 0;
		}
		s_displayPlaneCount = 0;
		s_displayPortalCount = 0;
		s_displayCurrentPortalId = 0;
	}
		
	void sdisplayList_finish()
	{
		for (s32 i = 0; i < SECTOR_PASS_COUNT; i++)
		{
			if (!s_displayListCount[i]) { continue; }

			s_displayListPosGPU[i].update(&s_displayListPos[i*MAX_DISP_ITEMS], sizeof(Vec4f) * s_displayListCount[i]);
			s_displayListDataGPU[i].update(&s_displayListData[i*MAX_DISP_ITEMS], sizeof(Vec4ui) * s_displayListCount[i]);
		}

		if (s_displayPlaneCount)
		{
			s_displayListPlanesGPU.update(s_displayListPlanes, sizeof(Vec4f) * s_displayPlaneCount);
		}
	}

	void sdisplayList_addCaps(RSector* curSector)
	{
		// TODO: Remove and replace with main frustum extrusion.
		Vec4f pos = { fixed16ToFloat(curSector->boundsMin.x), fixed16ToFloat(curSector->boundsMin.z), fixed16ToFloat(curSector->boundsMax.x), fixed16ToFloat(curSector->boundsMax.z) };
		u32 portalInfo = sdisplayList_getPackedPortalInfo(s_displayCurrentPortalId) << 7u;
		Vec4ui data = { 0, (u32)curSector->index/*sectorId*/, portalInfo, 0u/*textureId*/ };

		s_displayListPos[s_displayListCount[0]] = pos;
		s_displayListData[s_displayListCount[0]] = data;
		s_displayListData[s_displayListCount[0]].x = SPARTID_FLOOR_CAP;
		s_displayListData[s_displayListCount[0]].w = curSector->floorTex && *curSector->floorTex ? (*curSector->floorTex)->textureId : 0;
		if (curSector->flags1 & SEC_FLAGS1_PIT) { s_displayListData[s_displayListCount[0]].x |= SPARTID_SKY; }
		s_displayListCount[0]++;

		s_displayListPos[s_displayListCount[0]] = pos;
		s_displayListData[s_displayListCount[0]] = data;
		s_displayListData[s_displayListCount[0]].x = SPARTID_CEIL_CAP;
		s_displayListData[s_displayListCount[0]].w = curSector->ceilTex && *curSector->ceilTex ? (*curSector->ceilTex)->textureId : 0;
		if (curSector->flags1 & SEC_FLAGS1_EXTERIOR) { s_displayListData[s_displayListCount[0]].x |= SPARTID_SKY; }
		s_displayListCount[0]++;
	}

	u32 sdisplayList_getPackedPortalInfo(s32 portalId)
	{
		s32 portalIndex = portalId - 1;
		if (portalIndex < 0)
		{
			return 0;
		}
		return s_portalPlaneInfo[portalIndex];
	}
		
	bool sdisplayList_addPortal(Vec3f p0, Vec3f p1, s32 parentPortalId)
	{
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
		
		if (parentPortalId > 0)
		{
			Polygon clipped;
			s32 parentPortalIndex = parentPortalId - 1;
			if (frustum_clipQuadToPlanes(s_portalFrustumVert[parentPortalIndex].planeCount, s_portalFrustumVert[parentPortalIndex].planes, botEdge[0], topEdge[0], &clipped))
			{
				// Build a new frustum.
				u32& count = s_portalFrustumVert[s_displayPortalCount].planeCount;
				Vec4f* plane = s_portalFrustumVert[s_displayPortalCount].planes;
				count = 0;
				for (s32 i = 0; i < clipped.vertexCount; i++)
				{
					const s32 a = i, b = (i + 1) % clipped.vertexCount;
					if (fabsf(clipped.vtx[a].x - clipped.vtx[b].x) > FLT_EPSILON || fabsf(clipped.vtx[a].z - clipped.vtx[b].z) > FLT_EPSILON)
					{
						Vec3f edge[] = { clipped.vtx[a], clipped.vtx[b] };
						plane[count++] = frustum_calculatePlaneFromEdge(edge);
					}
				}
				s_maxPlaneCount = max(count, s_maxPlaneCount);
				assert(s_maxPlaneCount <= 6);
			}
			else
			{
				return false;
			}
		}
		else
		{
			s_portalFrustumVert[s_displayPortalCount].planeCount = 2;
			s_portalFrustumVert[s_displayPortalCount].planes[0] = frustum_calculatePlaneFromEdge(botEdge);
			s_portalFrustumVert[s_displayPortalCount].planes[1] = frustum_calculatePlaneFromEdge(topEdge);
		}
		s_portalPlaneInfo[s_displayPortalCount] = PACK_PORTAL_INFO(s_displayPlaneCount, min(6, s_portalFrustumVert[s_displayPortalCount].planeCount));

		// The new planes either match the parent or are created from the edges.
		const Frustum* frust = &s_portalFrustumVert[s_displayPortalCount];
		Vec4f* outPlanes = &s_displayListPlanes[s_displayPlaneCount];
		for (s32 i = 0; i < frust->planeCount && i < 6; i++)
		{
			outPlanes[i] = frust->planes[i];
			s_displayPlaneCount++;
		}

		s_displayCurrentPortalId = 1 + s_displayPortalCount;
		s_displayPortalCount++;
		
		return true;
	}

	void sdisplayList_addSegment(RSector* curSector, GPUCachedSector* cached, SegmentClipped* wallSeg)
	{
		s32 wallId = wallSeg->seg->id;
		RWall* srcWall = &curSector->walls[wallId];
		// Mark only visible walls as being rendered.
		srcWall->seen = JTRUE;

		u32 wallGpuId = u32(cached->wallStart + wallId) << 16u;
		u32 flip = (((srcWall->flags1 & WF1_FLIP_HORIZ) != 0) ? 1 : 0) << 6u;
		// Add 32 so the value is unsigned and easy to decode in the shader (just subtract 32).
		// Values should never to larger than [-31,31] but clamp just in case (larger values would have no effect anyway).
		u32 wallLight = u32(32 + clamp(floor16(srcWall->wallLight), -31, 31));
		u32 nextId = srcWall->nextSector ? u32(srcWall->nextSector->index) << 16u : 0xffff0000u;
		u32 portalInfo = sdisplayList_getPackedPortalInfo(s_displayCurrentPortalId) << 7u;

		Vec4f pos = { wallSeg->v0.x, wallSeg->v0.z, wallSeg->v1.x, wallSeg->v1.z };
		const Vec4ui data = {  nextId/*partId | nextSector*/, (u32)curSector->index/*sectorId*/,
				    		   wallLight | portalInfo, 0u/*textureId*/ };

		// Wall Flags.
		if (srcWall->drawFlags == WDF_MIDDLE && !srcWall->nextSector)
		{
			assert(srcWall->midTex && *srcWall->midTex && (*srcWall->midTex)->textureId >= 0 && (*srcWall->midTex)->textureId < 1024);

			s_displayListPos[s_displayListCount[0]] = pos;
			s_displayListData[s_displayListCount[0]] = data;
			s_displayListData[s_displayListCount[0]].x |= SPARTID_WALL_MID;
			s_displayListData[s_displayListCount[0]].z |= flip;
			s_displayListData[s_displayListCount[0]].w = wallGpuId | (srcWall->midTex && *srcWall->midTex ? (*srcWall->midTex)->textureId : 0);
			s_displayListCount[0]++;
		}
		else if (srcWall->midTex && srcWall->nextSector && (srcWall->flags1 & WF1_ADJ_MID_TEX)) // Transparent mid-texture.
		{
			// Transparent items go into SECTOR_PASS_TRANS
			s_displayListPos[s_displayListCount[1]  + MAX_DISP_ITEMS] = pos;
			s_displayListData[s_displayListCount[1] + MAX_DISP_ITEMS] = data;
			s_displayListData[s_displayListCount[1] + MAX_DISP_ITEMS].x |= SPARTID_WALL_MID;
			s_displayListData[s_displayListCount[1] + MAX_DISP_ITEMS].z |= flip;
			s_displayListData[s_displayListCount[1] + MAX_DISP_ITEMS].w = wallGpuId | (*srcWall->midTex ? (*srcWall->midTex)->textureId : 0);
			s_displayListCount[1]++;
		}

		if ((srcWall->drawFlags & WDF_TOP) && srcWall->nextSector && !(srcWall->nextSector->flags1 & SEC_FLAGS1_EXT_ADJ))
		{
			s_displayListPos[s_displayListCount[0]] = pos;
			s_displayListData[s_displayListCount[0]] = data;
			s_displayListData[s_displayListCount[0]].x |= SPARTID_WALL_TOP;
			s_displayListData[s_displayListCount[0]].z |= flip;
			s_displayListData[s_displayListCount[0]].w = wallGpuId | (srcWall->topTex && *srcWall->topTex ? (*srcWall->topTex)->textureId : 0);
			s_displayListCount[0]++;
		}
		if ((srcWall->drawFlags & WDF_BOT) && srcWall->nextSector && !(srcWall->nextSector->flags1 & SEC_FLAGS1_EXT_FLOOR_ADJ))
		{
			s_displayListPos[s_displayListCount[0]] = pos;
			s_displayListData[s_displayListCount[0]] = data;
			s_displayListData[s_displayListCount[0]].x |= SPARTID_WALL_BOT;
			s_displayListData[s_displayListCount[0]].z |= flip;
			s_displayListData[s_displayListCount[0]].w = wallGpuId | (srcWall->botTex && *srcWall->botTex ? (*srcWall->botTex)->textureId : 0);
			s_displayListCount[0]++;
		}

		if (srcWall->signTex && *srcWall->signTex)
		{
			// Transparent items go into SECTOR_PASS_TRANS
			s_displayListPos[s_displayListCount[1]  + MAX_DISP_ITEMS] = pos;
			s_displayListData[s_displayListCount[1] + MAX_DISP_ITEMS] = data;
			s_displayListData[s_displayListCount[1] + MAX_DISP_ITEMS].w = wallGpuId | (*srcWall->signTex)->textureId;

			// If there is a bottom texture, it goes there..
			if ((srcWall->drawFlags & WDF_BOT) && srcWall->nextSector && !(srcWall->nextSector->flags1 & SEC_FLAGS1_EXT_FLOOR_ADJ))
			{
				s_displayListData[s_displayListCount[1] + MAX_DISP_ITEMS].x |= SPARTID_WALL_BOT_SIGN;
				s_displayListCount[1]++;
			}
			// Otherwise if there is a top
			else if ((srcWall->drawFlags & WDF_TOP) && srcWall->nextSector && !(srcWall->nextSector->flags1 & SEC_FLAGS1_EXT_ADJ))
			{
				s_displayListData[s_displayListCount[1] + MAX_DISP_ITEMS].x |= SPARTID_WALL_TOP_SIGN;
				s_displayListCount[1]++;
			}
			// And finally mid.
			else if (srcWall->midTex)
			{
				s_displayListData[s_displayListCount[1] + MAX_DISP_ITEMS].x |= SPARTID_WALL_MID_SIGN;
				s_displayListCount[1]++;
			}
		}

		// Add Floor
		s_displayListPos[s_displayListCount[0]] = pos;
		s_displayListData[s_displayListCount[0]] = data;
		s_displayListData[s_displayListCount[0]].x |= SPARTID_FLOOR;
		s_displayListData[s_displayListCount[0]].w = curSector->floorTex && *curSector->floorTex ? (*curSector->floorTex)->textureId : 0;
		if (curSector->flags1 & SEC_FLAGS1_PIT) { s_displayListData[s_displayListCount[0]].x |= SPARTID_SKY; }
		s_displayListCount[0]++;

		// Add Ceiling
		s_displayListPos[s_displayListCount[0]] = pos;
		s_displayListData[s_displayListCount[0]] = data;
		s_displayListData[s_displayListCount[0]].x |= SPARTID_CEILING;
		s_displayListData[s_displayListCount[0]].w = curSector->ceilTex && *curSector->ceilTex ? (*curSector->ceilTex)->textureId : 0;
		if (curSector->flags1 & SEC_FLAGS1_EXTERIOR) { s_displayListData[s_displayListCount[0]].x |= SPARTID_SKY; }
		s_displayListCount[0]++;
	}

	s32 sdisplayList_getSize(SectorPass passId)
	{
		return s_displayListCount[passId];
	}

	void sdisplayList_draw(SectorPass passId)
	{
		if (!s_displayListCount[passId]) { return; }

		s_displayListPosGPU[passId].bind(s_posIndex[passId]);
		s_displayListDataGPU[passId].bind(s_dataIndex[passId]);
		s_displayListPlanesGPU.bind(s_planesIndex);

		TFE_RenderBackend::drawIndexedTriangles(2 * s_displayListCount[passId], sizeof(u16));

		s_displayListPosGPU[passId].unbind(s_posIndex[passId]);
		s_displayListDataGPU[passId].unbind(s_dataIndex[passId]);
		s_displayListPlanesGPU.unbind(s_planesIndex);
	}
}
