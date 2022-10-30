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
#include "sectorDisplayList.h"
#include "objectPortalPlanes.h"
#include "frustum.h"
#include "../rcommon.h"

using namespace TFE_RenderBackend;

namespace TFE_Jedi
{
	extern s32 s_drawnSpriteCount;
	extern SecObject* s_drawnSprites[];

	enum
	{
		SPRITE_BUFFER_COUNT = 2,
	};

	static s32 s_displayListCount = 0;
	static Vec4f* s_displayListPosXZTexture[SPRITE_BUFFER_COUNT] = { nullptr };
	static Vec4f* s_displayListPosYUTexture[SPRITE_BUFFER_COUNT] = { nullptr };
	static Vec2i* s_displayListTexIdTexture[SPRITE_BUFFER_COUNT] = { nullptr };
	static void** s_displayListObjList = { nullptr };
	static ShaderBuffer s_displayListPosXZTextureGPU;
	static ShaderBuffer s_displayListPosYUTextureGPU;
	static ShaderBuffer s_displayListTexIdTextureGPU;
	static s32 s_posXZTextureIndex;
	static s32 s_posYUTextureIndex;
	static s32 s_texIdTextureIndex;
	static s32 s_planesIndex;

	// TODO: Refactor
	extern s32 s_displayCurrentPortalId;
	extern ShaderBuffer s_displayListPlanesGPU;

	extern Vec3f s_cameraPos;
	extern Vec3f s_cameraDir;
	extern Vec3f s_cameraRight;
	void sprdisplayList_sort();

	void sprdisplayList_init(s32 startIndex)
	{
		for (s32 i = 0; i < SPRITE_BUFFER_COUNT; i++)
		{
			s_displayListPosXZTexture[i] = (Vec4f*)malloc(sizeof(Vec4f*) * MAX_DISP_ITEMS);
			s_displayListPosYUTexture[i] = (Vec4f*)malloc(sizeof(Vec4f*) * MAX_DISP_ITEMS);
			s_displayListTexIdTexture[i] = (Vec2i*)malloc(sizeof(Vec2i*) * MAX_DISP_ITEMS);
		}
		s_displayListObjList = (void**)malloc(sizeof(void**) * MAX_DISP_ITEMS);

		ShaderBufferDef bufferDefDisplayList =
		{
			4,				// 1, 2, 4 channels (R, RG, RGBA)
			sizeof(f32),	// 1, 2, 4 bytes (u8; s16,u16; s32,u32,f32)
			BUF_CHANNEL_FLOAT
		};
		ShaderBufferDef bufferDefTexDisplayList =
		{
			2,				// 1, 2, 4 channels (R, RG, RGBA)
			sizeof(s32),	// 1, 2, 4 bytes (u8; s16,u16; s32,u32,f32)
			BUF_CHANNEL_INT
		};

		s_displayListPosXZTextureGPU.create(MAX_DISP_ITEMS, bufferDefDisplayList, true);
		s_displayListPosYUTextureGPU.create(MAX_DISP_ITEMS, bufferDefDisplayList, true);
		s_displayListTexIdTextureGPU.create(MAX_DISP_ITEMS, bufferDefTexDisplayList, true);

		s_posXZTextureIndex = startIndex;
		s_posYUTextureIndex = startIndex + 1;
		s_texIdTextureIndex = startIndex + 2;
		// TODO: Refactor
		s_planesIndex = 7;

		sprdisplayList_clear();
	}

	void sprdisplayList_destroy()
	{
		for (s32 i = 0; i < SPRITE_BUFFER_COUNT; i++)
		{
			free(s_displayListPosXZTexture[i]);
			free(s_displayListPosYUTexture[i]);
			free(s_displayListTexIdTexture[i]);
			s_displayListPosXZTexture[i] = nullptr;
			s_displayListPosYUTexture[i] = nullptr;
			s_displayListTexIdTexture[i] = nullptr;
		}
		free(s_displayListObjList);
		s_displayListObjList = nullptr;

		s_displayListPosXZTextureGPU.destroy();
		s_displayListPosYUTextureGPU.destroy();
		s_displayListTexIdTextureGPU.destroy();
	}

	void sprdisplayList_clear()
	{
		s_displayListCount = 0;
	}
				
	void sprdisplayList_finish()
	{
		if (!s_displayListCount) { return; }
		sprdisplayList_sort();

		s_displayListPosXZTextureGPU.update(s_displayListPosXZTexture[1], sizeof(Vec4f) * s_displayListCount);
		s_displayListPosYUTextureGPU.update(s_displayListPosYUTexture[1], sizeof(Vec4f) * s_displayListCount);
		s_displayListTexIdTextureGPU.update(s_displayListTexIdTexture[1], sizeof(Vec2i) * s_displayListCount);
	}

	void sprdisplayList_addFrame(const SpriteDrawFrame* const drawFrame)
	{
		if (!drawFrame->basePtr || !drawFrame->frame) { return; }
		if (s_displayListCount >= MAX_DISP_ITEMS)
		{
			assert(0);
			return;
		}

		const WaxCell* cell = WAX_CellPtr(drawFrame->basePtr, drawFrame->frame);
		assert(cell->textureId >= 0 && cell->textureId < 32768);

		s32 flip = drawFrame->frame->flip ? 1 : 0;
		s32 ambient = drawFrame->fullbright ? MAX_LIGHT_LEVEL : clamp(floor16(drawFrame->curSector->ambient), 0, MAX_LIGHT_LEVEL);
		
		Vec2f offset0 = { drawFrame->c0.x - drawFrame->v0.x, drawFrame->c0.z - drawFrame->v0.z };
		Vec2f offset1 = { drawFrame->c1.x - drawFrame->v0.x, drawFrame->c1.z - drawFrame->v0.z };
		Vec2f ext = { drawFrame->v1.x - drawFrame->v0.x, drawFrame->v1.z - drawFrame->v0.z };
		f32 lenExtScale = 1.0f / sqrtf(ext.x*ext.x + ext.z*ext.z);
		f32 len0 = sqrtf(offset0.x*offset0.x + offset0.z*offset0.z);
		f32 len1 = sqrtf(offset1.x*offset1.x + offset1.z*offset1.z);

		f32 u0, u1;
		const f32 scaleU = f32(cell->sizeX);
		if (drawFrame->frame->flip)
		{
			u0 = (1.0f - len0 * lenExtScale) * scaleU;
			u1 = (1.0f - len1 * lenExtScale) * scaleU;
		}
		else
		{
			u0 = len0 * lenExtScale * scaleU;
			u1 = len1 * lenExtScale * scaleU;
		}

		const u32 portalInfo = drawFrame->portalInfo;
		const f32 heightWS = fixed16ToFloat(drawFrame->frame->heightWS);
		const f32 fOffsetY = fixed16ToFloat(drawFrame->frame->offsetY);
		s_displayListPosXZTexture[0][s_displayListCount] = { drawFrame->c0.x, drawFrame->c0.z, drawFrame->c1.x, drawFrame->c1.z };
		s_displayListPosYUTexture[0][s_displayListCount] = { drawFrame->posY + fOffsetY, drawFrame->posY + fOffsetY - heightWS, u0, u1 };
		s_displayListTexIdTexture[0][s_displayListCount] = { cell->textureId | (ambient << 16), s32(portalInfo) };
		s_displayListObjList[s_displayListCount] = drawFrame->objPtr;
		s_displayListCount++;
	}

	s32 sprdisplayList_getSize()
	{
		return s_displayListCount;
	}
	
	void sprdisplayList_draw()
	{
		if (!s_displayListCount) { return; }

		s_displayListPosXZTextureGPU.bind(s_posXZTextureIndex);
		s_displayListPosYUTextureGPU.bind(s_posYUTextureIndex);
		s_displayListTexIdTextureGPU.bind(s_texIdTextureIndex);
		objectPortalPlanes_bind(s_planesIndex);

		TFE_RenderBackend::drawIndexedTriangles(2 * s_displayListCount, sizeof(u16));

		s_displayListPosXZTextureGPU.unbind(s_posXZTextureIndex);
		s_displayListPosYUTextureGPU.unbind(s_posYUTextureIndex);
		s_displayListTexIdTextureGPU.unbind(s_texIdTextureIndex);
		objectPortalPlanes_unbind(s_planesIndex);
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
		static SpriteSortKey sortKey[MAX_DISP_ITEMS];
		for (s32 i = 0; i < s_displayListCount; i++)
		{
			sortKey[i].index = i;

			const Vec4f* posXZ = &s_displayListPosXZTexture[0][i];
			const Vec4f* posYU = &s_displayListPosYUTexture[0][i];
			Vec3f relPos = { posXZ->x - s_cameraPos.x, posYU->x - s_cameraPos.y, posXZ->y - s_cameraPos.z };
			sortKey[i].key = relPos.x*s_cameraDir.x + relPos.y*s_cameraDir.y + relPos.z*s_cameraDir.z;
		}

		// Sort from back to front (largest to smallest).
		std::qsort(sortKey, s_displayListCount, sizeof(SpriteSortKey), spriteSort);

		// Fill in the sorted values.
		for (s32 i = 0; i < s_displayListCount; i++)
		{
			s_displayListPosXZTexture[1][i] = s_displayListPosXZTexture[0][sortKey[i].index];
			s_displayListPosYUTexture[1][i] = s_displayListPosYUTexture[0][sortKey[i].index];
			s_displayListTexIdTexture[1][i] = s_displayListTexIdTexture[0][sortKey[i].index];
		}

		//if (autoaim)
		{
			for (s32 i = s_displayListCount - 1; i > 0 && s_drawnSpriteCount < MAX_DRAWN_SPRITE_STORE; i--)
			{
				s_drawnSprites[s_drawnSpriteCount++] = (SecObject*)s_displayListObjList[sortKey[i].index];
			}
		}
	}
}
