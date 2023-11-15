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
#include <TFE_Polygon/polygon.h>
#include <TFE_Editor/history.h>

namespace LevelEditor
{
	enum LevelEditMode
	{
		LEDIT_DRAW = 1,
		//LEDIT_SMART,	// vertex + wall + height "smart" edit.
		LEDIT_VERTEX,	// vertex only
		LEDIT_WALL,		// wall only in 2D, wall + floor/ceiling in 3D
		LEDIT_SECTOR,
		LEDIT_ENTITY
	};

	enum DrawMode
	{
		DMODE_RECT = 0,
		DMODE_SHAPE,
		DMODE_RECT_VERT,
		DMODE_SHAPE_VERT,
		DMODE_COUNT
	};

	enum WallPart
	{
		WP_MID = 0,
		WP_TOP,
		WP_BOT,
		WP_SIGN,
		WP_COUNT
	};

	enum HitPart
	{
		HP_MID = 0,
		HP_TOP,
		HP_BOT,
		HP_SIGN,
		HP_FLOOR,
		HP_CEIL,
		HP_COUNT,
		HP_NONE = HP_COUNT
	};

	enum RayConst
	{
		LAYER_ANY = -256,
	};

	struct LevelTextureAsset
	{
		std::string name;
		TFE_Editor::AssetHandle handle = NULL_ASSET;
	};

	struct LevelTexture
	{
		s32 texIndex = -1;
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

		u32 ambient = 0;
		u32 flags[3] = { 0 };

		// Geometry
		std::vector<Vec2f> vtx;
		std::vector<EditorWall> walls;

		// Bounds
		Vec3f bounds[2];
		s32 layer = 0;

		// Polygon
		Polygon poly;

		// For searches.
		u32 searchKey = 0;
	};

	struct EditorLevel
	{
		std::string name;
		std::string slot;
		std::string palette;
		TFE_Editor::FeatureSet featureSet = TFE_Editor::FSET_VANILLA;

		// Sky Parallax.
		Vec2f parallax = { 1024.0f, 1024.0f };

		// Texture data.
		std::vector<LevelTextureAsset> textures;

		// Sector data.
		std::vector<EditorSector> sectors;

		// Level bounds.
		Vec3f bounds[2] = { 0 };
		s32 layerRange[2] = { 0 };
	};

	// Collision
	struct Ray
	{
		Vec3f origin;
		Vec3f dir;
		f32 maxDist;
		s32 layer;
	};

	struct RayHitInfo
	{
		// What was hit.
		s32 hitSectorId;
		s32 hitWallId;
		HitPart hitPart;
		// TODO: hitObj

		// Actual hit position.
		Vec3f hitPos;
		f32 dist;
	};

	bool loadLevelFromAsset(TFE_Editor::Asset* asset);
	void sectorToPolygon(EditorSector* sector);
	void polygonToSector(EditorSector* sector);

	TFE_Editor::EditorTexture* getTexture(s32 index);
	s32 getTextureIndex(const char* name);

	s32 findSector2d(s32 layer, const Vec2f* pos);
	bool traceRay(const Ray* ray, RayHitInfo* hitInfo, bool flipFaces, bool canHitSigns);

	f32 getWallLength(const EditorSector* sector, const EditorWall* wall);
	bool getSignExtents(const EditorSector* sector, const EditorWall* wall, Vec2f ext[2]);
	void centerSignOnSurface(const EditorSector* sector, EditorWall* wall);

	void level_createSnapshot(TFE_Editor::SnapshotBuffer* buffer);
	void level_unpackSnapshot(s32 id, u32 size, void* data);
}
