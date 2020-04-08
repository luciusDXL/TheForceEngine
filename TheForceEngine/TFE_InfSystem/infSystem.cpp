#include "infSystem.h"
#include <TFE_System/system.h>
#include <TFE_System/memoryPool.h>
#include <TFE_System/math.h>
#include <TFE_Game/level.h>
#include <TFE_Game/physics.h>
#include <TFE_Game/player.h>
#include <TFE_Game/renderCommon.h>
#include <TFE_Game/gameHud.h>
#include <TFE_Game/gameConstants.h>
#include <TFE_Game/geometry.h>
#include <TFE_Game/GameUI/gameUi.h>
#include <TFE_Audio/audioSystem.h>
#include <TFE_Asset/gameMessages.h>
#include <TFE_Asset/vocAsset.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <algorithm>

using namespace TFE_GameConstants;

namespace TFE_InfSystem
{
	// Allocate 1 Mb. The largest MOD level uses around 32Kb @ 2K sectors. The engine supports 32K sectors, so if we assume
	// this can be roughly 16x larger - this gives about 0.5Mb. So bumping it up to 1Mb just to be sure.
#define TFE_RUNTIME_INF_POOL (1 * 1024 * 1024)
#define MAX_FUNC_IN_FLIGHT 1024

	struct FuncQueue
	{
		u32 funcCount;
		u32 evt;
		InfFunction* func;
	};
	
	const f32 c_lightSpeedScale = 3.2f;
	static u32 s_frame = 0;

	static u32 s_funcQueueCount = 0;
	static FuncQueue s_funcQueue[MAX_FUNC_IN_FLIGHT];

	enum InfState
	{
		INF_STATE_MOVING = 0,	// Moving to the next stop.
		INF_STATE_WAITING,		// Waiting for a timer to tick down before continuing.
		INF_STATE_HOLDING,		// Holding or waiting to be triggered.
		INF_STATE_TERMINATED,	// Done, nothing more to do here.
		INF_STATE_ACTIVATED,	// Already activated but not terminated (trigger).
		INT_STATE_COUNT
	};

	enum NudgeType
	{
		NUDGE_NONE = 0,
		NUDGE_SET,
		NUDGE_TOGGLE,
	};

	// Runtime state for each INF item.
	struct ItemSlaveState
	{
		f32 initState[2];
		f32 curValue;
	};

	struct ItemState
	{
		InfState state;
		f32 initState[2];	// initial floor height, ceiling height, etc.
		f32 curValue;		// current value.
		// Value state for slaves.
		ItemSlaveState* slaveState;
		SoundSource* moveSound;

		s32 curStop;
		s32 nextStop;
		f32 delay;
	};

	struct SectorLineId
	{
		u32 wallId;	// sector wall
		u32 itemId; // Item ID
	};

	struct SectorItems
	{
		s32 sectorItemId;	// sector Item ID (elevators, etc.), may be -1.
		u32 lineCount;		// line based items in this sector.
		SectorLineId* lineItemId;	// list of line item ids.
	};

	static MemoryPool* s_memoryPool;

	static InfData* s_infData;
	static LevelData* s_levelData;
	static std::vector<u8> s_buffer;

	static f32 s_accum = 0.0f;
	static ItemState* s_infState;
	static SectorItems* s_sectorItemMap;
	static u32 s_stateCount;

	static bool s_useVertexCache;

	Sector* getSlaveSector(const InfClassData* classData, u32 index);
	void executeFunctions(u32 funcCount, InfFunction* func, u32 evt = 0);
	NudgeType activateLineOrSector(InfClassData* classData);

	void startMovingSound(Sector* sector, const InfClassData* classData);
	void stopMovingSound(Sector* sector, const InfClassData* classData);

	bool init()
	{
		TFE_System::logWrite(LOG_MSG, "Startup", "TFE_InfSystem::init");

		s_infData = nullptr;
		s_levelData = nullptr;

		s_memoryPool = new MemoryPool();
		s_memoryPool->init(TFE_RUNTIME_INF_POOL, "Inf_Runtime_Memory");
		// Set the warning watermark at 75% capacity.
		s_memoryPool->setWarningWatermark(TFE_RUNTIME_INF_POOL * 3 / 4);

		return true;
	}

	void shutdown()
	{
		delete s_memoryPool;
		s_memoryPool = nullptr;
	}

	Sector* getSlaveSector(const InfClassData* classData, u32 index)
	{
		return s_levelData->sectors.data() + classData->slaves[index];
	}

	void executeFunc(u32 type, u32 argCount, InfArg* arg, u32 sectorId, u32 wallId, u32 evt)
	{
		s32 itemId = -1;
		if (sectorId < 0xffffu)
		{
			// Find the correct item to target.
			if (wallId < 0xffffu)
			{
				for (u32 i = 0; i < s_sectorItemMap[sectorId].lineCount; i++)
				{
					if (s_sectorItemMap[sectorId].lineItemId[i].wallId == wallId)
					{
						itemId = s_sectorItemMap[sectorId].lineItemId[i].itemId;
						break;
					}
				}
			}
			else
			{
				itemId = s_sectorItemMap[sectorId].sectorItemId;
			}
		}

		InfItem* item = itemId >= 0 ? &s_infData->item[itemId] : nullptr;
		const u32 classCount = item ? item->classCount : 0;

		switch (type)
		{
		case INF_MSG_M_TRIGGER:
			for (u32 c = 0; c < classCount; c++)
			{
				InfClassData* classData = &item->classData[c];
				// If the class has been turned off, then skip.
				if (!classData->var.master) { continue; }

				// Optional event mask.
				if (argCount > 0 && arg[0].iValue != 0 && !(classData->var.event_mask & arg[0].iValue)) { continue; }
				if (evt != 0 && !(classData->var.event_mask & evt)) { continue; }

				// And finally activate - but only if it is going "holding" and waiting to be activated.
				ItemState* itemState = &s_infState[classData->stateIndex];
				if (classData->iclass == INF_CLASS_ELEVATOR && itemState->state == INF_STATE_HOLDING)
				{
					itemState->state = INF_STATE_MOVING;
					itemState->nextStop = (itemState->curStop + 1) % classData->stopCount;
				}
				else if (classData->iclass == INF_CLASS_TRIGGER && itemState->state == INF_STATE_HOLDING)
				{
					if (classData->isubclass == TRIGGER_SINGLE)
					{
						itemState->state = INF_STATE_TERMINATED;
					}
					else if (classData->isubclass == TRIGGER_SWITCH1)
					{
						itemState->state = INF_STATE_ACTIVATED;
					}

					if (classData->isubclass == TRIGGER_TOGGLE)
					{
						TFE_Level::toggleTextureFrame(sectorId, SP_SIGN, WSP_NONE, wallId);
					}
					else if (classData->isubclass == TRIGGER_SINGLE || classData->isubclass == TRIGGER_SWITCH1)
					{
						TFE_Level::setTextureFrame(sectorId, SP_SIGN, WSP_NONE, 1, wallId);
					}
				}
			}
			break;
		case INF_MSG_GOTO_STOP:
			if (argCount >= 1 && arg)
			{
				for (u32 c = 0; c < classCount; c++)
				{
					InfClassData* classData = &item->classData[c];
					// If the class has been turned off, then skip.
					if (!classData->var.master) { continue; }

					// And finally activate - but only if it is going "holding" and waiting to be activated.
					ItemState* itemState = &s_infState[classData->stateIndex];
					if (classData->iclass == INF_CLASS_ELEVATOR && itemState->state == INF_STATE_HOLDING)
					{
						itemState->state = INF_STATE_MOVING;
						itemState->nextStop = arg[0].iValue;
					}
				}
			}
			else
			{
				TFE_System::logWrite(LOG_ERROR, "INF", "Function goto_stop() called without enough arguments, %u supplied but 1 is required. Target sector \"%s\"",
					s_levelData->sectors[sectorId].name);
			}
			break;
		case INF_MSG_NEXT_STOP:
			for (u32 c = 0; c < classCount; c++)
			{
				InfClassData* classData = &item->classData[c];
				// If the class has been turned off, then skip.
				if (!classData->var.master) { continue; }

				// Optional event mask.
				if (argCount > 0 && arg[0].iValue != 0 && !(classData->var.event_mask & arg[0].iValue)) { continue; }
				if (evt != 0 && !(classData->var.event_mask & evt)) { continue; }

				// And finally activate - but only if it is going "holding" and waiting to be activated.
				ItemState* itemState = &s_infState[classData->stateIndex];
				if (classData->iclass == INF_CLASS_ELEVATOR && itemState->state == INF_STATE_HOLDING)
				{
					itemState->state = INF_STATE_MOVING;
					itemState->nextStop = (itemState->curStop + 1) % classData->stopCount;
				}
			}
			break;
		case INF_MSG_PREV_STOP:
			for (u32 c = 0; c < classCount; c++)
			{
				InfClassData* classData = &item->classData[c];
				// If the class has been turned off, then skip.
				if (!classData->var.master) { continue; }

				// Optional event mask.
				if (argCount > 0 && arg[0].iValue != 0 && !(classData->var.event_mask & arg[0].iValue)) { continue; }
				if (evt != 0 && !(classData->var.event_mask & evt)) { continue; }

				// And finally activate - but only if it is going "holding" and waiting to be activated.
				ItemState* itemState = &s_infState[classData->stateIndex];
				if (classData->iclass == INF_CLASS_ELEVATOR && itemState->state == INF_STATE_HOLDING)
				{
					itemState->state = INF_STATE_MOVING;
					itemState->nextStop = itemState->curStop > 0 ? itemState->curStop - 1 : classData->stopCount - 1;
				}
			}
			break;
		case INF_MSG_MASTER_ON:
			for (u32 c = 0; c < classCount; c++)
			{
				InfClassData* classData = &item->classData[c];

				// Optional event mask.
				if (argCount > 0 && arg[0].iValue != 0 && !(classData->var.event_mask & arg[0].iValue)) { continue; }
				if (evt != 0 && !(classData->var.event_mask & evt)) { continue; }

				classData->var.master = true;
			}
			break;
		case INF_MSG_MASTER_OFF:
			for (u32 c = 0; c < classCount; c++)
			{
				InfClassData* classData = &item->classData[c];

				// Optional event mask.
				if (argCount > 0 && arg[0].iValue != 0 && !(classData->var.event_mask & arg[0].iValue)) { continue; }
				if (evt != 0 && !(classData->var.event_mask & evt)) { continue; }

				ItemState* state = &s_infState[classData->stateIndex];
				if (state->moveSound)
				{
					TFE_Audio::freeSource(state->moveSound);
					state->moveSound = nullptr;
				}

				classData->var.master = false;
			}
			break;
		case INF_MSG_CLEAR_BITS:
			if (argCount >= 2 && arg)
			{
				const s32 flagIdx = arg[0].iValue - 1;
				const s32 bits = arg[1].iValue;

				if (wallId < 0xffffu)
				{
					TFE_Level::clearFlagBits(sectorId, SP_WALL, flagIdx, bits, wallId);
				}
				else
				{
					TFE_Level::clearFlagBits(sectorId, SP_SECTOR, flagIdx, bits);
				}
			}
			else
			{
				TFE_System::logWrite(LOG_ERROR, "INF", "Function clearBits() called without enough arguments, %u supplied but 2 are required. Target \"%s\"(%d)",
					argCount, s_levelData->sectors[sectorId].name, wallId < 0xffffu ? wallId : -1);
			}
			break;
		case INF_MSG_SET_BITS:
			if (argCount >= 2 && arg)
			{
				const s32 flagIdx = arg[0].iValue - 1;
				const s32 bits = arg[1].iValue;

				if (wallId < 0xffffu)
				{
					TFE_Level::setFlagBits(sectorId, SP_WALL, flagIdx, bits, wallId);
				}
				else
				{
					TFE_Level::setFlagBits(sectorId, SP_SECTOR, flagIdx, bits);
				}
			}
			else
			{
				TFE_System::logWrite(LOG_ERROR, "INF", "Function setBits() called without enough arguments, %u supplied but 2 are required. Target \"%s\"(%d)",
					argCount, s_levelData->sectors[sectorId].name, wallId < 0xffffu ? wallId : -1);
			}
			break;
		case INF_MSG_COMPLETE:
			{
				// Parameter determines the GOL being completed.
				const s32 goalId = argCount >= 1 ? arg[0].iValue : 0;
				// TODO: Update player goals with goalId.

				// Move the recipient elevator to its next stop.
				for (u32 c = 0; c < classCount; c++)
				{
					InfClassData* classData = &item->classData[c];
					// If the class has been turned off, then skip.
					if (!classData->var.master) { continue; }

					// Optional event mask.
					if (evt != 0 && !(classData->var.event_mask & evt)) { continue; }

					// And finally activate - but only if it is going "holding" and waiting to be activated.
					ItemState* itemState = &s_infState[classData->stateIndex];
					if (classData->iclass == INF_CLASS_ELEVATOR && itemState->state == INF_STATE_HOLDING)
					{
						itemState->state = INF_STATE_MOVING;
						itemState->nextStop = (itemState->curStop + 1) % classData->stopCount;
					}
				}
			}
			break;
		case INF_MSG_DONE:
			for (u32 c = 0; c < classCount; c++)
			{
				InfClassData* classData = &item->classData[c];
				if (!classData->var.master) { continue; }

				if (classData->iclass == INF_CLASS_TRIGGER && classData->isubclass != TRIGGER_SINGLE && s_infState[classData->stateIndex].state == INF_STATE_ACTIVATED)
				{
					s_infState[classData->stateIndex].state = INF_STATE_HOLDING;

					if (wallId < 0xffffu && classData->isubclass == TRIGGER_SWITCH1)
					{
						TFE_Level::setTextureFrame(sectorId, SP_SIGN, WSP_NONE, 0, wallId);
					}
				}
			}
			break;
		case INF_MSG_WAKEUP:
			TFE_Level::wakeUp(sectorId);
			break;
		case INF_MSG_LIGHTS:
			TFE_Level::turnOnTheLights();
			break;
		case INF_MSG_ADJOIN:
			if (argCount >= 4 && arg)
			{
				TFE_Level::changeAdjoin(arg[0].iValue, arg[1].iValue, arg[2].iValue, arg[3].iValue);
			}
			else
			{
				TFE_System::logWrite(LOG_ERROR, "INF", "Function adjoin() called without enough arguments, %u supplied but 4 are required.");
			}
			break;
		case INF_MSG_PAGE:
			TFE_Audio::playOneShot(SOUND_2D, 1.0f, MONO_SEPERATION, TFE_VocAsset::getFromIndex(arg[0].iValue), false);
			break;
		case INF_MSG_TEXT:
			if (argCount >= 1 && arg)
			{
				TFE_GameHud::setMessage(TFE_GameMessages::getMessage(arg[0].iValue));
			}
			else
			{
				TFE_System::logWrite(LOG_ERROR, "INF", "Function text() called without enough arguments, %u supplied but 1 is required.");
			}
			break;
		case INF_MSG_TEXTURE:
			if (argCount >= 2 && arg)
			{
				const SectorPart part = arg[0].iValue == 0 ? SP_FLOOR : SP_CEILING;
				const u32 donorTexId = TFE_Level::getTextureId(arg[1].iValue, part, WSP_NONE);
				if (sectorId < 0xffffu)
				{
					TFE_Level::setTextureId(sectorId, part, WSP_NONE, donorTexId);
				}
				else
				{
					TFE_System::logWrite(LOG_ERROR, "INF", "Function texture() called on invalid sector.");
				}
			}
			else
			{
				TFE_System::logWrite(LOG_ERROR, "INF", "Function texture() called without enough arguments, %u supplied but 2 are required. Target sector \"%s\"",
					s_levelData->sectors[sectorId].name);
			}
			break;
		}
	}

	void executeFunctions(u32 funcCount, InfFunction* func, u32 evt/*=0*/)
	{
		for (u32 f = 0; f < funcCount; f++)
		{
			const u32 type = func[f].code & 255;
			const u32 clientCount = (func[f].code >> 8u) & 255;
			const u32 argCount = (func[f].code >> 16u) & 255;

			for (u32 c = 0; c < clientCount; c++)
			{
				const u32 clientId = func[f].client[c];
				const u32 sectorId = clientId & 0xffffu;
				const u32 wallId = (clientId >> 16u) & 0xffffu;

				executeFunc(type, argCount, func[f].arg, sectorId, wallId, evt);
			}
			if (clientCount < 1)
			{
				executeFunc(type, argCount, func[f].arg, 0xffffu, 0xffffu, evt);
			}
		}
	}

	// Add a function to get the stop value in absolute terms.
	f32 getStopValue(const InfClassData* classData, ItemState* curState, Sector* sector, s32 stopId, s32 slaveIndex)
	{
		if (!sector) { return 0.0f; }

		const InfStop* stop = &classData->stop[stopId];
		InfStopValue0Type value0Type = InfStopValue0Type(stop->code & 15);
		InfStopValue1Type value1Type = InfStopValue1Type((stop->code >> 4) & 15);

		const InfSubClass isubclass = (InfSubClass)classData->isubclass;

		f32 value = 0.0f;
		f32 init0 = slaveIndex >= 0 ? curState->slaveState[slaveIndex].initState[0] : curState->initState[0];

		// Elevators that move the floor and/or ceiling.
		if (isubclass == ELEVATOR_MOVE_OFFSET)
		{
			if (value0Type == INF_STOP0_ABSOLUTE)
				value = stop->value0.fValue;
			else if (value0Type == INF_STOP0_RELATIVE)
				value = (init0 + stop->value0.fValue);
			else if (stop->value0.iValue >= 0)
				value = TFE_Level::getFloorHeight(stop->value0.iValue);
		}
		else if (isubclass == ELEVATOR_DOOR || isubclass == ELEVATOR_INV || isubclass == ELEVATOR_MOVE_CEILING ||
			isubclass == ELEVATOR_BASIC || isubclass == ELEVATOR_MOVE_FLOOR || isubclass == ELEVATOR_BASIC_AUTO || isubclass == ELEVATOR_DOOR_INV ||
			isubclass == ELEVATOR_MOVE_FC || isubclass == ELEVATOR_DOOR_MID)
		{
			// Does move_offset use the floorAlt for relative and sectorname?
			if (value0Type == INF_STOP0_ABSOLUTE)
				value = -stop->value0.fValue;
			else if (value0Type == INF_STOP0_RELATIVE)
				value = (init0 - stop->value0.fValue);
			else if (stop->value0.iValue >= 0)
				value = TFE_Level::getFloorHeight(stop->value0.iValue);
		}
		else if (isubclass == ELEVATOR_CHANGE_LIGHT)
		{
			if (value0Type == INF_STOP0_ABSOLUTE)
				value = stop->value0.fValue;
			else if (value0Type == INF_STOP0_RELATIVE)
				value = (init0 + stop->value0.fValue);
			else if (stop->value0.iValue >= 0)
				value = TFE_Level::getAmbient(stop->value0.iValue);
		}
		else if (isubclass == ELEVATOR_MORPH_SPIN1 || isubclass == ELEVATOR_MORPH_SPIN2 || isubclass == ELEVATOR_ROTATE_WALL ||
			isubclass == ELEVATOR_MORPH_MOVE1 || isubclass == ELEVATOR_MORPH_MOVE2 || isubclass == ELEVATOR_MOVE_WALL ||
			isubclass == ELEVATOR_SCROLL_FLOOR || isubclass == ELEVATOR_SCROLL_CEILING || isubclass == ELEVATOR_SCROLL_WALL)
		{
			if (value0Type == INF_STOP0_ABSOLUTE)
				value = stop->value0.fValue;
			else if (value0Type == INF_STOP0_RELATIVE)
				value = (init0 + stop->value0.fValue);
			else if (stop->value0.iValue >= 0)
			{
				value = 0.0f;
				TFE_System::logWrite(LOG_ERROR, "INF", "Sectorname used with morphing or scrolling elevator, results unknown.");
			}
		}
		return value;
	}

	void applyValueToSector(const InfClassData* classData, const ItemState* curState, u32 sectorId, f32 value, f32 valueDelta, s32 slaveIndex)
	{
		// For some reason scroll slaves don't cause objects to move but slave rotation/move does...
		const bool applyMove = slaveIndex < 0 || (classData->mergeStart >= 0 && slaveIndex >= classData->mergeStart)
			|| classData->isubclass != ELEVATOR_SCROLL_FLOOR;
		const bool moveFloor  = applyMove && (classData->var.flags & INF_MOVE_FLOOR);
		const bool moveSecAlt = applyMove && (classData->var.flags & INF_MOVE_SECALT);

		// interpret the value based on the elevator type.
		switch (classData->isubclass)
		{
		case ELEVATOR_DOOR:
		case ELEVATOR_INV:
		case ELEVATOR_MOVE_CEILING:
			TFE_Level::setCeilingHeight(sectorId, value);
			break;
		case ELEVATOR_BASIC:
		case ELEVATOR_MOVE_FLOOR:
		case ELEVATOR_BASIC_AUTO:
		case ELEVATOR_DOOR_INV:
			TFE_Level::setFloorHeight(sectorId, value, true);
			break;
		case ELEVATOR_MOVE_OFFSET:
			TFE_Level::setSecondHeight(sectorId, value, true);
			break;
		case ELEVATOR_MOVE_FC:
		{
			f32 init0 = slaveIndex >= 0 ? curState->slaveState[slaveIndex].initState[0] : curState->initState[0];
			f32 init1 = slaveIndex >= 0 ? curState->slaveState[slaveIndex].initState[1] : curState->initState[1];
			TFE_Level::setFloorHeight(sectorId, value, true);
			TFE_Level::setCeilingHeight(sectorId, value - init0 + init1);
		}   break;
		case ELEVATOR_DOOR_MID:
		{
			f32 init0 = slaveIndex >= 0 ? curState->slaveState[slaveIndex].initState[0] : curState->initState[0];
			f32 init1 = slaveIndex >= 0 ? curState->slaveState[slaveIndex].initState[1] : curState->initState[1];
			const f32 mid = (init0 + init1) * 0.5f;
			TFE_Level::setFloorHeight(sectorId, mid + value);
			TFE_Level::setCeilingHeight(sectorId, mid - value);
		}   break;
		case ELEVATOR_CHANGE_LIGHT:
			TFE_Level::setAmbient(sectorId, (u8)std::min(31.0f, std::max(0.0f, value)));
			break;
		case ELEVATOR_MORPH_SPIN1:
		case ELEVATOR_MORPH_SPIN2:
		case ELEVATOR_ROTATE_WALL:
			TFE_Level::rotate(sectorId, value, valueDelta, &classData->var.center, moveFloor, moveSecAlt, s_useVertexCache);
			if (slaveIndex < 0) { s_useVertexCache = false; }
			break;
		case ELEVATOR_MORPH_MOVE1:
		case ELEVATOR_MORPH_MOVE2:
		case ELEVATOR_MOVE_WALL:
			TFE_Level::moveWalls(sectorId, classData->var.angle, value, valueDelta, moveFloor, moveSecAlt, s_useVertexCache);
			if (slaveIndex < 0) { s_useVertexCache = false; }
			break;
		case ELEVATOR_SCROLL_FLOOR:
		{
			// Modify the floor texture offset.
			f32 angle = classData->var.angle * PI / 180.0f - PI * 0.5f;
			Vec2f offset = { cosf(angle) * value, -sinf(angle) * value };
			TFE_Level::setTextureOffset(sectorId, SP_FLOOR, &offset, moveFloor, moveSecAlt);
		} break;
		case ELEVATOR_SCROLL_CEILING:
		{
			// Modify the floor texture offset.
			f32 angle = classData->var.angle * PI / 180.0f - PI * 0.5f;
			Vec2f offset = { cosf(angle) * value, -sinf(angle) * value };
			TFE_Level::setTextureOffset(sectorId, SP_CEILING, &offset);
		} break;
		case ELEVATOR_SCROLL_WALL:
		{
			// Modify the floor texture offset.
			const f32 angle = -classData->var.angle * PI / 180.0f + PI * 0.5f;
			Vec2f offset = { cosf(angle) * value * c_texelToWorldScale, sinf(angle) * value * c_texelToWorldScale };
			TFE_Level::setTextureOffset(sectorId, SP_WALL, &offset);
			TFE_Level::setTextureOffset(sectorId, SP_SIGN, &offset);
		} break;
		}
	}

	void setInitialState(const InfClassData* classData, const Sector* sector, ItemState* curState)
	{
		switch (classData->isubclass)
		{
		case ELEVATOR_CHANGE_LIGHT:
			curState->initState[0] = (f32)sector->ambient;
			for (u32 i = 0; i < classData->slaveCount; i++)
			{
				curState->slaveState[i].initState[0] = getSlaveSector(classData, i)->ambient;
			}
			break;
		case ELEVATOR_BASIC:
		case ELEVATOR_MOVE_FLOOR:
		case ELEVATOR_BASIC_AUTO:
		case ELEVATOR_DOOR_INV:
		case ELEVATOR_INV:
		case ELEVATOR_DOOR:
		case ELEVATOR_MOVE_CEILING:
		case ELEVATOR_MOVE_FC:
		case ELEVATOR_DOOR_MID:
			curState->initState[0] = sector->floorAlt;
			curState->initState[1] = sector->ceilAlt;
			for (u32 i = 0; i < classData->slaveCount; i++)
			{
				curState->slaveState[i].initState[0] = getSlaveSector(classData, i)->floorAlt;
				curState->slaveState[i].initState[1] = getSlaveSector(classData, i)->ceilAlt;
			}
			break;
		case ELEVATOR_MOVE_OFFSET:
			// For some reason relative movement is no different from absolute movement
			// for "move_offset" elevators. This is the only setting that works for the "Assassin" mod.
			curState->initState[0] = 0.0f; // sector->secAlt;
			for (u32 i = 0; i < classData->slaveCount; i++)
			{
				curState->slaveState[i].initState[0] = 0.0f; // getSlaveSector(classData, i)->secAlt;
			}
			break;
		case ELEVATOR_CHANGE_WALL_LIGHT:
			break;
		case ELEVATOR_SCROLL_FLOOR:
		case ELEVATOR_SCROLL_CEILING:
		case ELEVATOR_SCROLL_WALL:
		case ELEVATOR_MORPH_MOVE1:
		case ELEVATOR_MORPH_MOVE2:
		case ELEVATOR_MOVE_WALL:
		case ELEVATOR_MORPH_SPIN1:
		case ELEVATOR_MORPH_SPIN2:
		case ELEVATOR_ROTATE_WALL:
			curState->initState[0] = 0.0f;
			curState->curValue = 0.0f;
			for (u32 i = 0; i < classData->slaveCount; i++)
			{
				curState->slaveState[i].initState[0] = 0.0f;
				curState->slaveState[i].curValue = 0.0f;
			}
			break;
		};
	}
		
	void executeStopMove(const InfClassData* classData, ItemState* curState, Sector* sector, s32 wallId)
	{
		const InfStop* stop0 = &classData->stop[curState->curStop];
		const InfStop* stop1 = &classData->stop[curState->nextStop];

		const InfStopValue0Type value0Type = InfStopValue0Type(stop1->code & 15);
		const InfStopValue1Type value1Type = InfStopValue1Type((stop1->code >> 4) & 15);

		bool forceStop0 = false;
		if (curState->curStop < 0)
		{
			forceStop0 = true;
			curState->curStop = classData->var.start;
			curState->nextStop = classData->var.start;
		}
		const f32 stop0Value = getStopValue(classData, curState, sector, curState->curStop, -1);
		const f32 stop1Value = getStopValue(classData, curState, sector, curState->nextStop, -1);

		// Move towards the next value based on the var.speed
		f32 moveStep = stop1Value >= stop0Value ? c_step : -c_step;
		if (classData->isubclass == ELEVATOR_CHANGE_LIGHT || classData->isubclass == ELEVATOR_CHANGE_WALL_LIGHT)
		{
			moveStep *= c_lightSpeedScale;
		}

		const f32 prevValue = forceStop0 ? stop1Value : curState->curValue;
		if (!forceStop0)
		{
			if (curState->curValue == stop0Value && stop1Value != stop0Value)
			{
				startMovingSound(sector, classData);
			}

			if (classData->var.speed == 0.0f)
			{
				curState->curValue = stop1Value;
			}
			else
			{
				curState->curValue = curState->curValue + classData->var.speed * moveStep;
			}

			for (u32 i = 0; i < classData->slaveCount; i++)
			{
				if (classData->var.speed == 0.0f)
				{
					// This will get overwritten below anyway, so no point spending a lot of time here.
					curState->slaveState[i].curValue = stop1Value;
				}
				else
				{
					curState->slaveState[i].curValue = curState->slaveState[i].curValue + classData->var.speed * moveStep;
				}
			}
		}

		// Have we reached the next stop?
		if (forceStop0 || ((moveStep > 0.0f && curState->curValue >= stop1Value) || (moveStep < 0.0f && curState->curValue <= stop1Value)))
		{
			curState->curValue = stop1Value;
			for (u32 i = 0; i < classData->slaveCount; i++)
			{
				curState->slaveState[i].curValue = getStopValue(classData, curState, getSlaveSector(classData, i), curState->nextStop, i);
			}

			if (!forceStop0 && curState->curStop != curState->nextStop)
			{
				stopMovingSound(sector, classData);
			}

			curState->curStop = curState->nextStop;
			curState->nextStop = (curState->curStop + 1) % classData->stopCount;
			curState->delay = 0.0f;

			if (value1Type == INF_STOP1_HOLD)
			{
				curState->state = INF_STATE_HOLDING;
			}
			else if (value1Type == INF_STOP1_TERMINATE)
			{
				curState->state = INF_STATE_TERMINATED;
			}
			else if (value1Type == INF_STOP1_COMPLETE)
			{
				curState->state = INF_STATE_TERMINATED;
				TFE_GameHud::setMessage("Level Complete");
				TFE_GameUi::toggleNextMission(true);
			}
			else if (value1Type == INF_STOP1_TIME)
			{
				curState->state = INF_STATE_WAITING;
				curState->delay = std::max(stop1->time, c_minInfDelay);
			}

			// Only execute functions if the elevator has not been terminated.
			const u32 funcCount = stop1->code >> 8u;
			if (curState->state != INF_STATE_TERMINATED && (s_frame > 0 || curState->state != INF_STATE_HOLDING))
			{
				if (s_funcQueueCount < MAX_FUNC_IN_FLIGHT && funcCount)
				{
					s_funcQueue[s_funcQueueCount++] = { funcCount, 0, stop1->func };
				}
			}
		}
		const f32 valueDelta = curState->curValue - prevValue;

		// interpret the value based on the elevator type.
		for (u32 i = 0; i < classData->slaveCount; i++)
		{
			applyValueToSector(classData, curState, getSlaveSector(classData, i)->id, curState->slaveState[i].curValue, valueDelta, i);
		}
		applyValueToSector(classData, curState, sector->id, curState->curValue, valueDelta, -1);
	}

	void startMovingSound(Sector* sector, const InfClassData* classData)
	{
		ItemState* state = &s_infState[classData->stateIndex];
		if (classData->var.sound[0] >= 0)
		{
			TFE_Audio::playOneShot(SOUND_3D, 1.0f, MONO_SEPERATION, TFE_VocAsset::getFromIndex(classData->var.sound[0]), false, &sector->center, false);
		}

		if (classData->var.sound[1] >= 0 && !state->moveSound)
		{
			// Start the looping middle sound...
			state->moveSound = TFE_Audio::createSoundSource(SOUND_3D, 1.0f, MONO_SEPERATION, TFE_VocAsset::getFromIndex(classData->var.sound[1]), &sector->center);
			TFE_Audio::playSource(state->moveSound, true);
		}
	}

	void stopMovingSound(Sector* sector, const InfClassData* classData)
	{
		ItemState* state = &s_infState[classData->stateIndex];
		if (state->moveSound)
		{
			TFE_Audio::freeSource(state->moveSound);
			state->moveSound = nullptr;
		}
		if (classData->var.sound[2] >= 0)
		{
			TFE_Audio::playOneShot(SOUND_3D, 1.0f, MONO_SEPERATION, TFE_VocAsset::getFromIndex(classData->var.sound[2]), false, &sector->center, false);
		}
	}

	void prepareForFirstStop(const InfClassData* classData, Sector* sector)
	{
		// Verify that a first stop is setup.
		assert(classData->var.start >= 0);

		// Setup state, note that setting curStop to -1 will force the first stop to execute whenever the next move to stop is called.
		// If master = off is set initially, this won't happen until the item is turned on.
		// If stop 0 is a hold stop, this will be setup after the initial execution.
		ItemState* itemState = &s_infState[classData->stateIndex];
		itemState->state = INF_STATE_MOVING;
		itemState->curStop = -1;
		itemState->nextStop = 0;

		// Set the initial state, for relative changes.
		setInitialState(classData, sector, itemState);

		// Get the first stop value.
		itemState->curValue = getStopValue(classData, itemState, sector, classData->var.start, -1);
		for (u32 i = 0; i < classData->slaveCount; i++)
		{
			itemState->slaveState[i].curValue = getStopValue(classData, itemState, getSlaveSector(classData, i), classData->var.start, i);
		}

		// interpret the value based on the elevator type.
		for (u32 i = 0; i < classData->slaveCount; i++)
		{
			applyValueToSector(classData, itemState, getSlaveSector(classData, i)->id, itemState->slaveState[i].curValue, 0.0f, i);
		}
		applyValueToSector(classData, itemState, sector->id, itemState->curValue, 0.0f, -1);
	}

	void executeStopless(const InfClassData* classData, ItemState* itemState, Sector* sector, s32 wallId)
	{
		const bool moveFloor = (classData->var.flags & INF_MOVE_FLOOR) != 0;
		const bool moveSecAlt = (classData->var.flags & INF_MOVE_SECALT) != 0;

		if (classData->var.sound[1] >= 0 && !itemState->moveSound)
		{
			itemState->moveSound = TFE_Audio::createSoundSource(SOUND_3D, 1.0f, MONO_SEPERATION, TFE_VocAsset::getFromIndex(classData->var.sound[1]), &sector->center);
			TFE_Audio::playSource(itemState->moveSound, true);
		}

		switch (classData->isubclass)
		{
		case ELEVATOR_CHANGE_LIGHT:
		case ELEVATOR_CHANGE_WALL_LIGHT:
		case ELEVATOR_BASIC:
		case ELEVATOR_MOVE_FLOOR:
		case ELEVATOR_BASIC_AUTO:
		case ELEVATOR_DOOR_INV:
		case ELEVATOR_INV:
		case ELEVATOR_DOOR:
		case ELEVATOR_MOVE_CEILING:
		case ELEVATOR_MOVE_FC:
		case ELEVATOR_DOOR_MID:
		case ELEVATOR_MOVE_OFFSET:
		case ELEVATOR_MOVE_WALL:
			TFE_System::logWrite(LOG_WARNING, "INF", "Attempting to use a non-compatible elevator type without stops.");
			assert(0);
			break;
		case ELEVATOR_SCROLL_FLOOR:
		{
			// Modify the floor texture offset.
			f32 angle = classData->var.angle * PI / 180.0f - PI * 0.5f;
			f32 scale = classData->var.speed * c_step;
			Vec2f offset = { cosf(angle) * scale, -sinf(angle) * scale };
			TFE_Level::moveTextureOffset(sector->id, SP_FLOOR, &offset, moveFloor, moveSecAlt);

			u32 mergeStart = classData->mergeStart >= 0 ? classData->mergeStart : classData->slaveCount;
			for (u32 s = 0; s < classData->slaveCount; s++)
			{
				const bool applyMove = (classData->mergeStart >= 0 && s32(s) >= classData->mergeStart);
				Sector* slaveSector = getSlaveSector(classData, s);
				TFE_Level::moveTextureOffset(slaveSector->id, SP_FLOOR, &offset, applyMove && moveFloor, applyMove && moveSecAlt);
			}
		} break;
		case ELEVATOR_SCROLL_CEILING:
		{
			// Modify the floor texture offset.
			f32 angle = classData->var.angle * PI / 180.0f - PI * 0.5f;
			f32 scale = classData->var.speed * c_step;
			Vec2f offset = { cosf(angle) * scale, -sinf(angle) * scale };
			TFE_Level::moveTextureOffset(sector->id, SP_CEILING, &offset);

			for (u32 s = 0; s < classData->slaveCount; s++)
			{
				Sector* slaveSector = getSlaveSector(classData, s);
				TFE_Level::moveTextureOffset(slaveSector->id, SP_CEILING, &offset);
			}
		} break;
		case ELEVATOR_MORPH_SPIN1:
		case ELEVATOR_MORPH_SPIN2:
		case ELEVATOR_ROTATE_WALL:
		{
			u32 mergeStart = classData->mergeStart >= 0 ? classData->mergeStart : classData->slaveCount;
			TFE_Level::rotate(sector->id, itemState->curValue, classData->var.speed * c_step, &classData->var.center, moveFloor, moveSecAlt, s_useVertexCache);
			for (u32 s = 0; s < classData->slaveCount; s++)
			{
				Sector* slaveSector = getSlaveSector(classData, s);
				TFE_Level::rotate(slaveSector->id, itemState->curValue, classData->var.speed * c_step, &classData->var.center, moveFloor, moveSecAlt, s_useVertexCache);
			}
			s_useVertexCache = false;

			itemState->curValue += classData->var.speed * c_step;
		}	break;
		case ELEVATOR_MORPH_MOVE1:
		case ELEVATOR_MORPH_MOVE2:
			break;
		case ELEVATOR_SCROLL_WALL:
		{
			// Modify the floor texture offset.
			const f32 angle = -classData->var.angle * PI / 180.0f + PI * 0.5f;
			const f32 scale = classData->var.speed * c_step * c_texelToWorldScale;
			Vec2f offset = { cosf(angle) * scale, sinf(angle) * scale };

			const u32 count = 1 + classData->slaveCount;
			for (u32 i = 0; i < count; i++)
			{
				Sector* curSector = i == 0 ? sector : getSlaveSector(classData, i - 1);
				TFE_Level::moveTextureOffset(curSector->id, SP_WALL, &offset);
				TFE_Level::moveTextureOffset(curSector->id, SP_SIGN, &offset);
			}
		} break;
		}
	}

	void setupLevel(InfData* infData, LevelData* levelData)
	{
		assert(infData && levelData);
		s_infData = infData;
		s_levelData = levelData;
		s_accum = 0.0f;
		s_memoryPool->clear();
		s_frame = 0;
		s_funcQueueCount = 0;
		Sector* sectors = s_levelData->sectors.data();

		// Map between sectors and items.
		s_sectorItemMap = (SectorItems*)s_memoryPool->allocate(sizeof(SectorItems) * s_levelData->sectors.size());

		// First calculate the number of states to track.
		const u32 count = infData->itemCount;
		s_stateCount = 0;
		for (u32 i = 0; i < count; i++)
		{
			InfItem* item = &infData->item[i];
			const u32 classCount = item->classCount;
			for (u32 c = 0; c < classCount; c++)
			{
				item->classData[c].stateIndex = s_stateCount;
				s_stateCount++;
			}
		}

		// Then determine a mapping between sector ID and item ID.
		const u32 sectorCount = (u32)s_levelData->sectors.size();
		for (u32 i = 0; i < sectorCount; i++)
		{
			s_sectorItemMap[i].sectorItemId = -1;
			s_sectorItemMap[i].lineCount = 0;
			s_sectorItemMap[i].lineItemId = nullptr;
		}

		// First of 2 passes, setteing up sector item ids and counting lines.
		for (u32 i = 0; i < count; i++)
		{
			InfItem* item = &infData->item[i];
			u32 sectorId = item->id & 0xffffu;
			u32 lineId = item->id >> 16u;
			if (sectorId == 0xffffu) { continue; }

			if (lineId == 0xffffu)
			{
				s_sectorItemMap[sectorId].sectorItemId = i;
			}
			else if (lineId < 0xffffu)
			{
				s_sectorItemMap[sectorId].lineCount++;
			}
		}

		// Allocating line memory
		for (u32 i = 0; i < sectorCount; i++)
		{
			s_sectorItemMap[i].lineItemId = (SectorLineId*)s_memoryPool->allocate(sizeof(SectorLineId) * s_sectorItemMap[i].lineCount);
			s_sectorItemMap[i].lineCount = 0;
		}

		// Second of 2 passes, filling in line data.
		for (u32 i = 0; i < count; i++)
		{
			InfItem* item = &infData->item[i];
			u32 sectorId = item->id & 0xffffu;
			u32 lineId = item->id >> 16u;
			if (sectorId == 0xffffu || lineId == 0xffffu) { continue; }

			s_sectorItemMap[sectorId].lineItemId[s_sectorItemMap[sectorId].lineCount++] = { lineId, i };
		}

		// Everything should move to the start state.
		// Note that items with no stops, such as flowing water, don't need any initial setup.
		s_infState = (ItemState*)s_memoryPool->allocate(sizeof(ItemState) * s_stateCount);
		memset(s_infState, 0, sizeof(ItemState)*s_stateCount);

		for (u32 i = 0; i < count; i++)
		{
			InfItem* item = &infData->item[i];
			const u32 sectorId = item->id & 0xffff;
			if (sectorId == 0xffff) { continue; }
			Sector* sector = &sectors[sectorId];

			const u32 classCount = item->classCount;
			s_useVertexCache = true;
			for (u32 c = 0; c < classCount; c++)
			{
				InfClassData* classData = &item->classData[c];
				ItemState* state = &s_infState[classData->stateIndex];
				state->moveSound = nullptr;

				// Allocate state space for slaves.
				state->slaveState = (ItemSlaveState*)s_memoryPool->allocate(sizeof(ItemSlaveState) * classData->slaveCount);
				// Prepare for the first stop (if applicable).
				if (classData->iclass != INF_CLASS_ELEVATOR)
				{
					state->state = INF_STATE_HOLDING;
					continue;
				}
				if (classData->stopCount == 0)
				{
					state->state = INF_STATE_MOVING;
					continue;
				}
				prepareForFirstStop(classData, sector);
			}
		}

		TFE_System::logWrite(LOG_MSG, "INF", "Runtime INF state memory size: %u bytes.", s_memoryPool->getMemoryUsed());
	}

	bool firePlayerEvent(u32 evt, s32 sectorId, s32 wallId)
	{
		if (s_sectorItemMap[sectorId].lineCount < 1) { return false; }
		s32 itemId = -1;
		for (u32 i = 0; i < s_sectorItemMap[sectorId].lineCount; i++)
		{
			if (s_sectorItemMap[sectorId].lineItemId[i].wallId == wallId)
			{
				itemId = s_sectorItemMap[sectorId].lineItemId[i].itemId;
				break;
			}
		}
		if (itemId < 0) { return false; }

		// Go through each class and see if the entity_mask and event_mask match what we are doing.
		InfItem* item = &s_infData->item[itemId];
		const u32 classCount = item->classCount;
		for (u32 c = 0; c < classCount; c++)
		{
			InfClassData* classData = &item->classData[c];
			// If the class has been turned off, then skip.
			if (!classData->var.master || item->type != INF_ITEM_LINE) { continue; }
			// Otherwise check the mask flags.
			if ((classData->var.entity_mask & INF_ENTITY_PLAYER) && (classData->var.event_mask & evt))
			{
				activateLineOrSector(classData);
			}
		}
		return true;
	}

	bool firePlayerEvent(u32 evt, s32 sectorId, Player* player)
	{
		const s32 itemId = s_sectorItemMap[sectorId].sectorItemId;
		// If the sector is not linked to an INF item, it can be skipped.
		if (itemId < 0) { return false; }

		bool continueFalling = false;
		// Go through each class and see if the entity_mask and event_mask match what we are doing.
		InfItem* item = &s_infData->item[itemId];
		const u32 classCount = item->classCount;
		for (u32 c = 0; c < classCount; c++)
		{
			InfClassData* classData = &item->classData[c];
			// If the class has been turned off, then skip.
			if (!classData->var.master || item->type != INF_ITEM_SECTOR) { continue; }
			// Otherwise check the mask flags.
			if (classData->iclass == INF_CLASS_TELEPORTER && classData->isubclass == TELEPORTER_CHUTE && classData->var.target >= 0 && (classData->var.event_mask&evt))
			{
				// This needs more work but it does allow chute areas to be traversed.
				continueFalling = true;
				player->m_sectorId = classData->var.target;
			}
			else if ((classData->var.entity_mask & INF_ENTITY_PLAYER) && (classData->var.event_mask & evt))
			{
				activateLineOrSector(classData);
			}
		}
		return continueFalling;
	}

	// Activate a line or sector.
	// Ignores events, so this should be wrapped in other tests.
	NudgeType activateLineOrSector(InfClassData* classData)
	{
		NudgeType nudgeType = NUDGE_NONE;

		// And finally activate - but only if it is going "holding" and waiting to be activated.
		ItemState* itemState = &s_infState[classData->stateIndex];
		// And finally activate - but only if it is going "holding" and waiting to be activated.
		if (classData->iclass == INF_CLASS_ELEVATOR)
		{
			if (itemState->state == INF_STATE_HOLDING)
			{
				itemState->state = INF_STATE_MOVING;
				itemState->nextStop = (itemState->curStop + 1) % classData->stopCount;
			}
		}
		else if (classData->iclass == INF_CLASS_TRIGGER)
		{
			if (itemState->state == INF_STATE_HOLDING)
			{
				if (classData->isubclass == TRIGGER_SINGLE)
				{
					itemState->state = INF_STATE_TERMINATED;
				}
				else if (classData->isubclass == TRIGGER_SWITCH1)
				{
					itemState->state = INF_STATE_ACTIVATED;
				}

				const u32 funcCount = classData->stop[0].code >> 8u;
				if (funcCount)
				{
					s_funcQueue[s_funcQueueCount++] = { funcCount, classData->var.event, classData->stop[0].func };
				}

				if (classData->isubclass == TRIGGER_SWITCH1 || classData->isubclass == TRIGGER_SINGLE)
				{
					nudgeType = NUDGE_SET;
				}
				else if (classData->isubclass == TRIGGER_TOGGLE)
				{
					nudgeType = NUDGE_TOGGLE;
				}
			}
		}

		return nudgeType;
	}

	bool insideSign(const Vec3f* point, u32 sectorId, u32 wallId)
	{
		if (sectorId == 0xffffu || wallId == 0xffffu) { return false; }

		const Sector* sector = s_levelData->sectors.data() + sectorId;
		const SectorWall* wall = s_levelData->walls.data() + sector->wallOffset + wallId;
		const Vec2f* vtx = s_levelData->vertices.data() + sector->vtxOffset;
		// If there is no sign, use the whole line.
		if (wall->sign.texId < 0 || wall->sign.texId >= s_levelData->textures.size()) { return true; }

		const Vec2f* vtx0 = &vtx[wall->i0];
		const Vec2f* vtx1 = &vtx[wall->i1];
		const f32 dx = vtx1->x - vtx0->x;
		const f32 dz = vtx1->z - vtx0->z;
		
		const f32 wallLenInTexels = sqrtf(dx*dx + dz*dz) * c_worldToTexelScale;
		f32 y0, y1;
		f32 offsetX, offsetY;
		bool hasSign = false;
		if (wall->adjoin < 0)
		{
			offsetX =  wall->mid.offsetX - wall->sign.offsetX;
			offsetY = -TFE_Math::fract(std::max(wall->mid.offsetY, 0.0f)) + wall->sign.offsetY;
			if (wall->flags[0] & WF1_SCROLL_SIGN_TEX)
			{
				offsetY = wall->sign.offsetY;
			}
			if (wall->flags[0] & WF1_SIGN_ANCHORED)
			{
				offsetY += TFE_Level::getBaseSectorHeight(sector->id)->floorAlt - sector->floorAlt;
			}

			y0 = sector->floorAlt;
			y1 = sector->ceilAlt;
			hasSign = true;
		}
		else
		{
			const Sector* next = s_levelData->sectors.data() + wall->adjoin;
			if (next->floorAlt < sector->floorAlt)
			{
				offsetX =  wall->bot.offsetX - wall->sign.offsetX;
				offsetY = -TFE_Math::fract(std::max(wall->bot.offsetY, 0.0f)) + wall->sign.offsetY;
				if (wall->flags[0] & WF1_SCROLL_SIGN_TEX)
				{
					offsetY = wall->sign.offsetY;
				}
				if (wall->flags[0] & WF1_SIGN_ANCHORED)
				{
					f32 dy = 0.0f;

					// Handle next sector moving.
					f32 baseHeight = TFE_Level::getBaseSectorHeight(next->id)->floorAlt;
					dy -= (baseHeight - next->floorAlt);

					// Handle current sector moving.
					baseHeight = TFE_Level::getBaseSectorHeight(sector->id)->floorAlt;
					dy += (baseHeight - sector->floorAlt);

					offsetY += dy;
				}

				y0 = sector->floorAlt;
				y1 = next->floorAlt;
				hasSign = true;
			}
			else if (next->ceilAlt > sector->ceilAlt)
			{
				offsetX =  wall->top.offsetX - wall->sign.offsetX;
				offsetY = -TFE_Math::fract(std::max(wall->top.offsetY, 0.0f)) + wall->sign.offsetY;
				if (wall->flags[0] & WF1_SCROLL_SIGN_TEX)
				{
					offsetY = wall->sign.offsetY;
				}
				// Upper signs are "auto-anchored" due to the way the texture coordinates are calculated.
				// So ignore the WF1_SIGN_ANCHORED flag.

				y0 = next->ceilAlt;
				y1 = sector->ceilAlt;
				hasSign = true;
			}
		}
		if (!hasSign)
		{
			return false;
		}

		const f32 texW = s_levelData->textures[wall->sign.texId]->frames[0].width;
		const f32 texH = s_levelData->textures[wall->sign.texId]->frames[0].height;

		const f32 h = y0 - y1;
		if (fabsf(h) < 0.001f)
		{
			return false;
		}

		const f32 u0 = offsetX * c_worldToTexelScale;
		const f32 u1 = u0 + wallLenInTexels;
		const f32 v0 = (-offsetY - h) * c_worldToTexelScale;
		const f32 v1 = (-offsetY) * c_worldToTexelScale;

		const f32 s = fabsf(dx) >= fabsf(dz) ? (point->x - vtx0->x) / dx : (point->z - vtx0->z) / dz;
		const f32 t = (point->y - y1) / h;

		const f32 u =   u0 + (u1 - u0) * s;
		const f32 v = -(v0 + (v1 - v0) * t);
		if (u >= -4.0f && u < texW + 4.0f && v >= -4.0f && v < texH + 4.0f)
		{
			return true;
		}
		return false;
	}

	NudgeType nudgeLine(u32 evt, InfItem* item, const Vec3f* hitPoint)
	{
		const u32 classCount = item->classCount;
		NudgeType nudgeType = NUDGE_NONE;
		for (u32 c = 0; c < classCount; c++)
		{
			InfClassData* classData = &item->classData[c];
			// If the class has been turned off, then skip.
			if (!classData->var.master) { continue; }
			// Otherwise check the mask flags.
			const u32 entityRequired = (evt & INF_EVENT_SHOOT_LINE) ? INF_ENTITY_WEAPON : INF_ENTITY_PLAYER;
			if ((classData->var.entity_mask & entityRequired) && (classData->var.event_mask & evt))
			{
				// Determine if the "hitPoint" is close enough (or that we actually care, i.e. this is a sign).
				if (classData->iclass == INF_CLASS_TRIGGER && classData->isubclass >= TRIGGER_SWITCH1 && evt != INF_EVENT_EXPLOSION)
				{
					// Compute the sign uv from the hitPoint.
					if (!insideSign(hitPoint, item->id & 0xffffu, (item->id >> 16u)))
					{
						continue;
					}
				}
								
				NudgeType nt = activateLineOrSector(classData);
				if (nt != NUDGE_NONE)
				{
					nudgeType = nt;
					if (classData->var.sound[0] >= 0)
					{
						TFE_Audio::playOneShot(SOUND_3D, 1.0f, MONO_SEPERATION, TFE_VocAsset::getFromIndex(classData->var.sound[0]), false, hitPoint, true);
					}
				}
			}
		}

		return nudgeType;
	}

	void shootWall(const Vec3f* hitPoint, s32 hitSectorId, s32 hitWallId)
	{
		if (s_sectorItemMap[hitSectorId].lineCount < 1) { return; }
		s32 itemId = -1;
		for (u32 i = 0; i < s_sectorItemMap[hitSectorId].lineCount; i++)
		{
			if (s_sectorItemMap[hitSectorId].lineItemId[i].wallId == hitWallId)
			{
				itemId = s_sectorItemMap[hitSectorId].lineItemId[i].itemId;
				break;
			}
		}
		if (itemId < 0) { return; }

		// Go through each class and see if the entity_mask and event_mask match what we are doing.
		InfItem* item = &s_infData->item[itemId];
		const NudgeType nudgeType = nudgeLine(INF_EVENT_SHOOT_LINE, item, hitPoint);
		if (nudgeType == NUDGE_TOGGLE)
		{
			TFE_Level::toggleTextureFrame(hitSectorId, SP_SIGN, WSP_NONE, hitWallId);
		}
		else if (nudgeType == NUDGE_SET)
		{
			TFE_Level::setTextureFrame(hitSectorId, SP_SIGN, WSP_NONE, 1, hitWallId);
		}
	}

	void advanceElevator(s32 sectorId)
	{
		s32 itemId = s_sectorItemMap[sectorId].sectorItemId;
		if (itemId < 0) { return; }

		InfItem* item = &s_infData->item[itemId];
		const u32 classCount = item->classCount;

		for (u32 c = 0; c < classCount; c++)
		{
			InfClassData* classData = &item->classData[c];

			// And finally activate - but only if it is going "holding" and waiting to be activated.
			ItemState* itemState = &s_infState[classData->stateIndex];
			if (classData->iclass == INF_CLASS_ELEVATOR && itemState->state == INF_STATE_HOLDING)
			{
				itemState->state = INF_STATE_MOVING;
				itemState->nextStop = (itemState->curStop + 1) % classData->stopCount;
			}
		}
	}

	void advanceBossElevator()
	{
		// search for the "boss" elevator
		const u32 sectorCount = (u32)s_levelData->sectors.size();
		const Sector* sector = s_levelData->sectors.data();
		for (u32 s = 0; s < sectorCount; s++, sector++)
		{
			if (strcasecmp(sector->name, "boss") == 0)
			{
				advanceElevator(sector->id);
				return;
			}
		}
	}

	void advanceMohcElevator()
	{
		// Search for "mohc" elevator - a special elevator only used by the last boss.
		const u32 sectorCount = (u32)s_levelData->sectors.size();
		const Sector* sector = s_levelData->sectors.data();
		sector = s_levelData->sectors.data();
		for (u32 s = 0; s < sectorCount; s++, sector++)
		{
			if (strcasecmp(sector->name, "mohc") == 0)
			{
				advanceElevator(sector->id);
				return;
			}
		}
	}

	void advanceCompleteElevator()
	{
		// Find the "Complete" elevator and advance it.
		if (s_infData->completeId >= 0)
		{
			InfItem* item = &s_infData->item[s_infData->completeId];
			const u32 classCount = item->classCount;

			for (u32 c = 0; c < classCount; c++)
			{
				InfClassData* classData = &item->classData[c];

				// And finally activate - but only if it is going "holding" and waiting to be activated.
				ItemState* itemState = &s_infState[classData->stateIndex];
				if (classData->iclass == INF_CLASS_ELEVATOR && itemState->state == INF_STATE_HOLDING)
				{
					itemState->state = INF_STATE_MOVING;
					itemState->nextStop = (itemState->curStop + 1) % classData->stopCount;
				}
			}

			SoundBuffer* buffer = TFE_VocAsset::get("COMPLETE.VOC");
			TFE_Audio::playOneShot(SOUND_2D, 1.0f, MONO_SEPERATION, buffer, false);
		}
	}

	void activate(const Vec3f* pos, const MultiRayHitInfo* hitInfo, s32 curSectorId, u32 keys)
	{
		if (!hitInfo || (hitInfo->hitCount < 1 && hitInfo->sectorCount <1)) { return; }

		// Nudge Sectors
		for (u32 s = 0; s < hitInfo->sectorCount; s++)
		{
			u32 sectorId = hitInfo->sectors[s];

			// If the sector is not linked to an INF item, it can be skipped.
			if (s_sectorItemMap[sectorId].sectorItemId < 0) { continue; }
			// Tells the system if we are inside the sector or not.
			u32 nudgeDir = (sectorId == curSectorId) ? INF_EVENT_NUDGE_FRONT : INF_EVENT_NUDGE_BACK;
			// Go through each class and see if the entity_mask and event_mask match what we are doing.
			InfItem* item = &s_infData->item[s_sectorItemMap[sectorId].sectorItemId];
			const u32 classCount = item->classCount;
			for (u32 c = 0; c < classCount; c++)
			{
				InfClassData* classData = &item->classData[c];
				// If the class has been turned off, then skip.
				if (!classData->var.master) { continue; }
				// Otherwise check the mask flags.
				const bool maskMatches = (classData->var.entity_mask & INF_ENTITY_PLAYER) && (classData->var.event_mask & nudgeDir);
				if (!maskMatches) { continue; }
				// Then activate if an elevator or sector based trigger.
				if (classData->iclass == INF_CLASS_ELEVATOR)
				{
					if (!(classData->var.key & keys) && classData->var.key != 0)
					{
						u32 msgId = 0;
						if (classData->var.key == KEY_RED) msgId = 6;
						else if (classData->var.key == KEY_YELLOW) msgId = 7;
						else if (classData->var.key == KEY_BLUE) msgId = 8;
						if (msgId != 0)
						{
							TFE_GameHud::setMessage(TFE_GameMessages::getMessage(msgId));

							const SoundBuffer* buffer = TFE_VocAsset::get("LOCKED-1.VOC");
							TFE_Audio::playOneShot(SOUND_2D, 1.0f, MONO_SEPERATION, buffer, false);
						}
						continue;
					}

					// And finally activate - but only if it is going "holding" and waiting to be activated.
					ItemState* itemState = &s_infState[classData->stateIndex];
					if (itemState->state == INF_STATE_HOLDING)
					{
						itemState->state = INF_STATE_MOVING;
						itemState->nextStop = (itemState->curStop + 1) % classData->stopCount;
					}
				}
				else if (classData->iclass == INF_CLASS_TRIGGER && item->type == INF_ITEM_SECTOR)
				{
					ItemState* itemState = &s_infState[classData->stateIndex];

					if (itemState->state == INF_STATE_HOLDING)
					{
						if (classData->isubclass == TRIGGER_SINGLE)
						{
							itemState->state = INF_STATE_TERMINATED;
						}
						else if (classData->isubclass == TRIGGER_SWITCH1)
						{
							itemState->state = INF_STATE_ACTIVATED;
						}

						if (classData->var.sound[0] >= 0)
						{
							TFE_Audio::playOneShot(SOUND_2D, 1.0f, MONO_SEPERATION, TFE_VocAsset::getFromIndex(classData->var.sound[0]), false);
						}

						const u32 funcCount = classData->stop[0].code >> 8u;
						if (funcCount)
						{
							s_funcQueue[s_funcQueueCount++] = { funcCount, classData->var.event, classData->stop[0].func };
						}
					}
				}
			}
		}

		// Nudge Lines
		for (u32 s = 0; s < hitInfo->hitCount; s++)
		{
			u32 sectorId = hitInfo->hitSectorId[s];
			u32 lineId   = hitInfo->wallHitId[s];
			u32 evt = INF_EVENT_NUDGE_FRONT;
			if (s_sectorItemMap[sectorId].lineCount < 1)
			{
				// Try the backside of the line.
				Sector* sector = s_levelData->sectors.data() + sectorId;
				SectorWall* wall = s_levelData->walls.data() + sector->wallOffset + lineId;
				if (wall->adjoin >= 0 && s_sectorItemMap[wall->adjoin].lineCount >= 0)
				{
					sectorId = wall->adjoin;
					lineId   = wall->mirror;
					evt = INF_EVENT_NUDGE_BACK;
				}
				if (evt != INF_EVENT_NUDGE_BACK) { continue; }
			}

			// There are lines in this sector, is this particular line one of them?
			for (u32 i = 0; i < s_sectorItemMap[sectorId].lineCount; i++)
			{
				if (s_sectorItemMap[sectorId].lineItemId[i].wallId == lineId)
				{
					// For switches, make sure the player is looking right at the switch and isn't too high or low.
					InfItem* item = &s_infData->item[s_sectorItemMap[sectorId].lineItemId[i].itemId];
					const NudgeType nudgeType = nudgeLine(evt, item, &hitInfo->hitPoint[s]);
					if (nudgeType == NUDGE_TOGGLE)
					{
						TFE_Level::toggleTextureFrame(sectorId, SP_SIGN, WSP_NONE, lineId);
					}
					else if (nudgeType == NUDGE_SET)
					{
						TFE_Level::setTextureFrame(sectorId, SP_SIGN, WSP_NONE, 1, lineId);
					}
										
					break;
				}
			}
		}
	}

	void explosion(const Vec3f* pos, s32 curSectorId, f32 radius)
	{
		const f32 c_activateRadius = 2.5f;
		u32 lines[256];
		u32 sectors[256];
		u32 lineCount = 0, sectorCount = 0;
		TFE_Physics::getOverlappingLinesAndSectors(pos, curSectorId, c_activateRadius, 256, lines, sectors, &lineCount, &sectorCount);
		for (u32 s = 0; s < sectorCount; s++)
		{
			u32 sectorId = sectors[s];

			// If the sector is not linked to an INF item, it can be skipped.
			if (s_sectorItemMap[sectorId].sectorItemId < 0) { continue; }
			// Go through each class and see if the entity_mask and event_mask match what we are doing.
			InfItem* item = &s_infData->item[s_sectorItemMap[sectorId].sectorItemId];
			const u32 classCount = item->classCount;
			for (u32 c = 0; c < classCount; c++)
			{
				InfClassData* classData = &item->classData[c];
				// If the class has been turned off, then skip.
				if (!classData->var.master) { continue; }
				// Otherwise check the mask flags.
				const bool maskMatches = classData->var.event_mask & INF_EVENT_EXPLOSION;
				if (!maskMatches) { continue; }
				// Then activate if an elevator or sector based trigger.
				if (classData->iclass == INF_CLASS_ELEVATOR)
				{
					// And finally activate - but only if it is going "holding" and waiting to be activated.
					ItemState* itemState = &s_infState[classData->stateIndex];
					if (itemState->state == INF_STATE_HOLDING)
					{
						itemState->state = INF_STATE_MOVING;
						itemState->nextStop = (itemState->curStop + 1) % classData->stopCount;
					}
				}
				else if (classData->iclass == INF_CLASS_TRIGGER && item->type == INF_ITEM_SECTOR)
				{
					ItemState* itemState = &s_infState[classData->stateIndex];

					if (itemState->state == INF_STATE_HOLDING)
					{
						if (classData->isubclass == TRIGGER_SINGLE)
						{
							itemState->state = INF_STATE_TERMINATED;
						}
						else if (classData->isubclass == TRIGGER_SWITCH1)
						{
							itemState->state = INF_STATE_ACTIVATED;
						}

						const u32 funcCount = classData->stop[0].code >> 8u;
						if (funcCount)
						{
							s_funcQueue[s_funcQueueCount++] = { funcCount, classData->var.event, classData->stop[0].func };
						}
					}
				}
			}
		}

		// Nudge lines.
		for (u32 l = 0; l < lineCount; l++)
		{
			u32 sectorId = lines[l] & 0xffffu;
			u32 lineId = lines[l] >> 16u;
			if (s_sectorItemMap[sectorId].lineCount < 1) continue;

			// There are lines in this sector, is this particular line one of them?
			for (u32 i = 0; i < s_sectorItemMap[sectorId].lineCount; i++)
			{
				if (s_sectorItemMap[sectorId].lineItemId[i].wallId == lineId)
				{
					// For switches, make sure the player is looking right at the switch and isn't too high or low.
					InfItem* item = &s_infData->item[s_sectorItemMap[sectorId].lineItemId[i].itemId];
					nudgeLine(INF_EVENT_EXPLOSION, item, nullptr);
				}
			}
		}
	}

	bool isSlidingOrRotatingElevator(const u8 isubclass)
	{
		return isubclass >= ELEVATOR_MORPH_MOVE1 && isubclass <= ELEVATOR_ROTATE_WALL;
	}

	void tick()
	{
		if (!s_levelData) { return; }

		f32 dt = (f32)TFE_System::getDeltaTime();
		Sector* sectors = s_levelData->sectors.data();
				
		// Update INF at a fixed framerate.
		s_accum += dt;
		while (s_accum >= c_step)
		{
			s_accum -= c_step;

			const u32 count = s_infData->itemCount;
			for (u32 i = 0; i < count; i++)
			{
				InfItem* item = &s_infData->item[i];
				const u32 sectorId = item->id & 0xffff;
				const u32 wallId = item->id >> 16u;
				if (sectorId == 0xffff) { continue; }
				Sector* sector = &sectors[sectorId];

				const u32 classCount = item->classCount;

				f32 sectorAngle = 0.0f;
				Vec2f sectorMove = { 0 };

				s_useVertexCache = true;
				for (u32 c = 0; c < classCount; c++)
				{
					InfClassData* classData = &item->classData[c];
					// Switches don't need constant updates and just wait to be activated.
					// Do not update classes with master = off.
					if (classData->iclass != INF_CLASS_ELEVATOR) { continue; }

					ItemState* curState = &s_infState[classData->stateIndex];
					if (!classData->var.master)
					{
						// Moving and rotating sector types start from cached data and then apply one transform after the other.
						// For this to work, transforms must be applied even when the item is not active in case other classes are active on the same item.
						// Note that other types, such as moving floors or scrolling do not require this treatment.
						if (isSlidingOrRotatingElevator(classData->isubclass))
						{
							for (u32 i = 0; i < classData->slaveCount; i++)
							{
								applyValueToSector(classData, curState, getSlaveSector(classData, i)->id, curState->slaveState[i].curValue, 0.0f, i);
							}
							applyValueToSector(classData, curState, sector->id, curState->curValue, 0.0f, -1);
						}
						continue;
					}

					if (classData->iclass == INF_CLASS_ELEVATOR && classData->stopCount == 0)
					{
						// Elevators without stops keep going forever - useful for things like flowing water.
						executeStopless(classData, curState, sector, wallId < 0xffff ? wallId : -1);
					}
					else if (classData->iclass == INF_CLASS_ELEVATOR && classData->stopCount)
					{
						const bool applyCurValue = curState->state != INF_STATE_MOVING && isSlidingOrRotatingElevator(classData->isubclass);

						// Otherwise update the elevator state
						if (curState->state == INF_STATE_MOVING)
						{
							executeStopMove(classData, curState, sector, wallId < 0xffff ? wallId : -1);
						}
						else if (curState->state == INF_STATE_WAITING)
						{
							curState->delay -= c_step;
							if (curState->delay <= 0.0f)
							{
								curState->delay = 0.0f;
								curState->state = INF_STATE_MOVING;
							}
						}

						// Moving and rotating sector types start from cached data and then apply one transform after the other.
						// For this to work, transforms must be applied even when waiting.
						// Note that other types, such as moving floors or scrolling do not require this treatment.
						if (applyCurValue)
						{
							for (u32 i = 0; i < classData->slaveCount; i++)
							{
								applyValueToSector(classData, curState, getSlaveSector(classData, i)->id, curState->slaveState[i].curValue, 0.0f, i);
							}
							applyValueToSector(classData, curState, sector->id, curState->curValue, 0.0f, -1);
						}
					}
				}
			}

			// Execute queued functions.
			for (u32 f = 0; f < s_funcQueueCount; f++)
			{
				executeFunctions(s_funcQueue[f].funcCount, s_funcQueue[f].func, s_funcQueue[f].evt);
			}
			s_funcQueueCount = 0;

			s_frame++;
		}
	}
}