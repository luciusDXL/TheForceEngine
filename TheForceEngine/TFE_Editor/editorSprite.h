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
#include <TFE_RenderBackend/renderBackend.h>
#include <vector>

namespace TFE_Editor
{
	struct SpriteCell
	{
		// Sub-texture data.
		u16 u, v;
		u16 w, h;
	};

	struct SpriteFrame
	{
		s32 offsetX;
		s32 offsetY;
		s32 flip;
		s32 cellIndex;
		f32 widthWS;
		f32 heightWS;
	};

	struct SpriteView
	{
		s32 frameIndex[32];
	};

	struct SpriteAnim
	{
		f32 worldWidth;
		f32 worldHeight;
		s32 frameRate;
		s32 frameCount;
		SpriteView views[32];
	};

	struct EditorSprite
	{
		TextureGpu* texGpu = nullptr;
		std::vector<SpriteAnim>  anim;
		std::vector<SpriteFrame> frame;
		std::vector<SpriteCell>  cell;

		u8   paletteIndex = 0;
		u8   lightLevel = 32;
		u8   pad[2];
		char name[64] = "";
	};
	enum SpriteSourceType
	{
		SPRITE_WAX,
		SPRITE_COUNT
	};
		
	void freeCachedSprites();
	void freeSprite(const char* name);
	bool loadEditorSprite(SpriteSourceType type, Archive* archive, const char* filename, const u32* palette, s32 palIndex, EditorSprite* sprite);
	bool loadEditorSpriteLit(SpriteSourceType type, Archive* archive, const char* filename, const u32* palette, s32 palIndex, const u8* colormap, s32 lightLevel, EditorSprite* sprite);
}
