#include "lactorCust.h"
#include "lsystem.h"
#include "cutscene_film.h"
#include "ldraw.h"
#include "lview.h"
#include "ltimer.h"
#include <TFE_Game/igame.h>
#include <assert.h>

namespace TFE_DarkForces
{
	static JBool s_lactorCustInit = JFALSE;

	void  lactorCust_initActor(LActor* actor, LRect* frame, s16 xOffset, s16 yOffset, s16 zPlane);
	JBool lactorCust_draw(LActor* actor, LRect* rect, LRect* clipRect, s16 x, s16 y, JBool refresh);
	void  lactorCust_update(LActor* actor);
	
	void lactorCust_init()
	{
		lactor_createType(CF_TYPE_CUSTOM_ACTOR, nullptr, nullptr);
		s_lactorCustInit = JTRUE;
	}

	void lactorCust_destroy()
	{
		if (s_lactorCustInit)
		{
			lactor_destroyType(CF_TYPE_CUSTOM_ACTOR);
		}
		s_lactorCustInit = JFALSE;
	}

	LActor* lactorCust_alloc(u8* custom, LRect* frame, s16 xOffset, s16 yOffset, s16 zPlane)
	{
		LActor* actor = lactor_alloc(0);
		if (!actor) { return nullptr; }

		lactorCust_initActor(actor, frame, xOffset, yOffset, zPlane);
		actor->data = custom;
		lactor_setName(actor, CF_TYPE_CUSTOM_ACTOR, "CUSTOM");
		lactor_keepData(actor);

		return actor;
	}

	void lactorCust_initActor(LActor* actor, LRect* frame, s16 xOffset, s16 yOffset, s16 zPlane)
	{
		lrect_set(&actor->frame, frame->left, frame->top, frame->right, frame->bottom);
		actor->x = xOffset;
		actor->y = yOffset;
		actor->zplane = zPlane;
		
		lactor_discardData(actor);
		actor->drawFunc   = lactorCust_draw;
		actor->updateFunc = lactorCust_update;

		lactor_addActor(actor);
	}

	JBool lactorCust_draw(LActor* actor, LRect* rect, LRect* clipRect, s16 x, s16 y, JBool refresh)
	{
		if (!refresh) { return JFALSE; }

		LRect ra;
		lrect_set(&ra, x, y, x + actor->w, y + actor->h);
		JBool retValue = drawClippedColorRect(&ra, (u8)actor->fgColor);

		if (lactor_isDirty(actor))
		{
			// DirtyRect(&ra);
		}

		return retValue;
	}

	void lactorCust_update(LActor* actor)
	{
		lactor_move(actor);
		lactor_moveFrame(actor);
	}
}  // namespace TFE_DarkForces