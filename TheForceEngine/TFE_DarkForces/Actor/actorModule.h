#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// AI Actor structures - see actor.h for a full explaination.
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

enum ActorModuleType
{
	ACTMOD_MOVE = 0,
	ACTMOD_ATTACK,
	ACTMOD_DAMAGE,
	ACTMOD_THINKER,
	ACTMOD_FLYER,
	ACTMOD_FLYER_REMOTE,
	ACTMOD_COUNT
};

struct ActorModule
{
	ActorModuleType type;

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
	fixed16_16 meleeRate;
	u32 attackFlags;
};

struct MovementModule
{
	ActorModule   header;
	ActorTargetFunc updateTargetFunc;
	CollisionInfo physics;
	ActorTarget   target;

	vec3_fixed delta;
	RWall* collisionWall;
	u32 unused;	// member in structure but not actually used, other than being initialized.
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