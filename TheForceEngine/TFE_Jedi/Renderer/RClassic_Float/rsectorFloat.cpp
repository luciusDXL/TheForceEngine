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

#include "rclassicFloat.h"
#include "rsectorFloat.h"
#include "rflatFloat.h"
#include "rlightingFloat.h"
#include "redgePairFloat.h"
#include "rclassicFloatSharedState.h"
#include "robj3d_float/robj3dFloat.h"
#include "../rcommon.h"

using namespace TFE_Jedi::RClassic_Float;
#define PTR_OFFSET(ptr, base) size_t((u8*)ptr - (u8*)base)

namespace TFE_Jedi
{
	namespace
	{
		static TFE_Sectors_Float* s_ctx = nullptr;

		s32 wallSortX(const void* r0, const void* r1)
		{
			return ((const RWallSegmentFloat*)r0)->wallX0 - ((const RWallSegmentFloat*)r1)->wallX0;
		}

		s32 sortObjectsFloat(const void* r0, const void* r1)
		{
			SecObject* obj0 = *((SecObject**)r0);
			SecObject* obj1 = *((SecObject**)r1);

			const SectorCached* cached0 = &s_ctx->m_cachedSectors[obj0->sector->index];
			const SectorCached* cached1 = &s_ctx->m_cachedSectors[obj1->sector->index];

			if (obj0->type == OBJ_TYPE_3D && obj1->type == OBJ_TYPE_3D)
			{
				// Both objects are 3D.
				const f32 distSq0 = dotFloat(cached0->objPosVS[obj0->index], cached0->objPosVS[obj0->index]);
				const f32 distSq1 = dotFloat(cached1->objPosVS[obj1->index], cached1->objPosVS[obj1->index]);
				const f32 dist0 = sqrtf(distSq0);
				const f32 dist1 = sqrtf(distSq1);
				
				if (obj0->model->isBridge && obj1->model->isBridge)
				{
					return signZero(dist1 - dist0);
				}
				else if (obj0->model->isBridge == 1)
				{
					return -1;
				}
				else if (obj1->model->isBridge == 1)
				{
					return 1;
				}

				return signZero(dist1 - dist0);
			}
			else if (obj0->type == OBJ_TYPE_3D && obj0->model->isBridge)
			{
				return -1;
			}
			else if (obj1->type == OBJ_TYPE_3D && obj1->model->isBridge)
			{
				return 1;
			}

			// Default case:
			return signZero(cached1->objPosVS[obj1->index].z - cached0->objPosVS[obj0->index].z);
		}

		s32 cullObjects(RSector* sector, SecObject** buffer)
		{
			s32 drawCount = 0;
			SecObject** obj = sector->objectList;
			s32 count = sector->objectCount;

			const SectorCached* cached = &s_ctx->m_cachedSectors[sector->index];

			for (s32 i = count - 1; i >= 0 && drawCount < MAX_VIEW_OBJ_COUNT; i--, obj++)
			{
				// Search for the next allocated object.
				SecObject* curObj = *obj;
				while (!curObj)
				{
					obj++;
					curObj = *obj;
				}

				if (curObj->flags & OBJ_FLAG_NEEDS_TRANSFORM)
				{
					const s32 type = curObj->type;
					if (type == OBJ_TYPE_SPRITE || type == OBJ_TYPE_FRAME)
					{
						if (cached->objPosVS[curObj->index].z >= 1.0f)
						{
							buffer[drawCount++] = curObj;
						}
					}
					else if (type == OBJ_TYPE_3D)
					{
						const f32 radius = fixed16ToFloat(curObj->model->radius);
						const f32 zMax = cached->objPosVS[curObj->index].z + radius;
						// Near plane
						if (zMax < 1.0f) { continue; }

						// Left plane
						const f32 xMax = cached->objPosVS[curObj->index].x + radius;
						if (xMax < -zMax) { continue; }

						// Right plane
						const f32 xMin = cached->objPosVS[curObj->index].x - radius;
						if (xMin > zMax) { continue; }

						// The object straddles the near plane, so add it and move on.
						const f32 zMin = cached->objPosVS[curObj->index].z - radius;
						if (zMin < FLT_EPSILON)
						{
							buffer[drawCount++] = curObj;
							continue;
						}

						// Cull against the current "window."
						const f32 rcpZ = 1.0f / cached->objPosVS[curObj->index].z;
						const s32 x0 = roundFloat((xMin*s_rcfltState.focalLength)*rcpZ) + s_screenXMid;
						if (x0 > s_windowMaxX_Pixels) { continue; }

						const s32 x1 = roundFloat((xMax*s_rcfltState.focalLength)*rcpZ) + s_screenXMid;
						if (x1 < s_windowMinX_Pixels) { continue; }

						// Finally add the object to render.
						buffer[drawCount++] = curObj;
					}
				}
			}

			return drawCount;
		}

		void sprite_drawWax(s32 angle, SecObject* obj, vec3_float* cachedPosVS)
		{
			// Angles range from [0, 16384), divide by 512 to get 32 even buckets.
			s32 angleDiff = (angle - obj->yaw) >> 9;
			angleDiff &= 31;	// up to 32 views

			// Get the animation based on the object state.
			Wax* wax = obj->wax;
			WaxAnim* anim = WAX_AnimPtr(wax, obj->anim & 0x1f);
			if (anim)
			{
				// Then get the Sequence from the angle difference.
				WaxView* view = WAX_ViewPtr(wax, anim, 31 - angleDiff);
				// And finall the frame from the current sequence.
				WaxFrame* frame = WAX_FramePtr(wax, view, obj->frame & 0x1f);
				// Draw the frame.
				sprite_drawFrame((u8*)wax, frame, obj, cachedPosVS);
			}
		}
	}

	void TFE_Sectors_Float::reset()
	{
		m_cachedSectors = nullptr;
		m_cachedSectorCount = 0;
	}

	void TFE_Sectors_Float::prepare()
	{
		allocateCachedData();

		EdgePairFloat* flatEdge = &s_rcfltState.flatEdgeList[s_flatCount];
		s_rcfltState.flatEdge = flatEdge;
		flat_addEdges(s_screenWidth, s_minScreenX_Pixels, 0, s_rcfltState.windowMaxY, 0, s_rcfltState.windowMinY);
	}

	void transformPointByCameraFixedToFloat(vec3_fixed* worldPoint, vec3_float* viewPoint)
	{
		const f32 x = fixed16ToFloat(worldPoint->x);
		const f32 y = fixed16ToFloat(worldPoint->y);
		const f32 z = fixed16ToFloat(worldPoint->z);

		viewPoint->x = x*s_rcfltState.cosYaw + z*s_rcfltState.sinYaw + s_rcfltState.cameraTrans.x;
		viewPoint->y = y - s_rcfltState.eyeHeight;
		viewPoint->z = z*s_rcfltState.cosYaw + x*s_rcfltState.negSinYaw + s_rcfltState.cameraTrans.z;
	}
	
	void TFE_Sectors_Float::draw(RSector* sector)
	{
		s_ctx = this;
		s_curSector = sector;
		s_sectorIndex++;
		s_adjoinIndex++;
		if (s_adjoinIndex > s_maxAdjoinIndex)
		{
			s_maxAdjoinIndex = s_adjoinIndex;
		}

		s32* winTop = &s_windowTop_all[(s_adjoinDepth - 1) * s_width];
		s32* winBot = &s_windowBot_all[(s_adjoinDepth - 1) * s_width];
		s32* winTopNext = &s_windowTop_all[s_adjoinDepth * s_width];
		s32* winBotNext = &s_windowBot_all[s_adjoinDepth * s_width];

		s_rcfltState.depth1d = &s_rcfltState.depth1d_all[(s_adjoinDepth - 1) * s_width];

		s32 startWall = s_curSector->startWall;
		s32 drawWallCount = s_curSector->drawWallCnt;

		if (s_flatLighting)
		{
			s_sectorAmbient = s_flatAmbient;
		}
		else
		{
			s_sectorAmbient = round16(s_curSector->ambient);
		}
		s_scaledAmbient = (s_sectorAmbient >> 1) + (s_sectorAmbient >> 2) + (s_sectorAmbient >> 3);
		s_sectorAmbientFraction = s_sectorAmbient << 11;	// fraction of ambient compared to max.

		s_windowTop = winTop;
		s_windowBot = winBot;
		f32* depthPrev = nullptr;
		if (s_adjoinDepth > 1)
		{
			depthPrev = &s_rcfltState.depth1d_all[(s_adjoinDepth - 2) * s_width];
			memcpy(&s_rcfltState.depth1d[s_minScreenX_Pixels], &depthPrev[s_minScreenX_Pixels], s_width * 4);
		}

		s_wallMaxCeilY  = s_windowMinY_Pixels;
		s_wallMinFloorY = s_windowMaxY_Pixels;
		SectorCached* cachedSector = &m_cachedSectors[s_curSector->index];

		if (s_drawFrame != s_curSector->prevDrawFrame)
		{
			TFE_ZONE_BEGIN(secUpdateCache, "Update Sector Cache");
				updateCachedSector(cachedSector, s_curSector->dirtyFlags);
			TFE_ZONE_END(secUpdateCache);

			TFE_ZONE_BEGIN(secXform, "Sector Vertex Transform");
				vec2_fixed* vtxWS = s_curSector->verticesWS;
				vec2_float* vtxVS = cachedSector->verticesVS;
				for (s32 v = 0; v < s_curSector->vertexCount; v++)
				{
					const f32 x = fixed16ToFloat(vtxWS->x);
					const f32 z = fixed16ToFloat(vtxWS->z);

					vtxVS->x = x*s_rcfltState.cosYaw     + z*s_rcfltState.sinYaw + s_rcfltState.cameraTrans.x;
					vtxVS->z = x*s_rcfltState.negSinYaw  + z*s_rcfltState.cosYaw + s_rcfltState.cameraTrans.z;
					vtxVS++;
					vtxWS++;
				}
			TFE_ZONE_END(secXform);

			TFE_ZONE_BEGIN(objXform, "Sector Object Transform");
				SecObject** obj = s_curSector->objectList;
				vec3_float* objPosVS = cachedSector->objPosVS;
				for (s32 i = s_curSector->objectCount - 1; i >= 0; i--, obj++)
				{
					SecObject* curObj = *obj;
					while (!curObj)
					{
						obj++;
						curObj = *obj;
					}

					if (curObj->flags & OBJ_FLAG_NEEDS_TRANSFORM)
					{
						transformPointByCameraFixedToFloat(&curObj->posWS, &objPosVS[curObj->index]);
					}
				}
			TFE_ZONE_END(objXform);

			TFE_ZONE_BEGIN(wallProcess, "Sector Wall Process");
				startWall = s_nextWall;
				WallCached* wall = cachedSector->cachedWalls;
				for (s32 i = 0; i < s_curSector->wallCount; i++, wall++)
				{
					wall_process(wall);
				}
				drawWallCount = s_nextWall - startWall;

				s_curSector->startWall = startWall;
				s_curSector->drawWallCnt = drawWallCount;
				s_curSector->prevDrawFrame = s_drawFrame;
			TFE_ZONE_END(wallProcess);
		}

		RWallSegmentFloat* wallSegment = &s_rcfltState.wallSegListDst[s_curWallSeg];
		s32 drawSegCnt = wall_mergeSort(wallSegment, MAX_SEG - s_curWallSeg, startWall, drawWallCount);
		s_curWallSeg += drawSegCnt;

		TFE_ZONE_BEGIN(wallQSort, "Wall QSort");
			qsort(wallSegment, drawSegCnt, sizeof(RWallSegmentFloat), wallSortX);
		TFE_ZONE_END(wallQSort);

		s32 flatCount = s_flatCount;
		EdgePairFloat* flatEdge = &s_rcfltState.flatEdgeList[s_flatCount];
		s_rcfltState.flatEdge = flatEdge;

		s32 adjoinStart = s_adjoinSegCount;
		EdgePairFloat* adjoinEdges = &s_rcfltState.adjoinEdgeList[adjoinStart];
		RWallSegmentFloat* adjoinList[MAX_ADJOIN_DEPTH];

		s_rcfltState.adjoinEdge = adjoinEdges;
		s_rcfltState.adjoinSegment = adjoinList;

		// Draw each wall segment in the sector.
		TFE_ZONE_BEGIN(secDrawWalls, "Draw Walls");
		for (s32 i = 0; i < drawSegCnt; i++, wallSegment++)
		{
			RWall* srcWall = wallSegment->srcWall->wall;
			RSector* nextSector = srcWall->nextSector;

			if (!nextSector)
			{
				wall_drawSolid(wallSegment);
			}
			else
			{
				const s32 df = srcWall->drawFlags;
				assert(df >= 0);
				if (df <= WDF_MIDDLE)
				{
					if (df == WDF_MIDDLE || (nextSector->flags1 & SEC_FLAGS1_EXT_FLOOR_ADJ))
					{
						wall_drawMask(wallSegment);
					}
					else
					{
						wall_drawBottom(wallSegment);
					}
				}
				else if (df == WDF_TOP)
				{
					if (nextSector->flags1 & SEC_FLAGS1_EXT_ADJ)
					{
						wall_drawMask(wallSegment);
					}
					else
					{
						wall_drawTop(wallSegment);
					}
				}
				else if (df == WDF_TOP_AND_BOT)
				{
					if ((nextSector->flags1 & SEC_FLAGS1_EXT_ADJ) && (nextSector->flags1 & SEC_FLAGS1_EXT_FLOOR_ADJ))
					{
						wall_drawMask(wallSegment);
					}
					else if (nextSector->flags1 & SEC_FLAGS1_EXT_ADJ)
					{
						wall_drawBottom(wallSegment);
					}
					else if (nextSector->flags1 & SEC_FLAGS1_EXT_FLOOR_ADJ)
					{
						wall_drawTop(wallSegment);
					}
					else
					{
						wall_drawTopAndBottom(wallSegment);
					}
				}
				else // WDF_BOT
				{
					if (nextSector->flags1 & SEC_FLAGS1_EXT_FLOOR_ADJ)
					{
						wall_drawMask(wallSegment);
					}
					else
					{
						wall_drawBottom(wallSegment);
					}
				}
			}
		}
		TFE_ZONE_END(secDrawWalls);

		TFE_ZONE_BEGIN(secDrawFlats, "Draw Flats");
			// Draw flats
			// Note: in the DOS code flat drawing functions are called through function pointers.
			// Since the function pointers always seem to be the same, the functions are called directly in this code.
			// Most likely this was used for testing or debug drawing and may be added back in the future.
			const s32 newFlatCount = s_flatCount - flatCount;
			if (s_curSector->flags1 & SEC_FLAGS1_EXTERIOR)
			{
				if (s_curSector->flags1 & SEC_FLAGS1_NOWALL_DRAW)
				{
					wall_drawSkyTopNoWall(s_curSector);
				}
				else
				{
					wall_drawSkyTop(s_curSector);
				}
			}
			else
			{
				flat_drawCeiling(cachedSector, flatEdge, newFlatCount);
			}
			if (s_curSector->flags1 & SEC_FLAGS1_PIT)
			{
				if (s_curSector->flags1 & SEC_FLAGS1_NOWALL_DRAW)
				{
					wall_drawSkyBottomNoWall(s_curSector);
				}
				else
				{
					wall_drawSkyBottom(s_curSector);
				}
			}
			else
			{
				flat_drawFloor(cachedSector, flatEdge, newFlatCount);
			}
		TFE_ZONE_END(secDrawFlats);

		// Adjoins
		s32 adjoinCount = s_adjoinSegCount - adjoinStart;
		if (adjoinCount && s_adjoinDepth < MAX_ADJOIN_DEPTH)
		{
			adjoin_setupAdjoinWindow(winBot, winBotNext, winTop, winTopNext, adjoinEdges, adjoinCount);
			RWallSegmentFloat** seg = adjoinList;
			RWallSegmentFloat* prevAdjoinSeg = nullptr;
			RWallSegmentFloat* curAdjoinSeg  = nullptr;

			s32 adjoinEnd = adjoinCount - 1;
			for (s32 i = 0; i < adjoinCount; i++, seg++, adjoinEdges++)
			{
				prevAdjoinSeg = curAdjoinSeg;
				curAdjoinSeg = *seg;

				RWall* srcWall = curAdjoinSeg->srcWall->wall;
				RWallSegmentFloat* nextAdjoin = (i < adjoinEnd) ? *(seg + 1) : nullptr;
				RSector* nextSector = srcWall->nextSector;
				if (s_adjoinDepth < MAX_ADJOIN_DEPTH && s_adjoinDepth < s_maxDepthCount)
				{
					s32 index = s_adjoinDepth - 1;
					saveValues(index);

					adjoin_computeWindowBounds(adjoinEdges);
					s_adjoinDepth++;
					if (s_adjoinDepth > s_maxAdjoinDepth)
					{
						s_maxAdjoinDepth = s_adjoinDepth;
					}

					srcWall->drawFrame = s_drawFrame;
					s_windowTop = winTopNext;
					s_windowBot = winBotNext;
					if (prevAdjoinSeg != 0)
					{
						if (prevAdjoinSeg->wallX1 + 1 == curAdjoinSeg->wallX0)
						{
							s_windowX0 = s_windowMinX_Pixels;
						}
					}
					if (nextAdjoin)
					{
						if (curAdjoinSeg->wallX1 == nextAdjoin->wallX0 - 1)
						{
							s_windowX1 = s_windowMaxX_Pixels;
						}
					}

					s_rcfltState.windowMinZ = min(curAdjoinSeg->z0, curAdjoinSeg->z1);
					draw(nextSector);
					
					if (s_adjoinDepth)
					{
						s32 index = s_adjoinDepth - 2;
						s_adjoinDepth--;
						restoreValues(index);
					}
					srcWall->drawFrame = 0;
					if (srcWall->flags1 & WF1_ADJ_MID_TEX)
					{
						TFE_ZONE("Draw Transparent Walls");
						wall_drawTransparent(curAdjoinSeg, adjoinEdges);
					}
				}
			}
		}

		if (!(s_curSector->flags1 & SEC_FLAGS1_SUBSECTOR) && depthPrev && s_drawFrame != s_prevSector->prevDrawFrame2)
		{
			memcpy(&depthPrev[s_windowMinX_Pixels], &s_rcfltState.depth1d[s_windowMinX_Pixels], (s_windowMaxX_Pixels - s_windowMinX_Pixels + 1) * sizeof(f32));
		}

		// Objects
		TFE_ZONE_BEGIN(secDrawObjects, "Draw Objects");
		const s32 objCount = cullObjects(s_curSector, s_objBuffer);
		if (objCount > 0)
		{
			// Which top and bottom edges are we going to use to clip objects?
			s_objWindowTop = s_windowTop;
			if (s_windowMinY_Pixels < s_screenYMidFlt || s_windowMaxCeil < s_screenYMidFlt)
			{
				if (s_prevSector && s_prevSector->ceilingHeight <= s_curSector->ceilingHeight)
				{
					s_objWindowTop = s_windowTopPrev;
				}
			}
			s_objWindowBot = s_windowBot;
			if (s_windowMaxY_Pixels > s_screenYMidFlt || s_windowMinFloor > s_screenYMidFlt)
			{
				if (s_prevSector && s_prevSector->floorHeight >= s_curSector->floorHeight)
				{
					s_objWindowBot = s_windowBotPrev;
				}
			}

			// Sort objects in viewspace (generally back to front but there are special cases).
			qsort(s_objBuffer, objCount, sizeof(SecObject*), sortObjectsFloat);

			// Draw objects in order.
			vec3_float* cachedPosVS = cachedSector->objPosVS;
			for (s32 i = 0; i < objCount; i++)
			{
				SecObject* obj = s_objBuffer[i];
				const s32 type = obj->type;
				if (type == OBJ_TYPE_SPRITE)
				{
					TFE_ZONE("Draw WAX");

					f32 dx = s_rcfltState.cameraPos.x - fixed16ToFloat(obj->posWS.x);
					f32 dz = s_rcfltState.cameraPos.z - fixed16ToFloat(obj->posWS.z);
					s32 angle = vec2ToAngle(dx, dz);

					sprite_drawWax(angle, obj, &cachedPosVS[obj->index]);
				}
				else if (type == OBJ_TYPE_3D)
				{
					TFE_ZONE("Draw 3DO");

					robj3d_draw(obj, obj->model);
				}
				else if (type == OBJ_TYPE_FRAME)
				{
					TFE_ZONE("Draw Frame");

					sprite_drawFrame((u8*)obj->fme, obj->fme, obj, &cachedPosVS[obj->index]);
				}
			}
		}
		TFE_ZONE_END(secDrawObjects);

		s_curSector->flags1 |= SEC_FLAGS1_RENDERED;
		s_curSector->prevDrawFrame2 = s_drawFrame;
	}
		
	void TFE_Sectors_Float::adjoin_setupAdjoinWindow(s32* winBot, s32* winBotNext, s32* winTop, s32* winTopNext, EdgePairFloat* adjoinEdges, s32 adjoinCount)
	{
		TFE_ZONE("Setup Adjoin Window");

		// Note: This is pretty inefficient, especially at higher resolutions.
		// The column loops below can be adjusted to do the copy only in the required ranges.
		memcpy(&winTopNext[s_minScreenX_Pixels], &winTop[s_minScreenX_Pixels], s_width * 4);
		memcpy(&winBotNext[s_minScreenX_Pixels], &winBot[s_minScreenX_Pixels], s_width * 4);

		// Loop through each adjoin and setup the column range based on the edge pair and the parent
		// column range.
		for (s32 i = 0; i < adjoinCount; i++, adjoinEdges++)
		{
			const s32 x0 = adjoinEdges->x0;
			const s32 x1 = adjoinEdges->x1;

			const f32 ceil_dYdX = adjoinEdges->dyCeil_dx;
			f32 y = adjoinEdges->yCeil0;
			for (s32 x = x0; x <= x1; x++, y += ceil_dYdX)
			{
				s32 yPixel = roundFloat(y);
				s32 yBot = winBotNext[x];
				s32 yTop = winTop[x];
				if (yPixel > yTop)
				{
					winTopNext[x] = (yPixel <= yBot) ? yPixel : yBot + 1;
				}
			}
			const f32 floor_dYdX = adjoinEdges->dyFloor_dx;
			y = adjoinEdges->yFloor0;
			for (s32 x = x0; x <= x1; x++, y += floor_dYdX)
			{
				s32 yPixel = roundFloat(y);
				s32 yTop = winTop[x];
				s32 yBot = winBot[x];
				if (yPixel < yBot)
				{
					winBotNext[x] = (yPixel >= yTop) ? yPixel : yTop - 1;
				}
			}
		}
	}

	void TFE_Sectors_Float::adjoin_computeWindowBounds(EdgePairFloat* adjoinEdges)
	{
		s32 yC = adjoinEdges->yPixel_C0;
		if (yC > s_windowMinY_Pixels)
		{
			s_windowMinY_Pixels = yC;
		}
		s32 yF = adjoinEdges->yPixel_F0;
		if (yF < s_windowMaxY_Pixels)
		{
			s_windowMaxY_Pixels = yF;
		}
		yC = adjoinEdges->yPixel_C1;
		if (yC > s_windowMaxCeil)
		{
			s_windowMaxCeil = yC;
		}
		yF = adjoinEdges->yPixel_F1;
		if (yF < s_windowMinFloor)
		{
			s_windowMinFloor = yF;
		}
		s_wallMaxCeilY = s_windowMinY_Pixels - 1;
		s_wallMinFloorY = s_windowMaxY_Pixels + 1;
		s_windowMinX_Pixels = adjoinEdges->x0;
		s_windowMaxX_Pixels = adjoinEdges->x1;
		s_windowTopPrev = s_windowTop;
		s_windowBotPrev = s_windowBot;
		s_prevSector = s_curSector;
	}

	void TFE_Sectors_Float::saveValues(s32 index)
	{
		SectorSaveValues* dst = &s_sectorStack[index];
		dst->curSector = s_curSector;
		dst->prevSector = s_prevSector;
		dst->depth1d = s_rcfltState.depth1d;
		dst->windowX0 = s_windowX0;
		dst->windowX1 = s_windowX1;
		dst->windowMinY = s_windowMinY_Pixels;
		dst->windowMaxY = s_windowMaxY_Pixels;
		dst->windowMaxCeil = s_windowMaxCeil;
		dst->windowMinFloor = s_windowMinFloor;
		dst->wallMaxCeilY = s_wallMaxCeilY;
		dst->wallMinFloorY = s_wallMinFloorY;
		dst->windowMinX = s_windowMinX_Pixels;
		dst->windowMaxX = s_windowMaxX_Pixels;
		dst->windowTop = s_windowTop;
		dst->windowBot = s_windowBot;
		dst->windowTopPrev = s_windowTopPrev;
		dst->windowBotPrev = s_windowBotPrev;
		dst->sectorAmbient = s_sectorAmbient;
		dst->scaledAmbient = s_scaledAmbient;
		dst->sectorAmbientFraction = s_sectorAmbientFraction;
	}

	void TFE_Sectors_Float::restoreValues(s32 index)
	{
		const SectorSaveValues* src = &s_sectorStack[index];
		s_curSector = src->curSector;
		s_prevSector = src->prevSector;
		s_rcfltState.depth1d = (f32*)src->depth1d;
		s_windowX0 = src->windowX0;
		s_windowX1 = src->windowX1;
		s_windowMinY_Pixels = src->windowMinY;
		s_windowMaxY_Pixels = src->windowMaxY;
		s_windowMaxCeil = src->windowMaxCeil;
		s_windowMinFloor = src->windowMinFloor;
		s_wallMaxCeilY = src->wallMaxCeilY;
		s_wallMinFloorY = src->wallMinFloorY;
		s_windowMinX_Pixels = src->windowMinX;
		s_windowMaxX_Pixels = src->windowMaxX;
		s_windowTop = src->windowTop;
		s_windowBot = src->windowBot;
		s_windowTopPrev = src->windowTopPrev;
		s_windowBotPrev = src->windowBotPrev;
		s_sectorAmbient = src->sectorAmbient;
		s_scaledAmbient = src->scaledAmbient;
		s_sectorAmbientFraction = src->sectorAmbientFraction;
	}

	void TFE_Sectors_Float::freeCachedData()
	{
		level_free(m_cachedSectors);
		m_cachedSectors = nullptr;
		m_cachedSectorCount = 0;
	}
		
	void TFE_Sectors_Float::updateCachedWalls(SectorCached* cached, u32 flags)
	{
		if (!(flags & SDF_WALL_CHANGE)) { return; }

		RSector* srcSector = cached->sector;
		if (flags & SDF_INIT_SETUP)
		{
			cached->cachedWalls = (WallCached*)level_alloc(sizeof(WallCached) * srcSector->wallCount);
			memset(cached->cachedWalls, 0, sizeof(WallCached) * srcSector->wallCount);
		}

		for (s32 w = 0; w < srcSector->wallCount; w++)
		{
			WallCached* wcached = &cached->cachedWalls[w];
			RWall* srcWall = &srcSector->walls[w];

			if (flags & SDF_INIT_SETUP)
			{
				wcached->wall = srcWall;
				wcached->sector = cached;
				wcached->v0 = &cached->verticesVS[PTR_OFFSET(srcWall->v0, srcSector->verticesVS) / sizeof(vec2_fixed)];
				wcached->v1 = &cached->verticesVS[PTR_OFFSET(srcWall->v1, srcSector->verticesVS) / sizeof(vec2_fixed)];
			}

			if (flags & SDF_HEIGHTS)
			{
				wcached->topTexelHeight = fixed16ToFloat(srcWall->topTexelHeight);
				wcached->midTexelHeight = fixed16ToFloat(srcWall->midTexelHeight);
				wcached->botTexelHeight = fixed16ToFloat(srcWall->botTexelHeight);
			}

			if (flags & SDF_WALL_OFFSETS)
			{
				wcached->texelLength = fixed16ToFloat(srcWall->texelLength);

				wcached->topOffset.x = fixed16ToFloat(srcWall->topOffset.x);
				wcached->topOffset.z = fixed16ToFloat(srcWall->topOffset.z);
				wcached->midOffset.x = fixed16ToFloat(srcWall->midOffset.x);
				wcached->midOffset.z = fixed16ToFloat(srcWall->midOffset.z);
				wcached->botOffset.x = fixed16ToFloat(srcWall->botOffset.x);
				wcached->botOffset.z = fixed16ToFloat(srcWall->botOffset.z);
				wcached->signOffset.x = fixed16ToFloat(srcWall->signOffset.x);
				wcached->signOffset.z = fixed16ToFloat(srcWall->signOffset.z);
			}

			if (flags & SDF_WALL_SHAPE)
			{
				wcached->wallDir.x = fixed16ToFloat(srcWall->wallDir.x);
				wcached->wallDir.z = fixed16ToFloat(srcWall->wallDir.z);
				wcached->length = fixed16ToFloat(srcWall->length);
			}
		}
	}

	void TFE_Sectors_Float::updateCachedSector(SectorCached* cached, u32 flags)
	{
		RSector* srcSector = cached->sector;

		if (flags & SDF_INIT_SETUP)
		{
			cached->verticesVS = (vec2_float*)level_alloc(sizeof(vec2_float) * srcSector->vertexCount);
		}

		if (flags & SDF_HEIGHTS)
		{
			cached->floorHeight = fixed16ToFloat(srcSector->floorHeight);
			cached->ceilingHeight = fixed16ToFloat(srcSector->ceilingHeight);
		}

		if (flags & SDF_FLAT_OFFSETS)
		{
			cached->floorOffset.x = fixed16ToFloat(srcSector->floorOffset.x);
			cached->floorOffset.z = fixed16ToFloat(srcSector->floorOffset.z);
			cached->ceilOffset.x = fixed16ToFloat(srcSector->ceilOffset.x);
			cached->ceilOffset.z = fixed16ToFloat(srcSector->ceilOffset.z);
		}

		if (cached->objectCapacity < srcSector->objectCapacity)
		{
			cached->objectCapacity = srcSector->objectCapacity;
			cached->objPosVS = (vec3_float*)level_realloc(cached->objPosVS, sizeof(vec3_float) * cached->objectCapacity);
		}

		updateCachedWalls(cached, flags);
		srcSector->dirtyFlags = 0;
	}

	void TFE_Sectors_Float::allocateCachedData()
	{
		if (m_cachedSectorCount && m_cachedSectorCount != s_sectorCount)
		{
			freeCachedData();
		}

		if (!m_cachedSectors)
		{
			m_cachedSectorCount = s_sectorCount;
			m_cachedSectors = (SectorCached*)level_alloc(sizeof(SectorCached) * m_cachedSectorCount);
			memset(m_cachedSectors, 0, sizeof(SectorCached) * m_cachedSectorCount);

			for (u32 i = 0; i < m_cachedSectorCount; i++)
			{
				m_cachedSectors[i].sector = &s_sectors[i];
				updateCachedSector(&m_cachedSectors[i], SDF_ALL);
			}
		}
	}

	// Switch from float to fixed.
	void TFE_Sectors_Float::subrendererChanged()
	{
		freeCachedData();
	}
}