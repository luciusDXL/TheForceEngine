#pragma once
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_DarkForces/time.h>
#include <TFE_DarkForces/item.h>
#include <TFE_DarkForces/hitEffect.h>
#include <TFE_DarkForces/projectile.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Sound/soundSystem.h>
#include <TFE_Jedi/Collision/collision.h>
#include "actor.h"

using namespace TFE_Jedi;
using namespace TFE_DarkForces;

struct Actor;
struct AiActor;
struct ActorTarget;
struct Task;

enum LogicAnimFlags
{
	AFLAG_PLAYED = FLAG_BIT(0),
	AFLAG_READY = FLAG_BIT(1),
};

typedef JBool(*ActorFunc)(AiActor*, Actor*);
typedef JBool(*ActorMsgFunc)(s32 msg, AiActor*, Actor*);
typedef u32(*ActorTargetFunc)(Actor*, ActorTarget*);
typedef void(*ActorFreeFunc)(void*);

struct LogicAnimation
{
	s32 frameRate;
	fixed16_16 frameCount;
	Tick prevTick;
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

	fixed16_16 fireSpread;
	s32 state0NextTick;
	vec3_fixed fireOffset;

	ProjectileType projType;
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
	ActorTargetFunc updateTargetFunc;
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
	
	s32 u3c;
	Tick delay;
	Tick startDelay;
	LogicAnimation anim;

	Tick nextTick;
	Tick playerLastSeen;
	Tick prevColTick;

	fixed16_16 targetOffset;
	fixed16_16 targetVariation;
	angle14_32 approachVariation;
};

struct AiActor
{
	ActorEnemy enemy;
	fixed16_16 hp;
	ItemId itemDropId;

	// Sound source IDs
	SoundSourceID hurtSndSrc;
	SoundSourceID dieSndSrc;
	// Currently playing hurt sound effect ID.
	SoundEffectID hurtSndID;

	JBool stopOnHit;
	HitEffectID dieEffect;
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