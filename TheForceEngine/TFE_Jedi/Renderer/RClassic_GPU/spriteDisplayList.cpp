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

#include "spriteDisplayList.h"
#include "frustum.h"
#include "../rcommon.h"

using namespace TFE_RenderBackend;

namespace TFE_Jedi
{
	enum Constants
	{
		MAX_DISP_ITEMS = 1024,
	};

	static s32 s_displayListCount;
#ifdef SPRITE_CPU_CLIPPING
	static Vec4f s_displayListPosXZTexture[2][MAX_DISP_ITEMS];
	static Vec4f s_displayListPosYUTexture[2][MAX_DISP_ITEMS];
	static s32 s_displayListTexIdTexture[2][MAX_DISP_ITEMS];
	static ShaderBuffer s_displayListPosXZTextureGPU;
	static ShaderBuffer s_displayListPosYUTextureGPU;
	static ShaderBuffer s_displayListTexIdTextureGPU;
	static s32 s_posXZTextureIndex;
	static s32 s_posYUTextureIndex;
	static s32 s_texIdTextureIndex;
#else
	static Vec4f s_displayListPosTexture[2][MAX_DISP_ITEMS];
	static Vec4f s_displayListScaleOffset[2][MAX_DISP_ITEMS];
	static ShaderBuffer s_displayListPosTextureGPU;
	static ShaderBuffer s_displayListScaleOffsetGPU;
	static s32 s_posTextureIndex;
	static s32 s_scaleOffsetIndex;
#endif

	extern Vec3f s_cameraPos;
	extern Vec3f s_cameraDir;
	extern Vec3f s_cameraRight;
	void sprdisplayList_sort();

	void sprdisplayList_init(s32 startIndex)
	{
		ShaderBufferDef bufferDefDisplayList =
		{
			4,				// 1, 2, 4 channels (R, RG, RGBA)
			sizeof(f32),	// 1, 2, 4 bytes (u8; s16,u16; s32,u32,f32)
			BUF_CHANNEL_FLOAT
		};
	#ifdef SPRITE_CPU_CLIPPING
		ShaderBufferDef bufferDefTexDisplayList =
		{
			1,				// 1, 2, 4 channels (R, RG, RGBA)
			sizeof(s32),	// 1, 2, 4 bytes (u8; s16,u16; s32,u32,f32)
			BUF_CHANNEL_INT
		};

		s_displayListPosXZTextureGPU.create(MAX_DISP_ITEMS, bufferDefDisplayList, true);
		s_displayListPosYUTextureGPU.create(MAX_DISP_ITEMS, bufferDefDisplayList, true);
		s_displayListTexIdTextureGPU.create(MAX_DISP_ITEMS, bufferDefTexDisplayList, true);

		s_posXZTextureIndex = startIndex;
		s_posYUTextureIndex = startIndex + 1;
		s_texIdTextureIndex = startIndex + 2;
	#else
		s_displayListPosTextureGPU.create(MAX_DISP_ITEMS,  bufferDefDisplayList, true);
		s_displayListScaleOffsetGPU.create(MAX_DISP_ITEMS, bufferDefDisplayList, true);

		s_posTextureIndex  = startIndex;
		s_scaleOffsetIndex = startIndex + 1;
	#endif
		sprdisplayList_clear();
	}

	void sprdisplayList_destroy()
	{
	#ifdef SPRITE_CPU_CLIPPING
		s_displayListPosXZTextureGPU.destroy();
		s_displayListPosYUTextureGPU.destroy();
		s_displayListTexIdTextureGPU.destroy();
	#else
		s_displayListPosTextureGPU.destroy();
		s_displayListScaleOffsetGPU.destroy();
	#endif
	}

	void sprdisplayList_clear()
	{
		s_displayListCount = 0;
	}
				
	void sprdisplayList_finish()
	{
		if (!s_displayListCount) { return; }
		sprdisplayList_sort();

	#ifdef SPRITE_CPU_CLIPPING
		s_displayListPosXZTextureGPU.update(&s_displayListPosXZTexture[1], sizeof(Vec4f) * s_displayListCount);
		s_displayListPosYUTextureGPU.update(&s_displayListPosYUTexture[1], sizeof(Vec4f) * s_displayListCount);
		s_displayListTexIdTextureGPU.update(&s_displayListTexIdTexture[1], sizeof(s32)   * s_displayListCount);
	#else
		s_displayListPosTextureGPU.update(&s_displayListPosTexture[1], sizeof(Vec4f) * s_displayListCount);
		s_displayListScaleOffsetGPU.update(&s_displayListScaleOffset[1], sizeof(Vec4f) * s_displayListCount);
	#endif
	}

#ifdef SPRITE_CPU_CLIPPING
	void sprdisplayList_addFrame(void* basePtr, WaxFrame* frame, Vec2f v0, Vec2f v1, Vec2f c0, Vec2f c1, f32 posY, RSector* curSector, bool fullbright)
	{
		if (!basePtr || !frame) { return; }

		const WaxCell* cell = WAX_CellPtr(basePtr, frame);
		assert(cell->textureId >= 0 && cell->textureId < 2048);

		s32 flip = frame->flip ? 1 : 0;
		s32 ambient = fullbright ? MAX_LIGHT_LEVEL : clamp(floor16(curSector->ambient), 0, MAX_LIGHT_LEVEL);
		
		Vec2f offset0 = { c0.x - v0.x, c0.z - v0.z };
		Vec2f offset1 = { c1.x - v0.x, c1.z - v0.z };
		Vec2f ext = { v1.x - v0.x, v1.z - v0.z };
		f32 lenExtScale = 1.0f / sqrtf(ext.x*ext.x + ext.z*ext.z);
		f32 len0 = sqrtf(offset0.x*offset0.x + offset0.z*offset0.z);
		f32 len1 = sqrtf(offset1.x*offset1.x + offset1.z*offset1.z);

		f32 u0, u1;
		const f32 scaleU = f32(cell->sizeX);
		if (frame->flip)
		{
			u0 = (1.0f - len0 * lenExtScale) * scaleU;
			u1 = (1.0f - len1 * lenExtScale) * scaleU;
		}
		else
		{
			u0 = len0 * lenExtScale * scaleU;
			u1 = len1 * lenExtScale * scaleU;
		}
		
		const f32 heightWS = fixed16ToFloat(frame->heightWS);
		const f32 fOffsetY = fixed16ToFloat(frame->offsetY);
		s_displayListPosXZTexture[0][s_displayListCount] = { c0.x, c0.z, c1.x, c1.z };
		s_displayListPosYUTexture[0][s_displayListCount] = { posY + fOffsetY, posY + fOffsetY - heightWS, u0, u1 };
		s_displayListTexIdTexture[0][s_displayListCount] = cell->textureId | (ambient << 16);
		s_displayListCount++;
	}
#else
	void sprdisplayList_addFrame(void* basePtr, WaxFrame* frame, Vec3f posWS, RSector* curSector, bool fullbright)
	{
		if (!basePtr || !frame) { return; }

		const WaxCell* cell = WAX_CellPtr(basePtr, frame);
		assert(cell->textureId >= 0 && cell->textureId < 2048);

		s32 flip = frame->flip ? 1 : 0;
		s32 ambient = fullbright ? MAX_LIGHT_LEVEL : clamp(floor16(curSector->ambient), 0, MAX_LIGHT_LEVEL);
		s_displayListPosTexture[0][s_displayListCount] = { posWS.x, posWS.y, posWS.z, (f32)(cell->textureId | (ambient << 16) | (flip << 21)) };

		const f32 widthWS  = fixed16ToFloat(frame->widthWS);
		const f32 heightWS = fixed16ToFloat(frame->heightWS);
		const f32 fOffsetX = fixed16ToFloat(frame->offsetX);
		const f32 fOffsetY = fixed16ToFloat(frame->offsetY);
		s_displayListScaleOffset[0][s_displayListCount] = { fOffsetX, fOffsetY, widthWS, heightWS };

		Vec3f corner0 = { posWS.x - s_cameraRight.x*fOffsetX, posWS.y + fOffsetY, posWS.z - s_cameraRight.z*fOffsetX };
		Vec3f corner1 = { posWS.x + s_cameraRight.x*widthWS,  posWS.y - heightWS, posWS.z + s_cameraRight.z*widthWS };
		if (!frustum_quadInside(corner0, corner1))
		{
			return;
		}
		s_displayListCount++;
	}
#endif

	s32 sprdisplayList_getSize()
	{
		return s_displayListCount;
	}
	
	void sprdisplayList_draw()
	{
		if (!s_displayListCount) { return; }

	#ifdef SPRITE_CPU_CLIPPING
		s_displayListPosXZTextureGPU.bind(s_posXZTextureIndex);
		s_displayListPosYUTextureGPU.bind(s_posYUTextureIndex);
		s_displayListTexIdTextureGPU.bind(s_texIdTextureIndex);
	#else
		s_displayListPosTextureGPU.bind(s_posTextureIndex);
		s_displayListScaleOffsetGPU.bind(s_scaleOffsetIndex);
	#endif

		TFE_RenderBackend::drawIndexedTriangles(2 * s_displayListCount, sizeof(u16));

	#ifdef SPRITE_CPU_CLIPPING
		s_displayListPosXZTextureGPU.unbind(s_posXZTextureIndex);
		s_displayListPosYUTextureGPU.unbind(s_posYUTextureIndex);
		s_displayListTexIdTextureGPU.unbind(s_texIdTextureIndex);
	#else
		s_displayListPosTextureGPU.unbind(s_posTextureIndex);
		s_displayListScaleOffsetGPU.unbind(s_scaleOffsetIndex);
	#endif
	}

	struct SpriteSortKey
	{
		s32 index;
		f32 key;
	};

	s32 spriteSort(const void* a, const void* b)
	{
		const SpriteSortKey* spriteA = (SpriteSortKey*)a;
		const SpriteSortKey* spriteB = (SpriteSortKey*)b;
		if (spriteA->key > spriteB->key) { return -1; }
		if (spriteA->key < spriteB->key) { return  1; }
		return 0;
	}

	// Sort the display list from back to front.
	void sprdisplayList_sort()
	{
		// Fill in the sort keys.
		SpriteSortKey sortKey[MAX_DISP_ITEMS];
		for (s32 i = 0; i < s_displayListCount; i++)
		{
			sortKey[i].index = i;

		#ifdef SPRITE_CPU_CLIPPING
			Vec4f* posXZ = &s_displayListPosXZTexture[0][i];
			Vec4f* posYU = &s_displayListPosYUTexture[0][i];
			Vec3f relPos = { posXZ->x - s_cameraPos.x, posYU->x - s_cameraPos.y, posXZ->z - s_cameraPos.z };
			sortKey[i].key = relPos.x*s_cameraDir.x + relPos.y*s_cameraDir.y + relPos.z*s_cameraDir.z;
		#else
			Vec4f* pos = &s_displayListPosTexture[0][i];
			Vec3f relPos = { pos->x - s_cameraPos.x, pos->y - s_cameraPos.y, pos->z - s_cameraPos.z };
			sortKey[i].key = relPos.x*s_cameraDir.x + relPos.y*s_cameraDir.y + relPos.z*s_cameraDir.z;
		#endif
		}

		// Sort from back to front (largest to smallest).
		std::qsort(sortKey, s_displayListCount, sizeof(SpriteSortKey), spriteSort);

		// Fill in the sorted values.
	#ifdef SPRITE_CPU_CLIPPING
		for (s32 i = 0; i < s_displayListCount; i++)
		{
			s_displayListPosXZTexture[1][i] = s_displayListPosXZTexture[0][sortKey[i].index];
			s_displayListPosYUTexture[1][i] = s_displayListPosYUTexture[0][sortKey[i].index];
			s_displayListTexIdTexture[1][i] = s_displayListTexIdTexture[0][sortKey[i].index];
		}
	#else
		for (s32 i = 0; i < s_displayListCount; i++)
		{
			s_displayListPosTexture[1][i]  = s_displayListPosTexture[0][sortKey[i].index];
			s_displayListScaleOffset[1][i] = s_displayListScaleOffset[0][sortKey[i].index];
		}
	#endif
	}
}
