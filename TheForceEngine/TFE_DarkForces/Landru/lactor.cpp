#include "lactor.h"
#include "lview.h"
#include "ltimer.h"
#include <TFE_Game/igame.h>
#include <assert.h>

namespace TFE_DarkForces
{
	static JBool s_lactorInit  = JFALSE;
	static JBool s_zPlaneBuild = JFALSE;
	static JBool s_refreshActors;
	static LActorType* s_actorType = nullptr;
	static LActor* s_actorList     = nullptr;

	void lactor_freeData(LActor* actor);
	
	void lactor_init()
	{
		s_lactorInit = JTRUE;
		s_actorType = nullptr;
		s_actorList = nullptr;
	}

	void lactor_destroy()
	{
		s_lactorInit = JFALSE;
	}

	JBool lactor_createType(u32 type, LActorFrameFunc frameFunc, LActorStateFunc stateFunc)
	{
		assert(s_lactorInit);
		LActorType* newType = (LActorType*)game_alloc(sizeof(LActorType));
		if (newType)
		{
			newType->type = type;
			newType->frameFunc = frameFunc;
			newType->stateFunc = stateFunc;

			newType->next = s_actorType;
			s_actorType = newType;
		}
		return (newType) ? JTRUE : JFALSE;
	}

	void lactor_destroyType(u32 type)
	{
		LActorType* newType = s_actorType;
		LActorType* lastType = nullptr;
		while (newType && newType->type != type)
		{
			lastType = newType;
			newType = newType->next;
		}

		if (newType)
		{
			if (lastType) { lastType->next = newType->next; }
			else { s_actorType = s_actorType->next; }

			game_free(newType);
		}
	}

	LActorType* lactor_findType(u32 type)
	{
		LActorType* curType = s_actorType;
		while (curType && curType->type != type)
		{
			curType = curType->next;
		}
		return curType;
	}

	LActor* lactor_getList()
	{
		return s_actorList;
	}

	void lactor_addActor(LActor* actor)
	{
		LActor* curActor = s_actorList;
		LActor* lastActor = nullptr;

		while (curActor && curActor->zplane > actor->zplane)
		{
			lastActor = curActor;
			curActor = curActor->next;
		}

		if (!lastActor)
		{
			actor->next = curActor;
			s_actorList = actor;
		}
		else
		{
			actor->next = curActor;
			lastActor->next = actor;
		}

		actor->id = 0;
	}

	void lactor_removeActor(LActor* actor)
	{
		LActor* curActor = s_actorList;
		LActor* lastActor = nullptr;

		while (curActor && curActor != actor)
		{
			lastActor = curActor;
			curActor = curActor->next;
		}

		if (curActor)
		{
			if (!lastActor)
			{
				s_actorList = curActor->next;
			}
			else
			{
				lastActor->next = curActor->next;
			}
			actor->next = nullptr;
		}
	}

	LActor* lactor_alloc(s16 extend)
	{
		LActor* actor = (LActor*)game_alloc(sizeof(LActor) + extend);
		memset(actor, 0, sizeof(LActor) + extend);

		if (actor)
		{
			actor->next = nullptr;
			actor->start = 0;
			actor->stop = -1;

			actor->flags = LAFLAG_REFRESHABLE | LAFLAG_REFRESH | LAFLAG_DIRTY;

			actor->fgColor = 0x0f;
			actor->bgColor = 0x00;
			actor->xScale  = LA_SCALE_FACTOR;
			actor->yScale  = LA_SCALE_FACTOR;
		}
		return actor;
	}

	void lactor_free(LActor* actor)
	{
		if (!actor) { return; }
		lactor_freeData(actor);
		
		if (actor->varPtr)
		{
			game_free(actor->varPtr);
		}
		if (actor->varPtr2)
		{
			game_free(actor->varPtr2);
		}
		game_free(actor);
	}

	void lactor_freeList(LActor* actor)
	{
		while (actor)
		{
			LActor* next = actor->next;
			lactor_removeActor(actor);
			lactor_free(actor);

			actor = next;
		}
	}

	void lactor_refresh()
	{
		s_refreshActors = JTRUE;
	}

	void lactor_draw(JBool refresh)
	{
		refresh |= s_refreshActors;
		s_refreshActors = JFALSE;

		LActor* actorList = lactor_getList();
		for (s32 i = 0; i < LVIEW_COUNT; i++)
		{
			LRect rect, clipRect;
			lview_getFrame(i, &rect);

			LActor* curActor = actorList;
			if (!lrect_isEmpty(&rect))
			{
				while (curActor)
				{
					// This might not be needed in TFE.
					ltime_often();

					if (curActor->drawFunc && lactor_isVisible(curActor))
					{
						if (lview_clipObjToView(i, curActor->zplane, &curActor->frame, &rect, &clipRect))
						{
							s32 curRefresh = (curActor->flags&LAFLAG_REFRESH) | (curActor->flags&LAFLAG_REFRESHABLE) | refresh;
							// TODO:
							// set drawing canvas clip (&clipRect)
							s16 x, y;
							lactor_getRelativePos(curActor, &rect, &x, &y);
							curActor->drawFunc(curActor, &rect, &clipRect, x, y, curRefresh);
						}
					}

					curActor = curActor->next;
				}  // while (curActor)
			}  // if (!empty)
		}  // view loop

		// Clear actor refresh
		for (LActor* curActor = actorList; curActor; curActor = curActor->next)
		{
			curActor->flags &= ~LAFLAG_REFRESH;
		}
	}

	void lactor_update(LTick time)
	{
		LActor* actor = lactor_getList();
		while (actor)
		{
			if (actor->start == time)
			{
				lactor_start(actor);
				if (actor->stop == time)
				{
					lactor_stop(actor);
				}
			}
			else
			{
				if (actor->stop == time)
				{
					lactor_stop(actor);
				}
				else if (actor->updateFunc && (actor->flags & LAFLAG_ACTIVE))
				{
					actor->updateFunc(actor);
				}
			}

			actor = actor->next;
		}
	}

	void lactor_updateCallbacks(LTick time)
	{
		LActor* actor = lactor_getList();
		while (actor)
		{
			if (actor->callbackFunc)
			{
				actor->callbackFunc(actor, time);
			}
			actor = actor->next;
		}
	}

	LActor* lactor_find(u32 type, const char* name)
	{
		LActor* actor = lactor_getList();
		while (actor)
		{
			if (actor->resType == type && strcasecmp(actor->name, name) == 0)
			{
				return actor;
			}
			actor = actor->next;
		}
		return nullptr;
	}

	void lactor_start(LActor* actor)
	{
		actor->flags |= (LAFLAG_VISIBLE | LAFLAG_ACTIVE);
	}

	void lactor_stop(LActor* actor)
	{
		actor->flags &= ~(LAFLAG_VISIBLE | LAFLAG_ACTIVE);
	}

	void lactor_getRelativePos(LActor* actor, LRect* rect, s16* x, s16* y)
	{
		*x = (actor->x - actor->frame.left) + rect->left;
		*y = (actor->y - actor->frame.top ) + rect->top;
	}
		
	void lactor_setPos(LActor* actor, s16 x, s16 y, s16 xFract, s16 yFract)
	{
		actor->x = x;
		actor->y = y;
		actor->xFract = xFract;
		actor->yFract = yFract;
	}

	void lactor_getPos(LActor* actor, s16* x, s16* y, s16* xFract, s16* yFract)
	{
		*x = actor->x;
		*y = actor->y;
		*xFract = actor->xFract;
		*yFract = actor->yFract;
	}
		
	void lactor_setSize(LActor* actor, s16 w, s16 h)
	{
		actor->w = w;
		actor->h = h;
	}
	
	void lactor_getSize(LActor* actor, s16* w, s16* h)
	{
		*w = actor->w;
		*h = actor->h;
	}

	JBool lactor_isVisible(LActor* actor)
	{
		return (actor->flags & LAFLAG_VISIBLE) ? JTRUE : JFALSE;
	}
	
	void lactor_freeData(LActor* actor)
	{
		if (!actor) { return; }

		if (actor->flags & LAFLAG_DISCARD)
		{
			if (actor->data)
			{
				game_free(actor->data);
			}
			if (actor->array)
			{
				for (s32 i = 0; i < actor->arraySize; i++)
				{
					if (actor->array[i])
					{
						game_free(actor->array[i]);
					}
				}
				game_free(actor->array);
			}
		}

		actor->data  = nullptr;
		actor->array = nullptr;
		actor->arraySize = 0;
	}
}  // namespace TFE_DarkForces