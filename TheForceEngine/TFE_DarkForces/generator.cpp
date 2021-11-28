#include <cstring>

#include "generator.h"
#include "time.h"
#include <TFE_Game/igame.h>
#include <TFE_DarkForces/random.h>
#include <TFE_DarkForces/util.h>
#include <TFE_DarkForces/player.h>
#include <TFE_DarkForces/Actor/actor.h>
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
		s32        aliveCount;
		s32        numTerminate;
		Tick       wanderTime;
		Wax*       wax;
		JBool      active;
	};

	void generatorTaskFunc(MessageType msg)
	{
		struct LocalContext
		{
			Generator* gen;
			SecObject* obj;
		};
		task_begin_ctx;
		local(gen) = (Generator*)task_getUserData();
		local(obj) = local(gen)->logic.obj;

		// Initial delay before it starts working.
		do
		{
			task_yield(local(gen)->delay);
		} while (msg != MSG_RUN_TASK);

		while (1)
		{
			task_yield(local(gen)->interval);
			while (local(gen)->numTerminate == 0 && msg == MSG_RUN_TASK)
			{
				task_yield(TASK_SLEEP);
			}

			if (msg == MSG_FREE)
			{
				SecObject* entity = (SecObject*)s_msgEntity;
				SecObject** entityList = (SecObject**)allocator_getHead(local(gen)->entities);
				while (entityList)
				{
					if (entity == *entityList)
					{
						allocator_deleteItem(local(gen)->entities, entityList);
						local(gen)->aliveCount--;
						break;
					}
					entityList = (SecObject**)allocator_getNext(local(gen)->entities);
				}
			}
			else if (msg == MSG_MASTER_ON)
			{
				local(gen)->active |= 1;
			}
			else if (msg == MSG_MASTER_OFF)
			{
				local(gen)->active &= ~1;
			}
			else if (msg == MSG_RUN_TASK && (local(gen)->active & 1) && local(gen)->aliveCount < local(gen)->maxAlive)
			{
				SecObject* spawn = allocateObject();
				spawn->posWS = local(obj)->posWS;
				spawn->yaw = random_next() & ANGLE_MASK;
				spawn->worldHeight = FIXED(7);
				sector_addObject(local(obj)->sector, spawn);

				fixed16_16 dy = TFE_Jedi::abs(s_playerObject->posWS.y - spawn->posWS.y);
				fixed16_16 dist = dy + distApprox(spawn->posWS.x, spawn->posWS.z, s_playerObject->posWS.x, s_playerObject->posWS.z);
				if (dist >= local(gen)->minDist && dist <= local(gen)->maxDist && !actor_canSeeObject(spawn, s_playerObject))
				{
					sprite_setData(spawn, local(gen)->wax);
					obj_setEnemyLogic(spawn, local(gen)->type, nullptr);
					Logic** head = (Logic**)allocator_getHead_noIterUpdate((Allocator*)spawn->logic);
					ActorLogic* actorLogic = *((ActorLogic**)head);

					actorLogic->flags &= ~1;
					actorLogic->freeTask = task_getCurrent();
					local(gen)->aliveCount++;
					local(gen)->numTerminate--;
					if (local(gen)->wanderTime == 0xffffffff)
					{
						s_actorState.curEnemyActor->timing.nextTick = 0xffffffff;
					}
					else
					{
						s32 randomWanderOffset = floor16(random(intToFixed16(local(gen)->wanderTime >> 1)));
						s_actorState.curEnemyActor->timing.nextTick = s_curTick + local(gen)->wanderTime + floor16(randomWanderOffset);
					}

					SecObject** entityPtr = (SecObject**)allocator_newItem(local(gen)->entities);
					*entityPtr = spawn;
					actor_removeRandomCorpse();
				}
				else
				{
					freeObject(spawn);
				}
			}
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
			s32 wanderTime = strToInt(s_objSeqArg1);
			if (wanderTime == -1)
			{
				genLogic->wanderTime = 0xffffffffu;
			}
			else 
			{
				genLogic->wanderTime = Tick(wanderTime * 145);
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
		generator->entities = allocator_create(sizeof(SecObject**));
		generator->aliveCount   = 0;
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