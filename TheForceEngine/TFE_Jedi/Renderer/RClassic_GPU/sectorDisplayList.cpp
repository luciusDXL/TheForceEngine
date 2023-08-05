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
	// Converting to 10-bits.
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
		SPARTID_MASK = 15,

		// Flags
		SPARTID_STRETCH_TO_TOP = 16,
		SPARTID_STRETCH = 32,
		SPARTID_FULLBRIGHT = 64,
		SPARTID_OPAQUE = 128,
		SPARTID_SKY_ADJ = 256,
		SPARTID_SKY = 512,
		SPARTID_COUNT
	};

	// Threshold were matching vertices are too far apart between an adjoin segment and its associated mirror.
	// Used to determine when adjoins are bad for "no wall" setups (which auto-heal the issue in software due to the way the no wall segments are drawn).
	// This value is probably *much* bigger than it should be, but I don't want to accidentally break cases where the adjoin/mirror is split intentionally.
	#define BAD_ADJOIN_THRES 4194304	// FIXED(64)

	// TODO: factor out so the sprite, sector, and geometry passes can use it.
	s32 s_displayCurrentPortalId = 0;
	ShaderBuffer s_displayListPlanesGPU;

	static s32 s_displayListCount[SECTOR_PASS_COUNT];
	static s32 s_displayPlaneCount  = 0;
	static s32 s_displayPortalCount = 0;
	static Vec4f*  s_displayListPos    = nullptr;
	static Vec4ui* s_displayListData   = nullptr;
	static Vec4f*  s_displayListPlanes = nullptr;
	static u32*    s_portalPlaneInfo   = nullptr;
	static Frustum* s_portalFrustumVert = nullptr;
	// GPU Memory size = 32 * SECTOR_PASS_COUNT * MAX_DISP_ITEMS = 64 * MAX_DISP_ITEMS = 64 * 8192 = 512Kb; 64 * 65536 = 4Mb 
	static ShaderBuffer s_displayListPosGPU[SECTOR_PASS_COUNT];
	static ShaderBuffer s_displayListDataGPU[SECTOR_PASS_COUNT];
	static s32 s_posIndex[SECTOR_PASS_COUNT];
	static s32 s_dataIndex[SECTOR_PASS_COUNT];
	static s32 s_planesIndex = -1;
	static s32 s_maxPlaneCount = 0;

	void sdisplayList_init(s32* posIndex, s32* dataIndex, s32 planesIndex)
	{
		TFE_COUNTER(s_displayPortalCount, "GPU Portal Count");
		TFE_COUNTER(s_displayPlaneCount, "GPU Plane Count");
		
		// Allocate CPU buffers
		// CPU Memory size = (32 * SECTOR_PASS_COUNT + 16 * MAX_PORTAL_PLANES) * MAX_DISP_ITEMS
		//                 = 192 * MAX_DISP_ITEMS
		//				   : MAX_DISP_ITEMS = 8192  = 192 * 8192  = 1.5Mb
		//                 : MAX_DISP_ITEMS = 65536 = 192 * 65536 = 12Mb
		// TODO: Can increase the display list size here, up to ShaderBuffer::getMaxSize().
		//       Or reduce size needed to 1 item per wall segment (instead of 5 - 7).
		s_displayListPos    = (Vec4f*)malloc(sizeof(Vec4f) * SECTOR_PASS_COUNT * MAX_DISP_ITEMS);
		s_displayListData   = (Vec4ui*)malloc(sizeof(Vec4ui) * SECTOR_PASS_COUNT * MAX_DISP_ITEMS);
		s_displayListPlanes = (Vec4f*)malloc(sizeof(Vec4f) * MAX_BUFFER_SIZE);
		s_portalPlaneInfo   = (u32*)malloc(sizeof(u32) * MAX_BUFFER_SIZE);
		s_portalFrustumVert = (Frustum*)malloc(sizeof(Frustum) * MAX_BUFFER_SIZE);

		const ShaderBufferDef bufferDefDisplayListPos  = { 4, sizeof(f32), BUF_CHANNEL_FLOAT };
		const ShaderBufferDef bufferDefDisplayListData = { 4, sizeof(u32), BUF_CHANNEL_UINT };
		for (s32 i = 0; i < SECTOR_PASS_COUNT; i++)
		{
			s_displayListPosGPU[i].create(MAX_DISP_ITEMS, bufferDefDisplayListPos, true);
			s_displayListDataGPU[i].create(MAX_DISP_ITEMS, bufferDefDisplayListData, true);

			s_posIndex[i] = posIndex[i];
			s_dataIndex[i] = dataIndex[i];
		}

		const ShaderBufferDef bufferDefDisplayListPlanes = { 4, sizeof(f32), BUF_CHANNEL_FLOAT };
		s_displayListPlanesGPU.create(MAX_BUFFER_SIZE, bufferDefDisplayListPlanes, true);
		s_planesIndex = planesIndex;

		sdisplayList_clear();
	}

	void sdisplayList_destroy()
	{
		free(s_displayListPos);
		free(s_displayListData);
		free(s_displayListPlanes);
		free(s_portalPlaneInfo);
		free(s_portalFrustumVert);
		s_displayListPos = nullptr;
		s_displayListData = nullptr;
		s_displayListPlanes = nullptr;
		s_portalPlaneInfo = nullptr;
		s_portalFrustumVert = nullptr;

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
			assert(s_displayListCount[i] <= MAX_DISP_ITEMS);

			s_displayListPosGPU[i].update(&s_displayListPos[i*MAX_DISP_ITEMS], sizeof(Vec4f) * s_displayListCount[i]);
			s_displayListDataGPU[i].update(&s_displayListData[i*MAX_DISP_ITEMS], sizeof(Vec4ui) * s_displayListCount[i]);
		}

		if (s_displayPlaneCount)
		{
			s_displayListPlanesGPU.update(s_displayListPlanes, sizeof(Vec4f) * s_displayPlaneCount);
		}
	}
		
	u32 sdisplayList_getPlanesFromPortal(u32 portalId, u32 planeType, Vec4f* outPlanes)
	{
		if (portalId == 0) { return 0u; }
		const u32 planeInfo = sdisplayList_getPackedPortalInfo(portalId);
		const u32 count  = UNPACK_PORTAL_INFO_COUNT(planeInfo);
		const u32 offset = UNPACK_PORTAL_INFO_OFFSET(planeInfo);
		const Vec4f* planes = &s_displayListPlanes[offset];

		if ((planeType & PLANE_TYPE_BOTH) == PLANE_TYPE_BOTH)
		{
			for (u32 i = 0; i < count; i++)
			{
				outPlanes[i] = planes[i];
			}
			return count;
		}

		u32 finalCount = 0;
		for (u32 i = 0; i < count && finalCount < MAX_PORTAL_PLANES; i++)
		{
			if ((planes[i].y > 0.0f && (planeType & PLANE_TYPE_TOP)) || (planes[i].y < 0.0f && (planeType & PLANE_TYPE_BOT)))
			{
				outPlanes[finalCount++] = planes[i];
			}
		}
		return finalCount;
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
				s_maxPlaneCount = max((s32)count, (s32)s_maxPlaneCount);
				assert(s_maxPlaneCount <= FRUSTUM_PLANE_MAX);

				// Add left and right planes if there is enough room...
				// This is so that caps are properly clipped.
				if (count <= 6)
				{
					for (s32 i = 0; i < clipped.vertexCount && count < MAX_PORTAL_PLANES; i++)
					{
						const s32 a = i, b = (i + 1) % clipped.vertexCount;
						if (fabsf(clipped.vtx[a].x - clipped.vtx[b].x) <= FLT_EPSILON && fabsf(clipped.vtx[a].z - clipped.vtx[b].z) <= FLT_EPSILON)
						{
							Vec3f edge[] = { clipped.vtx[a], clipped.vtx[b] };
							plane[count++] = frustum_calculatePlaneFromEdge(edge);
						}
					}
				}
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

			// Add left and right planes if there is enough room...
			// This is so that caps are properly clipped.
			const Vec3f leftEdge[] =
			{
				{ p0.x, p0.y, p0.z },
				{ p0.x, p1.y, p0.z },
			};
			const Vec3f rightEdge[] =
			{
				{ p1.x, p1.y, p1.z },
				{ p1.x, p0.y, p1.z },
			};

			s_portalFrustumVert[s_displayPortalCount].planeCount += 2;
			s_portalFrustumVert[s_displayPortalCount].planes[2] = frustum_calculatePlaneFromEdge(leftEdge);
			s_portalFrustumVert[s_displayPortalCount].planes[3] = frustum_calculatePlaneFromEdge(rightEdge);
		}

		const u32 planeCount = min((s32)MAX_PORTAL_PLANES, (s32)(s_portalFrustumVert[s_displayPortalCount].planeCount));
		if (s_displayPortalCount + planeCount < MAX_BUFFER_SIZE)
		{
			s_portalPlaneInfo[s_displayPortalCount] = PACK_PORTAL_INFO(s_displayPlaneCount, planeCount);

			// The new planes either match the parent or are created from the edges.
			const Frustum* frust = &s_portalFrustumVert[s_displayPortalCount];
			Vec4f* outPlanes = &s_displayListPlanes[s_displayPlaneCount];
			for (u32 i = 0; i < planeCount; i++)
			{
				outPlanes[i] = frust->planes[i];
				s_displayPlaneCount++;
			}

			s_displayCurrentPortalId = 1 + s_displayPortalCount;
			s_displayPortalCount++;
		}
		else
		{
			TFE_System::logWrite(LOG_WARNING, "GPU Renderer", "Too many portal planes.");
			assert(0);
		}
		return true;
	}

	void addDisplayListItem(const Vec4f pos, const Vec4ui data, const SectorPass bufferIndex)
	{
		assert(s_displayListCount[bufferIndex] < MAX_DISP_ITEMS);
		if (s_displayListCount[bufferIndex] >= MAX_DISP_ITEMS)
		{
			return;
		}

		const s32 index = s_displayListCount[bufferIndex] + MAX_DISP_ITEMS*bufferIndex;
		s_displayListCount[bufferIndex]++;

		s_displayListPos[index] = pos;
		s_displayListData[index] = data;
	}
		
	/*********************************
	Current GPU Renderer Limits:
	* Walls - 65536 per sector   (because of wallGpuId)
	          16.7M total due to storing wall start as a float.
	* Textures (unique) - 16384  (because of texture packer)
	* Sectors - 4M               (because of nextId, 0xfffffc00 = no sector)
	**********************************/
	void sdisplayList_addSegment(RSector* curSector, GPUCachedSector* cached, SegmentClipped* wallSeg, bool forceTreatAsSolid)
	{
		s32 wallId = wallSeg->seg->id;
		RWall* srcWall = &curSector->walls[wallId];
		RSector* nextSector = srcWall->nextSector;
		// Mark only visible walls as being rendered.
		srcWall->seen = JTRUE;

		// Limit 65536 walls **per sector**.
		u32 wallGpuId = u32(wallId) << 16u;
		u32 flip = (((srcWall->flags1 & WF1_FLIP_HORIZ) != 0) ? 1 : 0) << 6u;
		// Add 32 so the value is unsigned and easy to decode in the shader (just subtract 32).
		// Values should never to larger than [-31,31] but clamp just in case (larger values would have no effect anyway).
		u32 wallLight = u32(32 + clamp(floor16(srcWall->wallLight), -31, 31));
		u32 nextId = nextSector ? u32(nextSector->index) << 10u : 0xfffffc00;
		u32 portalInfo = sdisplayList_getPackedPortalInfo(s_displayCurrentPortalId) << 7u;

		assert(srcWall->sector == curSector);
		const bool noWallDraw = (curSector->flags1 & SEC_FLAGS1_NOWALL_DRAW) && ((curSector->flags1 & SEC_FLAGS1_EXTERIOR) || (curSector->flags1 & SEC_FLAGS1_PIT));
		bool noTop = false;
		bool stretchToTop = false;

		Vec4f pos = { wallSeg->v0.x, wallSeg->v0.z, wallSeg->v1.x, wallSeg->v1.z };
		const Vec4ui data = {  nextId/*partId | nextSector*/, (u32)curSector->index/*sectorId*/,
				    		   wallLight | portalInfo, 0u/*textureId*/ };

		if (nextSector)
		{
			if (!(curSector->flags1 & SEC_FLAGS1_EXTERIOR) && (nextSector->flags1 & SEC_FLAGS1_EXTERIOR) && curSector->ceilingHeight < nextSector->ceilingHeight)
			{
				noTop = true;
			}
			if ((curSector->flags1 & SEC_FLAGS1_EXTERIOR) && (curSector->flags1 & SEC_FLAGS1_EXT_ADJ) && (srcWall->flags1 & WF1_ADJ_MID_TEX) &&
				(nextSector->flags1 & SEC_FLAGS1_EXTERIOR) && (nextSector->flags1 & SEC_FLAGS1_EXT_ADJ))
			{
				stretchToTop = true;
			}

			// At least one mod has a "no wall" scenario where an invalid adjoin is causing a gap to show up in a sky.
			// In software, this is handled because of the way "no wall" drawing works.
			// For GPU Rendering, we have to manually check if the adjoin is valid.
			if ((curSector->flags1 & SEC_FLAGS1_EXTERIOR) && (curSector->flags1 & SEC_FLAGS1_NOWALL_DRAW))
			{
				// Check to see if the next sector is valid.
				if (srcWall->mirrorWall)
				{
					fixed16_16 dx = TFE_Jedi::abs(srcWall->w0->x - srcWall->mirrorWall->w1->x);
					fixed16_16 dz = TFE_Jedi::abs(srcWall->w0->z - srcWall->mirrorWall->w1->z);
					if (dx > BAD_ADJOIN_THRES || dz > BAD_ADJOIN_THRES)
					{
						nextSector = nullptr;
					}
				}
				else
				{
					nextSector = nullptr;
				}
			}
		}

		//////////////////////////////
		// Mid
		//////////////////////////////
		if (!nextSector || forceTreatAsSolid)
		{
			if (noWallDraw || forceTreatAsSolid)
			{
				// If the floor and ceiling textures are different, split into two.
				// Since no next sector is required, this is treated as a solid wall, the space is re-used as flags to differeniate between the three cases:
				// (0) Both above and below the horizon using the same texture (the ceiling).
				// (1) Below the horizon line, using the floor texture.
				// (2) Above the horizon line, using the ceiling texture.
				if (curSector->floorTex == curSector->ceilTex)
				{
					// Above AND below the horizon line (single quad).
					u32 flags = 0;
					addDisplayListItem(pos, { flags | SPARTID_WALL_MID | SPARTID_SKY, data.y, data.z,
						wallGpuId | (curSector->ceilTex && *curSector->ceilTex ? (*curSector->ceilTex)->textureId : 0) }, SECTOR_PASS_OPAQUE);
				}
				else
				{
					// Below the horizon line.
					u32 flags = ((curSector->flags1 & SEC_FLAGS1_PIT) || forceTreatAsSolid) ? (1 << 10) : (3 << 10);
					u32 texId = ((curSector->flags1 & SEC_FLAGS1_PIT) || forceTreatAsSolid) ?
						(curSector->floorTex && *curSector->floorTex ? (*curSector->floorTex)->textureId : 0) :
						(srcWall->midTex && *srcWall->midTex ? (*srcWall->midTex)->textureId : 0);
					addDisplayListItem(pos, { flags | SPARTID_WALL_MID | SPARTID_SKY, data.y, data.z,
						wallGpuId | texId }, SECTOR_PASS_OPAQUE);

					// Above the horizon line.
					texId = ((curSector->flags1 & SEC_FLAGS1_EXTERIOR) || forceTreatAsSolid) ?
						(curSector->ceilTex && *curSector->ceilTex ? (*curSector->ceilTex)->textureId : 0) :
						(srcWall->midTex && *srcWall->midTex ? (*srcWall->midTex)->textureId : 0);
					flags = ((curSector->flags1 & SEC_FLAGS1_EXTERIOR) || forceTreatAsSolid) ? (2 << 10) : (4 << 10);
					addDisplayListItem(pos, { flags | SPARTID_WALL_MID | SPARTID_SKY, data.y, data.z,
						wallGpuId | texId }, SECTOR_PASS_OPAQUE);
				}
			}
			else
			{
				addDisplayListItem(pos, {data.x | SPARTID_WALL_MID, data.y, data.z | flip,
					wallGpuId | (srcWall->midTex && *srcWall->midTex ? (*srcWall->midTex)->textureId : 0) }, SECTOR_PASS_OPAQUE);
			}
		}
		else if (srcWall->midTex && (*srcWall->midTex) && nextSector && (srcWall->flags1 & WF1_ADJ_MID_TEX))
		{
			// Funky stretching adjoins...
			if (!(curSector->flags1 & SEC_FLAGS1_EXTERIOR) && (nextSector->flags1 & SEC_FLAGS1_EXTERIOR) && curSector->ceilingHeight < nextSector->ceilingHeight)
			{
				// Transparent mid-texture.
				addDisplayListItem(pos, { data.x | SPARTID_WALL_MID | SPARTID_STRETCH, data.y, data.z | flip,
					wallGpuId | (*srcWall->midTex ? (*srcWall->midTex)->textureId : 0) }, SECTOR_PASS_TRANS);
			}
			else if (stretchToTop)
			{
				// Transparent mid-texture.
				addDisplayListItem(pos, { data.x | SPARTID_WALL_MID | SPARTID_STRETCH_TO_TOP, data.y, data.z | flip,
					wallGpuId | (*srcWall->midTex ? (*srcWall->midTex)->textureId : 0) }, SECTOR_PASS_TRANS);
			}
			else
			{
				// Transparent mid-texture.
				addDisplayListItem(pos, {data.x | SPARTID_WALL_MID, data.y, data.z | flip,
					wallGpuId | (*srcWall->midTex ? (*srcWall->midTex)->textureId : 0) }, SECTOR_PASS_TRANS);
			}
		}

		//////////////////////////////
		// Top and Bottom
		//////////////////////////////
		if ((srcWall->drawFlags & WDF_TOP) && nextSector && !(nextSector->flags1 & SEC_FLAGS1_EXT_ADJ) && !noWallDraw && !noTop)
		{
			addDisplayListItem(pos, {data.x | SPARTID_WALL_TOP, data.y, data.z | flip,
				wallGpuId | (srcWall->topTex && *srcWall->topTex ? (*srcWall->topTex)->textureId : 0) }, SECTOR_PASS_OPAQUE);
		}
		else if ((srcWall->drawFlags & WDF_TOP) && nextSector && noWallDraw && !noTop)
		{
			addDisplayListItem(pos, { data.x | SPARTID_WALL_TOP | SPARTID_SKY, data.y, data.z | flip,
				wallGpuId | (curSector->ceilTex && *curSector->ceilTex ? (*curSector->ceilTex)->textureId : 0u) }, SECTOR_PASS_OPAQUE);
		}
		else if ((srcWall->drawFlags & WDF_TOP) && nextSector && (nextSector->flags1 & SEC_FLAGS1_EXT_ADJ) && !(curSector->flags1 & SEC_FLAGS1_EXTERIOR) && !noTop)
		{
			addDisplayListItem(pos, { data.x | SPARTID_WALL_TOP | SPARTID_SKY, data.y, data.z | flip,
				wallGpuId | (nextSector->ceilTex && *nextSector->ceilTex ? (*nextSector->ceilTex)->textureId : 0u) }, SECTOR_PASS_OPAQUE);
		}

		if ((srcWall->drawFlags & WDF_BOT) && nextSector && !(nextSector->flags1 & SEC_FLAGS1_EXT_FLOOR_ADJ) && !noWallDraw)
		{
			addDisplayListItem(pos, { data.x | SPARTID_WALL_BOT, data.y, data.z | flip,
				wallGpuId | (srcWall->botTex && *srcWall->botTex ? (*srcWall->botTex)->textureId : 0) }, SECTOR_PASS_OPAQUE);
		}
		else if ((srcWall->drawFlags & WDF_BOT) && nextSector && noWallDraw)
		{
			addDisplayListItem(pos, { data.x | SPARTID_WALL_BOT | SPARTID_SKY, data.y, data.z | flip,
				wallGpuId | (curSector->floorTex && *curSector->floorTex ? (*curSector->floorTex)->textureId : 0u) }, SECTOR_PASS_OPAQUE);
		}
		// If there is an exterior pit adjoin, we only add an item if the current sector is *not* a pit.
		else if ((srcWall->drawFlags & WDF_BOT) && nextSector && (nextSector->flags1 & SEC_FLAGS1_EXT_FLOOR_ADJ) && !(curSector->flags1 & SEC_FLAGS1_PIT))
		{
			addDisplayListItem(pos, { data.x | SPARTID_WALL_BOT | SPARTID_SKY, data.y, data.z | flip,
				wallGpuId | (nextSector->floorTex && *nextSector->floorTex ? (*nextSector->floorTex)->textureId : 0u) }, SECTOR_PASS_OPAQUE);
		}

		//////////////////////////////
		// Sign
		//////////////////////////////
		if (srcWall->signTex && *srcWall->signTex)
		{
			const u32 signGpuId = wallGpuId | (*srcWall->signTex)->textureId;
			u32 signFlags = data.x;
			if (srcWall->flags1 & WF1_ILLUM_SIGN)
			{
				signFlags |= SPARTID_FULLBRIGHT;
			}
			if (!((*srcWall->signTex)->flags & OPACITY_TRANS))
			{
				signFlags |= SPARTID_OPAQUE;
			}
			
			// If there is a bottom texture, it goes there..
			if ((srcWall->drawFlags & WDF_BOT) && nextSector && !(nextSector->flags1 & SEC_FLAGS1_EXT_FLOOR_ADJ))
			{
				addDisplayListItem(pos, { signFlags | SPARTID_WALL_BOT_SIGN, data.y, data.z, signGpuId }, SECTOR_PASS_TRANS);
			}
			// Otherwise if there is a top
			else if ((srcWall->drawFlags & WDF_TOP) && nextSector && !(nextSector->flags1 & SEC_FLAGS1_EXT_ADJ))
			{
				addDisplayListItem(pos, { signFlags | SPARTID_WALL_TOP_SIGN, data.y, data.z, signGpuId }, SECTOR_PASS_TRANS);
			}
			// And finally mid.
			else if (srcWall->midTex && srcWall->drawFlags == WDF_MIDDLE && !nextSector)
			{
				addDisplayListItem(pos, { signFlags | SPARTID_WALL_MID_SIGN, data.y, data.z, signGpuId }, SECTOR_PASS_TRANS);
			}
		}

		//////////////////////////////
		// Flats
		//////////////////////////////
		u32 floorSkyFlags = 0u;
		u32 ceilSkyFlags = 0u;
		if (curSector->flags1 & SEC_FLAGS1_PIT)
		{
			floorSkyFlags |= SPARTID_SKY;
			// Special handling for the NoWall flag.
			if (nextSector && (nextSector->flags1 & SEC_FLAGS1_EXT_FLOOR_ADJ) && !(curSector->flags1 & SEC_FLAGS1_NOWALL_DRAW))
			{
				floorSkyFlags |= SPARTID_SKY_ADJ;
			}
		}
		if (curSector->flags1 & SEC_FLAGS1_EXTERIOR)
		{
			ceilSkyFlags |= SPARTID_SKY;
			// Special handling for the NoWall flag.
			if (nextSector && (nextSector->flags1 & SEC_FLAGS1_EXT_ADJ) && !(curSector->flags1 & SEC_FLAGS1_NOWALL_DRAW))
			{
				ceilSkyFlags |= SPARTID_SKY_ADJ;
			}
		}

		// Floor
		addDisplayListItem(pos, {data.x | SPARTID_FLOOR | floorSkyFlags, data.y, data.z,
			curSector->floorTex && *curSector->floorTex ? (*curSector->floorTex)->textureId : 0u }, SECTOR_PASS_OPAQUE);
		addDisplayListItem(pos, { data.x | SPARTID_FLOOR_CAP | floorSkyFlags, data.y, data.z,
			curSector->floorTex && *curSector->floorTex ? (*curSector->floorTex)->textureId : 0u }, SECTOR_PASS_OPAQUE);

		// Ceiling
		addDisplayListItem(pos, { data.x | SPARTID_CEILING | ceilSkyFlags, data.y, data.z,
			curSector->ceilTex && *curSector->ceilTex ? (*curSector->ceilTex)->textureId : 0u }, SECTOR_PASS_OPAQUE);
		addDisplayListItem(pos, { data.x | SPARTID_CEIL_CAP | ceilSkyFlags, data.y, data.z,
			curSector->ceilTex && *curSector->ceilTex ? (*curSector->ceilTex)->textureId : 0u }, SECTOR_PASS_OPAQUE);
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

		TFE_RenderBackend::drawIndexedTriangles(2 * s_displayListCount[passId], sizeof(u32));

		s_displayListPosGPU[passId].unbind(s_posIndex[passId]);
		s_displayListDataGPU[passId].unbind(s_dataIndex[passId]);
		s_displayListPlanesGPU.unbind(s_planesIndex);
	}
}
