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
	ACTOR_MAX_GAME_OBJ = 6
};

enum ActorCollisionFlags
{
	ACTORCOL_GRAVITY = FLAG_BIT(1),
};

typedef JBool(*ActorFunc)(Actor*, Actor*);
typedef void(*ActorFreeFunc)(void*);

struct ActorHeader
{
	ActorFunc func;
	ActorFunc func2;
	s32 u08;
	ActorFreeFunc freeFunc;
	Tick nextTick;
	SecObject* obj;
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

	ActorHeader* gameObj[ACTOR_MAX_GAME_OBJ];
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
	void actor_addLogicGameObj(ActorLogic* logic, ActorHeader* gameObj);

	extern Logic* s_curLogic;
}  // namespace TFE_DarkForces