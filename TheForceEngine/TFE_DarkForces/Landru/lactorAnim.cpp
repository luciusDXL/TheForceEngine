#include "lactorAnim.h"
#include "lactorDelt.h"
#include "lsystem.h"
#include "cutscene_film.h"
#include "lview.h"
#include "ltimer.h"
#include <TFE_System/endian.h>
#include <TFE_Game/igame.h>
#include <assert.h>

#include "ldraw.h"

namespace TFE_DarkForces
{
	static JBool s_animActorInit = JFALSE;

	void  lactorAnim_getFrame(LActor* actor, LRect* rect);
	void  lactorAnim_setState(LActor* actor, s16 state, s16 stateFract);
	void  lactorAnim_initActor(LActor* actor, u8** array, LRect* frame, s16 x, s16 y, s16 zPlane);
	JBool lactorAnim_draw(LActor* actor, LRect* rect, LRect* clipRect, s16 x, s16 y, JBool refresh);
	void  lactorAnim_update(LActor* actor);
	void  lactorAnim_getBounds(LActor* actor, LRect* rect);
		
	void lactorAnim_init()
	{
		lactor_createType(CF_TYPE_ANIM_ACTOR, lactorAnim_getFrame, lactorAnim_setState);
		s_animActorInit = JTRUE;
	}

	void lactorAnim_destroy()
	{
		if (s_animActorInit)
		{
			lactor_destroyType(CF_TYPE_ANIM_ACTOR);
		}
		s_animActorInit = JFALSE;
	}

	void lactorAnim_getFrame(LActor* actor, LRect* rect)
	{
		lrect_set(rect, 0, 0, 0, 0);
		s16* data = (s16*)lactor_getArrayData(actor, actor->state);
		if (data && data[0] != -1 && data[1] != -1)
		{
			lrect_set(rect, TFE_Endian::le16_to_cpu(data[0]), TFE_Endian::le16_to_cpu(data[1]), TFE_Endian::le16_to_cpu(data[2]) + 1, TFE_Endian::le16_to_cpu(data[3]) + 1);
			JBool xFlip, yFlip;
			lactor_getFlip(actor, &xFlip, &yFlip);
			lrect_flip(rect, &actor->bounds, xFlip, yFlip);
		}
	}

	void lactorAnim_setState(LActor* actor, s16 state, s16 stateFract)
	{
		actor->state = state;
		actor->stateFract = stateFract;

		LRect rect;
		lactorAnim_getFrame(actor, &rect);
		actor->w = rect.right - rect.left;
		actor->h = rect.bottom - rect.top;
	}
		
	LActor* lactorAnim_alloc(u8** array, LRect* frame, s16 xOffset, s16 yOffset, s16 zPlane)
	{
		LActor* actor = lactor_alloc(0);
		if (!actor) { return nullptr; }

		lactorAnim_initActor(actor, array, frame, xOffset, yOffset, zPlane);
		lactor_setName(actor, CF_TYPE_ANIM_ACTOR, "");
		lactor_keepData(actor);

		return actor;
	}

	LActor* lactorAnim_load(const char* name, LRect* rect, s16 x, s16 y, s16 zPlane)
	{
		LActor* actor = lactor_alloc(0);
		if (!actor)
		{
			return nullptr;
		}

		char animName[32];
		sprintf(animName, "%s.ANIM", name);
		FilePath path;
		if (TFE_Paths::getFilePath(animName, &path))
		{
			FileStream file;
			file.open(&path, Stream::MODE_READ);

			s16 animCount;
			file.read(&animCount);
			animCount = TFE_Endian::le16_to_cpu(animCount); 
			u8** array = (u8**)landru_alloc(sizeof(u8*) * animCount);
			if (array)
			{
				for (s32 i = 0; i < animCount; i++)
				{
					s32 deltaSize;
					file.read(&deltaSize);
					deltaSize = TFE_Endian::le32_to_cpu(deltaSize); 
					if (deltaSize <= 0) { array[i] = nullptr; continue; }

					array[i] = (u8*)landru_alloc(deltaSize);
					file.readBuffer(array[i], deltaSize);
				}
			}
			else
			{
				lactor_free(actor);
				file.close();
				return nullptr;
			}
			file.close();

			actor->arraySize = animCount;
			lactorAnim_initActor(actor, array, rect, x, y, zPlane);
			lactor_setName(actor, CF_TYPE_ANIM_ACTOR, name);
			lactorAnim_getBounds(actor, &actor->bounds);
		}
		else
		{
			lactor_free(actor);
			actor = nullptr;
		}

		return actor;
	}

	void lactorAnim_initActor(LActor* actor, u8** array, LRect* frame, s16 x, s16 y, s16 zPlane)
	{
		lrect_set(&actor->frame, frame->left, frame->top, frame->right, frame->bottom);
		actor->x = x;
		actor->y = y;
		actor->zplane = zPlane;
		lactor_discardData(actor);

		actor->drawFunc   = lactorAnim_draw;
		actor->updateFunc = lactorAnim_update;
		actor->array = array;
		lactor_addActor(actor);

		LRect rect;
		lactor_getFrame(actor, &rect);
		actor->w = rect.right - rect.left;
		actor->h = rect.bottom - rect.top;
	}

	void lactorAnim_getBounds(LActor* actor, LRect* rect)
	{
		lrect_clear(rect);
		s16 state = actor->state;
		for (s32 i = 0; i < actor->arraySize; i++)
		{
			LRect cellRect;
			actor->state = i;
			lactorAnim_getFrame(actor, &cellRect);
			lrect_enclose(rect, &cellRect);
		}

		actor->state = state;
	}

	JBool lactorAnim_draw(LActor* actor, LRect* rect, LRect* clipRect, s16 x, s16 y, JBool refresh)
	{
		if (!refresh)
		{
			return JFALSE;
		}
		u8* data = lactor_getArrayData(actor, actor->state);
				
		JBool retValue = JFALSE;
		if (data)
		{
			if (actor->flags & LAFLAG_HFLIP)
			{
				retValue = lactorDelt_drawFlippedClipped(actor, data, x, y, lactor_isDirty(actor));
			}
			else
			{
				retValue = lactorDelt_drawClipped(data, x, y, lactor_isDirty(actor));
			}
		}

		return retValue;
	}

	void lactorAnim_update(LActor* actor)
	{
		s16 prevState = actor->state;
		lactor_move(actor);
		lactor_moveFrame(actor);
		lactor_moveState(actor);

		if (actor->arraySize)
		{
			while (actor->state < 0) { actor->state += actor->arraySize; }
			while (actor->state >= actor->arraySize) { actor->state -= actor->arraySize; }
		}
		else
		{
			actor->state = 0;
		}

		if (prevState != actor->state)
		{
			LRect rect;
			lactorAnim_getFrame(actor, &rect);
			actor->w = rect.right - rect.left;
			actor->h = rect.bottom - rect.top;
		}
	}

}  // namespace TFE_DarkForces