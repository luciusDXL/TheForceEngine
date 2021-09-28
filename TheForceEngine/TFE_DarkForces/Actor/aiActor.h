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
	s32 state;
};

struct ActorTiming
{
	Tick delay;
	Tick state0Delay;
	Tick state2Delay;
	Tick state4Delay;
	Tick state1Delay;
	Tick nextTick;
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

struct ActorTarget
{
	vec3_fixed pos;
	angle14_16 pitch;
	angle14_16 yaw;
	angle14_16 roll;
	s16 pad;

	fixed16_16 speed;
	fixed16_16 speedVert;
	fixed16_16 speedRotation;
	u32 flags;
};

struct ActorEnemy
{
	ActorHeader header;
	ActorTarget target;
	ActorTiming timing;
	LogicAnimation anim;

	s32 u74;
	s32 state0NextTick;
	s32 u7c;
	fixed16_16 centerOffset;		// offset from the base to the object center.
	s32 u84;
	s32 u88;
	SoundSourceID attackSecSndSrc;
	SoundSourceID attackPrimSndSrc;
	s32 u94;
	fixed16_16 minDist;
	fixed16_16 maxDist;
	s32 ua0;
	s32 ua4;
	u32 attackFlags;
};

struct Actor
{
	ActorHeader   header;
	ActorFunc     func3;
	CollisionInfo physics;
	ActorTarget   target;

	vec3_fixed delta;
	RWall* collisionWall;
	s32 u9c;
	u32 collisionFlags;
};

struct ActorSimple
{
	ActorHeader header;
	ActorTarget target;
	ActorTiming timing;
	LogicAnimation anim;

	s32 u74;
	Tick state0NextTick;
	s32 u7c;
};

struct AiActor
{
	ActorEnemy enemy;

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