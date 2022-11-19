#include <cstring>

#include "actor.h"
#include "actorInternal.h"
#include "actorSerialization.h"
#include "../logic.h"
#include "../gameMusic.h"
#include "../sound.h"
#include "animTables.h"
#include "actorModule.h"
#include "mousebot.h"
#include "dragon.h"
#include "bobaFett.h"
#include "welder.h"
#include "turret.h"
#include "phaseOne.h"
#include "phaseTwo.h"
#include "phaseThree.h"
#include <TFE_Game/igame.h>
#include <TFE_DarkForces/random.h>
#include <TFE_DarkForces/player.h>
#include <TFE_DarkForces/hitEffect.h>
#include <TFE_DarkForces/projectile.h>
#include <TFE_DarkForces/pickup.h>
#include <TFE_DarkForces/player.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_Jedi/Level/rwall.h>
#include <TFE_Jedi/Level/levelData.h>
#include <TFE_Jedi/InfSystem/message.h>
#include <TFE_Jedi/Memory/list.h>
#include <TFE_Jedi/Memory/allocator.h>
#include <TFE_Jedi/Serialization/serialization.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	// Forward Declarations
	void actor_serializeObject(Stream* stream, SecObject*& obj);
	void actor_serializeWall(Stream* stream, RWall*& wall);
	void actor_serializeCollisionInfo(Stream* stream, CollisionInfo* colInfo);
	void actor_serializeTarget(Stream* stream, ActorTarget* target);
	void actor_serializeMovementModule(Stream* stream, MovementModule*& moveMod, ActorDispatch* dispatch);

	void actorDispatch_serialize(Logic*& logic, Stream* stream)
	{
		ActorDispatch* dispatch = nullptr;
		if (serialization_getMode() == SMODE_WRITE)
		{
			dispatch = (ActorDispatch*)logic;
		}
		else
		{
			dispatch = (ActorDispatch*)allocator_newItem(s_istate.actorDispatch);
			memset(dispatch->modules, 0, sizeof(ActorModule*) * 6);

			logic = (Logic*)dispatch;
			logic->task = s_istate.actorTask;
			logic->cleanupFunc = actorLogicCleanupFunc;
		}
		s_actorState.curLogic = (Logic*)dispatch;

		serialization_serializeDfSound(stream, SaveVersionInit, &dispatch->alertSndSrc);
		SERIALIZE(SaveVersionInit, dispatch->delay, 72);
		SERIALIZE(SaveVersionInit, dispatch->nextTick, 0);
		SERIALIZE(SaveVersionInit, dispatch->fov, 9557);			// ~210 degrees
		SERIALIZE(SaveVersionInit, dispatch->awareRange, FIXED(20));
		SERIALIZE(SaveVersionInit, dispatch->vel, {0});
		SERIALIZE(SaveVersionInit, dispatch->lastPlayerPos, {0});
		SERIALIZE(SaveVersionInit, dispatch->flags, 4);
		// Animation Table.
		s32 animTableIndex = -1;
		if (serialization_getMode() == SMODE_WRITE)
		{
			animTableIndex = animTables_getIndex(dispatch->animTable);
		}
		SERIALIZE(SaveVersionInit, animTableIndex, -1);
		if (serialization_getMode() == SMODE_READ)
		{
			dispatch->animTable = animTables_getTable(animTableIndex);
		}

		actor_serializeMovementModule(stream, dispatch->moveMod, dispatch);
		// TODO:
		// ActorModule* modules[ACTOR_MAX_MODULES];
		// Task* freeTask;
	}
		
	void actor_serializeObject(Stream* stream, SecObject*& obj)
	{
		s32 objId;
		if (serialization_getMode() == SMODE_WRITE)
		{
			objId = obj ? obj->serializeIndex : -1;
		}
		SERIALIZE(SaveVersionInit, objId, -1);
		if (serialization_getMode() == SMODE_READ)
		{
			obj = objData_getObjectBySerializationId(objId);
		}
	}

	void actor_serializeWall(Stream* stream, RWall*& wall)
	{
		RSector* wallSector = nullptr;
		s32 wallId = -1;
		if (serialization_getMode() == SMODE_WRITE)
		{
			wallSector = wall ? wall->sector : nullptr;
			wallId = wall ? wall->id : -1;
		}
		serialization_serializeSectorPtr(stream, SaveVersionInit, wallSector);
		SERIALIZE(SaveVersionInit, wallId, -1);
		if (serialization_getMode() == SMODE_READ)
		{
			wall = nullptr;
			if (wallSector && wallId >= 0)
			{
				wall = &wallSector->walls[wallId];
			}
		}
	}

	void actor_serializeCollisionInfo(Stream* stream, CollisionInfo* colInfo)
	{
		actor_serializeObject(stream, colInfo->obj);
		actor_serializeObject(stream, colInfo->collidedObj);
		actor_serializeWall(stream, colInfo->wall);

		SERIALIZE(SaveVersionInit, colInfo->offsetX, 0);
		SERIALIZE(SaveVersionInit, colInfo->offsetY, 0);
		SERIALIZE(SaveVersionInit, colInfo->offsetZ, 0);
		SERIALIZE(SaveVersionInit, colInfo->botOffset, 0);
		SERIALIZE(SaveVersionInit, colInfo->yPos, 0);
		SERIALIZE(SaveVersionInit, colInfo->height, 0);
		SERIALIZE(SaveVersionInit, colInfo->u24, 0);
		SERIALIZE(SaveVersionInit, colInfo->width, 0);
		SERIALIZE(SaveVersionInit, colInfo->flags, 0);
		SERIALIZE(SaveVersionInit, colInfo->responseStep, 0);
		SERIALIZE(SaveVersionInit, colInfo->responseDir, {0});
		SERIALIZE(SaveVersionInit, colInfo->responsePos, {0});
		SERIALIZE(SaveVersionInit, colInfo->responseAngle, 0);

		if (serialization_getMode() == SMODE_READ)
		{
			colInfo->unused = 0;
		}
	}

	void actor_serializeTarget(Stream* stream, ActorTarget* target)
	{
		SERIALIZE(SaveVersionInit, target->pos, {0});
		SERIALIZE(SaveVersionInit, target->pitch, 0);
		SERIALIZE(SaveVersionInit, target->yaw, 0);
		SERIALIZE(SaveVersionInit, target->roll, 0);
		SERIALIZE(SaveVersionInit, target->speed, 0);
		SERIALIZE(SaveVersionInit, target->speedVert, 0);
		SERIALIZE(SaveVersionInit, target->speedRotation, 0);
		SERIALIZE(SaveVersionInit, target->flags, 0);

		if (serialization_getMode() == SMODE_READ)
		{
			target->pad = 0;
		}
	}

	void actor_serializeMovementModule(Stream* stream, MovementModule*& moveMod, ActorDispatch* dispatch)
	{
		if (serialization_getMode() == SMODE_READ)
		{
			moveMod = (MovementModule*)level_alloc(sizeof(MovementModule));
			memset(moveMod, 0, sizeof(MovementModule));
			actor_initModule((ActorModule*)moveMod, (Logic*)dispatch);

			moveMod->header.func = defaultActorFunc;
			moveMod->header.freeFunc = nullptr;
			moveMod->header.type = ACTMOD_MOVE;
			moveMod->updateTargetFunc = defaultUpdateTargetFunc;
		}
		else
		{
			assert(moveMod->header.func == defaultActorFunc);
			assert(moveMod->header.freeFunc == nullptr);
			assert(moveMod->updateTargetFunc == defaultUpdateTargetFunc);
		}

		actor_serializeCollisionInfo(stream, &moveMod->physics);
		actor_serializeTarget(stream, &moveMod->target);
		SERIALIZE(SaveVersionInit, moveMod->delta, {0});
		actor_serializeWall(stream, moveMod->collisionWall);
		SERIALIZE(SaveVersionInit, moveMod->collisionFlags, 0);
	}
}  // namespace TFE_DarkForces