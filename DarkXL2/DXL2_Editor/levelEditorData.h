#pragma once
/////////////////////////////////////////////////////////////////////////
// DarkXL 2 Level Editor Data
// The runtime has fixed sectors and INF data that doesn't change in size
// whereas the data the Level Editor needs to access and change is
// constantly malleable. So the LevelEditor needs to copy the tight
// runtime structures into more malleable structures and be able to reverse
// the process in order to test.
/////////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>
#include <DXL2_Asset/levelAsset.h>
#include <DXL2_Asset/paletteAsset.h>
#include <DXL2_Asset/infAsset.h>
#include <DXL2_Asset/levelObjectsAsset.h>
#include <DXL2_RenderBackend/textureGpu.h>
#include <DXL2_Game/physics.h>
#include <string>
#include <map>

// TODO:
// Implement conversion routine back to runtime data.
// Add conversion from INF to editor and back.
// Convert the level editor to use this instead of the source data directly.

#define TEXTURES_GOB_START_TEX 41
struct Model;

enum InfType
{
	INF_NONE = 0,
	INF_SELF,
	INF_OTHER,
	INF_COUNT,
	INF_ALL = INF_COUNT
};

enum InfTriggerType
{
	TRIGGER_LINE = 0,
	TRIGGER_SWITCH
};

struct Target
{
	std::string sectorName;
	s32 wallId;
};

struct EditorInfArg
{
	union
	{
		s32 iValue;
		f32 fValue;
	};
	std::string sValue;
};

struct EditorInfFunction
{
	u32 funcId;
	std::vector<Target> client;
	std::vector<EditorInfArg> arg;
};

// A trigger will have 1 stop with code = 0 | funcCount << 8, time = 0
struct EditorInfStop
{
	u8  stopValue0Type;
	u8  stopValue1Type;
	EditorInfArg value0;
	f32 time;		// time at stop or 0.
	std::vector<EditorInfFunction> func;
};

struct EditorInfVariables
{
	bool master = true;	// Determines if an elevator or trigger can function.
	u32 event = 0;		// Custom event value(s) for trigger.
	s32 start = 0;		// Determines which stop an elevator starts at on level load.
	/////////////////////// Masks ///////////////////////
	u32 event_mask = 0;	// (InfEventMask) Determines if an event will operate on an elevator or trigger. Defaults: 52 (elev basic, inv, basic_auto), 60 (morph1/2, spin1/2), other elev 0; trigger 0xffffffff
	u32 entity_mask =   // (InfEntityMask) Defines the type of entities that can activate a Trigger.
		INF_ENTITY_PLAYER;
	//////////////////////// Flags & keys ///////////////////////
	u32 key = 0;		// Key required to activate an elevator (yellow, blue, red).
	u32 flags = 0;		// (InfMoveFlags) Determines whether the player moves with morphing or scrolling elevators. Defaults: 3 (scroll_floor, morph2, spin2) otherwise 0.
	/////////////////////// Movement & rotation ///////////////////////
	Vec2f center =		// Center coordinates for rotation.
	{ 0.0f, 0.0f };
	f32 angle = 0.0f;	// Direction of texture scrolling or horizontal elevators. For walls 0 = down others 0 = north.
	f32 speed = 0.0f;	// Speed an elevator moves between stops. 0 = instant.
	f32 speed_addon[2]; // Addon for door_mid
	/////////////////////// Sound ///////////////////////
	u32 sound[3];		// index | soundAssetId << 8 (TODO: Fill in with defaults)
	/////////////////////// Target/Slave ///////////////////////
	std::string target;	// Target sector for teleport.
};

struct EditorInfClassData
{
	u8 iclass, isubclass;
	u8 pad8[2];

	EditorInfVariables var;
	std::vector<EditorInfStop> stop;
	std::vector<std::string> slaves;
};

struct EditorInfItem
{
	std::string sectorName;
	s32 wallId;

	u8 type;
	u8 pad8[3];

	std::vector<EditorInfClassData> classData;
};

struct EditorInfData
{
	std::vector<EditorInfItem> item;
};

struct EditorTexture
{
	TextureGpu* texture = nullptr;
	u32  width = 0;
	u32  height = 0;
	f32  rect[4];
	Vec2f scale = { 1.0f, 1.0f };
	char name[64] = "";
};

struct EditorLevelObject
{
	// Render Data
	ObjectClass oclass;
	std::string dataFile;
	Vec3f pos;
	Vec3f orientation;
	s32 difficulty;

	// Editor display
	EditorTexture* display;
	Model* displayModel;
	// Bounds
	Vec3f worldCen;
	Vec3f worldExt;
	// Matrix
	Mat3  rotMtx;
	Mat3  rotMtxT;

	// Common Variables.
	u32 comFlags = 0;		// common flags - see LogicCommonFlags.
	f32 radius = 0.0f;
	f32 height = 0.0f;

	// Logics
	std::vector<Logic> logics;
	std::vector<EnemyGenerator> generators;
};

struct EditorSectorTexture
{
	EditorTexture* tex;
	u16 flag;
	f32 offsetX, offsetY;
	u32 frame;
};

struct EditorWall
{
	u16 i0, i1;
	EditorSectorTexture mid;
	EditorSectorTexture top;
	EditorSectorTexture bot;
	EditorSectorTexture sign;
	s32 adjoin;	// visible adjoin
	s32 walk;	// collision adjoin
	s32 mirror;	// wall in visible adjoin.
	s16 light;	// light level.
	s16 pad16;
	u32 flags[4];
	InfType infType;
	InfTriggerType triggerType;

	// INF
	EditorInfItem* infItem;
};

struct SectorTriangles
{
	u32 count;
	u32 timestamp;
	std::vector<Vec2f> vtx;
};

struct EditorSector
{
	u32 id;
	char name[32];
	u8 ambient;
	EditorSectorTexture floorTexture;
	EditorSectorTexture ceilTexture;
	f32 floorAlt;
	f32 ceilAlt;
	f32 secAlt;
	u32 flags[3];
	s8 layer;
	InfType infType;

	// Dynamically resizable, self-contained geometry data.
	std::vector<EditorWall> walls;
	std::vector<Vec2f> vertices;

	// Objects
	std::vector<EditorLevelObject> objects;

	// INF
	EditorInfItem* infItem;

	// Polygon data.
	SectorTriangles triangles;

	// Update flag
	bool needsUpdate;
};

struct InfEditState
{
	EditorSector* sector;
	EditorInfItem* item;
	s32 wallId = -1;
	std::vector<char>  itemMem;
	std::vector<char*> itemList;
	s32 editIndex;
};

typedef std::map<std::string, EditorTexture*> TextureMap;

struct EditorLevel
{
	std::string name;
	f32 parallax[2];

	s8 layerMin;
	s8 layerMax;
	u8 pad8[2];

	std::vector<EditorTexture> textures;
	TextureMap textureMap;

	std::vector<EditorSector> sectors;
};

struct RayHitInfoLE
{
	Vec3f hitPoint;
	s32 hitSectorId;
	s32 hitWallId;
	RayHitPart hitPart;

	s32 hitObjectId;
};

namespace LevelEditorData
{
	// Convert runtime level data to editor format.
	bool convertLevelDataToEditor(const LevelData* levelData, const Palette256* palette, const InfData* infData, const LevelObjectData* objData);

	// Update any sectors that have been flagged. This handles re-triangulation and any other updates needed for rendering.
	void updateSectors();

	// Convert runtime level data from editor format.
	bool generateLevelData();
	bool generateInfAsset();
	bool generateObjects();

	EditorTexture* createTexture(const Texture* src);

	// Get the editor level data.
	EditorLevel* getEditorLevelData();

	// Inf Edit State
	InfEditState* getInfEditState();

	s32 findSector(s32 layer, const Vec2f* pos);
	EditorSector* getSector(const char* name);

	s32 findClosestWall(s32* sectorId, s32 layer, const Vec2f* pos, f32 maxDist);
	bool traceRay(const Ray* ray, RayHitInfoLE* hitInfo);
}