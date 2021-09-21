#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Actors - AI agents
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_DarkForces/logic.h>
#include <TFE_Jedi/Collision/collision.h>
#include <TFE_Jedi/Sound/soundSystem.h>

struct Actor;
struct ActorHeader;
struct AiActor;

enum ActorConst
{
	ACTOR_MAX_AI = 6
};

enum ActorCollisionFlags
{
	ACTORCOL_GRAVITY = FLAG_BIT(1),
};

typedef JBool(*ActorFunc)(AiActor*, Actor*);
typedef JBool(*ActorMsgFunc)(s32 msg, AiActor*, Actor*);
typedef void(*ActorFreeFunc)(void*);

struct ActorHeader
{
	ActorFunc func;
	ActorMsgFunc msgFunc;
	s32 u08;
	ActorFreeFunc freeFunc;
	Tick nextTick;
	SecObject* obj;
};

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

// Logic for 'actors' -
// an Actor is something with animated 'actions' that can move around in the world.
struct ActorLogic
{
	Logic logic;

	AiActor* aiActors[ACTOR_MAX_AI];
	Actor* actor;
	const s32* animTable;
	Tick delay;
	Tick nextTick;
	SoundSourceID alertSndSrc;

	// Temp - Replace with proper names.
	s32 u44;
	s32 u48;
	s32 u4c;
	vec3_fixed vel;
	s32 u5c;
	s32 u60;
	// End Temp

	Task* freeTask;
	u32 flags;
};

namespace TFE_DarkForces
{
	void actor_loadSounds();
	void actor_allocatePhysicsActorList();
	void actor_createTask();

	ActorLogic* actor_setupActorLogic(SecObject* obj, LogicSetupFunc* setupFunc);
	AiActor* actor_createAiActor(Logic* logic);
	Actor* actor_create(Logic* logic);
	void actor_addLogicGameObj(ActorLogic* logic, AiActor* aiActor);

	void actor_hitEffectMsgFunc(s32 msg, void* logic);
	JBool exploderFunc(AiActor* aiActor, Actor* actor);
	JBool exploderMsgFunc(s32 msg, AiActor* aiActor, Actor* actor);

	extern Logic* s_curLogic;
}  // namespace TFE_DarkForces