#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Editor/EditorAsset/editorAsset.h>
#include <TFE_Editor/EditorAsset/editorTexture.h>
#include <TFE_Editor/editorProject.h>

namespace LevelEditor
{
	enum WallPart
	{
		WP_MID = 0,
		WP_TOP,
		WP_BOT,
		WP_SIGN,
		WP_COUNT
	};

	struct LevelTexture
	{
		TFE_Editor::AssetHandle handle = NULL_ASSET;
		Vec2f offset = { 0 };
	};

	struct EditorWall
	{
		LevelTexture tex[WP_COUNT] = {};

		s32 idx[2] = { 0 };
		s32 adjoinId = -1;
		s32 mirrorId = -1;

		u32 flags[3] = { 0 };
		s32 wallLight = 0;
	};

	struct EditorSector
	{
		s32 id = 0;
		std::string name;	// may be empty.

		LevelTexture floorTex = {};
		LevelTexture ceilTex = {};

		f32 floorHeight = 0.0f;
		f32 ceilHeight = 0.0f;
		f32 secHeight = 0.0f;

		s32 ambient = 0;
		u32 flags[3] = { 0 };

		// Geometry
		std::vector<Vec2f> vtx;
		std::vector<EditorWall> walls;

		// Bounds
		Vec3f bounds[2];
		s32 layer = 0;
	};

	struct EditorLevel
	{
		std::string name;
		std::string slot;
		std::string palette;
		TFE_Editor::FeatureSet featureSet = TFE_Editor::FSET_VANILLA;

		// Sky Parallax.
		Vec2f parallax = { 1024.0f, 1024.0f };

		// Sector data.
		std::vector<EditorSector> sectors;

		// Level bounds.
		Vec3f bounds[2] = { 0 };
		s32 layerRange[2] = { 0 };
	};

	bool loadLevelFromAsset(TFE_Editor::Asset* asset, EditorLevel* level);
}
