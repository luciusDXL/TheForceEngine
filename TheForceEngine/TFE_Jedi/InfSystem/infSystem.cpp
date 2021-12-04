#include <cstring>

#include "infSystem.h"
#include "message.h"
#include <TFE_Game/igame.h>
#include <TFE_Asset/dfKeywords.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_DarkForces/hud.h>
#include <TFE_DarkForces/agent.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Jedi/Sound/soundSystem.h>
#include <TFE_Jedi/Memory/allocator.h>
#include <TFE_Jedi/Level/level.h>
#include <TFE_System/parser.h>
#include <TFE_System/system.h>
#include <TFE_System/memoryPool.h>
#include <TFE_System/math.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_Jedi/Task/task.h>
// TODO: This will make adding Outlaws harder, fix the abstraction.
#include <TFE_DarkForces/player.h>
#include <TFE_DarkForces/time.h>
#include "infTypesInternal.h"
// Include update functions
#include "infElevatorUpdateFunc.h"

using namespace TFE_Jedi;
using namespace TFE_DarkForces;

namespace TFE_Jedi
{
	enum InfConstants
	{
		MAX_INF_ITEMS = 512,
		DELAY_SLEEP = 0xffffffff,
	};

	typedef union { RSector* sector; RWall* wall; } InfTriggerObject;

	// These need to be filled out somewhere.
	static SoundSourceID s_moveCeilSound0 = NULL_SOUND;
	static SoundSourceID s_moveCeilSound1 = NULL_SOUND;
	static SoundSourceID s_moveCeilSound2 = NULL_SOUND;
	static SoundSourceID s_moveFloorSound0 = NULL_SOUND;
	static SoundSourceID s_moveFloorSound1 = NULL_SOUND;
	static SoundSourceID s_moveFloorSound2 = NULL_SOUND;
	static SoundSourceID s_doorSound = NULL_SOUND;
	static SoundSourceID s_needKeySoundId = NULL_SOUND;
	static SoundSourceID s_switchDefaultSndId = NULL_SOUND;
	
	// INF delta time in ticks.
	static s32 s_triggerCount = 0;
	static Allocator* s_infElevators = nullptr;
	static Allocator* s_infTeleports = nullptr;
	static Task* s_infElevTask = nullptr;
	static Task* s_infTriggerTask = nullptr;
	static Task* s_teleportTask = nullptr;
	
	static std::vector<char> s_buffer;
	// Loading
	static char s_infArg0[256];
	static char s_infArg1[256];
	static char s_infArg2[256];
	static char s_infArg3[256];
	static char s_infArg4[256];
	static char s_infArgExtra[256];
	static Stop* s_nextStop;
		
	void inf_elevatorTaskFunc(MessageType msg);
	void inf_telelporterTaskFunc(MessageType msg);
	void inf_triggerTaskFunc(MessageType msg);

	void infElevatorMsgFunc(MessageType msgType);
	void infTriggerMsgFunc(MessageType msgType);
	void inf_handleTriggerMsg(InfTrigger* trigger);
	
	void deleteElevator(InfElevator* elev);
	void deleteTrigger(InfTrigger* trigger);
	JBool updateElevator(InfElevator* elev);
	void elevHandleStopDelay(InfElevator* elev);
	Stop* inf_advanceStops(Allocator* stops, s32 absoluteStop, s32 relativeStop);
	bool inf_parseElevatorCommand(s32 argCount, KEYWORD action, Allocator* linkAlloc, bool seqEnd, InfElevator*& elev, s32& initStopIndex, InfLink*& link);
	void inf_parseMessage(MessageType* type, u32* arg1, u32* arg2, u32* evt, const char* infArg0, const char* infArg1, const char* infArg2, MessageType defType);
	void inf_setWallBits(RWall* wall);
	void inf_clearWallBits(RWall* wall);

	InfTrigger* inf_createTrigger(TriggerType type, InfTriggerObject obj);

	void inf_stopAdjoinCommands(Stop* stop);
	void inf_stopHandleMessages(MessageType msg);
	void inf_handleMsgLights();
	vec3_fixed inf_getElevSoundPos(InfElevator* elev);

	void inf_teleporterTaskLocal(MessageType msg);
	void inf_elevatorTaskLocal(MessageType msg);
	void inf_triggerTaskLocal(MessageType msg);

	/////////////////////////////////////////////////////
	// API
	/////////////////////////////////////////////////////
	bool inf_init()
	{
		return false;
	}

	void inf_shutdown()
	{
	}

	// No need to do the conversion ourselves like the DOS code did.
	s32 strToInt(const char* str)
	{
		char* endPtr;
		return strtol(str, &endPtr, 10);
	}

	u32 strToUInt(const char* str)
	{
		char* endPtr;
		return strtoul(str, &endPtr, 10);
	}

	void inf_clearState()
	{
		s_infElevators   = nullptr;
		s_infElevTask    = nullptr;
		s_teleportTask   = nullptr;
		s_infTeleports   = nullptr;
		s_infTriggerTask = nullptr;
		s_nextStop       = nullptr;
		s_triggerCount   = 0;
	}

	void inf_createElevatorTask()
	{
		s_infElevators = allocator_create(sizeof(InfElevator));
		s_infElevTask = createSubTask("elevator", inf_elevatorTaskFunc, inf_elevatorTaskLocal);
	}

	void inf_createTeleportTask()
	{
		s_teleportTask = createSubTask("teleporter", inf_telelporterTaskFunc, inf_teleporterTaskLocal);
		task_setNextTick(s_teleportTask, TASK_SLEEP);
		s_infTeleports = allocator_create(sizeof(Teleport));
	}

	void inf_createTriggerTask()
	{
		s_infTriggerTask = createSubTask("trigger", inf_triggerTaskFunc, inf_triggerTaskLocal);
		s_triggerCount = 0;
	}

	InfLink* allocateLink(Allocator* infLinks, InfElevator* elev)
	{
		InfLink* link = (InfLink*)allocator_newItem(infLinks);
		link->type = LTYPE_SECTOR;
		link->entityMask = INF_ENTITY_PLAYER;
		link->eventMask = INF_EVENT_NONE;
		link->freeFunc = nullptr;
		link->elev = elev;
		link->parent = infLinks;
		link->task = s_infElevTask;

		return link;
	}

	InfLink* inf_addElevatorToSector(InfElevator* elev, RSector* sector)
	{
		if (!sector->infLink)
		{
			sector->infLink = allocator_create(sizeof(InfLink));
		}
		return allocateLink(sector->infLink, elev);
	}

	Stop* allocateStop(Allocator* stops)
	{
		Stop* stop = (Stop*)allocator_newItem(stops);
		stop->value = 0;
		stop->delay = TICKS(4);	// 4 seconds.
		stop->messages = 0;
		stop->adjoinCmds = 0;
		stop->pageId = 0;
		stop->floorTex = nullptr;
		stop->ceilTex = 0;

		return stop;
	}

	Stop* allocateStop(InfElevator* elev)
	{
		if (!elev->stops)
		{
			elev->stops = allocator_create(sizeof(Stop));
		}
		return allocateStop(elev->stops);
	}

	void inf_gotoInitialStop(InfElevator* elev, s32 stopIndex)
	{
		if (!elev || !elev->stops)
		{
			if (elev) { elev->nextTick = s_curTick; }
			return;
		}
		Stop* stop = (Stop*)allocator_getByIndex(elev->stops, stopIndex);
		if (!stop) { return; }

		elev->nextStop = stop;
		fixed16_16 speed = elev->speed;
		elev->speed = 0;

		fixed16_16 dt = s_deltaTime;
		fixed16_16 fixedStep = elev->fixedStep;
		elev->fixedStep = 0;
		if (!s_deltaTime)
		{
			s_deltaTime = ONE_16;
		}
		updateElevator(elev);

		elev->fixedStep = fixedStep;
		elev->speed = speed;

		Stop* next = elev->nextStop;
		Tick delay = next->delay;
		
		if (delay == IDELAY_TERMINATE)
		{
			deleteElevator(elev);
			elev = nullptr;
			return;
		}
		else if (delay == IDELAY_HOLD)
		{
			elev->nextTick = DELAY_SLEEP;
		}
		else
		{
			elev->nextTick = s_curTick + next->delay;
		}

		// Setup the next stop.
		elev->nextStop = inf_advanceStops(elev->stops, 0, 1);
	}
		
	InfElevator* inf_allocateElevItem(RSector* sector, InfElevatorType type)
	{
		InfElevator* elev = (InfElevator*)allocator_newItem(s_infElevators);
		elev->trigMove = TRIGMOVE_HOLD;
		elev->key = KEY_NONE;
		elev->fixedStep = 0;
		elev->u1c = -1;
		elev->stops = nullptr;
		elev->slaves = nullptr;
		elev->nextStop = nullptr;
		elev->value = nullptr;
		elev->iValue = 0;
		elev->dirOrCenter.x = 0;
		elev->dirOrCenter.z = 0;
		elev->flags = 0;
		elev->loopingSoundID = NULL_SOUND;

		elev->type = type;
		elev->self = elev;
		elev->sector = sector;
		elev->updateFlags = ELEV_MASTER_ON;
		elev->sound0 = NULL_SOUND;
		elev->sound1 = NULL_SOUND;
		elev->sound2 = NULL_SOUND;

		if (type > IELEV_CHANGE_WALL_LIGHT)
		{
			return elev;
		}

		switch (type)
		{
		case IELEV_MOVE_CEILING:
			elev->speed = FIXED(8);
			elev->trigMove = TRIGMOVE_LAST;
			elev->value = &sector->ceilingHeight;
			elev->sound0 = s_moveCeilSound0;
			elev->sound1 = s_moveCeilSound1;
			elev->sound2 = s_moveCeilSound2;
			break;
		case IELEV_MOVE_FLOOR:
		case IELEV_MOVE_FC:
			elev->speed = FIXED(8);
			elev->trigMove = TRIGMOVE_CONT;
			elev->value = &sector->floorHeight;
			elev->sound0 = s_moveFloorSound0;
			elev->sound1 = s_moveFloorSound1;
			elev->sound2 = s_moveFloorSound2;
			break;
		case IELEV_MOVE_OFFSET:	
			elev->speed = FIXED(8);
			elev->trigMove = TRIGMOVE_CONT;
			elev->value = &sector->secHeight;
			elev->sound0 = s_moveFloorSound0;
			elev->sound1 = s_moveFloorSound1;
			elev->sound2 = s_moveFloorSound2;
			break;
		case IELEV_MOVE_WALL:
			elev->speed = FIXED(15);
			elev->trigMove = TRIGMOVE_CONT;
			elev->value = &elev->iValue;
			sinCosFixed(0, &elev->dirOrCenter.x, &elev->dirOrCenter.z);
			elev->sound0 = s_moveCeilSound0;
			elev->sound1 = s_moveCeilSound1;
			elev->sound2 = s_moveCeilSound2;
			break;
		case IELEV_ROTATE_WALL:
			elev->speed = FIXED(1024);
			elev->trigMove = TRIGMOVE_CONT;
			elev->value = &elev->iValue;
			elev->sound0 = s_moveCeilSound0;
			elev->sound1 = s_moveCeilSound1;
			elev->sound2 = s_moveCeilSound2;
			break;
		case IELEV_SCROLL_WALL:
			elev->speed = FIXED(4);
			elev->value = &elev->iValue;
			elev->trigMove = TRIGMOVE_CONT;
			break;
		case IELEV_SCROLL_FLOOR:
			elev->speed = FIXED(4);
			elev->value = &elev->iValue;
			elev->trigMove = TRIGMOVE_CONT;
			elev->flags = INF_EFLAG_MOVE_FLOOR | INF_EFLAG_MOVE_SECHT;
			sinCosFixed(0, &elev->dirOrCenter.x, &elev->dirOrCenter.z);
			break;
		case IELEV_SCROLL_CEILING:
			elev->speed = FIXED(4);
			elev->value = &elev->iValue;
			elev->trigMove = TRIGMOVE_CONT;
			elev->flags = INF_EFLAG_MOVE_CEIL;
			sinCosFixed(0, &elev->dirOrCenter.x, &elev->dirOrCenter.z);
			break;
		case IELEV_CHANGE_LIGHT:
			elev->speed = FIXED(32);
			elev->value = &sector->ambient;
			break;
		case IELEV_CHANGE_WALL_LIGHT:
			elev->speed = FIXED(32);
			elev->value = &elev->iValue;
			break;
		};

		return elev;
	}

	InfElevator* inf_allocateSpecialElevator(RSector* sector, InfSpecialElevator type)
	{
		if (type >= IELEV_SP_COUNT)
		{
			return nullptr;
		}

		InfElevator* elev = nullptr;
		InfLink* link = nullptr;
		switch (type)
		{
			case IELEV_SP_BASIC:
			{
				elev = inf_allocateElevItem(sector, IELEV_MOVE_FLOOR);
				link = inf_addElevatorToSector(elev, sector);

				elev->trigMove = TRIGMOVE_CONT;
				elev->speed = FIXED(8);
				link->entityMask = INF_ENTITY_PLAYER | INF_ENTITY_SMART_OBJ;
				link->eventMask = INF_EVENT_11 | INF_EVENT_10 | INF_EVENT_NUDGE_BACK | INF_EVENT_NUDGE_FRONT | INF_EVENT_ENTER_SECTOR;
			} break;
			case IELEV_SP_BASIC_AUTO:
			{
				elev = inf_allocateElevItem(sector, IELEV_MOVE_FLOOR);
				link = inf_addElevatorToSector(elev, sector);

				fixed16_16 maxFloor = -FIXED(9999);
				fixed16_16 minFloor = FIXED(9999);
				s32 wallCount = sector->wallCount;
				RWall* wall = sector->walls;
				for (s32 i = 0; i < wallCount; i++, wall++)
				{
					RSector* nextSector = wall->nextSector;
					if (nextSector)
					{
						maxFloor = max(maxFloor, nextSector->floorHeight);
						minFloor = min(minFloor, nextSector->floorHeight);
					}
				}

				fixed16_16 floorHeight = sector->floorHeight;
				if (minFloor == floorHeight)
				{
					std::swap(maxFloor, minFloor);
				}
				Stop* stop0 = allocateStop(elev);
				stop0->value = maxFloor;
				stop0->index = 0;

				// Add another stop.
				Stop* stop1 = allocateStop(elev);
				stop1->value = minFloor;
				stop1->index = 1;

				elev->speed = FIXED(8);
				elev->trigMove = TRIGMOVE_CONT;
				stop1->delay = IDELAY_HOLD;
				link->entityMask = INF_ENTITY_PLAYER | INF_ENTITY_SMART_OBJ;
				link->eventMask = INF_EVENT_11 | INF_EVENT_10 | INF_EVENT_NUDGE_BACK | INF_EVENT_NUDGE_FRONT | INF_EVENT_ENTER_SECTOR;
			} break;
			case IELEV_SP_UNIMPLEMENTED:
			case IELEV_SP_MID:
				// Unimplemented in the DOS code (or implementation removed).
				break;
			case IELEV_SP_INV:
			{
				elev = inf_allocateElevItem(sector, IELEV_MOVE_CEILING);
				link = inf_addElevatorToSector(elev, sector);

				elev->trigMove = TRIGMOVE_CONT;
				elev->speed = FIXED(8);
				link->entityMask = INF_ENTITY_PLAYER | INF_ENTITY_SMART_OBJ;
				link->eventMask = INF_EVENT_11 | INF_EVENT_10 | INF_EVENT_NUDGE_BACK | INF_EVENT_NUDGE_FRONT | INF_EVENT_ENTER_SECTOR;
			} break;
			case IELEV_SP_DOOR:
			{
				elev = inf_allocateElevItem(sector, IELEV_MOVE_CEILING);
				link = inf_addElevatorToSector(elev, sector);

				Stop* stop0 = allocateStop(elev);
				stop0->value = sector->floorHeight;
				stop0->delay = IDELAY_HOLD;
				stop0->index = 0;

				Stop* stop1 = allocateStop(elev);
				stop1->value = sector->ceilingHeight;
				stop1->index = 1;

				elev->trigMove = TRIGMOVE_LAST;
				elev->sound1 = NULL_SOUND;
				elev->sound2 = NULL_SOUND;
				elev->sound0 = s_doorSound;

				elev->speed = FIXED(30);
				link->entityMask = INF_ENTITY_PLAYER | INF_ENTITY_SMART_OBJ;
				link->eventMask = INF_EVENT_11 | INF_EVENT_10 | INF_EVENT_NUDGE_BACK | INF_EVENT_NUDGE_FRONT;
			} break;
			case IELEV_SP_DOOR_INV:
			{
				elev = inf_allocateElevItem(sector, IELEV_MOVE_FLOOR);
				link = inf_addElevatorToSector(elev, sector);

				Stop* stop0 = allocateStop(elev);
				Stop* stop1 = allocateStop(elev);
				stop0->value = sector->ceilingHeight;
				stop0->delay = IDELAY_HOLD;
				stop0->index = 0;
				stop1->value = sector->floorHeight;
				stop1->index = 1;

				elev->trigMove = TRIGMOVE_LAST;
				elev->speed = FIXED(15);
				link->entityMask = INF_ENTITY_PLAYER | INF_ENTITY_SMART_OBJ;
				link->eventMask = INF_EVENT_11 | INF_EVENT_10 | INF_EVENT_NUDGE_BACK | INF_EVENT_NUDGE_FRONT;
			} break;
			case IELEV_SP_DOOR_MID:
			{
				fixed16_16 halfHeight = TFE_Jedi::abs(sector->ceilingHeight - sector->floorHeight) >> 1;
				fixed16_16 middle = sector->floorHeight - halfHeight;

				// Upper Part
				elev = inf_allocateElevItem(sector, IELEV_MOVE_CEILING);
				link = inf_addElevatorToSector(elev, sector);

				Stop* stop0 = allocateStop(elev);
				stop0->value = middle;
				stop0->delay = IDELAY_HOLD;
				stop0->index = 0;

				Stop* stop1 = allocateStop(elev);
				stop1->value = sector->ceilingHeight;
				stop1->index = 1;
				inf_gotoInitialStop(elev, 0);

				elev->trigMove = TRIGMOVE_LAST;
				elev->speed = FIXED(15);
				link->entityMask = INF_ENTITY_PLAYER | INF_ENTITY_SMART_OBJ;
				link->eventMask = INF_EVENT_11 | INF_EVENT_10 | INF_EVENT_NUDGE_BACK | INF_EVENT_NUDGE_FRONT;

				// Lower Part
				elev = inf_allocateElevItem(sector, IELEV_MOVE_FLOOR);
				link = inf_addElevatorToSector(elev, sector);

				stop0 = allocateStop(elev);
				stop0->value = middle;
				stop0->delay = IDELAY_HOLD;
				stop0->index = 0;

				stop1 = allocateStop(elev);
				stop1->value = sector->floorHeight;
				stop1->index = 1;

				elev->trigMove = TRIGMOVE_LAST;
				elev->speed = FIXED(15);
				link->entityMask = INF_ENTITY_PLAYER | INF_ENTITY_SMART_OBJ;
				link->eventMask = INF_EVENT_11 | INF_EVENT_10 | INF_EVENT_NUDGE_BACK | INF_EVENT_NUDGE_FRONT;
			} break;
			case IELEV_SP_MORPH_SPIN1:
			{
				elev = inf_allocateElevItem(sector, IELEV_ROTATE_WALL);
				link = inf_addElevatorToSector(elev, sector);
				link->entityMask = INF_ENTITY_PLAYER | INF_ENTITY_SMART_OBJ;
				link->eventMask = INF_EVENT_ANY;
			} break;
			case IELEV_SP_MORPH_SPIN2:
			{
				elev = inf_allocateElevItem(sector, IELEV_ROTATE_WALL);
				link = inf_addElevatorToSector(elev, sector);
				link->entityMask = INF_ENTITY_PLAYER | INF_ENTITY_SMART_OBJ;
				link->eventMask = INF_EVENT_ANY;
				// This makes the player move with the sector.
				elev->flags = INF_EFLAG_MOVE_FLOOR | INF_EFLAG_MOVE_SECHT | INF_EFLAG_MOVE_CEIL;
			} break;
			case IELEV_SP_MORPH_MOVE1:
			{
				elev = inf_allocateElevItem(sector, IELEV_MOVE_WALL);
				link = inf_addElevatorToSector(elev, sector);
				link->entityMask = INF_ENTITY_PLAYER | INF_ENTITY_SMART_OBJ;
				link->eventMask = INF_EVENT_ANY;
			} break;
			case IELEV_SP_MORPH_MOVE2:
			{
				elev = inf_allocateElevItem(sector, IELEV_MOVE_WALL);
				link = inf_addElevatorToSector(elev, sector);
				link->entityMask = INF_ENTITY_PLAYER | INF_ENTITY_SMART_OBJ;
				link->eventMask = INF_EVENT_ANY;
				// This makes the player move with the sector.
				elev->flags = INF_EFLAG_MOVE_FLOOR | INF_EFLAG_MOVE_SECHT | INF_EFLAG_MOVE_CEIL;
			} break;
			case IELEV_SP_EXPLOSIVE_WALL:
			{
				elev = inf_allocateElevItem(sector, IELEV_MOVE_CEILING);
				link = inf_addElevatorToSector(elev, sector);

				Stop* stop0 = allocateStop(elev);
				Stop* stop1 = allocateStop(elev);

				stop0->value = sector->floorHeight;
				stop0->delay = IDELAY_HOLD;
				stop0->index = 0;

				stop1->value = sector->ceilingHeight;
				stop1->delay = IDELAY_TERMINATE;
				stop1->index = 1;

				elev->trigMove = TRIGMOVE_LAST;
				elev->speed = 0;
				elev->sound0 = NULL_SOUND;
				elev->sound1 = NULL_SOUND;
				elev->sound2 = NULL_SOUND;

				link->entityMask = INF_ENTITY_ANY;
				link->eventMask = INF_EVENT_EXPLOSION;
			} break;
		};

		// TODO(Core Game Loop Release):
		// link->freeFunc = inf_elevFreeFunc;
		if (elev)
		{
			inf_gotoInitialStop(elev, 0);
		}
		return elev;
	}

	Stop* inf_addStop(InfElevator* elev, s32 value)
	{
		Allocator* stops = elev->stops;
		if (!elev->stops)
		{
			elev->stops = allocator_create(sizeof(Stop));
			stops = elev->stops;
		}
		Stop* stop = (Stop*)allocator_newItem(stops);
		memset(stop, 0, sizeof(Stop));

		stop->value = value;
		stop->delay = TICKS(4);	// default delay = 4 seconds.
		stop->messages = nullptr;
		stop->adjoinCmds = nullptr;
		stop->pageId = NULL_SOUND;	// no page sound by default.
		stop->floorTex = nullptr;
		stop->ceilTex = nullptr;
		// This is a slow operation so it is only included if TFE_INCLUDE_STOP_INDEX == 1
		stop->index = -1;
		#if TFE_INCLUDE_STOP_INDEX == 1
			stop->index = allocator_getCount(stops) - 1;
		#endif

		return stop;
	}

	void inf_setStopDelay(Stop* stop, u32 delay)
	{
		stop->delay = delay;
	}

	void inf_getMessageTarget(const char* arg, RSector** sector, RWall** targetWall)
	{
		KEYWORD key = getKeywordIndex(arg);
		if (key == KW_SYSTEM)
		{
			*sector = nullptr;
			*targetWall = nullptr;
			return;
		}

		RSector* msgSector = nullptr;
		RWall* wall = nullptr;
		s32 wallIndex = -1;

		// There should be a variant of strstr() that returns a non-constant pointer, but in Visual Studio it is always constant.
		char* parenOpen = (char*)strstr(arg, "(");
		// This message targets a wall rather than a whole sector.
		if (parenOpen)
		{
			*parenOpen = 0;
			parenOpen++;

			char* parenClose = (char*)strstr(arg, ")");
			// This should never be true and this doesn't seem to be hit at runtime.
			// I wonder if this was meant to be { char* parenClose = (char*)strstr(parenOpen, ")"); } - which would make more sense.
			// Or it could have been check *before* the location at ")" was set to 0 above.
			if (parenClose)
			{
				*parenClose = 0;
			}
			// Finally parse the integer and set the wall index.
			wallIndex = strToInt(parenOpen);
		}

		MessageAddress* msgAddr = message_getAddress(arg);
		if (msgAddr)
		{
			msgSector = msgAddr->sector;
			if (wallIndex != -1)	// >= 0 would be safer, but the original code specifically checks against -1.
			{
				// Sets wall and removes sector.
				if (wallIndex < msgSector->wallCount)
				{
					wall = &msgSector->walls[wallIndex];
				}
				// Note that even if the wall index is invalid, msgSector is nulled out.
				msgSector = nullptr;
			}
		}

		*sector = msgSector;
		*targetWall = wall;
	}

	Stop* inf_getStopByIndex(InfElevator* elev, s32 index)
	{
		Allocator* stops = elev->stops;
		if (!stops)
		{
			return nullptr;
		}

		Stop* stop = (Stop*)allocator_getHead(stops);
		while (stop)
		{
			index--;
			if (index == -1)
			{
				return stop;
			}
			stop = (Stop*)allocator_getNext(stops);
		}
		return nullptr;
	}

	void inf_addSlave(InfElevator* elev, fixed16_16 value, RSector* sector)
	{
		if (!elev->slaves)
		{
			elev->slaves = allocator_create(sizeof(Slave));
		}
		Slave* slave = (Slave*)allocator_newItem(elev->slaves);
		slave->sector = sector;
		slave->value = value;
	}

	void inf_setDirFromAngle(InfElevator* elev, angle14_32 angle)
	{
		sinCosFixed(angle, &elev->dirOrCenter.x, &elev->dirOrCenter.z);
	}

	TriggerTarget* inf_addTriggerTarget(InfTrigger* trigger, RSector* targetSector, RWall* targetWall, u32 eventMask)
	{
		TriggerTarget* target = (TriggerTarget*)allocator_newItem(trigger->targets);
		target->sector = targetSector;
		target->wall = targetWall;
		target->eventMask = eventMask;

		return target;
	}

	Teleport* inf_createTeleport(TeleportType type, RSector* sector)
	{
		Teleport* teleport = (Teleport*)allocator_newItem(s_infTeleports);
		teleport->dstAngle[0] = 0;
		teleport->dstAngle[1] = 0;
		teleport->dstAngle[2] = 0;
		teleport->sector = sector;
		teleport->type = type;

		if (!sector->infLink)
		{
			sector->infLink = allocator_create(sizeof(InfLink));
		}

		InfLink* link = (InfLink*)allocator_newItem(sector->infLink);
		link->type = LTYPE_TELEPORT;
		link->entityMask = INF_ENTITY_ANY;
		link->eventMask = INF_EVENT_ENTER_SECTOR;
		link->teleport = teleport;
		link->task = s_teleportTask;
		link->parent = sector->infLink;

		return teleport;
	}

	void inf_setTeleportTarget(Teleport* teleport, RSector* sector)
	{
		teleport->target = sector;
	}

	void inf_setTeleportDestPosition(Teleport* teleport, fixed16_16 x, fixed16_16 y, fixed16_16 z)
	{
		teleport->dstPosition.x = x;
		teleport->dstPosition.y = y;
		teleport->dstPosition.z = z;
	}

	void inf_setTeleportDestAngle(Teleport* teleport, angle14_16 pitch, angle14_16 yaw, angle14_16 roll)
	{
		teleport->dstAngle[0] = pitch;
		teleport->dstAngle[1] = yaw;
		teleport->dstAngle[2] = roll;
	}
	
	// Return true if "SEQEND" found.
	bool parseElevator(TFE_Parser& parser, size_t& bufferPos, const char* itemName)
	{
		const char* line;

		MessageAddress* msgAddr = message_getAddress(itemName);
		RSector* sector = msgAddr->sector;
		KEYWORD itemSubclass = getKeywordIndex(s_infArg1);

		InfElevator* elev = nullptr;
		InfLink* link = nullptr;
		Allocator* linkAlloc = nullptr;

		// Special classes - these map to the 11 core elevator types but have special defaults and/or automatically setup stops.
		if (itemSubclass <= KW_MORPH_MOVE2)
		{
			InfSpecialElevator type;
			switch (itemSubclass)
			{
			case KW_BASIC:
				type = IELEV_SP_BASIC;
				break;
			case KW_BASIC_AUTO:
				type = IELEV_SP_BASIC_AUTO;
				break;
			case KW_ENCLOSED:
				// Unused.
				return false;
				break;
			case KW_INV:
				type = IELEV_SP_INV;
				break;
			case KW_MID:
				type = IELEV_SP_MID;
				break;
			case KW_DOOR:
				type = IELEV_SP_DOOR;
				break;
			case KW_DOOR_INV:
				type = IELEV_SP_DOOR_INV;
				break;
			case KW_DOOR_MID:
				type = IELEV_SP_DOOR_MID;
				break;
			case KW_MORPH_SPIN1:
				type = IELEV_SP_MORPH_SPIN1;
				break;
			case KW_MORPH_SPIN2:
				type = IELEV_SP_MORPH_SPIN2;
				break;
			case KW_MORPH_MOVE1:
				type = IELEV_SP_MORPH_MOVE1;
				break;
			case KW_MORPH_MOVE2:
				type = IELEV_SP_MORPH_MOVE2;
				break;
			default:
				// Invalid type.
				return false;
			};
			// index is mapped to a type internally.
			elev = inf_allocateSpecialElevator(sector, type);

			linkAlloc = sector->infLink;
			link = (InfLink*)allocator_getTail(linkAlloc);
			elev = link->elev;
		}
		// One of the core 11 types.
		else
		{
			InfElevatorType type;
			switch (itemSubclass)
			{
			case KW_MOVE_CEILING:
				type = IELEV_MOVE_CEILING;
				break;
			case KW_MOVE_FLOOR:
				type = IELEV_MOVE_FLOOR;
				break;
			case KW_MOVE_FC:
				type = IELEV_MOVE_FC;
				break;
			case KW_MOVE_OFFSET:
				type = IELEV_MOVE_OFFSET;
				break;
			case KW_MOVE_WALL:
				type = IELEV_MOVE_WALL;
				break;
			case KW_ROTATE_WALL:
				type = IELEV_ROTATE_WALL;
				break;
			case KW_SCROLL_WALL:
				type = IELEV_SCROLL_WALL;
				break;
			case KW_SCROLL_FLOOR:
				type = IELEV_SCROLL_FLOOR;
				break;
			case KW_SCROLL_CEILING:
				type = IELEV_SCROLL_CEILING;
				break;
			case KW_CHANGE_LIGHT:
				type = IELEV_CHANGE_LIGHT;
				break;
			case KW_CHANGE_WALL_LIGHT:
				type = IELEV_CHANGE_WALL_LIGHT;
				break;
			default:
				// Invalid type.
				return false;
			}

			elev = inf_allocateElevItem(sector, type);
			link = inf_addElevatorToSector(elev, sector);
			linkAlloc = sector->infLink;
		}

		s32 initStopIndex = 0;
		bool seqEnd = false;
		while (!seqEnd)
		{
			line = parser.readLine(bufferPos);
			// There is another class in this sequence, so finish the current class by setting up the initial stop.
			if (strstr(line, "CLASS"))
			{
				inf_gotoInitialStop(elev, initStopIndex);
				break;
			}

			char id[256];
			s32 argCount = sscanf(line, " %s %s %s %s %s %s %s", id, s_infArg0, s_infArg1, s_infArg2, s_infArg3, s_infArg4, s_infArgExtra);
			KEYWORD action = getKeywordIndex(id);
			if (action == KW_UNKNOWN)
			{
				TFE_System::logWrite(LOG_WARNING, "INF", "Unknown elevator command - '%s'.", id);
			}

			seqEnd = inf_parseElevatorCommand(argCount, action, linkAlloc, seqEnd, elev, initStopIndex, link);
		} // while (!seqEnd)

		return seqEnd;
	}

	// Return true if "SEQEND" found.
	bool parseSectorTrigger(TFE_Parser& parser, size_t& bufferPos, s32 argCount, const char* itemName)
	{
		MessageAddress* msgAddr = message_getAddress(itemName);
		assert(msgAddr);
		RSector* sector = msgAddr ? msgAddr->sector : nullptr;

		// The original code is a bit strange here.
		// Since this is a sector trigger, all of the different types behave exactly the same, though there are multiple
		// conditionals to get to that result.
		// This code simplifies this.
		InfTriggerObject obj; obj.sector = sector;
		InfTrigger* trigger = sector ? inf_createTrigger(ITRIGGER_SECTOR, obj) : nullptr;

		// Loop through trigger parameters.
		const char* line;
		bool seqEnd = false;
		while (!seqEnd)
		{
			line = parser.readLine(bufferPos);
			// There is another class in this sequence, so we are done with the trigger.
			if (strstr(line, "CLASS"))
			{
				break;
			}
			
			char id[256];
			argCount = sscanf(line, " %s %s %s %s %s", id, s_infArg0, s_infArg1, s_infArg2, s_infArg3);
			KEYWORD itemId = getKeywordIndex(id);
			assert(itemId != KW_UNKNOWN);

			// Continue reading, but the information cannot be filled in properly.
			if (!sector)
			{
				if (itemId == KW_SEQEND)
				{
					seqEnd = true;
					break;
				}
				continue;
			}

			switch (itemId)
			{
				case KW_CLIENT:
				{
					RWall* targetWall;
					RSector* targetSector;
					inf_getMessageTarget(s_infArg0, &targetSector, &targetWall);
					TriggerTarget* target = inf_addTriggerTarget(trigger, targetSector, targetWall, INF_EVENT_ANY);
					if (argCount > 2)
					{
						target->eventMask = strToUInt(s_infArg1);
					}
				} break;
				case KW_MASTER:
				{
					trigger->master = JFALSE;
				} break;
				case KW_TEXT:
				{
					if (s_infArg0[0] >= '0' && s_infArg0[0] <= '9')
					{
						trigger->textId = strToUInt(s_infArg0);
					}
				} break;
				case KW_MESSAGE:
				{
					inf_parseMessage(&trigger->cmd, &trigger->arg0, &trigger->arg1, nullptr, s_infArg0, s_infArg1, s_infArg2, MSG_TRIGGER);
				} break;
				case KW_EVENT_MASK:
				{
					if (s_infArg0[0] == '*')
					{
						trigger->link->eventMask = INF_EVENT_ANY;
					}
					else
					{
						trigger->link->eventMask = strToUInt(s_infArg0);
					}
				} break;
				case KW_ENTITY_MASK:
				case KW_OBJECT_MASK:
				{
					if (s_infArg0[0] == '*')
					{
						trigger->link->entityMask = INF_ENTITY_ANY;
					}
					else
					{
						trigger->link->entityMask = strToUInt(s_infArg0);
					}
				} break;
				case KW_EVENT:
				{
					trigger->event = strToUInt(s_infArg0);
				} break;
				case KW_SEQEND:
				{
					// The sequence for this item has completed (no more classes).
					seqEnd = true;
				} break;
			}
		}

		return seqEnd;
	}
		
	// Return true if "SEQEND" found.
	bool parseTeleport(TFE_Parser& parser, size_t& bufferPos, const char* itemName)
	{
		MessageAddress* msgAddr = message_getAddress(itemName);
		RSector* sector = msgAddr->sector;

		KEYWORD type = getKeywordIndex(s_infArg1);
		Teleport* teleport = nullptr;
		if (type == KW_BASIC)
		{
			teleport = inf_createTeleport(TELEPORT_BASIC, sector);
		}
		else if (type == KW_CHUTE)
		{
			teleport = inf_createTeleport(TELEPORT_CHUTE, sector);
		}

		// Loop through trigger parameters.
		const char* line;
		bool seqEnd = false;
		while (!seqEnd)
		{
			line = parser.readLine(bufferPos);
			// There is another class in this sequence, so we are done with the trigger.
			if (strstr(line, "CLASS"))
			{
				break;
			}

			char name[256];
			sscanf(line, " %s %s %s %s %s", name, s_infArg0, s_infArg1, s_infArg2, s_infArg3);
			KEYWORD kw = getKeywordIndex(name);

			if (kw == KW_TARGET)
			{
				msgAddr = message_getAddress(s_infArg0);
				inf_setTeleportTarget(teleport, msgAddr->sector);
			}
			else if (kw == KW_MOVE)
			{
				char* endPtr;
				fixed16_16 dstPosX = floatToFixed16(strtof(s_infArg0, &endPtr));
				fixed16_16 dstPosY = floatToFixed16(strtof(s_infArg1, &endPtr));
				fixed16_16 dstPosZ = floatToFixed16(strtof(s_infArg2, &endPtr));
				angle14_16 yaw = s16(strtof(s_infArg3, &endPtr) * 360.0f / 16383.0f);

				inf_setTeleportDestPosition(teleport, dstPosX, dstPosY, dstPosZ);
				inf_setTeleportDestAngle(teleport, 0, yaw, 0);
			}
			else if (kw == KW_SEQEND)
			{
				seqEnd = true;
				break;
			}
			else
			{
				break;
			}
		}

		return seqEnd;
	}

	// Return true if "SEQEND" found.
	bool parseLineTrigger(TFE_Parser& parser, size_t& bufferPos, s32 argCount, const char* name, s32 num)
	{
		KEYWORD typeId = getKeywordIndex(s_infArg0);
		assert(typeId != KW_UNKNOWN);

		MessageAddress* msgAddr = message_getAddress(name);
		RSector* sector = msgAddr->sector;
		RWall* wall = &sector->walls[num];

		InfTriggerObject obj;
		InfTrigger* trigger = nullptr;
		if (argCount > 2)
		{
			KEYWORD subTypeId = getKeywordIndex(s_infArg1);
			assert(subTypeId != KW_UNKNOWN);
							
			switch (subTypeId)
			{
				case KW_SWITCH1:
				{
					obj.wall = wall;
					trigger = inf_createTrigger(ITRIGGER_SWITCH1, obj);
				} break;
				case KW_TOGGLE:
				{
					obj.wall = wall;
					trigger = inf_createTrigger(ITRIGGER_TOGGLE, obj);
				} break;
				case KW_SINGLE:
				{
					obj.wall = wall;
					trigger = inf_createTrigger(ITRIGGER_SINGLE, obj);
				} break;
				case KW_STANDARD:
				{
					obj.wall = wall;
					trigger = inf_createTrigger(ITRIGGER_WALL, obj);
				} break;
				default:
				{
					obj.wall = wall;
					trigger = inf_createTrigger(ITRIGGER_WALL, obj);
				}
			}
		}
		else
		{
			obj.wall = wall;
			trigger = inf_createTrigger(ITRIGGER_WALL, obj);
		}

		// Trigger parameters
		const char* line;
		bool seqEnd = false;
		while (!seqEnd)
		{
			line = parser.readLine(bufferPos);
			if (strstr(line, "CLASS"))
			{
				break;
			}

			char id[256];
			argCount = sscanf(line, " %s %s %s %s %s", id, s_infArg0, s_infArg1, s_infArg2, s_infArg3);
			KEYWORD itemId = getKeywordIndex(id);
			if (itemId == KW_UNKNOWN)
			{
				TFE_System::logWrite(LOG_WARNING, "INF", "Unknown trigger parameter - '%s'.", id);
			}

			switch (itemId)
			{
				case KW_SEQEND:
				{
					seqEnd = true;
				} break;
				case KW_CLIENT:
				{
					RWall* targetWall;
					RSector* targetSector;
					inf_getMessageTarget(s_infArg0, &targetSector, &targetWall);

					TriggerTarget* target = inf_addTriggerTarget(trigger, targetSector, targetWall, INF_EVENT_ANY);
					if (argCount > 2)
					{
						target->eventMask = strToUInt(s_infArg1);
					}
				} break;
				case KW_EVENT_MASK:
				{
					if (s_infArg0[0] == '*')
					{
						trigger->link->eventMask = INF_EVENT_ANY;
					}
					else
					{
						trigger->link->eventMask = strToUInt(s_infArg0);
					}
				} break;
				case KW_MASTER:
				{
					trigger->master = JFALSE;
				} break;
				case KW_TEXT:
				{
					if (s_infArg0[0] >= '0' && s_infArg0[0] <= '9')
					{
						trigger->textId = strToUInt(s_infArg0);
					}
				} break;
				case KW_ENTITY_MASK:
				case KW_OBJECT_MASK:
				{
					if (s_infArg0[0] == '*')
					{
						trigger->link->entityMask = INF_ENTITY_ANY;
					}
					else
					{
						trigger->link->entityMask = strToUInt(s_infArg0);
					}
				} break;
				case KW_EVENT:
				{
					trigger->event = strToUInt(s_infArg0);
				} break;
				case KW_SOUND_COLON:
				{
					SoundSourceID soundSourceId = NULL_SOUND;
					// Not ascii
					if (s_infArg0[0] < '0' || s_infArg0[0] > '9')
					{
						soundSourceId = sound_Load(s_infArg0);
					}
					trigger->soundId = soundSourceId;
				} break;
				case KW_MESSAGE:
				{
					inf_parseMessage(&trigger->cmd, &trigger->arg0, &trigger->arg1, nullptr, s_infArg0, s_infArg1, s_infArg2, MSG_DONE);
				} break;
			}  // switch (itemId)
		}  // while (!seqEnd)

		return seqEnd;
	}

	// For now load the INF data directly.
	// Move back to asset later.
	JBool inf_load(const char* levelName)
	{
		char levelPath[TFE_MAX_PATH];
		strcpy(levelPath, levelName);
		strcat(levelPath, ".INF");

		FilePath filePath;
		if (!TFE_Paths::getFilePath(levelPath, &filePath))
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadINF", "Cannot find level INF '%s'.", levelPath);
			return JFALSE;
		}
		FileStream file;
		if (!file.open(&filePath, FileStream::MODE_READ))
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadINF", "Cannot open level INF '%s'.", levelPath);
			return JFALSE;
		}
		size_t len = file.getSize();
		s_buffer.resize(len);
		file.readBuffer(s_buffer.data(), u32(len));
		file.close();

		TFE_Parser parser;
		size_t bufferPos = 0;
		parser.init(s_buffer.data(), s_buffer.size());
		parser.enableBlockComments();
		parser.addCommentString("//");
		parser.convertToUpperCase(true);

		const char* line;
		line = parser.readLine(bufferPos);
		f32 version;
		if (sscanf(line, "INF %f", &version) != 1)
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadINF", "Cannot read INF version.");
			return JFALSE;
		}
		if (version != 1.0f)
		{
			TFE_System::logWrite(LOG_ERROR, "level_loadINF", "Incorrect INF version %f, should be 1.0.", version);
			return JFALSE;
		}

		// Keep looping until ITEMS is found.
		s32 itemCount = 0;
		while (1)
		{
			line = parser.readLine(bufferPos);
			if (!line)
			{
				TFE_System::logWrite(LOG_ERROR, "level_loadINF", "Cannot find ITEMS in INF: '%s'.", levelName);
				return JFALSE;
			}

			if (sscanf(line, "ITEMS %d", &itemCount) == 1)
			{
				break;
			}
		}

		// Then loop through all of the items and parse their classes.
		itemCount = min(itemCount, MAX_INF_ITEMS);
		// It turns out that Dark Forces is relying on using the previous value of 'num' if a line does not set it properly.
		// If I clear it every time, then some things stop working.
		// So rather than relying on the local memory to just line up every loop, I pulled out the local variable.
		s32 num = 0;
		for (s32 i = 0; i < itemCount; i++)
		{
			line = parser.readLine(bufferPos);
			if (!line)
			{
				TFE_System::logWrite(LOG_WARNING, "level_loadINF", "Hit the end of INF '%s' before parsing all items: %d/%d", levelName, i, itemCount);
				return JTRUE;
			}

			char item[256], name[256];
			while (sscanf(line, " ITEM: %s NAME: %s NUM: %d", item, name, &num) < 1)
			{
				line = parser.readLine(bufferPos);
				if (!line)
				{
					TFE_System::logWrite(LOG_ERROR, "level_loadINF", "Hit the end of INF '%s' before parsing all items: %d/%d", levelName, i, itemCount);
					return JTRUE;
				}
				continue;
			}

			KEYWORD itemType = getKeywordIndex(item);
			switch (itemType)
			{
				case KW_LEVEL:
				{
					// This is very incomplete and needs to be fixed.
					// This doesn't actually work correctly in the base game though, so its lower priority.
					line = parser.readLine(bufferPos);
					if (strstr(line, "SEQ"))
					{
						char itemName[256];
						s32 argCount = sscanf(line, " %s %s %s %s %s %s %s", itemName, s_infArg0, s_infArg1, s_infArg2, s_infArg3, s_infArgExtra, s_infArgExtra);
						KEYWORD levelItem = getKeywordIndex(itemName);
						if (levelItem == KW_AMB_SOUND)
						{
							// TODO(Core Game Loop Release)
						}
						else if (levelItem == KW_SEQEND)
						{
							// TODO(Core Game Loop Release)
						}
						else
						{
							// TODO(Core Game Loop Release)
						}
					}
				} break;
				case KW_SECTOR:
				{
					line = parser.readLine(bufferPos);
					if (!strstr(line, "SEQ"))
					{
						continue;
					}

					line = parser.readLine(bufferPos);
					// Loop until seqend since an INF item may have multiple classes.
					while (1)
					{
						if (!strstr(line, "CLASS"))
						{
							break;
						}

						char id[256];
						s32 argCount = sscanf(line, " %s %s %s %s %s %s %s", id, s_infArg0, s_infArg1, s_infArg2, s_infArg3, s_infArg4, s_infArgExtra);
						KEYWORD itemClass = getKeywordIndex(s_infArg0);
						assert(itemClass != KW_UNKNOWN);

						if (itemClass == KW_ELEVATOR)
						{
							if (parseElevator(parser, bufferPos, name))
							{
								// If we have reached the end of the sequence, time to break out of the loop.
								break;
							}
						}
						else if (itemClass == KW_TRIGGER)
						{
							if (parseSectorTrigger(parser, bufferPos, argCount, name))
							{
								// If we have reached the end of the sequence, time to break out of the loop.
								break;
							}
						}
						else if (itemClass == KW_TELEPORTER)
						{
							if (parseTeleport(parser, bufferPos, name))
							{
								// If we have reached the end of the sequence, time to break out of the loop.
								break;
							}
						}
					}
				} break;
				case KW_LINE:
				{
					line = parser.readLine(bufferPos);
					if (!strstr(line, "SEQ"))
					{
						continue;
					}

					line = parser.readLine(bufferPos);
					// Loop until seqend since an INF item may have multiple classes.
					while (1)
					{
						if (!strstr(line, "CLASS"))
						{
							break;
						}

						char id[256];
						s32 argCount = sscanf(line, " %s %s %s %s %s", id, s_infArg0, s_infArg1, s_infArg2, s_infArg3);
						if (parseLineTrigger(parser, bufferPos, argCount, name, num))
						{
							break;
						}
					}  // while (!seqEnd) - outer (Line Classes).
				} break;
			}
		}  // for (s32 i = 0; i < itemCount; i++)

		return JTRUE;
	}

	// Sounds
	void inf_loadSounds()
	{
		s_moveCeilSound0  = sound_Load("door2-1.voc");
		s_moveCeilSound1  = sound_Load("door2-2.voc");
		s_moveCeilSound2  = sound_Load("door2-3.voc");
		s_moveFloorSound0 = sound_Load("elev2-1.voc");
		s_moveFloorSound1 = sound_Load("elev2-2.voc");
		s_moveFloorSound2 = sound_Load("elev2-3.voc");
		s_doorSound       = sound_Load("door.voc");
		s_needKeySoundId  = sound_Load("locked-1.voc");
	}

	void inf_loadDefaultSwitchSound()
	{
		s_switchDefaultSndId = sound_Load("switch3.voc");
	}

	void inf_elevatorTaskLocal(MessageType msg)
	{
		infElevatorMsgFunc(msg);
	}
			
	// Per frame update.
	void inf_elevatorTaskFunc(MessageType msg)
	{
		struct LocalContext
		{
			InfElevator* elev;
			Stop* nextStop;
			s32 elevDeleted;

		};
		task_begin_ctx;

		while (msg != MSG_FREE_TASK)
		{
			if (msg != MSG_RUN_TASK)	// General elevator message, just pass it along.
			{
				infElevatorMsgFunc(msg);
			}
			else  // id == 0
			{
				taskCtx->elev = (InfElevator*)allocator_getHead(s_infElevators);
				while (taskCtx->elev)
				{
					taskCtx->elevDeleted = 0;
					if ((taskCtx->elev->updateFlags & ELEV_MASTER_ON) && taskCtx->elev->nextTick < s_curTick)
					{
						// If not already moving, get started.
						if (!(taskCtx->elev->updateFlags & ELEV_MOVING) && !taskCtx->elevDeleted)
						{
							// Figure out the source position for the sound effect.
							vec3_fixed sndPos = inf_getElevSoundPos(taskCtx->elev);

							// Play the startup sound effect if the elevator is not already at the next stop.
							Stop* nextStop = taskCtx->elev->nextStop;
							// Play the initial sound as the elevator starts moving.
							if (nextStop && *taskCtx->elev->value != nextStop->value)
							{
								playSound3D_oneshot(taskCtx->elev->sound0, sndPos);
							}

							// Update the next time, so this will move on the next update.
							taskCtx->elev->nextTick = s_curTick;

							// Flag the elevator as moving.
							taskCtx->elev->updateFlags |= ELEV_MOVING;
						}
						// If the elevator is moving, then play the looping sound.
						if (taskCtx->elev->updateFlags & ELEV_MOVING)
						{
							// Figure out the source position for the sound effect.
							vec3_fixed sndPos = inf_getElevSoundPos(taskCtx->elev);

							// Start up the sound effect, track it since it is looping.
							taskCtx->elev->loopingSoundID = playSound3D_looping(taskCtx->elev->sound1, taskCtx->elev->loopingSoundID, sndPos);
						}

						// if reachedStop = JFALSE, elevator has not reached the next stop.
						if (updateElevator(taskCtx->elev))
						{
							elevHandleStopDelay(taskCtx->elev);

							taskCtx->nextStop = taskCtx->elev->nextStop;
							if (taskCtx->elev->updateFlags & ELEV_MOVING_REVERSE)
							{
								taskCtx->elev->nextTick = s_curTick + TICKS_PER_SECOND;	// this will pause the elevator for one second.
								taskCtx->elev->updateFlags &= ~ELEV_MOVING_REVERSE;		// remove the reverse flag.
							}
							else
							{
								u32 delay = taskCtx->nextStop->delay;
								if (delay == IDELAY_HOLD)
								{
									taskCtx->elev->trigMove = TRIGMOVE_HOLD;
									taskCtx->elev->nextTick = DELAY_SLEEP;
								}
								else if (delay == IDELAY_COMPLETE || delay == IDELAY_TERMINATE)
								{
									// delete the elevator, we're done here.
									deleteElevator(taskCtx->elev);
									taskCtx->elevDeleted = 1;
									if (delay == IDELAY_COMPLETE)
									{
										agent_levelComplete();
										agent_createLevelEndTask();
									}
								}
								else  // Timed
								{
									taskCtx->elev->nextTick = s_curTick + taskCtx->nextStop->delay;
								}
							}

							// Process stop messages if the elevator has not been deleted.
							if (!taskCtx->elevDeleted)
							{
								// Messages
								s_nextStop = taskCtx->nextStop;
								task_callTaskFunc(inf_stopHandleMessages);

								{
									// Adjoin Commands.
									inf_stopAdjoinCommands(taskCtx->nextStop);

									// Floor texture change.
									TextureData** floorTex = taskCtx->nextStop->floorTex;
									if (floorTex)
									{
										RSector* sector = taskCtx->elev->sector;
										sector->floorTex = floorTex;
									}

									// Ceiling texture change.
									TextureData** ceilTex = taskCtx->nextStop->ceilTex;
									if (ceilTex)
									{
										RSector* sector = taskCtx->elev->sector;
										sector->ceilTex = ceilTex;
									}

									// Page (special 2D sound effect that plays, such as voice overs).
									SoundSourceID pageId = taskCtx->nextStop->pageId;
									if (pageId)
									{
										playSound2D(pageId);
									}

									// Advance to the next stop.
									taskCtx->elev->nextStop = inf_advanceStops(taskCtx->elev->stops, 0, 1);
								}
							} // (!elevDeleted)
						}
					} // ((elev->updateFlags & ELEV_MASTER_ON) && elev->nextTick < s_curTick)

					// Next elevator.
					taskCtx->elev = (InfElevator*)allocator_getNext(s_infElevators);
				} // while (elev)
			}  // id == 0 (main elevator update loop)
			task_yield(TASK_NO_DELAY);
		}  // while (id != -1)

		task_end;
	}
		
	void inf_teleporterTaskLocal(MessageType msg)
	{
		if (msg == MSG_TRIGGER && s_msgEvent == INF_EVENT_ENTER_SECTOR)
		{
			task_makeActive(s_teleportTask);
		}
		else
		{
			// TODO(Core Game Loop Release)
		}
	}
	
	void inf_telelporterTaskFunc(MessageType msg)
	{
		struct LocalContext
		{
			Tick delay;
		};
		task_begin_ctx;

		while (msg != MSG_FREE_TASK)
		{
			taskCtx->delay = TASK_SLEEP;

			if (msg != MSG_RUN_TASK)
			{
				inf_teleporterTaskLocal(msg);
				taskCtx->delay = TASK_NO_DELAY;
			}
			else
			{
				Teleport* teleport = (Teleport*)allocator_getHead(s_infTeleports);
				while (teleport)
				{
					RSector* sector = teleport->sector;
					s32 objCount = sector->objectCount;
					SecObject** objList = sector->objectList;

					for (s32 i = 0; i < objCount; objList++)
					{
						SecObject* obj = *objList;
						if (obj)
						{
							i++;
							taskCtx->delay = TASK_NO_DELAY;
							TeleportType type = teleport->type;
							if (type <= TELEPORT_BASIC)
							{
								// So dstPosition is actually an absolute position.
								obj->posWS = teleport->dstPosition;
								obj->pitch = teleport->dstAngle[0];
								obj->yaw   = teleport->dstAngle[1];
								obj->roll  = teleport->dstAngle[2];
								sector_addObject(teleport->target, obj);
							}
							else if (type == TELEPORT_CHUTE)
							{
								sector = teleport->sector;
								fixed16_16 floorThreshold = sector->floorHeight - HALF_16;
								// if the object is lower than 0.5 units above the floor.
								if (floorThreshold < obj->posWS.y)
								{
									sector_addObject(teleport->target, obj);
								}
							}

							if (obj->entityFlags & ETFLAG_PLAYER)
							{
								// automap_setLayer(obj->sector->layer);
							}
						}  // if (obj)
					}  // for (s32 i = 0; i < objCount; objList++)
					teleport = (Teleport*)allocator_getNext(s_infTeleports);
				}  // while (teleport)
			}

			task_yield(taskCtx->delay);
		}  // while (id != -1)
		task_end;
	}

	void inf_triggerTaskLocal(MessageType msg)
	{
		InfTrigger* trigger = (InfTrigger*)s_msgTarget;

		switch (msg)
		{
			case MSG_FREE:
			{
				InfLink* link = trigger->link;
				allocator_deleteItem(link->parent, link);
				allocator_free(trigger->targets);

				level_free(trigger);
				s_triggerCount--;

				if (s_triggerCount == 0)
				{
					task_free(s_infTriggerTask);
					return;
				}
			} break;
			case MSG_TRIGGER:
			{
				inf_handleTriggerMsg(trigger);
			} break;
			case MSG_MASTER_OFF:
			{
				trigger->master = JFALSE;
			} break;
			case MSG_MASTER_ON:
			{
				trigger->master = JTRUE;
			} break;
			case MSG_DONE:
			{
				if (trigger->type == ITRIGGER_SWITCH1)
				{
					AnimatedTexture* animTex = trigger->animTex;
					// If the trigger is still in its original state, nothing to do.
					if (animTex)
					{
						trigger->state = 0;
						trigger->tex = animTex->frameList[0];
					}
					trigger->master = JTRUE;
				}
			} break;
		}
	}

	void inf_triggerTaskFunc(MessageType msg)
	{
		task_begin;
		// Wait until explicitly called.
		task_yield(TASK_SLEEP);

		while (msg != MSG_FREE_TASK)
		{
			inf_triggerTaskLocal(msg);
			task_yield(TASK_SLEEP);
		}

		task_end;
	}
		   			   
	// Send messages so that entities and the player can interact with the INF system.
	// If msgType = MSG_SET/CLEAR_BITS the msg is processed directly for this wall OR
	// this iterates through the valid links and calls their msgFunc.
	void inf_wallSendMessage(RWall* wall, SecObject* entity, u32 evt, MessageType msgType)
	{
		if (msgType == MSG_SET_BITS)
		{
			inf_setWallBits(wall);
		}
		else if (msgType == MSG_CLEAR_BITS)
		{
			inf_clearWallBits(wall);
		}
		else
		{
			inf_sendLinkMessages(wall->infLink, entity, evt, msgType);
		}
	}
		
	// Sends a message to a wall at a specific position (parametric along wall + height).
	// This is used so that switches are only activated if directly hit.
	void inf_wallSendMessageAtPos(RWall* hitWall, SecObject* obj, u32 evt, fixed16_16 paramPos, fixed16_16 yPos)
	{
		if (hitWall->signTex)
		{
			fixed16_16 baseHeight;
			fixed16_16 uOffset;
			switch (hitWall->drawFlags)
			{
				case WDF_MIDDLE:
				{
					baseHeight = hitWall->sector->floorHeight;
					uOffset = hitWall->midOffset.x;
				} break;
				case WDF_TOP:
				{
					baseHeight = hitWall->nextSector->ceilingHeight;
					uOffset = hitWall->topOffset.x;	// esi
				} break;
				case WDF_BOT:
				case WDF_TOP_AND_BOT:
				{
					baseHeight = hitWall->sector->floorHeight;
					uOffset = hitWall->botOffset.x;
				} break;
			}

			if (yPos)
			{
				fixed16_16 base = baseHeight + (hitWall->signOffset.z >> 3);
				// base + 0.5
				fixed16_16 y0 = base + HALF_16;
				// base - texHeight/8 - 0.5
				fixed16_16 y1 = base - ((*hitWall->signTex)->height << 13) - HALF_16;
				if (y0 < yPos || y1 > yPos)
				{
					return;
				}
			}

			if (paramPos)
			{
				fixed16_16 base = -(uOffset >> 3) + (hitWall->signOffset.x >> 3);
				fixed16_16 left = base - HALF_16;
				fixed16_16 right = base + ((*hitWall->signTex)->width << 13) + HALF_16;
				if (paramPos < left || paramPos > right)
				{
					return;
				}
			}
		}

		inf_wallSendMessage(hitWall, obj, evt, MSG_TRIGGER);
	}

	void inf_wallAndMirrorSendMessageAtPos(RWall* hitWall, SecObject* obj, u32 evt, fixed16_16 paramPos, fixed16_16 yPos)
	{
		inf_wallSendMessageAtPos(hitWall, obj, evt, paramPos, yPos);
		if (hitWall->mirrorWall)
		{
			// evt*2 is the next event. This is obviously incorrect for shooting the wall, but the land event shouldn't be used for a line
			// so it doesn't seem to be an issue...
			inf_wallSendMessageAtPos(hitWall->mirrorWall, obj, evt * 2, paramPos, yPos);	// eax, edx, ebx, ecx, stack.
		}
	}

	void inf_triggerWallEvent(RWall* wall, SecObject* obj, u32 event)
	{
		inf_wallSendMessage(wall, obj, event, MSG_TRIGGER);

		RWall* mirror = wall->mirrorWall;
		if (mirror)
		{
			// Opposite event.
			u32 backsideEvent = event * 2;
			inf_wallSendMessage(mirror, obj, backsideEvent, MSG_TRIGGER);
		}
	}

	/////////////////////////////////////////////////////
	// Internal
	/////////////////////////////////////////////////////
	void inf_parseMessage(MessageType* type, u32* arg1, u32* arg2, u32* evt, const char* infArg0, const char* infArg1, const char* infArg2, MessageType defType)
	{
		const KEYWORD msgId = getKeywordIndex(infArg0);

		switch (msgId)
		{
			case KW_NEXT_STOP:
				*type = MSG_NEXT_STOP;
				break;
			case KW_PREV_STOP:
				*type = MSG_PREV_STOP;
				break;
			case KW_GOTO_STOP:
				*type = MSG_GOTO_STOP;
				*arg1 = strToUInt(infArg1);
				if (evt) { *evt = INF_EVENT_NONE; }
				break;
			case KW_MASTER_ON:
				*type = MSG_MASTER_ON;
				break;
			case KW_MASTER_OFF:
				*type = MSG_MASTER_OFF;
				break;
			case KW_DONE:
				*type = MSG_DONE;
				break;
			case KW_SET_BITS:
				*type = MSG_SET_BITS;
				*arg1 = strToUInt(infArg1);
				*arg2 = strToUInt(infArg2);
				if (evt) { *evt = INF_EVENT_NONE; }
				break;
			case KW_CLEAR_BITS:
				*type = MSG_CLEAR_BITS;
				*arg1 = strToUInt(infArg1);
				*arg2 = strToUInt(infArg2);
				if (evt) { *evt = INF_EVENT_NONE; }
				break;
			case KW_COMPLETE:
				*type = MSG_COMPLETE;
				*arg1 = strToUInt(infArg1);
				if (evt) { *evt = INF_EVENT_NONE; }
				break;
			case KW_LIGHTS:
				*type = MSG_LIGHTS;
				break;
			case KW_WAKEUP:
				*type = MSG_WAKEUP;
				break;
			case KW_M_TRIGGER:
				*type = MSG_TRIGGER;
				break;
			default:
				*type = defType;
		}
	}

	bool inf_parseElevatorCommand(s32 argCount, KEYWORD action, Allocator* linkAlloc, bool seqEnd, InfElevator*& elev, s32& initStopIndex, InfLink*& link)
	{
		char* endPtr;
		MessageAddress* msgAddr;

		switch (action)
		{
			case KW_START:
			{
				initStopIndex = strToInt(s_infArg0);
			} break;
			case KW_STOP:
			{
				fixed16_16 stopValue;
				// Calculate the stop value.
				if (s_infArg0[0] == '@')  // Relative Value
				{
					f32 value = strtof(&s_infArg0[1], &endPtr);
					stopValue = floatToFixed16(value);

					if (elev->type == IELEV_MOVE_CEILING || elev->type == IELEV_MOVE_FLOOR || elev->type == IELEV_MOVE_FC)
					{
						RSector* sector = elev->sector;
						stopValue = sector->floorHeight - stopValue;
					}
					else if (elev->type == IELEV_ROTATE_WALL)
					{
						// This is a bug (in the original code):
						// This will always evaluate to 0 because strtof('@') = 0
						value = strtof(s_infArg0, &endPtr);
						stopValue = s32(value * 16383.0f / 360.0f);
						stopValue = floatToFixed16(f32(stopValue));
					}
				}
				else if ((s_infArg0[0] >= '0' && s_infArg0[0] <= '9') || s_infArg0[0] == '-' || s_infArg0[0] == '.')  // Numeric Value
				{
					f32 value = strtof(s_infArg0, &endPtr);
					stopValue = floatToFixed16(value);

					if (elev->type == IELEV_MOVE_CEILING || elev->type == IELEV_MOVE_FLOOR || elev->type == IELEV_MOVE_FC)
					{
						// Elevators that move the floor and/or ceiling need to be converted from +Y up to -Y up.
						stopValue = -stopValue;
					}
					else if (elev->type == IELEV_ROTATE_WALL)
					{
						// Rotation stop values need to be converted to angles.
						value = strtof(s_infArg0, &endPtr);
						stopValue = s32(value * 16383.0f / 360.0f);
						stopValue = floatToFixed16(f32(stopValue));
					}
				}
				else  // Value from named sector.
				{
					msgAddr = message_getAddress(s_infArg0);
					RSector* sector = msgAddr->sector;
					stopValue = sector->floorHeight;
				}

				Stop* stop = inf_addStop(elev, stopValue);
				if (argCount < 3)
				{
					return seqEnd;
				}

				// Delay is optional, if not specified each elevator has its own default.
				Tick delay = 0;
				// Numeric
				if ((s_infArg1[0] >= '0' && s_infArg1[0] <= '9') || s_infArg1[0] == '-' || s_infArg1[0] == '.')
				{
					f32 value = strtof(s_infArg1, &endPtr);
					// Convert from seconds to ticks.
					delay = Tick(SECONDS_TO_TICKS_ROUNDED * value);
				}
				else if (strcasecmp(s_infArg1, "HOLD") == 0)
				{
					delay = IDELAY_HOLD;
				}
				else if (strcasecmp(s_infArg1, "TERMINATE") == 0)
				{
					delay = IDELAY_TERMINATE;
				}
				else if (strcasecmp(s_infArg1, "COMPLETE") == 0)
				{
					delay = IDELAY_COMPLETE;
				}

				inf_setStopDelay(stop, delay);
			} break;
			case KW_SPEED:
			{
				if (elev->type == IELEV_ROTATE_WALL)
				{
					f32 value = strtof(s_infArg0, &endPtr);
					// 360 degrees is split into 16384 angular units.
					// Speed is in angular units.
					s32 speed = s32(value * 16383.0f / 360.0f);
					// Then speed is converted to a fixed point value, essentially 14.16 fixed point.
					elev->speed = intToFixed16(speed & 0xffff);
				}
				else
				{
					f32 value = strtof(s_infArg0, &endPtr);
					elev->speed = floatToFixed16(value);
				}
			} break;
			case KW_MASTER:
			{
				// KW_MASTER (this seems to always turn master off)
				elev->updateFlags &= ~ELEV_MASTER_ON;
			} break;
			case KW_ANGLE:
			{
				f32 value = strtof(s_infArg0, &endPtr);
				s32 angle = s32(value * 16383.0f / 360.0f);
				inf_setDirFromAngle(elev, angle);
			} break;
			case KW_ADJOIN:
			{
				s32 stopId = strToInt(s_infArg0);
				Stop* stop = inf_getStopByIndex(elev, stopId);
				if (stop)
				{
					if (!stop->adjoinCmds)
					{
						stop->adjoinCmds = allocator_create(sizeof(AdjoinCmd));
					}
					AdjoinCmd* adjoinCmd = (AdjoinCmd*)allocator_newItem(stop->adjoinCmds);
					MessageAddress* msgAddr0 = message_getAddress(s_infArg1);
					MessageAddress* msgAddr1 = message_getAddress(s_infArg3);
					RSector* sector0 = msgAddr0->sector;
					RSector* sector1 = msgAddr1->sector;

					s32 wallIndex0 = strToInt(s_infArg2);
					s32 wallIndex1 = strToInt(s_infArg4);

					adjoinCmd->wall0 = &sector0->walls[wallIndex0];
					adjoinCmd->wall1 = &sector1->walls[wallIndex1];
					adjoinCmd->sector0 = sector0;
					adjoinCmd->sector1 = sector1;
				}
			} break;
			case KW_TEXTURE:
			{
				s32 stopId = strToInt(s_infArg0);
				Stop* stop = inf_getStopByIndex(elev, stopId);
				if (stop)
				{
					msgAddr = message_getAddress(s_infArg2);
					RSector* sector = msgAddr->sector;
					if (s_infArg1[0] == 'C' || s_infArg1[0] == 'c')
					{
						stop->ceilTex = sector->ceilTex;
					}
					else
					{
						stop->floorTex = sector->floorTex;
					}
				}
			} break;
			case KW_SLAVE:
			{
				MessageAddress* msgAddr = message_getAddress(s_infArg0);
				s32 slaveValue = 0;
				if (argCount > 2)
				{
					f32 value = strtof(s_infArg1, &endPtr);
					slaveValue = floatToFixed16(value);
				}
				inf_addSlave(elev, slaveValue, msgAddr->sector);
			} break;
			case KW_MESSAGE:
			{
				s32 stopId = strToInt(s_infArg0);
				Stop* stop = inf_getStopByIndex(elev, stopId);
				if (!stop)
				{
					return seqEnd;
				}
				if (!stop->messages)
				{
					stop->messages = allocator_create(sizeof(InfMessage));
				}

				InfMessage* msg = (InfMessage*)allocator_newItem(stop->messages);
				RSector* targetSector;
				RWall* targetWall;
				inf_getMessageTarget(s_infArg1, &targetSector, &targetWall);
				msg->sector = targetSector;
				msg->wall = targetWall;

				msg->msgType = MSG_TRIGGER;
				msg->event = INF_EVENT_NONE;
				if (argCount >= 5)
				{
					msg->event = strToUInt(s_infArg3);
				}

				if (argCount > 3)
				{
					inf_parseMessage(&msg->msgType, &msg->arg1, &msg->arg2, &msg->event, s_infArg2, s_infArg3, s_infArg4, MSG_TRIGGER);
				}
			} break;
			case KW_EVENT_MASK:
			{
				if (s_infArg0[0] == '*')
				{
					link->eventMask = INF_EVENT_ANY;	// everything
				}
				else
				{
					link->eventMask = strToUInt(s_infArg0);	// specified
				}
			} break;
			case KW_ENTITY_MASK:
			case KW_OBJECT_MASK:
			{
				// Entity_mask and Object_mask are buggy for elevators...
				if (s_infArg0[0] == '*')
				{
					// This looks like a bug, this should be eventMask but is entityMask in the code...
					link->entityMask = INF_ENTITY_ANY;
				}
				else
				{
					link->eventMask = strToUInt(s_infArg0);
				}
			} break;
			case KW_CENTER:
			{
				f32 centerX = strtof(s_infArg0, &endPtr);
				f32 centerZ = strtof(s_infArg1, &endPtr);
				elev->dirOrCenter.x = floatToFixed16(centerX);
				elev->dirOrCenter.z = floatToFixed16(centerZ);
			} break;
			case KW_KEY_COLON:
			{
				KEYWORD key = getKeywordIndex(s_infArg0);
				if (key == KW_RED)
				{
					elev->key = KEY_RED;
				}
				else if (key == KW_YELLOW)
				{
					elev->key = KEY_YELLOW;
				}
				else if (key == KW_BLUE)
				{
					elev->key = KEY_BLUE;
				}
			} break;
			// This entry is required for special cases, like IELEV_SP_DOOR_MID, where multiple elevators are created at once.
			// This way, we can modify the first created elevator as well as the last.
			// It allows allows us to modify previous classes... but this isn't recommended.
			case KW_ADDON:
			{
				s32 index = strToInt(s_infArg0);
				link = (InfLink*)allocator_getByIndex(linkAlloc, index);
				elev = link->elev;
			} break;
			case KW_FLAGS:
			{
				elev->flags = strToUInt(s_infArg0);
			} break;
			case KW_SOUND_COLON:
			{
				SoundSourceID soundSourceId = 0;
				if (s_infArg1[0] >= '0' && s_infArg1[0] <= '9')
				{
					// Any numeric value means "no sound."
					soundSourceId = NULL_SOUND;
				}
				else
				{
					soundSourceId = sound_Load(s_infArg1);
				}

				// Determine which elevator sound to assign soundId to.
				s32 soundIdx = strToInt(s_infArg0);
				if (soundIdx == 1)
				{
					elev->sound0 = soundSourceId;
				}
				else if (soundIdx == 2)
				{
					elev->sound1 = soundSourceId;
				}
				else if (soundIdx == 3)
				{
					elev->sound2 = soundSourceId;
				}
			} break;
			case KW_PAGE:
			{
				SoundSourceID soundSourceId = sound_Load(s_infArg1);
				setSoundSourceVolume(soundSourceId, 127);

				s32 index = strToInt(s_infArg0);
				Stop* stop = inf_getStopByIndex(elev, index);
				if (stop) { stop->pageId = soundSourceId; }
			} break;
			case KW_SEQEND:
			{
				// The sequence for this item has completed (no more classes).
				// Finish the current class by setting up the initial stop.
				inf_gotoInitialStop(elev, initStopIndex);
				seqEnd = true;
			} break;
		}

		return seqEnd;
	}

	void inf_setWallBits(RWall* wall)
	{
		u32 flagsIndex = s_msgArg1;
		u32 bits = s_msgArg2;
		if (flagsIndex == 1)
		{
			wall->flags1 |= bits;

			// If there is a mirror, also set some of the bits there.
			RWall* mirror = wall->mirrorWall;
			if (mirror)
			{
				const u32 allowedMirrorFlags = (WF1_HIDE_ON_MAP | WF1_SHOW_NORMAL_ON_MAP | WF1_DAMAGE_WALL | WF1_SHOW_AS_LEDGE_ON_MAP | WF1_SHOW_AS_DOOR_ON_MAP);
				mirror->flags1 |= (bits & allowedMirrorFlags);
			}
		}
		else if (flagsIndex == 2)
		{
			wall->flags2 |= bits;
		}
		// It looks like if mirror is set for flags2, it also sets them for 3...
		else if (flagsIndex == 3)
		{
			wall->flags3 |= bits;

			// If there is a mirror, also set some of the bits there.
			RWall* mirror = wall->mirrorWall;
			if (mirror)
			{
				mirror->flags3 |= (bits & 0x0f);
			}
		}
	}

	void inf_clearWallBits(RWall* wall)
	{
		u32 flagsIndex = s_msgArg1;
		u32 bits = s_msgArg2;
		if (flagsIndex == 1)
		{
			wall->flags1 &= ~bits;

			// If there is a mirror, also clear some of the bits there.
			RWall* mirror = wall->mirrorWall;
			if (mirror)
			{
				const u32 allowedMirrorFlags = WF1_HIDE_ON_MAP | WF1_SHOW_NORMAL_ON_MAP | WF1_DAMAGE_WALL | WF1_SHOW_AS_LEDGE_ON_MAP | WF1_SHOW_AS_DOOR_ON_MAP;
				mirror->flags1 &= ~(bits & allowedMirrorFlags);
			}
		}
		else if (flagsIndex == 2)
		{
			wall->flags2 &= ~bits;
		}
		else if (flagsIndex == 3)
		{
			wall->flags3 &= ~bits;

			// If there is a mirror, also set some of the bits there.
			RWall* mirror = wall->mirrorWall;
			if (mirror)
			{
				mirror->flags3 &= ~(bits & 0x0f);
			}
		}
	}

	// Send messages for each link in the infLink list.
	// This is the same code for walls and sectors.
	void inf_sendLinkMessages(Allocator* infLink, SecObject* entity, u32 evt, MessageType msgType)
	{
		if (!infLink) { return; }

		InfLink* link = (InfLink*)allocator_getHead(infLink);
		while (link)
		{
			// Fire off the link task if the event and entity match the requirements.
			if ((evt == INF_EVENT_NONE || (link->eventMask & evt)) && (!entity || (link->entityMask & entity->entityFlags)) && link->task)
			{
				s_msgEntity = entity;
				s_msgTarget = link->target;
				s_msgEvent = evt;

				allocator_addRef(infLink);
				task_runLocal(link->task, msgType);

				// Drain pending tasks before moving on.
				/* TODO
				while (s_taskId)
				{
					func_1c7a02(s_curTask);
					yieldTask(0);
				}
				*/

				allocator_release(infLink);
			}
			link = (InfLink*)allocator_getNext(infLink);
		}
	}
	
	// Update an elevator.
	// Returns JTRUE if the elevator has reached the next stop, else JFALSE.
	JBool updateElevator(InfElevator* elev)
	{
		fixed16_16* value = elev->value;
		Stop* nextStop = elev->nextStop;

		fixed16_16 v0 = *value;
		fixed16_16 v1 = v0;
		if (nextStop)
		{
			v1 = nextStop->value;
		}

		// The elevator has reached the next stop.
		if (v1 == v0 && nextStop)
		{
			return JTRUE;
		}

		InfElevatorType type = elev->type;
		InfUpdateFunc updateFunc = nullptr;
		switch (type)
		{
			case IELEV_MOVE_CEILING:
			case IELEV_MOVE_FLOOR:
			case IELEV_MOVE_OFFSET:
			case IELEV_MOVE_FC:
				updateFunc = infUpdate_moveHeights;
				break;
			case IELEV_MOVE_WALL:
				updateFunc = infUpdate_moveWall;
				break;
			case IELEV_ROTATE_WALL:
				updateFunc = infUpdate_rotateWall;
				break;
			case IELEV_SCROLL_WALL:
				updateFunc = infUpdate_scrollWall;
				break;
			case IELEV_SCROLL_FLOOR:
			case IELEV_SCROLL_CEILING:
				updateFunc = infUpdate_scrollFlat;
				break;
			case IELEV_CHANGE_LIGHT:
				updateFunc = infUpdate_changeAmbient;
				break;
			case IELEV_CHANGE_WALL_LIGHT:
				updateFunc = infUpdate_changeWallLight;
				break;
		}

		fixed16_16 frameDelta = 0;
		if (!nextStop)
		{
			// If there are no stops, then the elevator keeps going forever...
			// Useful for scrolling textures, constantly spinning gears, etc.
			frameDelta = mul16(elev->speed, s_deltaTime);
		}
		else
		{
			fixed16_16 delta = v1 - v0;
			frameDelta = delta;
			if (elev->speed)
			{
				if (!elev->fixedStep)
				{
					fixed16_16 change = mul16(elev->speed, s_deltaTime);
					if (delta > change)
					{
						frameDelta = change;
					}
					else if (delta < -change)
					{
						frameDelta = -change;
					}
					else
					{
						frameDelta = delta;
					}
				}
				else
				{
					// This is a little strange, this could lead to framerate based speeds...
					frameDelta = elev->speed;
				}
			}
		}

		fixed16_16 newValue;
		if (updateFunc)
		{
			newValue = updateFunc(elev, frameDelta);
		}
		else
		{
			// If there is no update function, just increment the value based and move on.
			*elev->value += frameDelta;
			newValue = *elev->value;
		}

		// Returns JTRUE if the elevator has reached the next stop, otherwise returns JFALSE.
		return (v1 == newValue && elev->nextStop) ? JTRUE : JFALSE;
	}

	void inf_elevHandleTriggerMsg(InfElevator* elev, u32 evt)
	{
		switch (elev->trigMove)
		{
			case TRIGMOVE_CONT:
			{
				if (!(elev->updateFlags & ELEV_MOVING))
				{
					// If the elevator is not already at the next stop, play the start sound.
					if (elev->nextStop && *elev->value != elev->nextStop->value)
					{
						// Get the sound location.
						vec3_fixed pos = inf_getElevSoundPos(elev);
						playSound3D_oneshot(elev->sound0, pos);
					}

					// Update the next time so the elevator will move on the next update.
					elev->nextTick = s_curTick;
					elev->updateFlags |= ELEV_MOVING;
				}
			} break;
			case TRIGMOVE_LAST:
			{
				// Goto the last stop.
				elev->nextStop = inf_advanceStops(elev->stops, -1, 0);
				if (elev->nextStop && *elev->value != elev->nextStop->value)
				{
					// Get the sound location.
					vec3_fixed pos = inf_getElevSoundPos(elev);
					playSound3D_oneshot(elev->sound0, pos);
				}
				// Update the next time so the elevator will move on the next update.
				elev->nextTick = s_curTick;
				elev->updateFlags |= ELEV_MOVING;
			} break;
			case TRIGMOVE_PREV:
			{
				if (elev->nextTick < s_curTick)
				{
					elev->nextStop = inf_advanceStops(elev->stops, 0, -1);
				}
				elev->nextStop = inf_advanceStops(elev->stops, 0, -1);
				if (!(elev->updateFlags & ELEV_MOVING))
				{
					if (elev->nextStop && *elev->value != elev->nextStop->value)
					{
						vec3_fixed pos = inf_getElevSoundPos(elev);
						playSound3D_oneshot(elev->sound0, pos);
					}
					elev->nextTick = s_curTick;
					elev->updateFlags |= ELEV_MOVING;
				}
			} break;
			case TRIGMOVE_NEXT:
			default:
			{
				if (elev->nextTick < s_curTick)
				{
					elev->nextStop = inf_advanceStops(elev->stops, 0, 1);
				}
				if (!(elev->updateFlags & ELEV_MOVING))
				{
					if (elev->nextStop && *elev->value != elev->nextStop->value)
					{
						vec3_fixed pos = inf_getElevSoundPos(elev);
						playSound3D_oneshot(elev->sound0, pos);
					}
					elev->nextTick = s_curTick;
					elev->updateFlags |= ELEV_MOVING;
				}
			}
		}

		// Handle the explosion event.
		if (evt == INF_EVENT_EXPLOSION)
		{
			RSector* sector = elev->sector;
			RWall* wall = sector->walls;
			for (s32 i = 0; i < sector->wallCount; i++, wall++)
			{
				wall->flags1 &= ~(WF1_HIDE_ON_MAP | WF1_SHOW_NORMAL_ON_MAP);
			}
		}
	}

	void inf_handleTriggerMsg(InfTrigger* trigger)
	{
		if (trigger->master)
		{
			// Play trigger sound.
			playSound2D(trigger->soundId);

			// Trigger targets (clients).
			TriggerTarget* target = (TriggerTarget*)allocator_getHead(trigger->targets);
			u32 event = s_msgEvent;
			while (target)
			{
				if (target->eventMask & event)
				{
					s_msgArg1 = trigger->arg0;
					s_msgArg2 = trigger->arg1;

					if (target->wall)
					{
						inf_wallSendMessage(target->wall, nullptr, trigger->event, trigger->cmd);
					}
					else if (target->sector)
					{
						message_sendToSector(target->sector, nullptr, trigger->event, trigger->cmd);
					}
					else  // the target is a trigger, recursively call the msg func.
					{
						infTriggerMsgFunc(trigger->cmd);
					}
				}
				target = (TriggerTarget*)allocator_getNext(trigger->targets);
			}
		}

		// Send "TEXT" message.
		if (trigger->textId)
		{
			hud_sendTextMessage(trigger->textId);
		}

		// Update the trigger state (including switch textures).
		TriggerType type = trigger->type;
		switch (type)
		{
			case ITRIGGER_TOGGLE:
			{
				AnimatedTexture* animTex = trigger->animTex;
				if (animTex)
				{
					trigger->state++;
					if (trigger->state >= animTex->count)
					{
						trigger->state = 0;
					}

					s32 state = trigger->state;
					trigger->tex = animTex->frameList[state];
				}
			} break;
			case ITRIGGER_SINGLE:
			{
				AnimatedTexture* animTex = trigger->animTex;
				if (animTex)
				{
					if (animTex->count)
					{
						trigger->state = 1;
					}
					s32 state = trigger->state;
					trigger->tex = animTex->frameList[state];
					// Single can only be triggered once.
					trigger->master = JFALSE;
				}
			} break;
			case ITRIGGER_SWITCH1:
			{
				AnimatedTexture* animTex = trigger->animTex;
				if (animTex)
				{
					if (animTex->count)
					{
						trigger->state = 1;
					}
					s32 state = trigger->state;
					trigger->tex = animTex->frameList[state];
				}
				// Switch1 can only be triggered once until it recieves the "DONE" message.
				trigger->master = JFALSE;
			} break;
		}
	}

	void infElevatorMessageInternal(MessageType msgType)
	{
		u32 event = s_msgEvent;
		InfElevator* elev = (InfElevator*)s_msgTarget;
		SecObject* entity = (SecObject*)s_msgEntity;
		RSector* sector = elev->sector;
		u32 arg1 = s_msgArg1;

		if (entity && (entity->entityFlags&ETFLAG_SMART_OBJ))
		{
			if (sector->flags1 & SEC_FLAGS1_NO_SMART_OBJ)
			{
				return;
			}
			else if (!(sector->flags1 & SEC_FLAGS1_SMART_OBJ) && !(elev->flags & INF_EFLAG_DOOR))
			{
				return;
			}
		}

		// Master On message.
		if (msgType == MSG_MASTER_ON)
		{
			// Turn master on.
			elev->updateFlags |= ELEV_MASTER_ON;
			return;
		}
		else if (!(elev->updateFlags & ELEV_MASTER_ON))
		{
			return;
		}

		// For other messages, make sure the correct key is held.
		if (event != INF_EVENT_31)
		{
			// Non-player entities cannot use this because it requires a key.
			if (entity && (entity->entityFlags & ETFLAG_SMART_OBJ) && elev->key != KEY_NONE)
			{
				return;
			}

			// Does the player have the key?
			KeyItem key = elev->key;
			if (key == KEY_RED && !s_playerInfo.itemRedKey)
			{
				// "You need the red key."
				hud_sendTextMessage(6);
				playSound2D(s_needKeySoundId);
				return;
			}
			else if (key == KEY_YELLOW && !s_playerInfo.itemYellowKey)
			{
				// "You need the yellow key."
				hud_sendTextMessage(7);
				playSound2D(s_needKeySoundId);
				return;
			}
			else if (key == KEY_BLUE && !s_playerInfo.itemBlueKey)
			{
				// "You need the blue key."
				hud_sendTextMessage(8);
				playSound2D(s_needKeySoundId);
				return;
			}
		}
				
		// Other messages.
		switch (msgType)
		{
			case MSG_TRIGGER:
			{
				inf_elevHandleTriggerMsg(elev, event);
			} break;
			case MSG_NEXT_STOP:
			{
				// This will not fire if the elevator is in the HOLD or delay state.
				// This is because nextStop should already be set in that case, so firing it again
				// will cause the elevator to skip a stop.
				if (elev->nextTick < s_curTick)
				{
					elev->nextStop = inf_advanceStops(elev->stops, 0, 1);
				}
				// Play the start sound and update the flags / next update time.
				if (!(elev->updateFlags & ELEV_MOVING))
				{
					if (*elev->value != elev->nextStop->value)
					{
						vec3_fixed pos = inf_getElevSoundPos(elev);
						playSound3D_oneshot(elev->sound0, pos);
					}
					elev->nextTick = s_curTick;
					elev->updateFlags |= ELEV_MOVING;
				}
			} break;
			case MSG_PREV_STOP:
			{
				// If the elevator is in the HOLD state (i.e. the next time is in the future), go ahead and move back an additional time.
				// Why? Since nextStop is the next stop, this sets nextStop to the *current* stop.
				if (elev->nextTick > s_curTick)
				{
					// What this does is move the nextStop back to the "current" stop, so when calling inf_advanceStops() a second time we don't wind up 
					// back where we started.
					elev->nextStop = inf_advanceStops(elev->stops, 0, -1);
				}
				// Back to the previous stop (see the note above on why this can happen twice).
				elev->nextStop = inf_advanceStops(elev->stops, 0, -1);
				// Play the start sound and update the flags / next update time.
				if (!(elev->updateFlags & ELEV_MOVING))
				{
					if (*elev->value != elev->nextStop->value)
					{
						vec3_fixed pos = inf_getElevSoundPos(elev);
						playSound3D_oneshot(elev->sound0, pos);
					}
					elev->updateFlags |= ELEV_MOVING;
					elev->nextTick = s_curTick;
				}
			} break;
			case MSG_GOTO_STOP:
			if (elev->stops)
			{
				Stop* stop = inf_advanceStops(elev->stops, arg1, 0);
				if (stop->value != *elev->value)
				{
					elev->nextStop = stop;
					vec3_fixed pos = inf_getElevSoundPos(elev);

					// In this case the original code will play the start sound if the elevator isn't moving
					// and then get it moving...
					if (!(elev->updateFlags & ELEV_MOVING))
					{
						playSound3D_oneshot(elev->sound0, pos);
						elev->updateFlags |= ELEV_MOVING;
						elev->nextTick = s_curTick;
					}
					// ... and then stop which - which always happens because updateFlags is or will be set to moving
					// and then stops the looping sound if it is playing and then play the stop sound.
					// So in some cases you will get the start sound and the stop sound and then the start sound again slightly later.
					// This seems like buggy behavior... but I'm going to leave it alone for now.
					if (elev->updateFlags & ELEV_MOVING)
					{
						stopSound(elev->loopingSoundID);
						elev->loopingSoundID = NULL_SOUND;

						playSound3D_oneshot(elev->sound2, pos);
						elev->updateFlags &= ~ELEV_MOVING;
					}
				}
			} break;
			case MSG_REV_MOVE:
			{
				RSector* sector = elev->sector;
				if (!(sector->flags1 & SEC_FLAGS1_CRUSHING))
				{
					if (elev->trigMove <= TRIGMOVE_CONT)
					{
						// This will go to the previous stop.
						elev->nextStop = inf_advanceStops(elev->stops, 0, -1);
						elev->updateFlags |= ELEV_MOVING_REVERSE;
					}
					else  // TRIGMOVE_LAST or TRIGMOVE_NEXT or TRIGMOVE_PREV
					{
						// This will get the last stop.
						elev->nextStop = inf_advanceStops(elev->stops, -1, 0);
						elev->updateFlags |= ELEV_MOVING_REVERSE;
					}
					elev->nextTick = 0;
				}
			} break;
			case MSG_MASTER_OFF:
			{
				// Disable the elevator.
				elev->updateFlags &= ~ELEV_MASTER_ON;

				// Handle the case when the elevator is moving and needs to stop.
				if (elev->updateFlags & ELEV_MOVING)
				{
					// Get the sound position.
					vec3_fixed pos = inf_getElevSoundPos(elev);

					// Stop the looping sound and then play the stopping sound.
					stopSound(elev->loopingSoundID);
					elev->loopingSoundID = NULL_SOUND;
					// Play the stop one shot.
					playSound3D_oneshot(elev->sound2, pos);

					// Remove the "moving" flag.
					elev->updateFlags &= ~ELEV_MOVING;
				}
			} break;
			case MSG_COMPLETE:
			{
				// Fill in the goal specified by 'arg1'
				s_complete[0][arg1] = JTRUE;
				// Move the elevator to the stop specified by 'arg1' if it is NOT holding.
				if (elev->nextTick < s_curTick)
				{
					elev->nextStop = inf_advanceStops(elev->stops, arg1, 0);
				}
				// Play the sound effect, update the flags, update the next update time if NOT moving.
				if (!(elev->updateFlags & ELEV_MOVING))
				{
					if (*elev->value != elev->nextStop->value)
					{
						vec3_fixed pos = inf_getElevSoundPos(elev);
						playSound3D_oneshot(elev->sound0, pos);
					}
					elev->nextTick = s_curTick;
					elev->updateFlags |= ELEV_MOVING;
				}
			} break;
		}
	}

	void infElevatorMsgFunc(MessageType msgType)
	{
		if (msgType == MSG_FREE)
		{
			deleteElevator((InfElevator*)s_msgTarget);
			return;
		}
		infElevatorMessageInternal(msgType);
	}
		
	void infTriggerMsgFunc(MessageType msgType)
	{
		InfTrigger* trigger = (InfTrigger*)s_msgTarget;
		switch (msgType)
		{
			case MSG_FREE:
			{
				deleteTrigger(trigger);
				// In the original code, it this was the last trigger the "task" would be deallocated.
				// For TFE, this is just a callback so that is no longer necessary.
			} break;
			case MSG_TRIGGER:
			{
				inf_handleTriggerMsg(trigger);
			} break;
			case MSG_MASTER_OFF:
			{
				trigger->master = JFALSE;
			} break;
			case MSG_DONE:
			{
				if (trigger->type == ITRIGGER_SWITCH1)
				{
					AnimatedTexture* animTex = trigger->animTex;
					if (animTex)
					{
						trigger->state = 0;
						trigger->tex = animTex->frameList[0];
					}
					// reactive the trigger.
					trigger->master = JTRUE;
				}
			} break;
			case MSG_MASTER_ON:
			{
				trigger->master = JTRUE;
			} break;
		}
	}

	void elevHandleStopDelay(InfElevator* elev)
	{
		Stop* nextStop = elev->nextStop;
		if ((elev->updateFlags & ELEV_MOVING) && nextStop->delay)
		{
			// Get the position to play the stop sound.
			vec3_fixed pos = inf_getElevSoundPos(elev);

			// Stop the looping middle sound and then play the stop sound.
			stopSound(elev->loopingSoundID);
			elev->loopingSoundID = NULL_SOUND;
			// Play the stop one shot.
			playSound3D_oneshot(elev->sound2, pos);
		}
		// If there is a delay, then the elevator is not moving.
		if (nextStop->delay)
		{
			elev->updateFlags &= ~ELEV_MOVING;
		}
		else
		{
			elev->updateFlags |= ELEV_MOVING;
		}
	}

	Stop* inf_advanceStops(Allocator* stops, s32 absoluteStop, s32 relativeStop)
	{
		Stop* stop = nullptr;
		// advance to a future stop
		if (relativeStop > 0)
		{
			for (; relativeStop > 0; relativeStop--)
			{
				stop = (Stop*)allocator_getNext(stops);
				// Loop around.
				if (!stop)
				{
					stop = (Stop*)allocator_getHead(stops);
				}
			}
		}
		// advance to a previous stop
		else if (relativeStop < 0)
		{
			relativeStop = -relativeStop;
			for (; relativeStop > 0; relativeStop--)
			{
				stop = (Stop*)allocator_getPrev(stops);
				// Loop around.
				if (!stop)
				{
					stop = (Stop*)allocator_getTail(stops);
				}
			}
		}
		// relativeStop == 0
		else if (absoluteStop >= 0)
		{
			// Loop from head instead of the current stop.
			stop = (Stop*)allocator_getHead(stops);
			for (; absoluteStop > 0; absoluteStop--)
			{
				stop = (Stop*)allocator_getNext(stops);
				if (!stop)
				{
					stop = (Stop*)allocator_getHead(stops);
				}
			}
		}
		// relativeStop == 0 && absoluteStop < 0 -- this basically will send us to the last stop.
		else
		{
			stop = (Stop*)allocator_getTail_noIterUpdate(stops);
		}

		return stop;
	}

	void inf_deleteSectorElevatorLink(RSector* sector, InfElevator* elev)
	{
		InfLink* link = (InfLink*)allocator_getHead(sector->infLink);
		while (link)
		{
			if (elev == link->elev)
			{
				allocator_deleteItem(sector->infLink, link);
				return;
			}

			link = (InfLink*)allocator_getNext(sector->infLink);
		}
	}

	void deleteElevator(InfElevator* elev)
	{
		if (elev->slaves)
		{
			allocator_free(elev->slaves);
		}
		// Free the stops.
		if (elev->stops)
		{
			Stop* stop = (Stop*)allocator_getHead(elev->stops);
			while (stop)
			{
				if (stop->messages)
				{
					allocator_free(stop->messages);
				}
				if (stop->adjoinCmds)
				{
					allocator_free(stop->adjoinCmds);
				}
				stop = (Stop*)allocator_getNext(elev->stops);
			}
			allocator_free(elev->stops);
		}
		inf_deleteSectorElevatorLink(elev->sector, elev);
		allocator_deleteItem(s_infElevators, elev);
	}
		
	void deleteTrigger(InfTrigger* trigger)
	{
		InfLink* link = trigger->link;
		allocator_deleteItem(link->parent, link);
		allocator_free(trigger->targets);

		// TODO(Core Game Loop Release): what is trigger->u48?
		if (trigger->u48)
		{
			level_free(trigger->u48);
		}
		level_free(trigger);
		
		s_triggerCount--;
	}

	void inf_stopAdjoinCommands(Stop* stop)
	{
		Allocator* adjoinCmds = stop->adjoinCmds;
		if (adjoinCmds)
		{
			AdjoinCmd* cmd = (AdjoinCmd*)allocator_getHead(adjoinCmds);
			while (cmd)
			{
				RWall* wall0 = cmd->wall0;
				RWall* wall1 = cmd->wall1;
				RSector* sector0 = cmd->sector0;
				RSector* sector1 = cmd->sector1;

				wall0->nextSector = sector1;
				wall0->mirrorWall = wall1;

				wall1->nextSector = sector0;
				wall1->mirrorWall = wall0;

				sector_setupWallDrawFlags(sector0);
				sector_setupWallDrawFlags(sector1);

				cmd = (AdjoinCmd*)allocator_getNext(adjoinCmds);
			}
		}
	}

	// Returns JTRUE if the object is sitting on a moving floor or second height.
	JBool inf_isOnMovingFloor(SecObject* obj, InfElevator* elev, RSector* sector)
	{
		const fixed16_16 secHeight = sector->floorHeight + sector->secHeight;
		if ((obj->posWS.y == sector->floorHeight && (elev->flags&INF_EFLAG_MOVE_FLOOR)) || (obj->posWS.y == secHeight && (elev->flags&INF_EFLAG_MOVE_SECHT)))
		{
			return JTRUE;
		}
		return JFALSE;
	}

	void inf_getMovingElevatorVelocity(InfElevator* elev, vec3_fixed* vel, fixed16_16* _speed)
	{
		vel->x = 0;
		vel->y = 0;
		vel->z = 0;
		*_speed = 0;

		if (elev->updateFlags & ELEV_MOVING)
		{
			fixed16_16 toNext = 1;
			if (elev->nextStop)
			{
				toNext = elev->nextStop->value - *elev->value;
			}
			if (!toNext) { return; }

			fixed16_16 speed = (toNext > 0) ? elev->speed : -elev->speed;
			if (elev->type == IELEV_MOVE_WALL || elev->type == IELEV_SCROLL_FLOOR || elev->type == IELEV_SCROLL_CEILING)
			{
				vel->x = mul16(speed, elev->dirOrCenter.x);
				vel->z = mul16(speed, elev->dirOrCenter.z);
			}
		}
	}

	void inf_sendSectorMessage(RSector* sector, MessageType msgType)
	{
		switch (msgType)
		{
			case MSG_WAKEUP:
			{
				s32 objCount = sector->objectCount;
				SecObject** objList = sector->objectList;

				for (s32 i = 0; i < objCount; objList++)
				{
					SecObject* obj = *objList;
					if (obj)
					{
						if (obj->entityFlags & ETFLAG_CAN_WAKE)
						{
							message_sendToObj(obj, MSG_WAKEUP, nullptr);
						}
						i++;
					}
				}
			}
			// MSG_WAKEUP drops through to MSG_MASTER_ON/MSG_MASTER_OFF
			case MSG_MASTER_ON:
			case MSG_MASTER_OFF:
			{
				s32 objCount = sector->objectCount;
				SecObject** objList = sector->objectList;

				for (s32 i = 0; i < objCount; objList++)
				{
					SecObject* obj = *objList;
					if (obj)
					{
						if (obj->entityFlags & ETFLAG_CAN_DISABLE)
						{
							message_sendToObj(obj, msgType, nullptr);
						}
						i++;
					}
				}
			} break;
			case MSG_SET_BITS:
			{
				u32 flagsIndex = s_msgArg1;
				u32 bits = s_msgArg2;

				if (flagsIndex == 1)
				{
					sector->flags1 |= bits;
				}
				else if (flagsIndex == 2)
				{
					sector->flags2 |= bits;
				}
				else if (flagsIndex == 3)
				{
					sector->flags3 |= bits;
				}
			} break;
			case MSG_CLEAR_BITS:
			{
				u32 flagsIndex = s_msgArg1;
				u32 bits = s_msgArg2;

				if (flagsIndex == 1)
				{
					sector->flags1 &= ~bits;
				}
				else if (flagsIndex == 2)
				{
					sector->flags2 &= ~bits;
				}
				else if (flagsIndex == 3)
				{
					sector->flags3 &= ~bits;
				}
			} break;
		};
	}
		
	void inf_stopHandleMessages(MessageType msg)
	{
		struct LocalContext
		{
			Allocator* msgList;
			Allocator* infLink;
			InfMessage* msg;
			RSector* sector;
			RWall* wall;
			InfLink* link;
		};
		task_begin_ctx;

		taskCtx->msgList = s_nextStop->messages;
		taskCtx->msg = (InfMessage*)allocator_getHead(taskCtx->msgList);
		while (taskCtx->msg)
		{
			s_msgArg1 = taskCtx->msg->arg1;
			s_msgArg2 = taskCtx->msg->arg2;
			taskCtx->wall = taskCtx->msg->wall;
			if (taskCtx->wall)
			{
				inf_wallSendMessage(taskCtx->wall, nullptr, taskCtx->msg->event, taskCtx->msg->msgType);
			}
			else if (taskCtx->msg->sector)
			{
				taskCtx->sector = taskCtx->msg->sector;
				taskCtx->infLink = taskCtx->sector->infLink;
				if (taskCtx->infLink)
				{
					taskCtx->link = (InfLink*)allocator_getHead(taskCtx->infLink);
					while (taskCtx->link)
					{
						if (taskCtx->link->task && (taskCtx->msg->event <= 0 || (taskCtx->link->eventMask & taskCtx->msg->event)))
						{
							allocator_addRef(taskCtx->msgList);

							s_msgTarget = taskCtx->link->target;
							s_msgEntity = nullptr;
							s_msgArg1 = taskCtx->msg->arg1;
							s_msgArg2 = taskCtx->msg->arg2;
							s_msgEvent = taskCtx->msg->event;
							inf_sendSectorMessage(taskCtx->sector, taskCtx->msg->msgType);
							task_runLocal(taskCtx->link->task, taskCtx->msg->msgType);

							if (msg != MSG_RUN_TASK)
							{
								infElevatorMessageInternal(msg);
								task_yield(TASK_NO_DELAY);
							}

							allocator_release(taskCtx->msgList);
						}
						taskCtx->link = (InfLink*)allocator_getNext(taskCtx->infLink);
					}
				}
				else  // infLink
				{
					message_sendToSector(taskCtx->sector, nullptr, taskCtx->msg->event, taskCtx->msg->msgType);
				}
			}
			else  // world
			{
				// In the original game, this will call a specific function based on the type
				// But the only type is MSG_LIGHTS
				if (taskCtx->msg->msgType == MSG_LIGHTS)
				{
					inf_handleMsgLights();
				}
			}

			taskCtx->msg = (InfMessage*)allocator_getNext(taskCtx->msgList);
		} // while (msg)

		task_end;
	}
		
	void inf_handleMsgLights()
	{
		RSector* sector = s_sectors;
		for (u32 i = 0; i < s_sectorCount; i++, sector++)
		{
			fixed16_16 newAmbient = intToFixed16(sector->flags3);
			// Store the old value in flags3 so the lights can be toggled.
			sector->flags3 = floor16(sector->ambient);
			sector->ambient = newAmbient;
		}
	}

	vec3_fixed inf_getElevSoundPos(InfElevator* elev)
	{
		vec3_fixed pos;
		RSector* sector = elev->sector;

		// Figure out the source position for the sound effect.
		pos.y = sector->secHeight + sector->floorHeight;
		if (elev->type != IELEV_ROTATE_WALL)
		{
			// First vertex position.
			vec2_fixed* vtx = (vec2_fixed*)sector->verticesWS;
			pos.x = vtx->x;
			pos.z = vtx->z;
		}
		else
		{
			// Rotation center.
			pos.x = elev->dirOrCenter.x;
			pos.z = elev->dirOrCenter.z;
		}

		return pos;
	}

	///////////////////////////////////////////
	// Trigger creation and setup
	///////////////////////////////////////////
	void inf_triggerFreeFunc(void* data)
	{
		// TODO(Core Game Loop Release)
	}

	// Creates a trigger and adds a link to the sector or wall where it is located.
	// For example, if a player "nudges" a wall, then all of the wall's links will be
	// cycled through, calling the "msgFunc" on each to determine what happens, which than
	// sends the message to the appropriate "InfTrigger" or "InfElevator"
	InfTrigger* inf_createTrigger(TriggerType type, InfTriggerObject obj)
	{
		InfTrigger* trigger = (InfTrigger*)level_alloc(sizeof(InfTrigger));
		memset(trigger, 0, sizeof(InfTrigger));
		s_triggerCount++;

		InfLink* link = nullptr;
		trigger->soundId = NULL_SOUND;
		trigger->targets = allocator_create(sizeof(TriggerTarget));

		switch (type)
		{
			case ITRIGGER_WALL:
			{
				RWall* wall = obj.wall;
				if (!wall->infLink)
				{
					wall->infLink = allocator_create(sizeof(InfLink));
				}
				link = (InfLink*)allocator_newItem(wall->infLink);
				link->type = LTYPE_TRIGGER;
				link->entityMask = INF_ENTITY_PLAYER;
				link->eventMask = INF_EVENT_ANY;
				link->trigger = trigger;
				link->task = s_infTriggerTask;
				link->parent = wall->infLink;
			} break;
			case ITRIGGER_SECTOR:
			{
				RSector* sector = obj.sector;
				if (!sector->infLink)
				{
					sector->infLink = allocator_create(sizeof(InfLink));
				}
				link = (InfLink*)allocator_newItem(sector->infLink);
				link->type = LTYPE_TRIGGER;
				link->entityMask = INF_ENTITY_PLAYER;
				link->eventMask = INF_EVENT_ANY;
				link->trigger = trigger;
				link->task = s_infTriggerTask;
				link->parent = sector->infLink;
			} break;
			case ITRIGGER_SWITCH1:
			case ITRIGGER_TOGGLE:
			case ITRIGGER_SINGLE:
			{
				RWall* wall = obj.wall;
				trigger->soundId = s_switchDefaultSndId;
				if (!wall->infLink)
				{
					wall->infLink = allocator_create(sizeof(InfLink));
				}
				link = (InfLink*)allocator_newItem(wall->infLink);
				link->type = LTYPE_TRIGGER;
				link->entityMask = INF_ENTITY_PLAYER;
				link->eventMask = INF_EVENT_ANY;
				link->task = s_infTriggerTask;
				link->trigger = trigger;
				link->parent = wall->infLink;
				trigger->animTex = nullptr;

				// Handle trigger sign texture.
				TextureData** signTex = wall->signTex;
				if (signTex && (*signTex)->uvWidth == BM_ANIMATED_TEXTURE)
				{
					// In this case, image points to the AnimatedTexture rather than the actual image.
					AnimatedTexture* animTex = (AnimatedTexture*)(*signTex)->image;
					trigger->animTex = animTex;
					trigger->tex = animTex->frameList[0];
					wall->signTex = &trigger->tex;
					// Removes "cross line" events
					link->eventMask &= ~(INF_EVENT_CROSS_LINE_FRONT | INF_EVENT_CROSS_LINE_BACK);
				}
			} break;
		}
		trigger->cmd = MSG_TRIGGER;
		trigger->event  = 0;
		trigger->arg1   = 0;
		trigger->u30    = JTRUE;
		trigger->u34    = 21;
		trigger->master = JTRUE;
		trigger->state  = 0;
		trigger->u48    = nullptr;
		trigger->textId = 0;
		trigger->link   = link;
		trigger->type   = type;
		link->freeFunc  = inf_triggerFreeFunc;

		return trigger;
	}


	///////////////////////////////////////////////////
	// Elevator Update Functions
	///////////////////////////////////////////////////
	fixed16_16 infUpdate_moveHeights(InfElevator* elev, fixed16_16 delta)
	{
		RSector* sector = elev->sector;
		fixed16_16 secHeightDelta = 0;
		fixed16_16 ceilDelta = 0;
		fixed16_16 floorDelta = 0;

		switch (elev->type)
		{
		case IELEV_MOVE_CEILING:
			ceilDelta = delta;
			break;
		case IELEV_MOVE_FLOOR:
			floorDelta = delta;
			break;
		case IELEV_MOVE_OFFSET:
			secHeightDelta = delta;
			break;
		case IELEV_MOVE_FC:
			floorDelta = delta;
			ceilDelta = delta;
			break;
		}

		fixed16_16 maxObjHeight = sector_getMaxObjectHeight(sector);
		if (maxObjHeight)
		{
			maxObjHeight += 0x4000;	// maxObjHeight + 0.25
			if (secHeightDelta)
			{
				fixed16_16 height = sector->floorHeight + sector->secHeight;
				fixed16_16 maxObjHeightAbove = 0;
				fixed16_16 maxObjectHeightBelow = 0;

				// Remove dead objects.
				sector_removeCorpses(sector);

				s32 objCount = sector->objectCount;
				SecObject** objList = sector->objectList;
				for (; objCount > 0; objList++)
				{
					SecObject* obj = *objList;
					if (!obj)
					{
						continue;
					}

					objCount--;
					fixed16_16 objHeight = obj->worldHeight + ONE_16;
					// Object is below the second height
					if (obj->posWS.y > height)
					{
						if (objHeight > maxObjectHeightBelow)
						{
							maxObjectHeightBelow = objHeight;
						}
					}
					// Object is on or above the second height
					else
					{
						if (objHeight > maxObjHeightAbove)
						{
							maxObjHeightAbove = objHeight;
						}
					}
				}

				// The new second height after adjustment (only second height so far).
				height += secHeightDelta;
				// Difference between the base floor height and the adjusted second height.
				fixed16_16 floorDelta = sector->floorHeight - (height + ONE_16);
				// Difference betwen the adjusted second height and the ceiling.
				fixed16_16 ceilDelta = height - sector->ceilingHeight;
				if (floorDelta < maxObjectHeightBelow || ceilDelta < maxObjHeightAbove)
				{
					// If there are objects in the way, set the next stop as the previous.
					elev->nextStop = inf_advanceStops(elev->stops, 0, -1);
					floorDelta = 0;
					secHeightDelta = 0;
					ceilDelta = 0;
				}
			}
			else
			{
				fixed16_16 floorHeight = sector->floorHeight + floorDelta;
				fixed16_16 ceilHeight = sector->ceilingHeight + ceilDelta;
				fixed16_16 height = floorHeight - ceilHeight;
				if (height < maxObjHeight)
				{
					// Remove dead objects.
					sector_removeCorpses(sector);

					// Not sure why it needs to check again...
					fixed16_16 maxObjHeight = sector_getMaxObjectHeight(sector);
					fixed16_16 spaceNeeded = maxObjHeight + 0x4000;	// maxObjHeight + 0.25
					floorHeight = sector->floorHeight + floorDelta;
					ceilHeight = sector->ceilingHeight + ceilDelta;
					fixed16_16 spaceAvail = floorHeight - ceilHeight;

					// If the height between floor and ceiling is too small for the tallest object AND
					// If the floor is moving up or the ceiling is moving down and this is NOT a crushing sector.
					if (maxObjHeight > 0 && spaceAvail < spaceNeeded && (floorDelta < 0 || ceilDelta > 0) && !(sector->flags1 & SEC_FLAGS1_CRUSHING))
					{
						Stop* stop = inf_advanceStops(elev->stops, 0, -1);
						floorDelta = 0;
						secHeightDelta = 0;
						ceilDelta = 0;
						elev->nextStop = stop;
					}
				}
			}
		}

		if (floorDelta)
		{
			sector_adjustTextureWallOffsets_Floor(sector, floorDelta);
			sector_adjustTextureMirrorOffsets_Floor(sector, floorDelta);
		}
		sector_adjustHeights(sector, floorDelta, ceilDelta, secHeightDelta);

		// Apply adjustments to "slave" sectors.
		Slave* child = (Slave*)allocator_getHead(elev->slaves);
		while (child)
		{
			if (floorDelta)
			{
				sector_adjustTextureWallOffsets_Floor(child->sector, floorDelta);
				sector_adjustTextureMirrorOffsets_Floor(child->sector, floorDelta);
			}
			sector_adjustHeights(child->sector, floorDelta, ceilDelta, secHeightDelta);
			child = (Slave*)allocator_getNext(elev->slaves);
		}
		return *elev->value;
	}

	fixed16_16 infUpdate_moveWall(InfElevator* elev, fixed16_16 delta)
	{
		// First attempt to move walls in the sector.
		if (!sector_moveWalls(elev->sector, delta, elev->dirOrCenter.x, elev->dirOrCenter.z, elev->flags))
		{
			return elev->iValue;
		}

		// If successful move its slaves.
		Slave* child = (Slave*)allocator_getHead(elev->slaves);
		while (child)
		{
			sector_moveWalls(child->sector, delta, elev->dirOrCenter.x, elev->dirOrCenter.z, elev->flags);
			child = (Slave*)allocator_getNext(elev->slaves);
		}

		// Finally update the elevator value.
		elev->iValue += delta;
		return elev->iValue;
	}

	fixed16_16 infUpdate_rotateWall(InfElevator* elev, fixed16_16 delta)
	{
		RSector* sector = elev->sector;
		sector->dirtyFlags |= SDF_VERTICES;

		fixed16_16 deltaRounded = (delta > 0) ? fixed16_16((delta + HALF_16) & 0xffff0000) : -fixed16_16((HALF_16 - delta) & 0xffff0000);
		fixed16_16 angle = elev->iValue + delta;
		fixed16_16 centerX = elev->dirOrCenter.x;
		fixed16_16 centerZ = elev->dirOrCenter.z;
		angle14_32 angleInt = floor16(angle);
		if (!sector_canRotateWalls(sector, angleInt, centerX, centerZ))
		{
			return elev->iValue;
		}

		Slave* child = (Slave*)allocator_getHead(elev->slaves);
		while (child)
		{
			// The value in slave can be used as an angular offset.
			s32 childAngle = fract16(child->value) + angleInt;
			sector = child->sector;
			if (!sector_canRotateWalls(child->sector, childAngle, centerX, centerZ))
			{
				return elev->iValue;
			}
			child = (Slave*)allocator_getNext(elev->slaves);
		}

		sector_rotateWalls(elev->sector, centerX, centerZ, angleInt);

		angle14_32 deltaInt = floor16(deltaRounded);
		sector_rotateObjects(elev->sector, deltaInt, centerX, centerZ, elev->flags);

		child = (Slave*)allocator_getHead(elev->slaves);
		while (child)
		{
			s32 childAngle = (angle >> 16) + (child->value & 0xffff);
			sector_rotateWalls(child->sector, centerX, centerZ, childAngle);
			sector_rotateObjects(child->sector, deltaInt, centerX, centerZ, elev->flags);

			child = (Slave*)allocator_getNext(elev->slaves);
		}
		elev->iValue = angle;
		return elev->iValue;
	}

	fixed16_16 infUpdate_scrollWall(InfElevator* elev, fixed16_16 delta)
	{
		elev->iValue += delta;
		fixed16_16 deltaX = mul16(delta, elev->dirOrCenter.x);
		fixed16_16 deltaZ = mul16(delta, elev->dirOrCenter.z);
		sector_scrollWalls(elev->sector, deltaX, deltaZ);

		Slave* child = (Slave*)allocator_getHead(elev->slaves);
		while (child)
		{
			sector_scrollWalls(child->sector, deltaX, deltaZ);
			child = (Slave*)allocator_getNext(elev->slaves);
		}
		return elev->iValue;
	}

	fixed16_16 infUpdate_scrollFlat(InfElevator* elev, fixed16_16 delta)
	{
		elev->iValue += delta;
		fixed16_16 deltaX = mul16(delta, elev->dirOrCenter.x);
		fixed16_16 deltaZ = mul16(delta, elev->dirOrCenter.z);

		RSector* sector = elev->sector;
		sector->dirtyFlags |= SDF_FLAT_OFFSETS;
		if (elev->type == IELEV_SCROLL_FLOOR)
		{
			sector->floorOffset.x += deltaX;
			sector->floorOffset.z += deltaZ;
		}
		else if (elev->type == IELEV_SCROLL_CEILING)
		{
			sector->ceilOffset.x += deltaX;
			sector->ceilOffset.z += deltaZ;
		}

		Slave* child = (Slave*)allocator_getHead(elev->slaves);
		while (child)
		{
			sector = child->sector;
			sector->dirtyFlags |= SDF_FLAT_OFFSETS;
			if (elev->type == IELEV_SCROLL_FLOOR)
			{
				sector->floorOffset.x += deltaX;
				sector->floorOffset.z += deltaZ;
			}
			else if (elev->type == IELEV_SCROLL_CEILING)
			{
				sector->ceilOffset.x += deltaX;
				sector->ceilOffset.z += deltaZ;
			}
			child = (Slave*)allocator_getNext(elev->slaves);
		}

		return elev->iValue;
	}

	fixed16_16 infUpdate_changeAmbient(InfElevator* elev, fixed16_16 delta)
	{
		RSector* sector = elev->sector;
		sector->ambient += delta;

		Slave* child = (Slave*)allocator_getHead(elev->slaves);
		while (child)
		{
			child->sector->ambient += delta;
			child = (Slave*)allocator_getNext(elev->slaves);
		}
		return sector->ambient;
	}
		
	fixed16_16 infUpdate_changeWallLight(InfElevator* elev, fixed16_16 delta)
	{
		elev->iValue += delta;
		sector_changeWallLight(elev->sector, delta);

		Slave* child = (Slave*)allocator_getHead(elev->slaves);
		while (child)
		{
			sector_changeWallLight(child->sector, delta);
			child = (Slave*)allocator_getNext(elev->slaves);
		}
		return elev->iValue;
	}

	JBool sector_isDoor(RSector* sector)
	{
		if (!sector) { return JFALSE; }

		if (sector->flags1 & SEC_FLAGS1_DOOR)
		{
			return JTRUE;
		}
		else
		{
			InfLink* link = (InfLink*)allocator_getHead(sector->infLink);
			if (link && link->type == LTYPE_SECTOR)
			{
				InfElevator* elev = link->elev;
				InfElevatorType type = elev->type;
				if (type == IELEV_MOVE_CEILING || type == IELEV_MOVE_FLOOR || type == IELEV_MOVE_WALL || type == IELEV_ROTATE_WALL || type == IELEV_MOVE_FC)
				{
					return JTRUE;
				}
			}
		}
		return JFALSE;
	}
}
