#include <TFE_System/profiler.h>
#include <TFE_Asset/modelAsset_jedi.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>

#include "rclassicFixed.h"
#include "rsectorFixed.h"
#include "rflatFixed.h"
#include "rlightingFixed.h"
#include "redgePairFixed.h"
#include "rclassicFixedSharedState.h"
#include "robj3d_fixed/robj3dFixed.h"
#include "../rcommon.h"

using namespace TFE_Jedi::RClassic_Fixed;

namespace TFE_Jedi
{
	namespace
	{
		s32 wallSortX(const void* r0, const void* r1)
		{
			return ((const RWallSegmentFixed*)r0)->wallX0 - ((const RWallSegmentFixed*)r1)->wallX0;
		}

		s32 sortObjectsFixed(const void* r0, const void* r1)
		{
			SecObject* obj0 = *((SecObject**)r0);
			SecObject* obj1 = *((SecObject**)r1);

			if (obj0->type == OBJ_TYPE_3D && obj1->type == OBJ_TYPE_3D)
			{
				// Both objects are 3D.
				const fixed16_16 distSq0 = dotFixed(obj0->posVS, obj0->posVS);
				const fixed16_16 distSq1 = dotFixed(obj1->posVS, obj1->posVS);
				const fixed16_16 dist0 = fixedSqrt(distSq0);
				const fixed16_16 dist1 = fixedSqrt(distSq1);

				if (obj0->model->isBridge && obj1->model->isBridge)
				{
					return dist1 - dist0;
				}
				else if (obj0->model->isBridge == 1)
				{
					return -1;
				}
				else if (obj1->model->isBridge == 1)
				{
					return 1;
				}

				return dist1 - dist0;
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
			return obj1->posVS.z - obj0->posVS.z;
		}
				
		s32 cullObjects(RSector* sector, SecObject** buffer)
		{
			s32 drawCount = 0;
			SecObject** obj = sector->objectList;
			s32 count = sector->objectCount;

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
						if (curObj->posVS.z >= ONE_16)
						{
							buffer[drawCount++] = curObj;
						}
					}
					else if (type == OBJ_TYPE_3D)
					{
						const fixed16_16 radius = curObj->model->radius;
						const fixed16_16 zMax = curObj->posVS.z + radius;
						// Near plane
						if (zMax < ONE_16) { continue; }

						// Left plane
						const fixed16_16 xMax = curObj->posVS.x + radius;
						if (xMax < -zMax) { continue; }

						// Right plane
						const fixed16_16 xMin = curObj->posVS.x - radius;
						if (xMin > zMax) { continue; }

						// The object straddles the near plane, so add it and move on.
						const fixed16_16 zMin = curObj->posVS.z - radius;
						if (zMin <= 0)
						{
							buffer[drawCount++] = curObj;
							continue;
						}

						// Cull against the current "window."
						const fixed16_16 z = curObj->posVS.z;
						const s32 x0 = round16(div16(mul16(xMin, s_rcfState.focalLength), z)) + s_screenXMid;
						if (x0 > s_windowMaxX_Pixels) { continue; }

						const s32 x1 = round16(div16(mul16(xMax, s_rcfState.focalLength), z)) + s_screenXMid;
						if (x1 < s_windowMinX_Pixels) { continue; }

						// Finally add the object to render.
						buffer[drawCount++] = curObj;
					}
				}
			}

			return drawCount;
		}

		void sprite_drawWax(s32 angle, SecObject* obj)
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
				sprite_drawFrame((u8*)wax, frame, obj);
			}
		}
	}

	void TFE_Sectors_Fixed::prepare()
	{
		EdgePairFixed* flatEdge = &s_rcfState.flatEdgeList[s_flatCount];
		s_rcfState.flatEdge = flatEdge;
		flat_addEdges(s_screenWidth, s_minScreenX_Pixels, 0, s_rcfState.windowMaxY, 0, s_rcfState.windowMinY);
	}

	void TFE_Sectors_Fixed::draw(RSector* sector)
	{
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

		s_rcfState.depth1d = &s_rcfState.depth1d_all[(s_adjoinDepth - 1) * s_width];

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
		fixed16_16* depthPrev = nullptr;
		if (s_adjoinDepth > 1)
		{
			depthPrev = &s_rcfState.depth1d_all[(s_adjoinDepth - 2) * s_width];
			memcpy(&s_rcfState.depth1d[s_minScreenX_Pixels], &depthPrev[s_minScreenX_Pixels], s_width * 4);
		}

		s_wallMaxCeilY = s_windowMinY_Pixels;
		s_wallMinFloorY = s_windowMaxY_Pixels;

		if (s_drawFrame != s_curSector->prevDrawFrame)
		{
			TFE_ZONE_BEGIN(secXform, "Sector Vertex Transform");
				vec2_fixed* vtxWS = s_curSector->verticesWS;
				vec2_fixed* vtxVS = s_curSector->verticesVS;
				for (s32 v = 0; v < s_curSector->vertexCount; v++)
				{
					vtxVS->x = mul16(vtxWS->x, s_rcfState.cosYaw)    + mul16(vtxWS->z, s_rcfState.sinYaw) + s_rcfState.cameraTrans.x;
					vtxVS->z = mul16(vtxWS->x, s_rcfState.negSinYaw) + mul16(vtxWS->z, s_rcfState.cosYaw) + s_rcfState.cameraTrans.z;
					vtxVS++;
					vtxWS++;
				}
			TFE_ZONE_END(secXform);

			TFE_ZONE_BEGIN(objXform, "Sector Object Transform");
				SecObject** obj = s_curSector->objectList;
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
						transformPointByCamera(&curObj->posWS, &curObj->posVS);
					}
				}
			TFE_ZONE_END(objXform);

			TFE_ZONE_BEGIN(wallProcess, "Sector Wall Process");
				startWall = s_nextWall;
				RWall* wall = s_curSector->walls;
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

		RWallSegmentFixed* wallSegment = &s_rcfState.wallSegListDst[s_curWallSeg];
		s32 drawSegCnt = wall_mergeSort(wallSegment, MAX_SEG - s_curWallSeg, startWall, drawWallCount);
		s_curWallSeg += drawSegCnt;

		TFE_ZONE_BEGIN(wallQSort, "Wall QSort");
			qsort(wallSegment, drawSegCnt, sizeof(RWallSegmentFixed), wallSortX);
		TFE_ZONE_END(wallQSort);

		s32 flatCount = s_flatCount;
		EdgePairFixed* flatEdge = &s_rcfState.flatEdgeList[s_flatCount];
		s_rcfState.flatEdge = flatEdge;

		s32 adjoinStart = s_adjoinSegCount;
		EdgePairFixed* adjoinEdges = &s_rcfState.adjoinEdgeList[adjoinStart];
		RWallSegmentFixed* adjoinList[MAX_ADJOIN_DEPTH];

		s_rcfState.adjoinEdge = adjoinEdges;
		s_rcfState.adjoinSegment = adjoinList;

		// Draw each wall segment in the sector.
		TFE_ZONE_BEGIN(secDrawWalls, "Draw Walls");
		for (s32 i = 0; i < drawSegCnt; i++, wallSegment++)
		{
			RWall* srcWall = wallSegment->srcWall;
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
				flat_drawCeiling(s_curSector, flatEdge, newFlatCount);
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
				flat_drawFloor(s_curSector, flatEdge, newFlatCount);
			}
		TFE_ZONE_END(secDrawFlats);

		// Adjoins
		s32 adjoinCount = s_adjoinSegCount - adjoinStart;
		if (adjoinCount && s_adjoinDepth < MAX_ADJOIN_DEPTH)
		{
			adjoin_setupAdjoinWindow(winBot, winBotNext, winTop, winTopNext, adjoinEdges, adjoinCount);
			RWallSegmentFixed** seg = adjoinList;
			RWallSegmentFixed* prevAdjoinSeg = nullptr;
			RWallSegmentFixed* curAdjoinSeg  = nullptr;

			s32 adjoinEnd = adjoinCount - 1;
			for (s32 i = 0; i < adjoinCount; i++, seg++, adjoinEdges++)
			{
				prevAdjoinSeg = curAdjoinSeg;
				curAdjoinSeg = *seg;

				RWall* srcWall = curAdjoinSeg->srcWall;
				RWallSegmentFixed* nextAdjoin = (i < adjoinEnd) ? *(seg + 1) : nullptr;
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

					s_rcfState.windowMinZ = min(curAdjoinSeg->z0, curAdjoinSeg->z1);
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
			memcpy(&depthPrev[s_windowMinX_Pixels], &s_rcfState.depth1d[s_windowMinX_Pixels], (s_windowMaxX_Pixels - s_windowMinX_Pixels + 1) * sizeof(fixed16_16));
		}

		// Objects
		TFE_ZONE_BEGIN(secDrawObjects, "Draw Objects");
		const s32 objCount = cullObjects(s_curSector, s_objBuffer);
		if (objCount > 0)
		{
			// Which top and bottom edges are we going to use to clip objects?
			s_objWindowTop = s_windowTop;
			if (s_windowMinY_Pixels < s_screenYMid || s_windowMaxCeil < s_screenYMid)
			{
				if (s_prevSector && s_prevSector->ceilingHeight <= s_curSector->ceilingHeight)
				{
					s_objWindowTop = s_windowTopPrev;
				}
			}
			s_objWindowBot = s_windowBot;
			if (s_windowMaxY_Pixels > s_screenYMid || s_windowMinFloor > s_screenYMid)
			{
				if (s_prevSector && s_prevSector->floorHeight >= s_curSector->floorHeight)
				{
					s_objWindowBot = s_windowBotPrev;
				}
			}

			// Sort objects in viewspace (generally back to front but there are special cases).
			qsort(s_objBuffer, objCount, sizeof(SecObject*), sortObjectsFixed);

			// Draw objects in order.
			for (s32 i = 0; i < objCount; i++)
			{
				SecObject* obj = s_objBuffer[i];
				const s32 type = obj->type;
				if (type == OBJ_TYPE_SPRITE)
				{
					TFE_ZONE("Draw WAX");

					fixed16_16 dx = s_rcfState.cameraPos.x - obj->posWS.x;
					fixed16_16 dz = s_rcfState.cameraPos.z - obj->posWS.z;
					angle14_32 angle = vec2ToAngle(dx, dz);

					sprite_drawWax(angle, obj);
				}
				else if (type == OBJ_TYPE_3D)
				{
					TFE_ZONE("Draw 3DO");

					robj3d_draw(obj, obj->model);
				}
				else if (type == OBJ_TYPE_FRAME)
				{
					TFE_ZONE("Draw Frame");

					sprite_drawFrame((u8*)obj->fme, obj->fme, obj);
				}
			}
		}
		TFE_ZONE_END(secDrawObjects);

		s_curSector->flags1 |= SEC_FLAGS1_RENDERED;
		s_curSector->prevDrawFrame2 = s_drawFrame;
	}
		
	void TFE_Sectors_Fixed::adjoin_setupAdjoinWindow(s32* winBot, s32* winBotNext, s32* winTop, s32* winTopNext, EdgePairFixed* adjoinEdges, s32 adjoinCount)
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

			const fixed16_16 ceil_dYdX = adjoinEdges->dyCeil_dx;
			fixed16_16 y = adjoinEdges->yCeil0;
			for (s32 x = x0; x <= x1; x++, y += ceil_dYdX)
			{
				s32 yPixel = round16(y);
				s32 yBot = winBotNext[x];
				s32 yTop = winTop[x];
				if (yPixel > yTop)
				{
					winTopNext[x] = (yPixel <= yBot) ? yPixel : yBot + 1;
				}
			}
			const fixed16_16 floor_dYdX = adjoinEdges->dyFloor_dx;
			y = adjoinEdges->yFloor0;
			for (s32 x = x0; x <= x1; x++, y += floor_dYdX)
			{
				s32 yPixel = round16(y);
				s32 yTop = winTop[x];
				s32 yBot = winBot[x];
				if (yPixel < yBot)
				{
					winBotNext[x] = (yPixel >= yTop) ? yPixel : yTop - 1;
				}
			}
		}
	}

	void TFE_Sectors_Fixed::adjoin_computeWindowBounds(EdgePairFixed* adjoinEdges)
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

	void TFE_Sectors_Fixed::saveValues(s32 index)
	{
		SectorSaveValues* dst = &s_sectorStack[index];
		dst->curSector = s_curSector;
		dst->prevSector = s_prevSector;
		dst->depth1d = s_rcfState.depth1d;
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

	void TFE_Sectors_Fixed::restoreValues(s32 index)
	{
		const SectorSaveValues* src = &s_sectorStack[index];
		s_curSector = src->curSector;
		s_prevSector = src->prevSector;
		s_rcfState.depth1d = (fixed16_16*)src->depth1d;
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

	// Switch from float to fixed.
	void TFE_Sectors_Fixed::subrendererChanged()
	{
	}
}