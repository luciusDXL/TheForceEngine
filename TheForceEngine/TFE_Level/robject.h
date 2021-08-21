#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Asset/spriteAsset_Jedi.h>
#include "core_math.h"

using namespace TFE_CoreMath;

struct JediModel;
struct RSector;
	
enum ObjectType
{
	OBJ_TYPE_SPIRIT = 0,
	OBJ_TYPE_SPRITE = 1,
	OBJ_TYPE_3D     = 2,
	OBJ_TYPE_FRAME  = 3,
};

enum ObjectFlags
{
	OBJ_FLAG_NONE = 0,
	OBJ_FLAG_ENEMY           = FLAG_BIT(0),  // Enemy Unit
	OJB_FLAG_BIT_1           = FLAG_BIT(1),  // Unknown
	OBJ_FLAG_NEEDS_TRANSFORM = FLAG_BIT(2),  // Object needs to be transformed and rendered.
	OBJ_FLAG_FULLBRIGHT      = FLAG_BIT(3),  // Rendered as fullbright (not lit).
	OBJ_FLAG_HAS_COLLISION   = FLAG_BIT(4),  // Object is solid (has collision).
	OBJ_FLAG_BOSS            = FLAG_BIT(5),  // Boss enemy.
	OBJ_FLAG_BIT_6           = FLAG_BIT(6),  // Unknown
};

enum EntityTypeFlags
{
	ETFLAG_AI_ACTOR       = FLAG_BIT(0),	// AI Actor - moves and acts on its own.
	ETFLAG_HAS_GRAVITY    = FLAG_BIT(1),	// This entity is effected by gravity.
	ETFLAG_FLYING         = FLAG_BIT(2),	// This entity is flying (can change Y height).
	ETFLAG_PROJECTILE     = FLAG_BIT(3),	// The entity is a projectile.
	ETFLAG_CAN_WAKE       = FLAG_BIT(6),	// An inactive object or animation waiting to be "woken up" - such as Vues waiting to play.
	ETFLAG_PICKUP         = FLAG_BIT(7),	// An item pickup.
	ETFLAG_SCENERY        = FLAG_BIT(8),	// Set for scenery - is this really collision?
	ETFLAG_512			  = FLAG_BIT(9),	// Unknown
	ETFLAG_CAN_DISABLE    = FLAG_BIT(10),	// An object that can be enabled or disabled by INF.
	ETFLAG_SMART_OBJ      = FLAG_BIT(11),	// An object that can manipulate the level, such as opening doors.
	ETFLAG_LANDMINE       = FLAG_BIT(13),	// Specifically set if a pre-placed landmine.
	ETFLAG_LANDMINE_WPN   = FLAG_BIT(16),	// A landmine weapon - i.e. player placed landmines.
	ETFLAG_PLAYER         = FLAG_BIT(31),	// This is the player object.

	ETFLAG_NONE = 0,
};

#define SPRITE_SCALE_FIXED FIXED(10)

struct SecObject
{
	SecObject* self;
	ObjectType type;
	u32 entityFlags;    // see EntityTypeFlags above.
	void* projectileLogic;	// projectile logic.

	// Position
	vec3_fixed posWS;
	vec3_fixed posVS;

	// World Size
	fixed16_16 worldWidth;
	fixed16_16 worldHeight;

	// 3x3 transformation matrix.
	fixed16_16 transform[9];

	// Rendering data.
	union
	{
		JediModel* model;
		Wax*   wax;
		WaxFrame* fme;
		void* ptr;
	};
	s32 frame;
	s32 anim;
	RSector* sector;
	void* logic;

	// See ObjectFlags above.
	u32 flags;

	// Orientation.
	angle14_16 pitch;
	angle14_16 yaw;
	angle14_16 roll;
	// index in containing sector object list.
	s16 index;
};

namespace TFE_Level
{
	SecObject* allocateObject();
	void freeObject(SecObject* obj);

	// Spirits
	void setupObj_Spirit(SecObject* obj);

	// 3D objects
	void obj3d_setData(SecObject* obj, JediModel* pod);
	void obj3d_computeTransform(SecObject* obj);

	// Sprites
	void sprite_setData(SecObject* obj, JediWax* data);

	// Frames
	void frame_setData(SecObject* obj, WaxFrame* data);

	extern JBool s_freeObjLock;
}
