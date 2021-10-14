#include "generator.h"
#include "time.h"
#include <TFE_Game/igame.h>
#include <TFE_DarkForces/random.h>
#include <TFE_DarkForces/util.h>
#include <TFE_Jedi/Memory/allocator.h>
#include <TFE_Jedi/InfSystem/message.h>
#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/Level/rwall.h>
#include <TFE_Jedi/Collision/collision.h>
#include <TFE_System/system.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FileSystem/filestream.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	struct Generator
	{
		Logic logic;

		KEYWORD    type;
		Tick       delay;
		Tick       interval;
		s32        maxAlive;
		fixed16_16 minDist;
		fixed16_16 maxDist;
		Allocator* entities;
		s32        u34;
		s32        numTerminate;
		TickSigned wanderTime;
		Wax*       wax;
		JBool      active;
	};

	void generatorTaskFunc(MessageType msg)
	{
		task_begin;

		while (1)
		{
			task_yield(TASK_NO_DELAY);
		}

		task_end;
	}

	void generatorLogicCleanupFunc(Logic* logic)
	{
	}

	JBool generatorLogicSetupFunc(Logic* logic, KEYWORD key)
	{
		Generator* genLogic = (Generator*)logic;

		if (key == KW_MASTER)
		{
			genLogic->active &= ~1;
			return JTRUE;
		}
		else if (key == KW_DELAY)
		{
			genLogic->delay = strToInt(s_objSeqArg1) * 145;
			return JTRUE;
		}
		else if (key == KW_INTERVAL)
		{
			Tick interval = strToInt(s_objSeqArg1) * 145;
			Tick intInSec = strToInt(s_objSeqArg1);
			fixed16_16 halfIntervalInSec = (intInSec - (intInSec < 0 ? 1 : 0)) << 15;
			// Adds a random interval offset: interval = Seconds*145 + floor(random(HalfSeconds))
			genLogic->interval = interval + (Tick)floor16(random(halfIntervalInSec));
			return JTRUE;
		}
		else if (key == KW_MAX_ALIVE)
		{
			genLogic->maxAlive = strToInt(s_objSeqArg1);
			return JTRUE;
		}
		else if (key == KW_MIN_DIST)
		{
			genLogic->minDist = strToInt(s_objSeqArg1) << 16;
			return JTRUE;
		}
		else if (key == KW_MAX_DIST)
		{
			genLogic->maxDist = strToInt(s_objSeqArg1) << 16;
			return JTRUE;
		}
		else if (key == KW_NUM_TERMINATE)
		{
			genLogic->numTerminate = strToInt(s_objSeqArg1);
			return JTRUE;
		}
		else if (key == KW_WANDER_TIME)
		{
			TickSigned wanderTime = strToInt(s_objSeqArg1);
			if (wanderTime == -1)
			{
				genLogic->wanderTime = wanderTime;
			}
			else 
			{
				genLogic->wanderTime = wanderTime * 145;
			}
			return JTRUE;
		}
		return JFALSE;
	}

	Logic* obj_createGenerator(SecObject* obj, LogicSetupFunc* setupFunc, KEYWORD genType)
	{
		Generator* generator = (Generator*)level_alloc(sizeof(Generator));
		memset(generator, 0, sizeof(Generator));

		generator->type   = genType;
		generator->active = 1;
		generator->delay  = 0;

		generator->maxAlive = 4;
		generator->minDist  = FIXED(60);
		generator->interval = floor16(random(FIXED(728))) + 2913;
		generator->maxDist  = FIXED(200);
		generator->entities = allocator_create(sizeof(void*));
		generator->u34      = 0;
		generator->numTerminate = -1;
		generator->wax = obj->wax;
		generator->wanderTime = 17478;

		spirit_setData(obj);

		obj->worldWidth  = 0;
		obj->worldHeight = 0;
		obj->entityFlags |= ETFLAG_CAN_DISABLE;
		*setupFunc = generatorLogicSetupFunc;

		Task* task = createSubTask("Generator", generatorTaskFunc);
		task_setUserData(task, generator);
		obj_addLogic(obj, (Logic*)generator, task, generatorLogicCleanupFunc);

		return (Logic*)generator;
	}
}  // TFE_DarkForces