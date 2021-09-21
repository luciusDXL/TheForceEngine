#pragma once
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Jedi/Sound/soundSystem.h>
#include "actor.h"

struct GameObject2
{
	ActorHeader header;

	vec3_fixed vel;
	angle14_16 pitchRate;
	angle14_16 yawRate;
	angle14_16 rollRate;
	s16 u2a;

	u32 flags;
	s32 u30;
	fixed16_16 width;
	s32 u38;
	s32 u3c;
	s32 u40;
	s32 u44;
	fixed16_16 height;
	s32 u4c;
	Tick nextTick;
	s32 u54;
	s32 u58;
	s32 u5c;
	s32 u60;
	s32 u64;
	s32 u68;
	s32 u6c;
	s32 u70;
	s32 u74;
	s32 u78;
	s32 u7c;
	fixed16_16 centerOffset;		// offset from the base to the object center.
	s32 u84;
	s32 u88;
	s32 u8c;
	s32 u90;
	s32 u94;
	s32 u98;
	s32 u9c;
	s32 ua0;
	s32 ua4;
	s32 ua8;
};

struct AiActor
{
	Actor actor;
	LogicAnimation anim;	// Note: this is temporary, the original game has another structure to hold this.

	s32 ua4;
	s32 ua8;
	s32 hp;
	s32 itemDropId;
	SoundSourceID hurtSndSrc;
	SoundSourceID dieSndSrc;
	s32 ubc;
	s32 uc0;
	s32 uc4;
};