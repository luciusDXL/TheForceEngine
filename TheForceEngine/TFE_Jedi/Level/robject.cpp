#include "robject.h"
#include "level.h"
#include <TFE_Game/igame.h>
#include <TFE_Jedi/Memory/allocator.h>
#include <TFE_DarkForces/logic.h>

namespace TFE_Jedi
{
	JBool s_freeObjLock = JFALSE;

	void computeTransform3x3(fixed16_16* transform, angle14_32 yaw, angle14_32 pitch, angle14_32 roll);

	SecObject* allocateObject()
	{
		SecObject* obj = (SecObject*)level_alloc(sizeof(SecObject));
		obj->yaw = 0;
		obj->pitch = 0;
		obj->roll = 0;
		obj->frame = 0;
		obj->anim = 0;
		obj->worldWidth = -1;
		obj->worldHeight = -1;
		obj->ptr = nullptr;
		obj->sector = nullptr;
		obj->logic = nullptr;
		obj->projectileLogic = nullptr;
		obj->type = OBJ_TYPE_SPIRIT;
		obj->entityFlags = ETFLAG_NONE;
		obj->flags = OBJ_FLAG_NEEDS_TRANSFORM | OBJ_FLAG_MOVABLE;
		obj->self = obj;
		return obj;
	}

	void freeObject(SecObject* obj)
	{
		s_freeObjLock = JTRUE;
		// Free Logics
		Logic** head = (Logic**)allocator_getHead((Allocator*)obj->logic);
		while (head)
		{
			Logic* logic = *head;
			if (logic->cleanupFunc)
			{
				logic->cleanupFunc(logic);
			}
			
			head = (Logic**)allocator_getNext((Allocator*)obj->logic);
		}

		allocator_free((Allocator*)obj->logic);
		sector_removeObject(obj);
		level_free(obj);

		s_freeObjLock = JFALSE;
	}

	void obj3d_setData(SecObject* obj, JediModel* pod)
	{
		obj->model = pod;
		obj->type = OBJ_TYPE_3D;
		obj->flags |= OBJ_FLAG_NEEDS_TRANSFORM;
		if (obj->worldWidth == -1)	// the initial value.
		{
			obj->worldWidth = 0;
		}
		if (obj->worldHeight == -1)
		{
			obj->worldHeight = 0;
		}
	}
		
	void obj3d_computeTransform(SecObject* obj)
	{
		computeTransform3x3(obj->transform, -obj->yaw, obj->pitch, obj->roll);
	}

	void spirit_setData(SecObject* obj)
	{
		obj->ptr = nullptr;
		obj->type = OBJ_TYPE_SPIRIT;
		if (obj->worldWidth == -1)
		{
			obj->worldWidth = 0;
		}
		if (obj->worldHeight == -1)
		{
			obj->worldHeight = 0;
		}
	}

	void sprite_setData(SecObject* obj, JediWax* data)
	{
		obj->wax = data;
		obj->type = OBJ_TYPE_SPRITE;
		obj->flags |= OBJ_FLAG_NEEDS_TRANSFORM;

		if (data)
		{
			WaxAnim* anim   = WAX_AnimPtr(data, 0);
			WaxView* view   = WAX_ViewPtr(data, anim, 0);
			WaxFrame* frame = WAX_FramePtr(data, view, 0);
			WaxCell* cell   = WAX_CellPtr(data, frame);

			if (obj->worldWidth == -1)
			{
				const fixed16_16 width = intToFixed16(TFE_Jedi::abs(cell->sizeX)/2);
				obj->worldWidth = div16(mul16(data->xScale, width), SPRITE_SCALE_FIXED);
			}
			if (obj->worldHeight == -1)
			{
				const fixed16_16 height = intToFixed16(TFE_Jedi::abs(cell->sizeY));
				obj->worldHeight = div16(mul16(data->yScale, height), SPRITE_SCALE_FIXED);
			}
		}
	}

	void frame_setData(SecObject* obj, WaxFrame* data)
	{
		obj->fme = data;
		obj->type = OBJ_TYPE_FRAME;
		obj->flags |= OBJ_FLAG_NEEDS_TRANSFORM;
		WaxCell* cell = WAX_CellPtr(data, data);

		if (obj->worldWidth == -1)
		{
			const fixed16_16 width = intToFixed16(TFE_Jedi::abs(cell->sizeX)/2);
			obj->worldWidth = div16(width, SPRITE_SCALE_FIXED);
		}
		if (obj->worldHeight == -1)
		{
			const fixed16_16 height = intToFixed16(TFE_Jedi::abs(cell->sizeY));
			obj->worldHeight = div16(height, SPRITE_SCALE_FIXED);
		}
	}

	/////////////////////////////////
	// Internal
	void computeTransform3x3(fixed16_16* transform, angle14_32 yaw, angle14_32 pitch, angle14_32 roll)
	{
		fixed16_16 sinYaw, cosYaw;
		fixed16_16 sinPch, cosPch;
		fixed16_16 sinRol, cosRol;

		sinCosFixed(yaw,   &sinYaw, &cosYaw);
		sinCosFixed(pitch, &sinPch, &cosPch);
		sinCosFixed(roll,  &sinRol, &cosRol);

		transform[0] = mul16(cosYaw, cosRol);
		transform[1] = mul16(cosPch, sinRol) + mul16(mul16(sinPch, sinYaw), cosPch);
		transform[2] = mul16(sinPch, sinRol) - mul16(mul16(cosPch, sinYaw), cosRol);

		transform[3] = -mul16(cosYaw, sinRol);
		transform[4] = mul16(cosPch, cosRol) - mul16(mul16(sinPch, sinYaw), sinRol);
		transform[5] = mul16(sinPch, cosRol) + mul16(mul16(cosPch, sinYaw), sinRol);

		transform[6] =  sinYaw;
		transform[7] = -mul16(sinPch, cosYaw);
		transform[8] =  mul16(cosPch, cosYaw);
	}
} // namespace TFE_Jedi