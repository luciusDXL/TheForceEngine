#include "robject.h"
#include "level.h"

namespace TFE_Level
{
	void computeTransform3x3(fixed16_16* transform, angle14_32 yaw, angle14_32 pitch, angle14_32 roll);

	SecObject* allocateObject()
	{
		SecObject* obj = (SecObject*)malloc(sizeof(SecObject));
		obj->yaw = 0;
		obj->pitch = 0;
		obj->roll = 0;
		obj->frame = 0;
		obj->anim = 0;
		obj->worldWidth = -1;
		obj->ptr = nullptr;
		obj->sector = nullptr;
		obj->logic = nullptr;
		obj->projectileLogic = nullptr;
		obj->type = OBJ_TYPE_SPIRIT;
		obj->entityFlags = ETFLAG_NONE;
		obj->worldHeight = -1;
		obj->flags = 0;
		obj->self = obj;
		return obj;
	}

	void freeObject(SecObject* obj)
	{
		free(obj);
	}

	void obj3d_setData(SecObject* obj, JediModel* pod)
	{
		obj->model = pod;
		obj->type = OBJ_TYPE_3D;
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
		angle14_32 yaw = floor16(obj->yaw);
		angle14_32 pitch = floor16(obj->pitch >> 16);
		angle14_32 roll = floor16(obj->roll >> 16);

		computeTransform3x3(obj->transform, yaw, pitch, roll);
	}

	void setupObj_Spirit(SecObject* obj)
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

		WaxAnim* anim = WAX_AnimPtr(data, 0);
		WaxView* view = WAX_ViewPtr(data, anim, 0);
		WaxFrame* frame = WAX_FramePtr(data, view, 0);
		WaxCell* cell = WAX_CellPtr(data, frame);

		if (obj->worldWidth == -1)
		{
			const fixed16_16 width = intToFixed16(TFE_CoreMath::abs(cell->sizeX));
			obj->worldWidth = div16(mul16(data->xScale, width), SPRITE_SCALE_FIXED);
		}
		if (obj->worldHeight == -1)
		{
			const fixed16_16 height = intToFixed16(TFE_CoreMath::abs(cell->sizeY));
			obj->worldHeight = div16(mul16(data->yScale, height), SPRITE_SCALE_FIXED);
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
			const fixed16_16 width = intToFixed16(TFE_CoreMath::abs(cell->sizeX));
			obj->worldWidth = div16(width, SPRITE_SCALE_FIXED);
		}
		if (obj->worldHeight == -1)
		{
			const fixed16_16 height = intToFixed16(TFE_CoreMath::abs(cell->sizeY));
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

		sinCosFixed(yaw, sinYaw, cosYaw);
		sinCosFixed(pitch, sinPch, cosPch);
		sinCosFixed(roll, sinRol, cosRol);

		transform[0] = mul16(cosYaw, cosRol);
		transform[1] = mul16(cosPch, sinRol) + mul16(mul16(sinPch, sinYaw), cosPch);
		transform[2] = mul16(sinPch, sinRol) - mul16(mul16(cosPch, sinYaw), cosRol);

		transform[3] = -mul16(cosYaw, sinRol);
		transform[4] = mul16(cosPch, cosRol) - mul16(mul16(sinPch, sinYaw), sinRol);
		transform[5] = mul16(sinPch, cosRol) + mul16(mul16(cosPch, sinYaw), sinRol);

		transform[6] = sinYaw;
		transform[7] = -mul16(sinPch, cosYaw);
		transform[8] = mul16(cosPch, cosYaw);
	}
} // namespace TFE_Level