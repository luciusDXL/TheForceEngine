#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Archive/archive.h>
#include "editorColormap.h"
#include "editorFrame.h"
#include "editorSprite.h"
#include "editorTexture.h"
#include "EditorObj3D.h"
#include "editorLevelPreview.h"

namespace TFE_Editor
{
	enum AssetType : s32
	{
		TYPE_TEXTURE = 0,
		TYPE_SPRITE,
		TYPE_FRAME,
		TYPE_3DOBJ,
		TYPE_LEVEL,
		TYPE_PALETTE,
		TYPE_COLORMAP,
		TYPE_COUNT,
		TYPE_NOT_SET = TYPE_COUNT
	};
	#define NULL_ASSET AssetHandle(0)

	static const char* c_assetType[] =
	{
		"Texture",    // TYPE_TEXTURE
		"Sprite",     // TYPE_SPRITE
		"Frame",      // TYPE_FRAME
		"3D Object",  // TYPE_3DOBJ
		"Level",      // TYPE_LEVEL
		"Palette",    // TYPE_PALETTE
	};

	struct AssetColorData
	{
		const u32* palette;
		const u8* colormap;
		s32 palIndex;
		s32 lightLevel;
	};

	typedef u64 AssetHandle;

	AssetHandle loadAssetData(AssetType type, Archive* archive, const AssetColorData* colorData, const char* name);
	void  reloadAssetData(AssetHandle handle, Archive* archive, const AssetColorData* colorData);
	void  freeAssetData(AssetHandle handle);
	void* getAssetData(AssetHandle handle);

	void freeAllAssetData();
	void freeAllThumbnails();
}
