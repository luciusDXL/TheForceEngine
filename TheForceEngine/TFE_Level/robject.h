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
	OBJ_TYPE_SPRITE = 1,
	OBJ_TYPE_3D     = 2,
	OBJ_TYPE_FRAME  = 3,
};

enum ObjectFlags
{
	OBJ_FLAG_RENDERABLE = (1 << 2),		// The object is renderable, such as a 3D object, sprite or frame.
	OBJ_FLAG_FULLBRIGHT = (1 << 3),		// The object should be rendered fullbright (no shading).
};

enum ObjectTypeFlags
{
	OTFLAG_NONE = 0,
	// ... Unknown.
	OTFLAG_PLAYER = (1u << 31u),
	OTFLAG_UNSIGNED = 0xffffffff,
};

struct SecObject
{
	SecObject* self;
	s32  type;
	u32  typeFlags;

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

	// See ObjectFlags above.
	u32 flags;

	// Orientation.
	angle14_16 pitch;
	angle14_16 yaw;
	angle14_16 roll;
	// index in containing sector object list.
	s16 index;
};
