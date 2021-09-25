#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Actors - AI agents
//////////////////////////////////////////////////////////////////////
#include "aiActor.h"
#include <TFE_System/types.h>
#include <TFE_DarkForces/logic.h>
#include <TFE_Jedi/Collision/collision.h>
#include <TFE_Jedi/Sound/soundSystem.h>

struct Actor;
struct ActorHeader;

enum ActorConst
{
	ACTOR_MAX_AI = 6
};

enum ActorCollisionFlags
{
	ACTORCOL_GRAVITY = FLAG_BIT(1),
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
	void actor_addPhysicsActorToWorld(PhysicsActor* actor);
	void actor_removePhysicsActorFromWorld(PhysicsActor* phyActor);
	void actor_createTask();

	ActorLogic* actor_setupActorLogic(SecObject* obj, LogicSetupFunc* setupFunc);
	AiActor* actor_createAiActor(Logic* logic);
	Actor* actor_create(Logic* logic);
	void actor_addLogicGameObj(ActorLogic* logic, AiActor* aiActor);

	void actor_hitEffectMsgFunc(s32 msg, void* logic);
	JBool exploderFunc(AiActor* aiActor, Actor* actor);
	JBool exploderMsgFunc(s32 msg, AiActor* aiActor, Actor* actor);

	void actor_kill();
	JBool actor_canDie(PhysicsActor* phyActor);

	s32 actor_getAnimationIndex(s32 action);
	void actor_setupAnimation(s32 animIdx, LogicAnimation* aiAnim);
	void actor_addVelocity(fixed16_16 pushX, fixed16_16 pushY, fixed16_16 pushZ);
	void actor_removeLogics(SecObject* obj);
	void actor_setupSmartObj(Actor* actor);

	// Returns JTRUE if 'actorObj' can see 'obj'
	// The object must be as close or closer than 'closeDist' or be within the fov of the 'actorObj'.
	// Also the distance between actorObj and obj must be less than 200 - 256 units (randomized), and this value is 
	// adjusted based on the darkness level of the area where 'obj' is (so it is harder to see objects in the dark).
	// Conversely the headlamp will make 'obj' visible from further away.
	JBool actor_isObjectVisible(SecObject* actorObj, SecObject* obj, angle14_32 fov, fixed16_16 closeDist);

	extern LogicAnimation* s_curAnimation;
	extern Logic* s_curLogic;
}  // namespace TFE_DarkForces