#pragma once
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_DarkForces/time.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Sound/soundSystem.h>
#include <TFE_Jedi/Collision/collision.h>
#include "actor.h"

using namespace TFE_Jedi;

struct Actor;
struct AiActor;
struct Task;

enum LogicAnimFlags
{
	AFLAG_PLAYED = FLAG_BIT(0),
	AFLAG_READY = FLAG_BIT(1),
};

typedef JBool(*ActorFunc)(AiActor*, Actor*);
typedef JBool(*ActorMsgFunc)(s32 msg, AiActor*, Actor*);
typedef void(*ActorFreeFunc)(void*);

struct LogicAnimation
{
	s32 frameRate;
	fixed16_16 frameCount;
	u32 prevTick;
	fixed16_16 frame;
	fixed16_16 startFrame;
	u32 flags;
	s32 animId;
	s32 u1c;
};

struct ActorHeader
{
	ActorFunc func;
	ActorMsgFunc msgFunc;
	s32 u08;
	ActorFreeFunc freeFunc;
	Tick nextTick;
	SecObject* obj;
};

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

struct Actor
{
	ActorHeader header;
	ActorFunc func3;
	CollisionInfo physics;
	vec3_fixed nextPos;
	angle14_16 pitch;
	angle14_16 yaw;
	angle14_16 roll;
	s16 u7a;
	fixed16_16 speed;
	fixed16_16 speedVert;		// offset from the base to the object center.
	angle14_32 speedRotation;
	u32 updateFlags;
	vec3_fixed delta;
	RWall* collisionWall;
	s32 u9c;
	u32 collisionFlags;
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

struct PhysicsActor
{
	Actor actor;
	LogicAnimation anim;

	vec3_fixed vel;
	s32 ud0;
	PhysicsActor** parent;
	s32 ud8;
	fixed16_16 hp;
	Task* actorTask;
	JBool alive;
	s32 state;
};