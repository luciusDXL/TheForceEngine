#include "robject.h"
#include "level.h"
#include <TFE_Game/igame.h>
#include <TFE_Jedi/Memory/allocator.h>
#include <TFE_Jedi/Serialization/serialization.h>
#include <TFE_DarkForces/logic.h>
#include <TFE_Memory/chunkedArray.h>

namespace TFE_Jedi
{
	JBool s_freeObjLock = JFALSE;

	void computeTransform3x3(fixed16_16* transform, angle14_32 yaw, angle14_32 pitch, angle14_32 roll);

	SecObject* allocateObject()
	{
		SecObject* obj = objData_allocFromArray();
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
		obj->serializeIndex = 0;
		obj->defIndex = -1;
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
		objData_freeToArray(obj);

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
		computeTransform3x3(obj->transform, obj->yaw, obj->pitch, obj->roll);
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
		WaxCell* cell = data ? WAX_CellPtr(data, data) : nullptr;

		if (obj->worldWidth == -1 && cell)
		{
			const fixed16_16 width = intToFixed16(TFE_Jedi::abs(cell->sizeX)/2);
			obj->worldWidth = div16(width, SPRITE_SCALE_FIXED);
		}
		if (obj->worldHeight == -1 && cell)
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

		fixed16_16 sRol_sPitch = mul16(sinRol, sinPch);
		fixed16_16 cRol_sPitch = mul16(cosRol, sinPch);

		transform[0] =  mul16(cosRol, cosYaw) + mul16(sRol_sPitch, sinYaw);
		transform[1] = -mul16(sinRol, cosYaw) + mul16(cRol_sPitch, sinYaw);
		transform[2] =  mul16(cosPch, sinYaw);

		transform[3] =  mul16(cosPch, sinRol);
		transform[4] =  mul16(cosRol, cosPch);
		transform[5] = -sinPch;

		transform[6] = -mul16(cosRol, sinYaw) + mul16(sRol_sPitch, cosYaw);
		transform[7] =  mul16(sinRol, sinYaw) + mul16(cRol_sPitch, cosYaw);
		transform[8] =  mul16(cosPch, cosYaw);
	}
} // namespace TFE_Jedi