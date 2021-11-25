#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Landru Actor
// Landru was the cutscene system developed for XWing and Tie Fighter
// and is also used in Dark Forces.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "lview.h"
#include "ltimer.h"

///////////////////////////////////////////
// Constants
///////////////////////////////////////////
enum LActorConstants
{
	LA_SCALE_FACTOR = 256,
	// Flags
	LAFLAG_VISIBLE     = FLAG_BIT(0),
	LAFLAG_ACTIVE      = FLAG_BIT(1),
	LAFLAG_DISCARD	   = FLAG_BIT(2),
	LAFLAG_DIRTY	   = FLAG_BIT(3),
	LAFLAG_REFRESHABLE = FLAG_BIT(4),
	LAFLAG_REFRESH     = FLAG_BIT(5),
	LAFLAG_KEEPABLE	   = FLAG_BIT(6),
	LAFLAG_KEEP        = FLAG_BIT(7),
	LAFLAG_HFLIP       = FLAG_BIT(8),
	LAFLAG_VFLIP       = FLAG_BIT(9),
	LAFLAG_COLOR       = FLAG_BIT(10),
	LAFLAG_PACKET      = FLAG_BIT(11),
	LAFLAG_USER_FLAG1  = FLAG_BIT(14),
	LAFLAG_USER_FLAG2  = FLAG_BIT(15),
};

namespace TFE_DarkForces
{
	struct LActor;

	typedef s16(*LActorDrawFunc)(LActor*, LRect*, LRect*, s16, s16, s16);
	typedef void(*LActorUpdateFunc)(LActor*);
	typedef void(*LActorCallback)(LActor*, s32);
	typedef void(*LActorFrameFunc)(LActor*, LRect*);
	typedef LActor*(*LActorStateFunc)(LActor*, s16 state, s16 stateFract);

	struct LActor
	{
		u32 resType;
		char name[16];
		
		LActor* next;
		s32 start;
		s32 stop;

		LRect frame;
		LRect frameVelocity;
		LRect bounds;

		s16 x, y;
		s16 xFract, yFract;
		s16 xVel, yVel;
		s16 xVelFract, yVelFract;

		s16 w, h;
		s16 zplane;

		s16 flags;
		s16 id;

		s16 state;
		s16 stateFract;
		s16 stateVel;
		s16 stateVelFract;

		s16 fgColor;
		s16 bgColor;
		s16 xScale;
		s16 yScale;

		s16 var1;
		s16 var2;
		u8* varPtr;
		u8* varPtr2;

		u8* data;
		u8** array;
		u16 arraySize;

		LActorDrawFunc   drawFunc;
		LActorUpdateFunc updateFunc;
		LActorCallback   callbackFunc;
	};

	struct LActorType
	{
		u32 type;
		LActorType* next;
		LActorFrameFunc frameFunc;
		LActorStateFunc stateFunc;
	};

	void lactor_init();
	void lactor_destroy();

	JBool lactor_createType(u32 type, LActorFrameFunc frameFunc, LActorStateFunc stateFunc);
	void lactor_destroyType(u32 type);
	LActorType* lactor_findType(u32 type);

	void lactor_addActor(LActor* actor);
	void lactor_removeActor(LActor* actor);

	LActor* lactor_alloc(s16 extend);
	void lactor_free(LActor* actor);
	void lactor_freeList(LActor* actor);

	LActor* lactor_getList();
	LActor* lactor_find(u32 type, const char* name);

	void lactor_refresh();
	void lactor_draw(JBool refresh);
	void lactor_update(LTick time);
	void lactor_updateCallbacks(LTick time);

	void lactor_getRelativePos(LActor* actor, LRect* rect, s16* x, s16* y);
	void lactor_setPos(LActor* actor, s16 x, s16 y, s16 xFract, s16 yFract);
	void lactor_getPos(LActor* actor, s16* x, s16* y, s16* xFract, s16* yFract);
	void lactor_setSize(LActor* actor, s16 w, s16 h);
	void lactor_getSize(LActor* actor, s16* w, s16* h);
	void lactor_start(LActor* actor);
	void lactor_stop(LActor* actor);

	JBool lactor_isVisible(LActor* actor);
}  // namespace TFE_DarkForces