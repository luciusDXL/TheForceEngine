#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Level/Map
// Handles the following:
// 1. Parsing the level data (.LEV)
// 2. Runtime storage and manipulation of level data
//    (vertices, lines, sectors)
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Asset/textureAsset.h>
#include <string>
#include <vector>

/*
Finished flags:
SEC_FLAGS1_EXTERIOR
SEC_FLAGS1_PIT
SEC_FLAGS1_EXT_ADJ
SEC_FLAGS1_EXT_FLOOR_ADJ
SEC_FLAGS1_NOWALL_DRAW

Next:

*/
enum SectorFlags1
{
	SEC_FLAGS1_EXTERIOR = (1 << 0),
	SEC_FLAGS1_DOOR = (1 << 1),
	SEC_FLAGS1_MAG_SEAL = (1 << 2),
	SEC_FLAGS1_EXT_ADJ = (1 << 3),
	SEC_FLAGS1_ICE_FLOOR = (1 << 4),
	SEC_FLAGS1_SNOW_FLOOR = (1 << 5),
	SEC_FLAGS1_EXP_WALL = (1 << 6),
	SEC_FLAGS1_PIT = (1 << 7),
	SEC_FLAGS1_EXT_FLOOR_ADJ = (1 << 8),
	SEC_FLAGS1_CRUSHING = (1 << 9),
	SEC_FLAGS1_NOWALL_DRAW = (1 << 10),
	SEC_FLAGS1_LOW_DAMAGE = (1 << 11),
	SEC_FLAGS1_HIGH_DAMAGE = (1 << 12),
	SEC_FLAGS1_NO_SMART_OBJ = (1 << 13),
	SEC_FLAGS1_SMART_OBJ = (1 << 14),
	SEC_FLAGS1_SUBSECTOR = (1 << 15),
	SEC_FLAGS1_SAFESECTOR = (1 << 16),
	SEC_FLAGS1_RENDERED = (1 << 17),
	SEC_FLAGS1_PLAYER = (1 << 18),
	SEC_FLAGS1_SECRET = (1 << 19),

	SEC_FLAGS1_MORPH = (1 << 20),
	SEC_FLAGS1_MOVEFLOOR = (1 << 21),
	SEC_FLAGS1_MOVECEIL = (1 << 22),
	SEC_FLAGS1_FLOOR_MOVED = (1 << 23),
	SEC_FLAGS1_CEIL_MOVED = (1 << 24),
	SEC_FLAGS1_CHUTE = (1 << 25),
};

/*
Finished flags:
WF1_FLIP_HORIZ

Next:
WF1_ADJ_MID_TEX
WF1_ILLUM_SIGN
*/

enum WallFlags1
{
	WF1_ADJ_MID_TEX = (1 << 0),	// the mid texture is rendered even with adjoin (maskwall)
	WF1_ILLUM_SIGN = (1 << 1),  // render a sign.
	WF1_FLIP_HORIZ = (1 << 2),	// flip texture horizontally.
	WF1_CHANGE_WALL_LIGHT = (1 << 3),	// elevator can change wall light
	WF1_TEX_ANCHORED = (1 << 4),
	WF1_WALL_MORPHS = (1 << 5),
	WF1_SCROLL_TOP_TEX = (1 << 6),	// elevator can scroll various textures
	WF1_SCROLL_MID_TEX = (1 << 7),
	WF1_SCROLL_BOT_TEX = (1 << 8),
	WF1_SCROLL_SIGN_TEX = (1 << 9),
	WF1_HIDE_ON_MAP = (1 << 10),
	WF1_SHOW_NORMAL_ON_MAP = (1 << 11),
	WF1_SIGN_ANCHORED = (1 << 12),
	WF1_DAMAGE_WALL = (1 << 13),	// wall damages player
	WF1_SHOW_AS_LEDGE_ON_MAP = (1 << 14),
	WF1_SHOW_AS_DOOR_ON_MAP = (1 << 15),
};

// WallFlags2 used internally.

enum WallFlags3
{
	WF3_ALWAYS_WALK = (1 << 0),
	WF3_SOLID_WALL = (1 << 1),
	WF3_PLAYER_WALK_ONLY = (1 << 2),	// players can walk through but not enemies.
	WF3_CANNOT_FIRE_THROUGH = (1 << 3),	// cannot fire through the wall.
};

struct SectorTexture
{
	s16 texId;
	u16 flag;
	f32 offsetX, offsetY;
	f32 baseOffsetX, baseOffsetY;
	u32 frame;
};

struct SectorWall
{
	u16 i0, i1;
	SectorTexture mid;
	SectorTexture top;
	SectorTexture bot;
	SectorTexture sign;
	s32 adjoin;	// visible adjoin
	s32 walk;	// collision adjoin
	s32 mirror;	// wall in visible adjoin.
	s16 light;	// light level.
	s16 pad16;
	u32 flags[4];
};

struct Sector
{
	u32 id;
	char name[32];
	u8 ambient;
	SectorTexture floorTexture;
	SectorTexture ceilTexture;
	Vec3f center;
	f32 floorAlt;
	f32 ceilAlt;
	f32 secAlt;
	u32 flags[3];
	s8 layer;

	u16 vtxCount;
	u16 wallCount;
	u32 vtxOffset;
	u32 wallOffset;
};

struct LevelData
{
	char name[32];
	char palette[32];
	char music[32];
	f32 parallax[2];

	s8 layerMin;
	s8 layerMax;
	u8 pad8[2];

	// TODO: Move this over to use the frame base allocator instead of churning through memory with vectors.
	std::vector<Texture*>     textures;
	std::vector<Sector>       sectors;
	std::vector<Vec2f>        vertices;
	std::vector<SectorWall>   walls;
};

namespace TFE_LevelAsset
{
	bool load(const char* name);
	void unload();

	void save(const char* name, const char* path);

	const char* getName();
	LevelData* getLevelData();
};
