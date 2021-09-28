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

///////////////////////////////////////////
	// Constants
	///////////////////////////////////////////
enum ActorAlert
{
	ALERT_GAMOR = 0,
	ALERT_REEYEE,
	ALERT_BOSSK,
	ALERT_CREATURE,
	ALERT_PROBE,
	ALERT_INTDROID,
	ALERT_COUNT
};
enum AgentActionSounds
{
	AGENTSND_REMOTE_2 = 0,
	AGENTSND_AXE_1,
	AGENTSND_INTSTUN,
	AGENTSND_PROBFIRE_11,
	AGENTSND_PROBFIRE_12,
	AGENTSND_CREATURE2,
	AGENTSND_PROBFIRE_13,
	AGENTSND_STORM_HURT,
	AGENTSND_GAMOR_2,
	AGENTSND_REEYEE_2,
	AGENTSND_BOSSK_3,
	AGENTSND_CREATURE_HURT,
	AGENTSND_STORM_DIE,
	AGENTSND_REEYEE_3,
	AGENTSND_BOSSK_DIE,
	AGENTSND_GAMOR_1,
	AGENTSND_CREATURE_DIE,
	AGENTSND_EEEK_3,
	AGENTSND_SMALL_EXPLOSION,
	AGENTSND_PROBE_ALM,
	AGENTSND_TINY_EXPLOSION,
	AGENTSND_COUNT
};

enum
{
	OFFICER_ALERT_COUNT = 4,
	STORM_ALERT_COUNT = 8,
	ACTOR_MIN_VELOCITY = 0x1999,	// < 0.1
	ACTOR_MAX_AI = 6,
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
	// End Temp

	vec3_fixed vel;
	vec2_fixed lastPlayerPos;
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
	void actorLogic_addActor(ActorLogic* logic, AiActor* aiActor);

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

	ActorEnemy* actor_createEnemyActor(Logic* logic);
	ActorSimple* actor_createSimpleActor(Logic* logic);
	void actor_setupInitAnimation();

	// Returns JTRUE if 'actorObj' can see 'obj'
	// The object must be as close or closer than 'closeDist' or be within the fov of the 'actorObj'.
	// Also the distance between actorObj and obj must be less than 200 - 256 units (randomized), and this value is 
	// adjusted based on the darkness level of the area where 'obj' is (so it is harder to see objects in the dark).
	// Conversely the headlamp will make 'obj' visible from further away.
	JBool actor_isObjectVisible(SecObject* actorObj, SecObject* obj, angle14_32 fov, fixed16_16 closeDist);

	extern LogicAnimation* s_curAnimation;
	extern Logic* s_curLogic;
	extern ActorEnemy* s_curEnemyActor;
	extern SoundSourceID s_alertSndSrc[ALERT_COUNT];
	extern SoundSourceID s_officerAlertSndSrc[OFFICER_ALERT_COUNT];
	extern SoundSourceID s_stormAlertSndSrc[STORM_ALERT_COUNT];
	extern SoundSourceID s_agentSndSrc[AGENTSND_COUNT];
}  // namespace TFE_DarkForces