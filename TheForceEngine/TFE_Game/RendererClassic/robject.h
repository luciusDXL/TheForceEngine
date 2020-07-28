#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "rmath.h"
#include "rlimits.h"

struct RSector;
struct Model;
struct Wax;
struct WaxFrame;

enum ObjectType
{
	OBJ_TYPE_SPRITE = 1,
	OBJ_TYPE_3D = 2,
	OBJ_TYPE_FRAME = 3,
};

struct SecObject
{
	SecObject* self;
	s32  type;
	u32  typeFlags;

	vec3 posWS;
	vec3 posVS;

	s32 worldWidth;
	s32 worldHeight;
	s32 transform[9];

	// Rendering data.
	union
	{
		Model* model;
		Wax*   wax;
		WaxFrame* fme;
		void* ptr;
	};

	s32 frame;
	s32 anim;
	RSector* sector;

	// render = 4, fullbright = 8
	s32 flags;

	s16 pitch;
	s16 yaw;
	s16 roll;
	s16 index;	// index in containing sector object list.
};
