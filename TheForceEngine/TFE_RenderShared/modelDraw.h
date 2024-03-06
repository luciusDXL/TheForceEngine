#pragma once
//////////////////////////////////////////////////////////////////////
// GPU 2D antialiased line drawing
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderBackend/indexBuffer.h>
#include "camera3d.h"

namespace TFE_RenderShared
{
	enum ModelDrawMode
	{
		MDLMODE_TEX_UV = 0,
		MDLMODE_TEX_PROJ,
		MDLMODE_COLORED,
		MDLMODE_COUNT
	};
	struct TexProjection
	{
		Vec4f plane;
		Vec2f offset;
	};

	bool modelDraw_init();
	void modelDraw_destroy();

	void modelDraw_begin();
	void modelDraw_addModel(const VertexBuffer* vtx, const IndexBuffer* idx, const Vec3f& pos, const Mat3& xform, const Vec4f tint = { 1.0f,1.0f,1.0f,1.0f });
	void modelDraw_addMaterial(ModelDrawMode mode, u32 idxStart, u32 idxCount, TextureGpu* tex = nullptr, const TexProjection proj = {0});
	bool modelDraw_draw(const Camera3d* camera, f32 width, f32 height, bool enableCulling = true);
}
