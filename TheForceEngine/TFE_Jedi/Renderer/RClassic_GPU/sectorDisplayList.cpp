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
#include "../rcommon.h"

using namespace TFE_RenderBackend;

namespace TFE_Jedi
{
	static s32 s_displayListCount;
	static Vec4f  s_displayListPos[1024];
	static Vec4ui s_displayListData[1024];
	static ShaderBuffer s_displayListPosGPU;
	static ShaderBuffer s_displayListDataGPU;
	static s32 s_posIndex;
	static s32 s_dataIndex;

	void sdisplayList_init(s32 posIndex, s32 dataIndex)
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

		s_posIndex = posIndex;
		s_dataIndex = dataIndex;

		sdisplayList_clear();
	}

	void sdisplayList_clear()
	{
		s_displayListCount = 0;
	}

	void sdisplayList_finish()
	{
		if (!s_displayListCount) { return; }
		s_displayListPosGPU.update(s_displayListPos, sizeof(Vec4f) * s_displayListCount);
		s_displayListDataGPU.update(s_displayListData, sizeof(Vec4ui) * s_displayListCount);
	}

	void sdisplayList_addCaps(RSector* curSector)
	{
		// TODO: Constrain to portal frustum.
		Vec4f pos = { fixed16ToFloat(curSector->boundsMin.x), fixed16ToFloat(curSector->boundsMin.z), fixed16ToFloat(curSector->boundsMax.x), fixed16ToFloat(curSector->boundsMax.z) };
		Vec4ui data = { 0, (u32)curSector->index/*sectorId*/, (u32)floor16(curSector->ambient), 0u/*textureId*/ };

		s_displayListPos[s_displayListCount] = pos;
		s_displayListData[s_displayListCount] = data;
		s_displayListData[s_displayListCount].x = 5;	// part = floor cap;
		s_displayListCount++;

		s_displayListPos[s_displayListCount] = pos;
		s_displayListData[s_displayListCount] = data;
		s_displayListData[s_displayListCount].x = 6;	// part = ceiling cap;
		s_displayListCount++;
	}

	void sdisplayList_addSegment(RSector* curSector, WallSegBuffer* wallSeg)
	{
		s32 wallId = wallSeg->seg->id;
		RWall* srcWall = &curSector->walls[wallId];
		s32 ambient = floor16(curSector->ambient);

		Vec4f pos = { wallSeg->v0.x, wallSeg->v0.z, wallSeg->v1.x, wallSeg->v1.z };
		Vec4ui data = { (srcWall->nextSector ? u32(srcWall->nextSector->index) << 16u : 0u)/*partId | nextSector*/, (u32)curSector->index/*sectorId*/,
						ambient < 31 ? (u32)min(31, floor16(srcWall->wallLight) + floor16(curSector->ambient)) : 31u/*ambient*/, 0u/*textureId*/ };

		// Wall Flags.
		if (srcWall->drawFlags == WDF_MIDDLE && !srcWall->nextSector) // TODO: Fix transparent mid textures.
		{
			s_displayListPos[s_displayListCount] = pos;
			s_displayListData[s_displayListCount] = data;
			s_displayListData[s_displayListCount].x |= 0;	// part = mid;
			s_displayListCount++;
		}
		if ((srcWall->drawFlags & WDF_TOP) && srcWall->nextSector)
		{
			s_displayListPos[s_displayListCount] = pos;
			s_displayListData[s_displayListCount] = data;
			s_displayListData[s_displayListCount].x |= 1;	// part = top;
			s_displayListCount++;
		}
		if ((srcWall->drawFlags & WDF_BOT) && srcWall->nextSector)
		{
			s_displayListPos[s_displayListCount] = pos;
			s_displayListData[s_displayListCount] = data;
			s_displayListData[s_displayListCount].x |= 2;	// part = bottom;
			s_displayListCount++;
		}
		// Add Floor
		s_displayListPos[s_displayListCount] = pos;
		s_displayListData[s_displayListCount] = data;
		s_displayListData[s_displayListCount].x |= 3;	// part = floor;
		s_displayListData[s_displayListCount].z = ambient;
		s_displayListCount++;

		// Add Ceiling
		s_displayListPos[s_displayListCount] = pos;
		s_displayListData[s_displayListCount] = data;
		s_displayListData[s_displayListCount].x |= 4;	// part = floor;
		s_displayListData[s_displayListCount].z = ambient;
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

		TFE_RenderBackend::drawIndexedTriangles(2 * s_displayListCount, sizeof(u16));

		s_displayListPosGPU.unbind(s_posIndex);
		s_displayListDataGPU.unbind(s_dataIndex);
	}
}
