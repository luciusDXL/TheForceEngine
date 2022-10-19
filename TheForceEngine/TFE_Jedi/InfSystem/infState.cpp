#include "infState.h"
#include "infTypesInternal.h"
#include "infSystem.h"
#include <TFE_DarkForces/sound.h>
#include <TFE_Jedi/Serialization/serialization.h>
#include <TFE_Jedi/Memory/allocator.h>
#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/Level/levelData.h>

using namespace TFE_DarkForces;

namespace TFE_Jedi
{
	const s32 c_infStateVersion = 1;

	// INF State
	InfSerializableState s_infSerState = { };
	InfState s_infState = { };

	/////////////////////////////////////////////
	// Forward Declarations
	/////////////////////////////////////////////
	void inf_serializeStop(Stream* stream, Stop* stop);
	void inf_serializeSlave(Stream* stream, Slave* slave);
	void inf_serializeLink(Stream* stream, InfLink* link);

	void inf_deserializeStop(Stream* stream, Stop* stop);
	void inf_deserializeSlave(Stream* stream, Slave* slave);
	void inf_deserializeLink(Stream* stream, InfLink* link, Allocator* parent);

	void inf_computeElevValuePointer(InfElevator* elev);
	extern void inf_deleteElevator(InfElevator* elev);
	extern void inf_deleteTrigger(InfTrigger* trigger);

	/////////////////////////////////////////////
	// Implementation
	/////////////////////////////////////////////
	void inf_clearState()
	{
		s_infSerState = { 0 };
		s_infState = { 0 };
	}
		
	void inf_serialize(Stream* stream)
	{
		const s32 elevCount = allocator_getCount(s_infSerState.infElevators);
		const s32 teleCount = allocator_getCount(s_infSerState.infTeleports);
		const s32 trigCount = allocator_getCount(s_infSerState.infTriggers);

		// Version
		SERIALIZE(c_infStateVersion);

		// Counts.
		SERIALIZE(s_infSerState.activeTriggerCount);
		SERIALIZE(elevCount);
		SERIALIZE(teleCount);
		SERIALIZE(trigCount);

		// Elevators.
		InfElevator* elev = (InfElevator*)allocator_getHead(s_infSerState.infElevators);
		while (elev)
		{
			s32 del = elev->deleted ? 1 : 0;
			SERIALIZE(del);
			if (!del)
			{
				RSector* sector = elev->sector;

				SERIALIZE(elev->type);
				SERIALIZE(elev->trigMove);
				serialization_writeSectorPtr(stream, sector);

				SERIALIZE(elev->key);
				SERIALIZE(elev->fixedStep);
				SERIALIZE(elev->nextTick);
				SERIALIZE(elev->timer);

				// Matching sector link.
				InfLink* elevLink = nullptr;
				if (sector->infLink)
				{
					InfLink* link = (InfLink*)allocator_getHead(sector->infLink);
					while (link)
					{
						if (link->type == LTYPE_SECTOR && link->elev == elev)
						{
							elevLink = link;
							break;
						}
						link = (InfLink*)allocator_getNext(sector->infLink);
					}
				}
				if (elevLink)
				{
					serialization_writeSectorPtr(stream, sector);
					inf_serializeLink(stream, elevLink);
				}
				else
				{
					assert(0);
					s32 linkSectorIndex = -1;
					SERIALIZE(linkSectorIndex);
				}

				// Serialize the current stop allocator position.
				s32 stopPos  = allocator_getCurPos(elev->stops);
				s32 stopPrev = allocator_getPrevPos(elev->stops);
				SERIALIZE(stopPos);
				SERIALIZE(stopPrev);

				// Stops
				s32 stopCount = allocator_getCount(elev->stops);
				SERIALIZE(stopCount);
				Stop* stop = (Stop*)allocator_getHead(elev->stops);
				while (stop)
				{
					inf_serializeStop(stream, stop);
					stop = (Stop*)allocator_getNext(elev->stops);
				}

				// Slaves
				s32 slaveCount = allocator_getCount(elev->slaves);
				SERIALIZE(slaveCount);
				Slave* slave = (Slave*)allocator_getHead(elev->slaves);
				while (slave)
				{
					inf_serializeSlave(stream, slave);
					slave = (Slave*)allocator_getNext(elev->slaves);
				}

				// Next Stop
				s32 stopIndex = elev->nextStop ? elev->nextStop->index : -1;
				SERIALIZE(stopIndex);

				SERIALIZE(elev->speed);
				// elev->value: this can be derived from the type - the actual value will be saved with the associated sector data.
				SERIALIZE(elev->iValue);
				SERIALIZE(elev->dirOrCenter);
				SERIALIZE(elev->flags);

				serialization_writeDfSound(stream, elev->sound0);
				serialization_writeDfSound(stream, elev->sound1);
				serialization_writeDfSound(stream, elev->sound2);
				// elev->loopingSoundID is not serialized, the sound will resume on the next update.

				SERIALIZE(elev->updateFlags);
				SERIALIZE(elev->prevValue);
			}
			elev = (InfElevator*)allocator_getNext(s_infSerState.infElevators);
		}

		// Teleports
		Teleport* teleport = (Teleport*)allocator_getHead(s_infSerState.infTeleports);
		while (teleport)
		{
			serialization_writeSectorPtr(stream, teleport->sector);
			serialization_writeSectorPtr(stream, teleport->target);
			SERIALIZE(teleport->type);
			SERIALIZE(teleport->dstPosition);
			SERIALIZE_BUF(teleport->dstAngle, 3 * sizeof(angle14_16));

			// Matching sector link.
			InfLink* teleportLink = nullptr;
			RSector* sector = teleport->sector;
			if (sector->infLink)
			{
				InfLink* link = (InfLink*)allocator_getHead(sector->infLink);
				while (link)
				{
					if (link->type == LTYPE_TELEPORT && link->teleport == teleport)
					{
						teleportLink = link;
						break;
					}
					link = (InfLink*)allocator_getNext(sector->infLink);
				}
			}
			if (teleportLink)
			{
				serialization_writeSectorPtr(stream, sector);
				inf_serializeLink(stream, teleportLink);
			}
			else
			{
				assert(0);
				s32 linkSectorIndex = -1;
				SERIALIZE(linkSectorIndex);
			}

			teleport = (Teleport*)allocator_getNext(s_infSerState.infTeleports);
		}

		// Triggers
		InfTrigger* trigger = (InfTrigger*)allocator_getHead(s_infSerState.infTriggers);
		while (trigger)
		{
			SERIALIZE(trigger->deleted);
			if (!trigger->deleted)
			{
				SERIALIZE(trigger->type);

				// Inf Link
				s32 parentSectorIndex = -1;
				s32 parentWallIndex = -1;
				if (trigger->type == ITRIGGER_SECTOR)
				{
					RSector* sector = (RSector*)trigger->parent;
					parentSectorIndex = sector->index;
				}
				else
				{
					RWall* wall = (RWall*)trigger->parent;
					parentSectorIndex = wall->sector->index;
					parentWallIndex = wall->id;
				}
				SERIALIZE(parentSectorIndex);
				SERIALIZE(parentWallIndex);

				inf_serializeLink(stream, trigger->link);

				// Animated Texture
				serialize_writeAnimatedTexturePtr(stream, trigger->animTex);

				// targets
				s32 targetCount = allocator_getCount(trigger->targets);
				SERIALIZE(targetCount);
				TriggerTarget* target = (TriggerTarget*)allocator_getHead(trigger->targets);
				while (target)
				{
					s32 sectorIndex = -1;
					if (target->sector)
					{
						sectorIndex = target->sector->index;
					}
					else if (target->wall && target->wall->sector)
					{
						sectorIndex = target->wall->sector->index;
					}

					s32 wallIndex = -1;
					if (target->wall)
					{
						wallIndex = target->wall->id;
					}

					SERIALIZE(sectorIndex);
					SERIALIZE(wallIndex);
					SERIALIZE(target->eventMask);

					target = (TriggerTarget*)allocator_getNext(trigger->targets);
				}

				SERIALIZE(trigger->cmd);
				SERIALIZE(trigger->event);
				SERIALIZE(trigger->arg0);
				SERIALIZE(trigger->arg1);
				// Skip unused.
				SERIALIZE(trigger->master);

				// trigger->tex
				s32 frameIndex = -1;
				AnimatedTexture* animTex = trigger->animTex;
				if (trigger->tex && animTex)
				{
					// Which frame is it currently on?
					for (s32 f = 0; f < animTex->count; f++)
					{
						if (trigger->tex == animTex->frameList[f])
						{
							frameIndex = f;
							break;
						}
					}
				}
				SERIALIZE(frameIndex);

				serialization_writeDfSound(stream, trigger->soundId);
				SERIALIZE(trigger->state);
				SERIALIZE(trigger->textId);
			}

			trigger = (InfTrigger*)allocator_getNext(s_infSerState.infTriggers);
		}
	}

	void inf_deserialize(Stream* stream)
	{
		// Clear state.
		s_infState = { 0 };
		s_infSerState = { 0 };

		// Tasks and allocators.
		inf_createElevatorTask();
		inf_createTeleportTask();
		inf_createTriggerTask();

		// Version.
		s32 infStateVersion;
		DESERIALIZE(infStateVersion);

		// Counts.
		s32 elevCount, teleCount, trigCount;
		DESERIALIZE(s_infSerState.activeTriggerCount);
		DESERIALIZE(elevCount);
		DESERIALIZE(teleCount);
		DESERIALIZE(trigCount);

		// Elevators
		for (s32 i = 0; i < elevCount; i++)
		{
			InfElevator* elev = (InfElevator*)allocator_newItem(s_infSerState.infElevators);
			s32 del;
			DESERIALIZE(del);
			if (del)
			{
				memset(elev, 0, sizeof(InfElevator));
				elev->deleted = JTRUE;
				continue;
			}
			elev->deleted = JFALSE;
			elev->self = elev;

			DESERIALIZE(elev->type);
			DESERIALIZE(elev->trigMove);
			elev->sector = serialization_readSectorPtr(stream);

			DESERIALIZE(elev->key);
			DESERIALIZE(elev->fixedStep);
			DESERIALIZE(elev->nextTick);
			DESERIALIZE(elev->timer);

			// Matching sector link.
			RSector* linkSector = serialization_readSectorPtr(stream);
			if (linkSector)
			{
				if (!linkSector->infLink)
				{
					linkSector->infLink = allocator_create(sizeof(InfLink));
				}
				Allocator* parent = linkSector->infLink;
				InfLink* link = (InfLink*)allocator_newItem(parent);
				inf_deserializeLink(stream, link, parent);
			}

			// Serialize the current stop allocator position.
			s32 stopPos, stopPrev;
			DESERIALIZE(stopPos);
			DESERIALIZE(stopPrev);
			
			// Stops
			s32 stopCount;
			DESERIALIZE(stopCount);
			elev->stops = allocator_create(sizeof(Stop));
			for (s32 s = 0; s < stopCount; s++)
			{
				Stop* stop = (Stop*)allocator_newItem(elev->stops);
				inf_deserializeStop(stream, stop);
			}

			// Set stop position.
			if (stopPos >= 0)
			{
				allocator_setPos(elev->stops, stopPos);
			}
			if (stopPrev >= 0)
			{
				allocator_setPrevPos(elev->stops, stopPrev);
			}

			// Slaves
			s32 slaveCount;
			DESERIALIZE(slaveCount);
			elev->slaves = allocator_create(sizeof(Slave));
			for (s32 s = 0; s < slaveCount; s++)
			{
				Slave* slave = (Slave*)allocator_newItem(elev->slaves);
				inf_deserializeSlave(stream, slave);
			}

			// Next Stop
			s32 stopIndex;
			DESERIALIZE(stopIndex);
			elev->nextStop = (stopIndex < 0) ? nullptr : (Stop*)allocator_getByIndex(elev->stops, stopIndex);

			DESERIALIZE(elev->speed);
			DESERIALIZE(elev->iValue);
			DESERIALIZE(elev->dirOrCenter);
			DESERIALIZE(elev->flags);

			// Compute the elev->value pointer.
			inf_computeElevValuePointer(elev);

			serialization_readDfSound(stream, &elev->sound0);
			serialization_readDfSound(stream, &elev->sound1);
			serialization_readDfSound(stream, &elev->sound2);
			// elev->loopingSoundID is not serialized, the sound will resume on the next update.
			elev->loopingSoundID = NULL_SOUND;
			
			DESERIALIZE(elev->updateFlags);
			DESERIALIZE(elev->prevValue);
		}

		// Teleports
		for (s32 i = 0; i < teleCount; i++)
		{
			Teleport* teleport = (Teleport*)allocator_newItem(s_infSerState.infTeleports);

			teleport->sector = serialization_readSectorPtr(stream);
			teleport->target = serialization_readSectorPtr(stream);
			DESERIALIZE(teleport->type);
			DESERIALIZE(teleport->dstPosition);
			DESERIALIZE_BUF(teleport->dstAngle, 3 * sizeof(angle14_16));

			// Matching sector link.
			RSector* linkSector = serialization_readSectorPtr(stream);
			assert(linkSector);
			if (linkSector)
			{
				if (!linkSector->infLink)
				{
					linkSector->infLink = allocator_create(sizeof(InfLink));
				}
				Allocator* parent = linkSector->infLink;
				InfLink* link = (InfLink*)allocator_newItem(parent);
				inf_deserializeLink(stream, link, parent);
			}
		}

		// Triggers
		for (s32 i = 0; i < trigCount; i++)
		{
			InfTrigger* trigger = (InfTrigger*)allocator_newItem(s_infSerState.infTriggers);

			s32 del;
			DESERIALIZE(del);
			if (del)
			{
				memset(trigger, 0, sizeof(InfTrigger));
				trigger->deleted = JTRUE;
				continue;
			}
			trigger->deleted = JFALSE;

			DESERIALIZE(trigger->type);

			// InfLink
			s32 parentSectorIndex, parentWallIndex;
			DESERIALIZE(parentSectorIndex);
			DESERIALIZE(parentWallIndex);
			Allocator* parent = nullptr;
			RWall* triggerWall = nullptr;
			InfLink* link = nullptr;
			RSector* sector = parentSectorIndex >= 0 ? &s_levelState.sectors[parentSectorIndex] : nullptr;

			if (trigger->type == ITRIGGER_SECTOR && sector)
			{
				if (!sector->infLink)
				{
					sector->infLink = allocator_create(sizeof(InfLink));
				}
				parent = sector->infLink;
				link = (InfLink*)allocator_newItem(sector->infLink);
			}
			else if (sector && parentWallIndex >= 0)
			{
				RWall* wall = &sector->walls[parentWallIndex];
				if (!wall->infLink)
				{
					wall->infLink = allocator_create(sizeof(InfLink));
				}
				parent = wall->infLink;
				triggerWall = wall;
				link = (InfLink*)allocator_newItem(wall->infLink);
			}
			trigger->link = link;
			inf_deserializeLink(stream, link, parent);

			// Animated texture.
			trigger->animTex = serialize_readAnimatedTexturePtr(stream);

			// targets
			s32 targetCount;
			DESERIALIZE(targetCount);
			trigger->targets = allocator_create(sizeof(TriggerTarget));
			for (s32 t = 0; t < targetCount; t++)
			{
				TriggerTarget* target = (TriggerTarget*)allocator_newItem(trigger->targets);

				s32 sectorIndex;
				s32 wallIndex;
				DESERIALIZE(sectorIndex);
				DESERIALIZE(wallIndex);

				target->sector = nullptr;
				target->wall = nullptr;
				if (wallIndex >= 0 && sectorIndex >= 0)
				{
					target->wall = &s_levelState.sectors[sectorIndex].walls[wallIndex];
				}
				else if (sectorIndex >= 0)
				{
					target->sector = &s_levelState.sectors[sectorIndex];
				}

				DESERIALIZE(target->eventMask);
			}

			DESERIALIZE(trigger->cmd);
			DESERIALIZE(trigger->event);
			DESERIALIZE(trigger->arg0);
			DESERIALIZE(trigger->arg1);
			// Skip unused.
			DESERIALIZE(trigger->master);

			// trigger->tex
			s32 frameIndex;
			DESERIALIZE(frameIndex);
			AnimatedTexture* animTex = trigger->animTex;
			if (animTex && frameIndex >= 0)
			{
				trigger->tex = animTex->frameList[frameIndex];
				if (triggerWall)
				{
					triggerWall->signTex = &trigger->tex;
				}
			}

			serialization_readDfSound(stream, &trigger->soundId);
			DESERIALIZE(trigger->state);
			DESERIALIZE(trigger->textId);
		}
	}

	/////////////////////////////////////////////
	// Internal - Serialize
	/////////////////////////////////////////////
	void inf_serializeStop(Stream* stream, Stop* stop)
	{
		SERIALIZE(stop->value);
		SERIALIZE(stop->delay);

		// Messages
		s32 msgCount = allocator_getCount(stop->messages);
		SERIALIZE(msgCount);
		InfMessage* msg = (InfMessage*)allocator_getHead(stop->messages);
		while (msg)
		{
			s32 sectorIndex = -1;
			if (msg->sector)
			{
				sectorIndex = msg->sector->index;
			}
			else if (msg->wall && msg->wall->sector)
			{
				sectorIndex = msg->wall->sector->index;
			}
			s32 wallIndex = (msg->wall) ? msg->wall->id : -1;
			// It is possible for both wallIndex and sectorIndex to be null.
			SERIALIZE(sectorIndex);
			SERIALIZE(wallIndex);

			SERIALIZE(msg->msgType);
			SERIALIZE(msg->event);
			SERIALIZE(msg->arg1);
			SERIALIZE(msg->arg2);

			msg = (InfMessage*)allocator_getNext(stop->messages);
		}

		// Adjoin Commands
		s32 adjCount = allocator_getCount(stop->adjoinCmds);
		SERIALIZE(adjCount);
		AdjoinCmd* adjCmd = (AdjoinCmd*)allocator_getHead(stop->adjoinCmds);
		while (adjCmd)
		{
			serialization_writeSectorPtr(stream, adjCmd->sector0);
			serialization_writeSectorPtr(stream, adjCmd->sector1);
			SERIALIZE(adjCmd->wall0->id);
			SERIALIZE(adjCmd->wall1->id);
			adjCmd = (AdjoinCmd*)allocator_getNext(stop->adjoinCmds);
		}

		serialization_writeDfSound(stream, stop->pageId);

		SERIALIZE(stop->floorTexSecId);
		SERIALIZE(stop->ceilTexSecId);
		SERIALIZE(stop->index);
	}

	void inf_serializeSlave(Stream* stream, Slave* slave)
	{
		serialization_writeSectorPtr(stream, slave->sector);
		SERIALIZE(slave->value);
	}

	void inf_serializeLink(Stream* stream, InfLink* link)
	{
		SERIALIZE(link->type);
		// Task function is derived from the type.
		s32 index = -1;
		switch (link->type)
		{
			case LTYPE_SECTOR:
			{
				index = allocator_getIndex(s_infSerState.infElevators, link->elev);
			} break;
			case LTYPE_TRIGGER:
			{
				index = allocator_getIndex(s_infSerState.infTriggers, link->trigger);
			} break;
			case LTYPE_TELEPORT:
			{
				index = allocator_getIndex(s_infSerState.infTeleports, link->teleport);
			} break;
		}
		SERIALIZE(index);
		SERIALIZE(link->eventMask);
		SERIALIZE(link->entityMask);
		// link->parent will be computed on load 
		// link->task will be computed on load
		// link->freeFunc will be computed on load.
	}


	/////////////////////////////////////////////
	// Internal - Deserialize
	/////////////////////////////////////////////
	void inf_deserializeStop(Stream* stream, Stop* stop)
	{
		DESERIALIZE(stop->value);
		DESERIALIZE(stop->delay);

		// Messages
		s32 msgCount;
		DESERIALIZE(msgCount);
		stop->messages = allocator_create(sizeof(InfMessage));
		for (s32 m = 0; m < msgCount; m++)
		{
			InfMessage* msg = (InfMessage*)allocator_newItem(stop->messages);

			RSector* sector = serialization_readSectorPtr(stream);
			s32 wallIndex;
			DESERIALIZE(wallIndex);

			msg->wall = nullptr;
			msg->sector = nullptr;
			if (wallIndex >= 0)
			{
				msg->wall = &sector->walls[wallIndex];
			}
			else
			{
				msg->sector = sector;
			}

			DESERIALIZE(msg->msgType);
			DESERIALIZE(msg->event);
			DESERIALIZE(msg->arg1);
			DESERIALIZE(msg->arg2);
		}

		// Adjoin Commands
		s32 adjCount;
		DESERIALIZE(adjCount);
		stop->adjoinCmds = allocator_create(sizeof(AdjoinCmd));

		for (s32 a = 0; a < adjCount; a++)
		{
			AdjoinCmd* adjCmd = (AdjoinCmd*)allocator_newItem(stop->adjoinCmds);

			adjCmd->sector0 = serialization_readSectorPtr(stream);
			adjCmd->sector1 = serialization_readSectorPtr(stream);
			s32 wall0Id, wall1Id;
			DESERIALIZE(wall0Id);
			DESERIALIZE(wall1Id);
			adjCmd->wall0 = &adjCmd->sector0->walls[wall0Id];
			adjCmd->wall1 = &adjCmd->sector1->walls[wall1Id];
		}

		serialization_readDfSound(stream, &stop->pageId);

		DESERIALIZE(stop->floorTexSecId);
		DESERIALIZE(stop->ceilTexSecId);
		DESERIALIZE(stop->index);

		if (stop->floorTexSecId >= 0)
		{
			stop->floorTex = s_levelState.sectors[stop->floorTexSecId].floorTex;
		}
		if (stop->ceilTexSecId >= 0)
		{
			stop->ceilTex = s_levelState.sectors[stop->ceilTexSecId].ceilTex;
		}
	}

	void inf_deserializeSlave(Stream* stream, Slave* slave)
	{
		slave->sector = serialization_readSectorPtr(stream);
		DESERIALIZE(slave->value);
	}

	void inf_deserializeLink(Stream* stream, InfLink* link, Allocator* parent)
	{
		DESERIALIZE(link->type);

		// Task function is derived from the type.
		s32 index;
		DESERIALIZE(index);
		switch (link->type)
		{
			case LTYPE_SECTOR:
			{
				link->elev = (InfElevator*)allocator_getByIndex(s_infSerState.infElevators, index);
				link->task = s_infState.infElevTask;
				link->freeFunc = (InfFreeFunc)inf_deleteElevator;
			} break;
			case LTYPE_TRIGGER:
			{
				link->trigger = (InfTrigger*)allocator_getByIndex(s_infSerState.infTriggers, index);
				link->task = s_infState.infTriggerTask;
				link->freeFunc = (InfFreeFunc)inf_deleteTrigger;
			} break;
			case LTYPE_TELEPORT:
			{
				link->teleport = (Teleport*)allocator_getByIndex(s_infSerState.infTeleports, index);
				link->task = s_infState.teleportTask;
				link->freeFunc = nullptr;
			} break;
		}

		DESERIALIZE(link->eventMask);
		DESERIALIZE(link->entityMask);

		link->parent = parent;
	}

	void inf_computeElevValuePointer(InfElevator* elev)
	{
		RSector* sector = elev->sector;
		switch (elev->type)
		{
			case IELEV_MOVE_CEILING:
				elev->value = &sector->ceilingHeight;
				break;
			case IELEV_MOVE_FLOOR:
			case IELEV_MOVE_FC:
				elev->value = &sector->floorHeight;
				break;
			case IELEV_MOVE_OFFSET:
				elev->value = &sector->secHeight;
				break;
			case IELEV_CHANGE_LIGHT:
				elev->value = &sector->ambient;
				break;
			case IELEV_MOVE_WALL:
			case IELEV_ROTATE_WALL:
			case IELEV_SCROLL_WALL:
			case IELEV_SCROLL_FLOOR:
			case IELEV_SCROLL_CEILING:
			case IELEV_CHANGE_WALL_LIGHT:
				elev->value = &elev->iValue;
				break;
			default:
				// It shouldn't reach here.
				assert(0);
		};
	}
}
