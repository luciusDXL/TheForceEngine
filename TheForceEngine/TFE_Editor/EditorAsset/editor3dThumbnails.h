#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// Cache and generate thumbnails for 3d assets.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Archive/archive.h>
#include <TFE_Asset/modelAsset_jedi.h>
#include <TFE_RenderBackend/renderBackend.h>
#include "editorObj3D.h"

namespace TFE_Editor
{
	bool thumbnail_init(s32 thumbnailSize = 64);
	void thumbnail_destroy();
	void thumbnail_update();

	// Get a texture and uv coordinates within the texture.
	const TextureGpu* getThumbnail(EditorObj3D* obj3d, Vec2f* uv0, Vec2f* uv1, bool animate = false);
}
