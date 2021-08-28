#include "actor.h"
#include "logic.h"
#include <TFE_Jedi/Sound/soundSystem.h>
#include <TFE_Jedi/Memory/list.h>
#include <TFE_Jedi/Memory/allocator.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	struct Actor;
	struct ActorHeader;

	///////////////////////////////////////////
	// Structs
	///////////////////////////////////////////
	enum ActorConst
	{
		ACTOR_MAX_GAME_OBJ = 6
	};

	// Logic for 'actors' -
	// an Actor is something with animated 'actions' that can move around in the world.
	// 0x6c = 108
	struct ActorLogic
	{
		Logic logic;

		ActorHeader* gameObj[ACTOR_MAX_GAME_OBJ];
		Actor* actor;
		s32* animTable;
		Tick delay;
		Tick nextTick;
		SoundSourceID alertSndSrc;
		u32 flags;
	};

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
	};

	///////////////////////////////////////////
	// Internal State
	///////////////////////////////////////////
	static SoundSourceID s_alertSndSrc[ALERT_COUNT];
	static SoundSourceID s_officerAlertSndSrc[OFFICER_ALERT_COUNT];
	static SoundSourceID s_stormAlertSndSrc[STORM_ALERT_COUNT];
	static SoundSourceID s_agentSndSrc[AGENTSND_COUNT];
	static List* s_physicsActors;
		
	static Allocator* s_actorLogics = nullptr;
	static Task* s_actorTask = nullptr;
	static Task* s_actorPhysicsTask = nullptr;

	///////////////////////////////////////////
	// Forward Declarations
	///////////////////////////////////////////
	void actorLogicTaskFunc(s32 id);
	void actorPhysicsTaskFunc(s32 id);

	///////////////////////////////////////////
	// API Implementation
	///////////////////////////////////////////
	void actor_loadSounds()
	{
		s_alertSndSrc[ALERT_GAMOR]    = sound_Load("gamor-3.voc");
		s_alertSndSrc[ALERT_REEYEE]   = sound_Load("reeyee-1.voc");
		s_alertSndSrc[ALERT_BOSSK]    = sound_Load("bossk-1.voc");
		s_alertSndSrc[ALERT_CREATURE] = sound_Load("creatur1.voc");
		s_alertSndSrc[ALERT_PROBE]    = sound_Load("probe-1.voc");
		s_alertSndSrc[ALERT_INTDROID] = sound_Load("intalert.voc");

		s_officerAlertSndSrc[0] = sound_Load("ranofc02.voc");
		s_officerAlertSndSrc[1] = sound_Load("ranofc04.voc");
		s_officerAlertSndSrc[2] = sound_Load("ranofc05.voc");
		s_officerAlertSndSrc[3] = sound_Load("ranofc06.voc");

		s_stormAlertSndSrc[0] = sound_Load("ransto01.voc");
		s_stormAlertSndSrc[1] = sound_Load("ransto02.voc");
		s_stormAlertSndSrc[2] = sound_Load("ransto03.voc");
		s_stormAlertSndSrc[3] = sound_Load("ransto04.voc");
		s_stormAlertSndSrc[4] = sound_Load("ransto05.voc");
		s_stormAlertSndSrc[5] = sound_Load("ransto06.voc");
		s_stormAlertSndSrc[6] = sound_Load("ransto07.voc");
		s_stormAlertSndSrc[7] = sound_Load("ransto08.voc");

		// Attacks, hurt, death.
		s_agentSndSrc[AGENTSND_REMOTE_2]        = sound_Load("remote-2.voc");
		s_agentSndSrc[AGENTSND_AXE_1]           = sound_Load("axe-1.voc");
		s_agentSndSrc[AGENTSND_INTSTUN]         = sound_Load("intstun.voc");
		s_agentSndSrc[AGENTSND_PROBFIRE_11]     = sound_Load("probfir1.voc");
		s_agentSndSrc[AGENTSND_PROBFIRE_12]     = sound_Load("probfir1.voc");
		s_agentSndSrc[AGENTSND_CREATURE2]       = sound_Load("creatur2.voc");
		s_agentSndSrc[AGENTSND_PROBFIRE_13]     = sound_Load("probfir1.voc");
		s_agentSndSrc[AGENTSND_STORM_HURT]      = sound_Load("st-hrt-1.voc");
		s_agentSndSrc[AGENTSND_GAMOR_2]         = sound_Load("gamor-2.voc");
		s_agentSndSrc[AGENTSND_REEYEE_2]        = sound_Load("reeyee-2.voc");
		s_agentSndSrc[AGENTSND_BOSSK_3]         = sound_Load("bossk-3.voc");
		s_agentSndSrc[AGENTSND_CREATURE_HURT]   = sound_Load("creathrt.voc");
		s_agentSndSrc[AGENTSND_STORM_DIE]       = sound_Load("st-die-1.voc");
		s_agentSndSrc[AGENTSND_REEYEE_3]        = sound_Load("reeyee-3.voc");
		s_agentSndSrc[AGENTSND_BOSSK_DIE]       = sound_Load("bosskdie.voc");
		s_agentSndSrc[AGENTSND_GAMOR_1]         = sound_Load("gamor-1.voc");
		s_agentSndSrc[AGENTSND_CREATURE_DIE]    = sound_Load("creatdie.voc");
		s_agentSndSrc[AGENTSND_EEEK_3]          = sound_Load("eeek-3.voc");
		s_agentSndSrc[AGENTSND_SMALL_EXPLOSION] = sound_Load("ex-small.voc");
		s_agentSndSrc[AGENTSND_PROBE_ALM]       = sound_Load("probalm.voc");
		s_agentSndSrc[AGENTSND_TINY_EXPLOSION]  = sound_Load("ex-tiny1.voc");

		setSoundSourceVolume(s_agentSndSrc[AGENTSND_REMOTE_2], 40);
	}

	void actor_allocatePhysicsActorList()
	{
		s_physicsActors = list_allocate(sizeof(void*), 80);
	}
		
	void actor_createTask()
	{
		s_actorLogics = allocator_create(sizeof(ActorLogic));
		s_actorTask = createTask(actorLogicTaskFunc);
		s_actorPhysicsTask = createTask(actorPhysicsTaskFunc);
	}

	void actorLogicTaskFunc(s32 id)
	{
		task_begin;
		while (id != -1)
		{
			// TODO
			task_yield(TASK_NO_DELAY);
		}
		task_end;
	}

	void actorPhysicsTaskFunc(s32 id)
	{
		task_begin;
		while (id != -1)
		{
			// TODO
			task_yield(TASK_NO_DELAY);
		}
		task_end;
	}
}  // namespace TFE_DarkForces