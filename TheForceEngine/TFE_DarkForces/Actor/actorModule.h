#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// AI Actor structures.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_DarkForces/time.h>
#include <TFE_DarkForces/item.h>
#include <TFE_DarkForces/hitEffect.h>
#include <TFE_DarkForces/projectile.h>
#include <TFE_DarkForces/sound.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Collision/collision.h>
#include "actor.h"

using namespace TFE_Jedi;
using namespace TFE_DarkForces;

struct Actor;
struct ActorModule;
struct MovementModule;
struct ActorTarget;
struct Task;

enum LogicAnimFlags
{
	AFLAG_PLAYED = FLAG_BIT(0),
	AFLAG_READY  = FLAG_BIT(1),
};

typedef JBool(*ActorFunc)(ActorModule*, MovementModule*);
typedef JBool(*ActorMsgFunc)(s32 msg, ActorModule*, MovementModule*);
typedef void(*ActorAttribFunc)(ActorModule*);
typedef u32(*ActorTargetFunc)(MovementModule*, ActorTarget*);
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

// Was ActorHeader
// "Plugin" in the original code
struct ActorModule
{
	ActorFunc       func;
	ActorMsgFunc    msgFunc;
	ActorAttribFunc attribFunc;
	ActorFreeFunc   freeFunc;

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

struct AttackModule
{
	ActorModule header;
	ActorTarget target;
	ActorTiming timing;
	LogicAnimation anim;

	fixed16_16 fireSpread;
	Tick state0NextTick;
	vec3_fixed fireOffset;

	ProjectileType projType;
	SoundSourceId attackSecSndSrc;
	SoundSourceId attackPrimSndSrc;
	fixed16_16 meleeRange;
	fixed16_16 minDist;
	fixed16_16 maxDist;
	fixed16_16 meleeDmg;
	s32 ua4;
	u32 attackFlags;
};

struct ActorFlyer
{
	ActorModule header;
	ActorTarget target;

	Tick delay;
	s32 u40;
	s32 u44;
	s32 width;
	s32 u4c;
	Tick nextTick;
	s32 u54;
	s32 u58;
	s32 u5c;
	s32 u60;
	s32 state;
	s32 u68;
	s32 u6c;
	s32 u70;
};

struct MovementModule
{
	ActorModule   header;
	ActorTargetFunc updateTargetFunc;
	CollisionInfo physics;
	ActorTarget   target;

	vec3_fixed delta;
	RWall* collisionWall;
	s32 u9c;
	u32 collisionFlags;
};

struct ThinkerModule
{
	ActorModule header;
	ActorTarget target;
	
	Tick delay;
	Tick maxWalkTime;
	Tick startDelay;
	LogicAnimation anim;

	Tick nextTick;
	Tick playerLastSeen;
	Tick prevColTick;

	fixed16_16 targetOffset;
	fixed16_16 targetVariation;
	angle14_32 approachVariation;
};

struct DamageModule
{
	AttackModule attackMod;
	fixed16_16 hp;
	ItemId itemDropId;

	// Sound source IDs
	SoundSourceId hurtSndSrc;
	SoundSourceId dieSndSrc;
	// Currently playing hurt sound effect ID.
	SoundEffectId hurtSndID;

	JBool stopOnHit;
	HitEffectID dieEffect;
};

struct PhysicsActor
{
	MovementModule moveMod;
	LogicAnimation anim;
	PhysicsActor** parent;

	vec3_fixed vel;
	SoundEffectId moveSndId;

	vec2_fixed lastPlayerPos;

	fixed16_16 hp;
	Task* actorTask;
	JBool alive;
	s32 state;
};