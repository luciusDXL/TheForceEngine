#include "editorObj3D.h"
#include "editorColormap.h"
#include "editorAsset.h"
#include <TFE_RenderShared/modelDraw.h>
#include <TFE_Editor/editor.h>
#include <TFE_Editor/AssetBrowser/assetBrowser.h>
#include <TFE_DarkForces/mission.h>
#include <TFE_System/parser.h>
#include <TFE_System/system.h>
#include <TFE_System/math.h>
#include <TFE_Archive/archive.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_Jedi/Renderer/rcommon.h>
#include <algorithm>
#include <cstring>
#include <vector>
#include <map>

namespace TFE_Editor
{
	enum ThumbnailConst
	{
		TN_CellW = 16,
		TN_CellMask = TN_CellW - 1,
		TN_Cells_Count = TN_CellW * TN_CellW,
	};
	const f64 c_animateSpeed = 0.7854;	// about 45 degrees per second.
	const Vec3f c_thumbnailCamDir = { -0.57735f, 0.57735f, 0.57735f };

	struct ThumbCell
	{
		s32 index;
		EditorObj3D* obj = nullptr;
		s64 lastFrame = 0;
		f32 angle = 0.0f;
	};

	static s32 s_thumbnailSize;
	static s32 s_atlasSize;
	static s32 s_updateCount;
	static s64 s_thumbnailFrame = 0;
	static RenderTargetHandle s_atlasHandle = nullptr;
	static ThumbCell s_cells[256];
	static ThumbCell* s_cellsToUpdate[256];

	void computeThumbnailPos(s32 index, Vec2i* pos);
	void computeThumbnailUvs(s32 index, Vec2f* uv0, Vec2f* uv1, f32 sizeScale, f32 width);

	bool thumbnail_init(s32 thumbnailSize)
	{
		s_thumbnailSize = thumbnailSize;
		s_atlasSize = s_thumbnailSize * TN_CellW;	// store up to 256 thumbnails.
		s_thumbnailFrame = 0;
		s_updateCount = 0;
		memset(s_cells, 0, sizeof(ThumbCell) * TN_Cells_Count);

		for (s32 i = 0; i < TN_Cells_Count; i++)
		{
			s_cells[i].index = i;
		}

		// Create a backing render target.
		s_atlasHandle = TFE_RenderBackend::createRenderTarget(s_atlasSize, s_atlasSize, true);
		const f32 black[4] = { 0.2f, 0.2f, 0.2f, 1.0f };
		TFE_RenderBackend::bindRenderTarget(s_atlasHandle);
		TFE_RenderBackend::clearRenderTarget(s_atlasHandle, black);
		TFE_RenderBackend::unbindRenderTarget();
		return true;
	}

	void thumbnail_destroy()
	{
		TFE_RenderBackend::freeRenderTarget(s_atlasHandle);
		s_atlasHandle = nullptr;
	}

	void thumbnail_update()
	{
		s_thumbnailFrame++;
		if (!s_updateCount) { return; }

		Camera3d camera = { 0 };
		Vec3f objPos = { 0 };
		f32 fov = 1.05f;
		camera.projMtx = TFE_Math::computeProjMatrix(fov, 1.0f, 0.25f, 4096.0f);
		TFE_RenderBackend::bindRenderTarget(s_atlasHandle);

		// Handle updates.
		const Vec3f upDir = { 0.0f, 1.0f, 0.0f };
		Vec4f clearColor = { 0 };
		for (s32 i = 0; i < s_updateCount; i++)
		{
			Vec2i cellPos;
			computeThumbnailPos(s_cellsToUpdate[i]->index, &cellPos);

			Mat3 xform;
			const EditorObj3D* obj = s_cellsToUpdate[i]->obj;
			compute3x3Rotation(&xform, s_cellsToUpdate[i]->angle, 0.0f, 0.0f);

			// get a good distance...
			objPos.x = -(obj->bounds[0].x + obj->bounds[1].x) * 0.5f;
			objPos.z = -(obj->bounds[0].z + obj->bounds[1].z) * 0.5f;
			objPos.y = -obj->bounds[1].y * 0.5f;

			const f32 radius = std::max(std::max(obj->bounds[1].x + objPos.x, obj->bounds[1].z + objPos.z), (obj->bounds[1].y - obj->bounds[0].y) * 0.5f);
			const f32 dist = radius * 2.5f;

			camera.pos = { c_thumbnailCamDir.x * dist, c_thumbnailCamDir.y * dist, c_thumbnailCamDir.z * dist };
			camera.viewMtx = TFE_Math::computeViewMatrix(&c_thumbnailCamDir, &upDir);

			TFE_RenderBackend::setViewport(cellPos.x, cellPos.z, s_thumbnailSize, s_thumbnailSize);
			TFE_RenderBackend::setScissorRect(true, cellPos.x, cellPos.z, s_thumbnailSize, s_thumbnailSize);
			TFE_RenderBackend::clearRenderTarget(s_atlasHandle, clearColor.m);

			TFE_RenderShared::modelDraw_begin();
			TFE_RenderShared::modelDraw_addModel(&obj->vtxGpu, &obj->idxGpu, objPos, xform);
			s32 mtlCount = (s32)obj->mtl.size();
			const EditorMaterial* mtl = obj->mtl.data();
			TFE_RenderShared::TexProjection proj = { 0 };
			for (s32 m = 0; m < mtlCount; m++, mtl++)
			{
				TFE_RenderShared::ModelDrawMode mode;
				// for now just do colored or texture uv.
				mode = mtl->texture ? TFE_RenderShared::MDLMODE_TEX_UV : TFE_RenderShared::MDLMODE_COLORED;
				if (mtl->flatProj && mtl->texture)
				{
					mode = TFE_RenderShared::MDLMODE_TEX_PROJ;
				}
				TFE_RenderShared::modelDraw_addMaterial(mode, mtl->idxStart, mtl->idxCount, mtl->texture, proj);
			}
			TFE_RenderShared::modelDraw_draw(&camera, 64.0f, 64.0f, /*culling*/false);
			
		}
		s_updateCount = 0;
		TFE_RenderBackend::unbindRenderTarget();
		TFE_RenderBackend::setScissorRect(false);
	}
		
	void computeThumbnailPos(s32 index, Vec2i* pos)
	{
		pos->x = (index & TN_CellMask) * s_thumbnailSize;
		pos->z = (index / TN_CellW) * s_thumbnailSize;
	}

	void computeThumbnailUvs(s32 index, Vec2f* uv0, Vec2f* uv1, f32 sizeScale, f32 width)
	{
		Vec2i pos;
		computeThumbnailPos(index, &pos);

		uv0->x = f32(pos.x) * sizeScale;
		uv0->z = f32(pos.z) * sizeScale;
		uv1->x = uv0->x + width;
		uv1->z = uv0->z + width;

		std::swap(uv0->z, uv1->z);
	}

	// Get a texture and uv coordinates within the texture.
	const TextureGpu* getThumbnail(EditorObj3D* obj3d, Vec2f* uv0, Vec2f* uv1, bool animate)
	{
		if (!s_atlasHandle) { return nullptr; }
		const TextureGpu* tex = TFE_RenderBackend::getRenderTargetTexture(s_atlasHandle);

		// Has this object already been rendered?
		s32 cellIndex = -1;
		f32 sizeScale = 1.0f / f32(s_atlasSize);
		f32 thumbScale = 1.0f / f32(s_thumbnailSize);
		f32 width = f32(s_thumbnailSize) / f32(s_atlasSize);
		s32 freeCell = -1;

		s64 oldestFrame = INT64_MAX;
		s32 oldestIndex = -1;
		for (s32 i = 0; i < TN_Cells_Count; i++)
		{
			if (s_cells[i].obj == obj3d)
			{
				if (animate)
				{
					s_cells[i].angle += f32(TFE_System::getDeltaTime() * c_animateSpeed);
					s_cells[i].angle = fmodf(s_cells[i].angle, 2.0f * PI);
					s_cellsToUpdate[s_updateCount++] = &s_cells[i];
				}
				else if (!animate && s_cells[i].angle != 0.0f)
				{
					s_cells[i].angle = 0.0f;
					s_cellsToUpdate[s_updateCount++] = &s_cells[i];
				}
				s_cells[i].lastFrame = s_thumbnailFrame;
				computeThumbnailUvs(i, uv0, uv1, sizeScale, width);
				return tex;
			}
			else if (s_cells[i].obj)
			{
				if (s_cells[i].lastFrame < oldestFrame)
				{
					oldestFrame = s_cells[i].lastFrame;
					oldestIndex = i;
				}
			}
			else
			{
				freeCell = i;
				break;
			}
		}

		assert(freeCell >= 0 || oldestIndex >= 0);
		const s32 newIndex = (freeCell >= 0) ? freeCell : oldestIndex;

		s_cells[newIndex].lastFrame = s_thumbnailFrame;
		s_cells[newIndex].obj = obj3d;
		computeThumbnailUvs(newIndex, uv0, uv1, sizeScale, width);

		s_cells[newIndex].angle = 0.0f;
		s_cellsToUpdate[s_updateCount++] = &s_cells[newIndex];

		return tex;
	}
}