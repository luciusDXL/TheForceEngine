#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Core Actor/AI functionality.
//////////////////////////////////////////////////////////////////////
#include "actorModule.h"
#include "../sound.h"
#include <TFE_System/types.h>
#include <TFE_DarkForces/logic.h>
#include <TFE_Jedi/Collision/collision.h>

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
	ACTOR_MAX_MODULES = 6,
};

enum ActorCollisionFlags
{
	ACTORCOL_GRAVITY = FLAG_BIT(1),
};

// Logic for 'actors' -
// an Actor is something with animated 'actions' that can move around in the world.
// The "Dispatch" logic is the core actor, so rename to match.
struct ActorDispatch
{
	Logic logic;

	ActorModule* modules[ACTOR_MAX_MODULES];
	ActorModule* mover;
	const s32* animTable;

	Tick delay;
	Tick nextTick;

	SoundSourceId alertSndSrc;
	SoundEffectId alertSndID;

	angle14_32 fov;
	fixed16_16 awareRange;

	vec3_fixed vel;
	vec2_fixed lastPlayerPos;

	Task* freeTask;
	u32 flags;
};

struct ActorState
{
	LogicAnimation* curAnimation;
	Logic* curLogic;
	ActorEnemy* curEnemyActor;
	Tick nextAlertTick;
	u32 officerAlertIndex;
	u32 stormtrooperAlertIndex;
};

namespace TFE_DarkForces
{
	void actor_clearState();

	void actor_loadSounds();
	void actor_allocatePhysicsActorList();
	void actor_addPhysicsActorToWorld(PhysicsActor* actor);
	void actor_removePhysicsActorFromWorld(PhysicsActor* phyActor);
	void actor_createTask();

	ActorDispatch* actor_createDispatch(SecObject* obj, LogicSetupFunc* setupFunc);
	AiActor* actor_createAiActor(Logic* logic);
	Actor* actor_create(Logic* logic);
	void actor_addModule(ActorDispatch* logic, ActorModule* module);

	void actor_hitEffectMsgFunc(MessageType msg, void* logic);
	void actor_kill();
	void actor_initModule(ActorModule* module, Logic* logic);
	JBool actor_canDie(PhysicsActor* phyActor);
	JBool actor_handleSteps(Actor* actor, ActorTarget* target);
	JBool actor_arrivedAtTarget(ActorTarget* target, SecObject* obj);
	JBool actor_canSeeObjFromDist(SecObject* actorObj, SecObject* obj);
	JBool actor_canSeeObject(SecObject* actorObj, SecObject* obj);

	s32 actor_getAnimationIndex(s32 action);
	void actor_setupAnimation(s32 animIdx, LogicAnimation* aiAnim);
	void actor_setupAnimation2(SecObject* obj, s32 animId, LogicAnimation* anim);
	void actor_setAnimFrameRange(LogicAnimation* anim, s32 startFrame, s32 endFrame);
	void actor_addVelocity(fixed16_16 pushX, fixed16_16 pushY, fixed16_16 pushZ);
	void actor_removeLogics(SecObject* obj);
	void actor_setupSmartObj(Actor* actor);
	void actor_setCurAnimation(LogicAnimation* aiAnim);
	void actor_updatePlayerVisiblity(JBool playerVis, fixed16_16 posX, fixed16_16 posZ);
	void actor_changeDirFromCollision(Actor* actor, ActorTarget* target, Tick* prevColTick);
	void actor_jumpToTarget(PhysicsActor* physicsActor, SecObject* obj, vec3_fixed target, fixed16_16 speed, angle14_32 angleOffset);
	void actor_leadTarget(ProjectileLogic* proj);
	void actor_handleBossDeath(PhysicsActor* physicsActor);
	void actor_removeRandomCorpse();

	angle14_32 actor_offsetTarget(fixed16_16* targetX, fixed16_16* targetZ, fixed16_16 targetOffset, fixed16_16 targetVariation, angle14_32 angle, angle14_32 angleVariation);

	ActorEnemy* actor_createEnemyActor(Logic* logic);
	ActorSimple* actor_createSimpleActor(Logic* logic);
	void actor_setupInitAnimation();
	void actor_setDeathCollisionFlags();

	// Returns JTRUE if 'actorObj' can see 'obj'
	// The object must be as close or closer than 'closeDist' or be within the fov of the 'actorObj'.
	// Also the distance between actorObj and obj must be less than 200 - 256 units (randomized), and this value is 
	// adjusted based on the darkness level of the area where 'obj' is (so it is harder to see objects in the dark).
	// Conversely the headlamp will make 'obj' visible from further away.
	JBool actor_isObjectVisible(SecObject* actorObj, SecObject* obj, angle14_32 fov, fixed16_16 closeDist);

	extern ActorState s_actorState;
	extern SoundSourceId s_alertSndSrc[ALERT_COUNT];
	extern SoundSourceId s_officerAlertSndSrc[OFFICER_ALERT_COUNT];
	extern SoundSourceId s_stormAlertSndSrc[STORM_ALERT_COUNT];
	extern SoundSourceId s_agentSndSrc[AGENTSND_COUNT];

	extern JBool s_aiActive;
	// Special yield macro for entities so that LAREDLITE works.
	#define entity_yield(t) \
	do \
	{ \
		task_yield(s_aiActive ? t : 290); \
	} while (!s_aiActive);

	// Helper macro to get local object distance from player
	#define playerDist(obj) fixedSqrt(player_getSquaredDistance(local(obj)))
}  // namespace TFE_DarkForces