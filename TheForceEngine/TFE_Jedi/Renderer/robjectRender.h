#pragma once
//////////////////////////////////////////////////////////////////////
// Wall
// Dark Forces Derived Renderer - Wall functions
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Asset/spriteAsset_Jedi.h>
#include "rmath.h"

struct JediModel;
struct RSector;

namespace TFE_Jedi
{
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

	struct SecObject
	{
		SecObject* self;
		s32  type;
		u32  typeFlags;

		vec3 posWS;
		vec3 posVS;

		fixed16_16 worldWidth;
		fixed16_16 worldHeight;
		s32 transform[9];
		// Cached floating point transform (not used by Fixed Point sub-renderer).
		f32 transformFlt[9];

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
		s32 flags;

		s16 pitch;
		s16 yaw;
		s16 roll;
		s16 index;	// index in containing sector object list.

		// Temporary (until Jedi Renderer is finished)
		u32 gameObjId;
	};
}