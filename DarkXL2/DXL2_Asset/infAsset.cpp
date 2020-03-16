#include "infAsset.h"
#include <DXL2_System/system.h>
#include <DXL2_System/memoryPool.h>
#include <DXL2_Asset/assetSystem.h>
#include <DXL2_Asset/levelAsset.h>
#include <DXL2_Asset/vocAsset.h>
#include <DXL2_Archive/archive.h>
#include <DXL2_System/parser.h>
#include <assert.h>
#include <algorithm>
#include <map>
#include <string>

namespace DXL2_InfAsset
{
	// Allocate 4 MB. The largest MOD level uses around 132Kb @ 2K sectors. The engine supports 32K sectors, so if we assume
	// this can be roughly 16x larger - this gives slightly over 2MB. So bumping it up to 4MB just to be sure.
	#define INF_POOL_SIZE_BYTES (4 * 1024 * 1024)
	#define MAX_SLAVES 256
	#define MAX_CLIENTS 32
	#define MAX_STOPS 1024
	#define MAX_FUNC 1024

	#define STOP_FUNC_COUNT(x) ((x) << 8u)
	#define STOP_VALUE0_TYPE(x) (x)
	#define STOP_VALUE1_TYPE(x) ((x) << 4u)

	#define FUNC_TYPE(x) (x)
	#define FUNC_CLIENT_COUNT(x) ((x) << 8u)
	#define FUNC_ARG_COUNT(x) ((x) << 16u)

	struct InfFuncTemp
	{
		InfFunction func;
		u32 stopNum;
	};

	static const char* c_elevatorSubClasses[] =
	{
		"change_light",				//ELEVATOR_CHANGE_LIGHT
		"basic",					//ELEVATOR_BASIC
		"inv",						//ELEVATOR_INV
		"move_floor",				//ELEVATOR_MOVE_FLOOR
		"move_ceiling",				//ELEVATOR_MOVE_CEILING
		"move_fc",					//ELEVATOR_MOVE_FC
		"scroll_floor",				//ELEVATOR_SCROLL_FLOOR
		"scroll_ceiling",			//ELEVATOR_SCROLL_CEILING
		"move_offset",				//ELEVATOR_MOVE_OFFSET
		"basic_auto",				//ELEVATOR_BASIC_AUTO

		"change_wall_light",		//ELEVATOR_CHANGE_WALL_LIGHT
		"morph_move1",				//ELEVATOR_MORPH_MOVE1
		"morph_move2",				//ELEVATOR_MORPH_MOVE2

		"morph_spin1",				//ELEVATOR_MORPH_SPIN1
		"morph_spin2",				//ELEVATOR_MORPH_SPIN2
		"move_wall",				//ELEVATOR_MOVE_WALL
		"rotate_wall",				//ELEVATOR_ROTATE_WALL
		"scroll_wall",				//ELEVATOR_SCROLL_WALL

		"door",						//ELEVATOR_DOOR
		"door_mid",					//ELEVATOR_DOOR_MID
		"door_inv",					//ELEVATOR_DOOR_INV
	};

	static const char* c_infMessageTypes[] =
	{
		"m_trigger",	//INF_MSG_M_TRIGGER
		"goto_stop",	//INF_MSG_GOTO_STOP
		"next_stop",	//INF_MSG_NEXT_STOP
		"prev_stop",	//INF_MSG_PREV_STOP
		"master_on",	//INF_MSG_MASTER_ON
		"master_off",	//INF_MSG_MASTER_OFF
		"clear_bits",	//INF_MSG_CLEAR_BITS
		"set_bits",		//INF_MSG_SET_BITS
		"complete",		//INF_MSG_COMPLETE
		"done",			//INF_MSG_DONE
		"wakeup",		//INF_MSG_WAKEUP
		"lights",		//INF_MSG_LIGHTS
	};

	static const char* c_defaultGob = "DARK.GOB";

	static std::vector<char> s_buffer;
	static MemoryPool s_memoryPool;
	static InfData s_data;
	
	bool parseInf();
	InfClass getInfClass(const char* str);
	InfSubClass getInfSubClass(const char* str, InfClass iclass);
	InfMessage getInfMessage(const char* type);
	void splitReceiver(const char* str, char* outSecName, s32* outLineId);
	void setupClassDefaults(InfClassData* classInfo);
	void finishClass(InfItem* item, InfClassData* curClass, const u32* clients, const u16* slaves, const InfStop* stops, const InfFuncTemp* funcs, u32 clientCount, u32 slaveCount, u32 stopCount, u32 funcCount, s32 mergeStart = -1, bool overrideAsExp = false);

	s32 getSectorId(const char* name, bool getNextUnused = false, s32 count = -1);

	bool load(const char* name)
	{
		s_memoryPool.init(INF_POOL_SIZE_BYTES, "Inf_Asset_Memory");

		char gobPath[DXL2_MAX_PATH];
		DXL2_Paths::appendPath(PATH_SOURCE_DATA, c_defaultGob, gobPath);
		// Unload the current level.
		unload();

		if (!DXL2_AssetSystem::readAssetFromGob(c_defaultGob, name, s_buffer))
		{
			return false;
		}

		// Parse the file...
		if (!parseInf())
		{
			DXL2_System::logWrite(LOG_ERROR, "INF", "Failed to parse INF file \"%s\" from GOB \"%s\"", name, gobPath);

			unload();
			return false;
		}

		// Merge items with the same name and type.
		std::map<u32, u32> itemMap;
		for (s32 i = 0; i < (s32)s_data.itemCount; i++)
		{
			if (s_data.item[i].id == 0)
			{
				continue;
			}

			std::map<u32, u32>::iterator iItem = itemMap.find(s_data.item[i].id);
			if (iItem != itemMap.end())
			{
				u32 index = iItem->second;
				//copy s_data.item[i] -> s_data.item[itemId]
				u32 oldCount = s_data.item[index].classCount;
				u32 newCount = s_data.item[index].classCount + s_data.item[i].classCount;
				// ptr, oldsize, newsize
				s_data.item[index].classData = (InfClassData*)s_memoryPool.reallocate(s_data.item[index].classData, oldCount * sizeof(InfClassData), newCount * sizeof(InfClassData));
				memcpy(&s_data.item[index].classData[oldCount], s_data.item[i].classData, sizeof(InfClassData) * s_data.item[i].classCount);
				s_data.item[index].classCount = newCount;

				s_data.itemCount--;
				if (i < (s32)s_data.itemCount)
				{
					s_data.item[i] = s_data.item[s_data.itemCount];
					i--;
				}
			}
			else
			{
				itemMap[s_data.item[i].id] = i;
			}
		}

		// Setup Sector flag doors.
		setupDoors();
		
		return true;
	}

	void unload()
	{
		s_data.itemCount = 0;
		s_data.item = nullptr;
	}

	MemoryPool* getMemoryPool(bool clearPool)
	{
		if (clearPool)
		{
			s_memoryPool.init(INF_POOL_SIZE_BYTES, "Inf_Asset_Memory");
		}
		return &s_memoryPool;
	}

	InfData* getInfData()
	{
		return &s_data;
	}

	static const char* c_classNames[] = { "elevator", "trigger", "teleporter" };
	static const char* c_triggerSubclassNames[] = { "standard", "default", "switch1" , "single", "toggle" };
	static const char* c_teleportSubClassNames[] = { "chute" };

	const char* getClassName(u32 iclass)
	{
		return c_classNames[iclass];
	}

	const char* getSubclassName(u32 iclass, u32 sclass)
	{
		if (iclass == INF_CLASS_ELEVATOR)
		{
			return c_elevatorSubClasses[sclass - ELEVATOR_CHANGE_LIGHT];
		}
		else if (iclass == INF_CLASS_TRIGGER)
		{
			return c_triggerSubclassNames[sclass - TRIGGER_STANDARD];
		}
		else if (iclass == INF_CLASS_TELEPORTER)
		{
			return c_teleportSubClassNames[0];
		}

		return nullptr;
	}

	const char* c_infFuncName[] =
	{
		"adjoin",
		"page",
		"text",
		"texture",
		"unknown",
	};

	const char* getFuncName(u32 funcId)
	{
		if (funcId <= INF_MSG_LIGHTS)
		{
			return c_infMessageTypes[funcId];
		}
		if (funcId <= INF_MSG_TEXTURE)
		{
			return c_infFuncName[funcId - INF_MSG_ADJOIN];
		}
		return c_infFuncName[INF_MSG_TEXTURE + 1];
	}

	struct SectorBucket
	{
		std::vector<s32> sectors;
	};

	typedef std::map<std::string, SectorBucket> SectorBucketMap;
	static SectorBucketMap s_sectorNameMap;

	void strtolower(const char* srcStr, char* dstStr)
	{
		const size_t len = strlen(srcStr);
		for (size_t i = 0; i < len; i++)
		{
			dstStr[i] = tolower(srcStr[i]);
		}
		dstStr[len] = 0;
	}

	u32 countSectorFlagDoors()
	{
		const LevelData* levelData = DXL2_LevelAsset::getLevelData();
		u32 doorCount = 0;

		const s32 sectorCount = (s32)levelData->sectors.size();
		const Sector* sector = levelData->sectors.data();
		for (s32 s = 0; s < sectorCount; s++, sector++)
		{
			if ((sector->flags[0] & SEC_FLAGS1_DOOR) || (sector->flags[0] & SEC_FLAGS1_EXP_WALL))
			{
				doorCount++;
			}
		}

		return doorCount;
	}

	u32 registerSectorNamesAndCountDoors(const LevelData* levelData)
	{
		s_sectorNameMap.clear();

		u32 doorCount = 0;
		const s32 sectorCount = (s32)levelData->sectors.size();
		const Sector* sector = levelData->sectors.data();
		for (s32 s = 0; s < sectorCount; s++, sector++)
		{
			if (sector->name[0])
			{
				char lowerCaseName[64];
				strtolower(sector->name, lowerCaseName);

				SectorBucketMap::iterator iBucket = s_sectorNameMap.find(sector->name);
				if (iBucket != s_sectorNameMap.end())
				{
					iBucket->second.sectors.push_back(sector->id);
				}
				else
				{
					s_sectorNameMap[lowerCaseName] = {};
					s_sectorNameMap[lowerCaseName].sectors.push_back(sector->id);
				}
			}
			if ((sector->flags[0] & SEC_FLAGS1_DOOR) || (sector->flags[0] & SEC_FLAGS1_EXP_WALL))
			{
				doorCount++;
			}
		}
		return doorCount;
	}

	bool createDoors(u32 itemStart, const LevelData* levelData)
	{
		const s32 sectorCount = (s32)levelData->sectors.size();
		const Sector* sector = levelData->sectors.data();
		bool hasDoors = false;
		for (s32 s = 0; s < sectorCount; s++, sector++)
		{
			if ((sector->flags[0] & SEC_FLAGS1_DOOR) || (sector->flags[0] & SEC_FLAGS1_EXP_WALL))
			{
				hasDoors = true;

				InfItem* curItem = &s_data.item[s_data.itemCount];
				s_data.itemCount++;

				curItem->id = u32(sector->id) | (0xffffu << 16u);
				curItem->type = INF_ITEM_SECTOR;
				curItem->classCount = 1;
				curItem->classData = (InfClassData*)s_memoryPool.allocate(sizeof(InfClassData));

				InfClassData* curClass = curItem->classData;
				curClass->iclass = INF_CLASS_ELEVATOR;
				curClass->isubclass = ELEVATOR_DOOR;
				curClass->stopCount = 0;
				curClass->stateIndex = 0;
				curClass->stop = nullptr;
				setupClassDefaults(curClass);

				finishClass(curItem, curClass, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0, -1, (sector->flags[0] & SEC_FLAGS1_EXP_WALL)!=0);
			}
		}
		return hasDoors;
	}

	s32 getSectorId(const char* name, bool getNextUnused, s32 itemCount)
	{
		char lowerCaseName[64];
		strtolower(name, lowerCaseName);

		SectorBucketMap::iterator iSec = s_sectorNameMap.find(lowerCaseName);
		if (iSec != s_sectorNameMap.end())
		{
			if (getNextUnused) // Return the next matching sector that is not used by INF.
			{
				// If there is only one sector with a name, then it is always what we return regardless of the
				// getNextUnused value.
				const u32  count = (u32)iSec->second.sectors.size();
				if (count == 1)
				{
					return iSec->second.sectors[0];
				}

				// Otherwise search for the next unused sector.
				const s32* index = iSec->second.sectors.data();
				for (u32 s = 0; s < count; s++, index++)
				{
					// Is this index used yet?
					bool itemFound = false;
					const InfItem* item = s_data.item;
					for (u32 i = 0; i < itemCount; i++, item++)
					{
						const u32 sectorId = item->id & 0xffffu;
						if (*index == sectorId && item->type == INF_ITEM_LINE)
						{
							itemFound = true;
							break;
						}
					}
					if (!itemFound)
					{
						return *index;
					}
				}
				// ?All sectors are used, just return the first one.
				return iSec->second.sectors[0];
			}
			else // Return the first sector.
			{
				return iSec->second.sectors[0];
			}
		}
		return -1;
	}

	u32 getSectorMatches(const char* name, s32* matches)
	{
		char lowerCaseName[64];
		strtolower(name, lowerCaseName);

		SectorBucketMap::iterator iSec = s_sectorNameMap.find(lowerCaseName);
		if (iSec != s_sectorNameMap.end())
		{
			const std::vector<s32>& list = iSec->second.sectors;
			const u32 count = (u32)list.size();
			assert(count < 256);

			for (u32 i = 0; i < count; i++)
			{
				matches[i] = list[i];
			}

			return count;
		}
		return 0;
	}

	s32 findWallWithSign(u32 sectorId)
	{
		if (sectorId >= 0xffffu) { return -1; }

		const LevelData* levelData = DXL2_LevelAsset::getLevelData();
		const Sector* sector = levelData->sectors.data() + sectorId;
		const u32 wallCount = sector->wallCount;
		const SectorWall* wall = levelData->walls.data() + sector->wallOffset;

		for (u32 w = 0; w < wallCount; w++, wall++)
		{
			if (wall->sign.texId >= 0)
			{
				return s32(w);
			}
		}
		return -1;
	}
		
	void finishClass(InfItem* item, InfClassData* curClass, const u32* clients, const u16* slaves, const InfStop* stops, const InfFuncTemp* funcs, u32 clientCount, u32 slaveCount, u32 stopCount, u32 funcCount, s32 mergeStart, bool overrideAsExp)
	{
		if (!curClass) { return; }

		// Triggers always have a function, so if there is none at this point,
		// the default "m_trigger" message should be added.
		if (curClass->iclass == INF_CLASS_TRIGGER && funcCount == 0)
		{
			curClass->stopCount = 1;
			curClass->stop = (InfStop*)s_memoryPool.allocate(sizeof(InfStop));
			curClass->stop[0].code = STOP_FUNC_COUNT(1u);
			curClass->stop[0].time = 0.0f;
			curClass->stop[0].func = (InfFunction*)s_memoryPool.allocate(sizeof(InfFunction));

			curClass->stop[0].func[0].code = INF_MSG_M_TRIGGER | FUNC_CLIENT_COUNT(clientCount) | FUNC_ARG_COUNT(0);
			curClass->stop[0].func[0].client = (u32*)s_memoryPool.allocate(sizeof(u32) * clientCount);
			curClass->stop[0].func[0].arg = nullptr;

			curClass->slaveCount = 0;
			curClass->mergeStart = -1;
			curClass->slaves = nullptr;

			if (clientCount)
			{
				memcpy(curClass->stop[0].func[0].client, clients, sizeof(u32)*clientCount);
			}
		}
		else if (curClass->iclass == INF_CLASS_TRIGGER)
		{
			curClass->stopCount = 1;
			curClass->stop = (InfStop*)s_memoryPool.allocate(sizeof(InfStop));
			curClass->stop[0].code = STOP_FUNC_COUNT(funcCount);
			curClass->stop[0].time = 0.0f;
			curClass->stop[0].func = (InfFunction*)s_memoryPool.allocate(sizeof(InfFunction) * funcCount);

			curClass->slaveCount = 0;
			curClass->mergeStart = -1;
			curClass->slaves = nullptr;

			for (u32 f = 0; f < funcCount; f++)
			{
				curClass->stop[0].func[f] = funcs[f].func;

				if (clientCount)
				{
					curClass->stop[0].func[f].code |= FUNC_CLIENT_COUNT(clientCount);
					curClass->stop[0].func[f].client = (u32*)s_memoryPool.allocate(sizeof(u32) * clientCount);
					memcpy(curClass->stop[0].func[f].client, clients, sizeof(u32)*clientCount);
				}
			}
		}
		else if (curClass->iclass == INF_CLASS_ELEVATOR && (curClass->isubclass == ELEVATOR_DOOR || curClass->isubclass == ELEVATOR_DOOR_INV || curClass->isubclass == ELEVATOR_DOOR_MID) && stopCount == 0)
		{
			// Get the sector.
			u32 sectorId = item->id & 0xffff;
			if (sectorId < 0xffff)
			{
				Sector* sector = DXL2_LevelAsset::getLevelData()->sectors.data() + sectorId;

				// Automatically add the proper stops to doors.
				curClass->stopCount = 2;
				curClass->stop = (InfStop*)s_memoryPool.allocate(sizeof(InfStop) * curClass->stopCount);

				curClass->stop[0].func = nullptr;
				curClass->stop[0].time = 0.0f;
				curClass->stop[1].func = nullptr;
				curClass->stop[1].time = 4.0f;

				curClass->slaveCount = 0;
				curClass->mergeStart = -1;
				curClass->slaves = nullptr;

				if (curClass->isubclass == ELEVATOR_DOOR && overrideAsExp)
				{
					curClass->stop[0].code = STOP_FUNC_COUNT(0) | STOP_VALUE0_TYPE(INF_STOP0_ABSOLUTE) | STOP_VALUE1_TYPE(INF_STOP1_HOLD);
					curClass->stop[1].code = STOP_FUNC_COUNT(0) | STOP_VALUE0_TYPE(INF_STOP0_ABSOLUTE) | STOP_VALUE1_TYPE(INF_STOP1_TERMINATE);
					curClass->var.speed = 0.0f;
					curClass->var.event_mask = INF_EVENT_EXPLOSION;

					curClass->stop[0].value0.fValue = -sector->floorAlt;
					curClass->stop[1].value0.fValue = -sector->ceilAlt;
				}
				else if (curClass->isubclass == ELEVATOR_DOOR)
				{
					curClass->stop[0].code = STOP_FUNC_COUNT(0) | STOP_VALUE0_TYPE(INF_STOP0_ABSOLUTE) | STOP_VALUE1_TYPE(INF_STOP1_HOLD);
					curClass->stop[1].code = STOP_FUNC_COUNT(0) | STOP_VALUE0_TYPE(INF_STOP0_ABSOLUTE) | STOP_VALUE1_TYPE(INF_STOP1_TIME);

					curClass->stop[0].value0.fValue = -sector->floorAlt;
					curClass->stop[1].value0.fValue = -sector->ceilAlt;
				}
				else if (curClass->isubclass == ELEVATOR_DOOR_INV)
				{
					curClass->stop[0].code = STOP_FUNC_COUNT(0) | STOP_VALUE0_TYPE(INF_STOP0_ABSOLUTE) | STOP_VALUE1_TYPE(INF_STOP1_HOLD);
					curClass->stop[1].code = STOP_FUNC_COUNT(0) | STOP_VALUE0_TYPE(INF_STOP0_ABSOLUTE) | STOP_VALUE1_TYPE(INF_STOP1_TIME);

					curClass->stop[0].value0.fValue = -sector->ceilAlt;
					curClass->stop[1].value0.fValue = -sector->floorAlt;
				}
				else if (curClass->isubclass == ELEVATOR_DOOR_MID)
				{
					curClass->stop[0].code = STOP_FUNC_COUNT(0) | STOP_VALUE0_TYPE(INF_STOP0_ABSOLUTE) | STOP_VALUE1_TYPE(INF_STOP1_HOLD);
					curClass->stop[1].code = STOP_FUNC_COUNT(0) | STOP_VALUE0_TYPE(INF_STOP0_ABSOLUTE) | STOP_VALUE1_TYPE(INF_STOP1_TIME);

					curClass->stop[0].value0.fValue = 0.0f;
					curClass->stop[1].value0.fValue = -fabsf(sector->floorAlt - sector->ceilAlt) * 0.5f;
				}
				else
				{
					assert(0);
				}
			}
		}
		else
		{
			curClass->stopCount = stopCount;
			curClass->mergeStart = mergeStart;
			curClass->slaveCount = slaveCount;
			curClass->stop = (InfStop*)s_memoryPool.allocate(sizeof(InfStop) * stopCount);
			curClass->slaves = (u16*)s_memoryPool.allocate(sizeof(u16) * slaveCount);

			memcpy(curClass->slaves, slaves, sizeof(u16) * slaveCount);

			u32 stopFuncCount[MAX_STOPS];
			u32 stopFuncIdx[MAX_STOPS];
			for (u32 s = 0; s < stopCount; s++)
			{
				curClass->stop[s] = stops[s];
				stopFuncCount[s] = 0;
				stopFuncIdx[s] = 0;
			}

			// Go through functions and determine counts.
			for (u32 f = 0; f < funcCount; f++)
			{
				stopFuncCount[funcs[f].stopNum]++;
			}

			// Then allocate memory.
			for (u32 s = 0; s < stopCount; s++)
			{
				curClass->stop[s].code |= STOP_FUNC_COUNT(stopFuncCount[s]);
				curClass->stop[s].func = (InfFunction*)s_memoryPool.allocate(sizeof(InfFunction) * stopFuncCount[s]);
			}

			// Finally assign functions.
			for (u32 f = 0; f < funcCount; f++)
			{
				const u32 s = funcs[f].stopNum;
				if (s < stopCount)
				{
					curClass->stop[s].func[stopFuncIdx[s]] = funcs[f].func;
					stopFuncIdx[s]++;
				}
				else
				{
					DXL2_System::logWrite(LOG_ERROR, "INF", "Function references an invalid stop %u.", s);
				}
			}
		}
	}
		
	bool parseInf()
	{
		if (s_buffer.empty()) { return false; }

		const size_t len = s_buffer.size();

		DXL2_Parser parser;
		size_t bufferPos = 0;
		parser.init(s_buffer.data(), len);
		parser.addCommentString("//");
		parser.addCommentString("#");
		parser.enableBlockComments();
		// Treat colon (:) as a seperator but do not remove it.
		parser.enableColonSeperator();

		InfItem* curItem = nullptr;
		InfClassData* curClass = nullptr;
		InfFuncTemp* curFunc = nullptr;
		u32 itemIndex = 0;
		u32 clientIndex = 0;
		s32 addon = -1;
		u32 clients[MAX_CLIENTS];
		bool inSequence = false;
		bool secondPass = false;

		// Work area so classes can be allocated together.
		InfClassData classes[64];
		u32 classCount = 0;

		InfStop stops[MAX_STOPS];
		u32 stopCount = 0;

		InfFuncTemp func[MAX_FUNC];
		u32 funcCount = 0;

		const LevelData* levelData = DXL2_LevelAsset::getLevelData();
		u32 doorCount = registerSectorNamesAndCountDoors(levelData);
		
		u16 slave[MAX_SLAVES];
		u32 slaveCount = 0;

		// Slaves added to handle multiple sectors with the same name.
		u16 slavesToAdd[MAX_SLAVES];
		u32 slaveToAddCount = 0;
		s_data.completeId = -1;

		TokenList tokens;
		while (bufferPos < len)
		{
			if (!secondPass)
			{
				const char* line = parser.readLine(bufferPos);
				if (!line) { break; }
				parser.tokenizeLine(line, tokens);
			}
			else
			{
				secondPass = false;
			}
			if (tokens.size() < 1) { continue; }

			char* endPtr = nullptr;
			if (strcasecmp("items", tokens[0].c_str()) == 0 && tokens.size() >= 2)
			{
				s_data.itemCount = strtoul(tokens[1].c_str(), &endPtr, 10);
				s_data.item = (InfItem*)s_memoryPool.allocate(sizeof(InfItem) * (s_data.itemCount + doorCount));
				memset(s_data.item, 0, sizeof(InfItem) * (s_data.itemCount + doorCount));
			}
			else if (strcasecmp("item:", tokens[0].c_str()) == 0)
			{
				assert(tokens.size() >= 2);
				assert(itemIndex < s_data.itemCount);
				if (itemIndex >= s_data.itemCount)
				{
					if (s_data.itemCount)
						DXL2_System::logWrite(LOG_ERROR, "INF", "Too many items - only %u were specified in the ITEMS field.", s_data.itemCount);
					else
						DXL2_System::logWrite(LOG_ERROR, "INF", "The ITEMS field is missing from the INF file.");
					break;
				}

				curItem = &s_data.item[itemIndex];
				curItem->classCount = 0;
				curItem->classData = nullptr;
				itemIndex++;

				classCount = 0;
				stopCount = 0;
				funcCount = 0;

				curItem->type = INF_ITEM_COUNT;
				if (strcasecmp(tokens[1].c_str(), "sector") == 0)
				{
					curItem->type = INF_ITEM_SECTOR;
				}
				else if (strcasecmp(tokens[1].c_str(), "line") == 0)
				{
					curItem->type = INF_ITEM_LINE;
				}
				else if (strcasecmp(tokens[1].c_str(), "level") == 0)
				{
					curItem->type = INF_ITEM_LEVEL;
				}
				assert(curItem->type != INF_ITEM_COUNT);

				curItem->id = curItem->type == INF_ITEM_LEVEL ? 0xffffffff : 0xffff0000;	// no wall by default.
				const char* name = nullptr;
				bool addExtraSectorsAsSlaves = curItem->type == INF_ITEM_SECTOR;
				bool wallNumFound = false;
				for (size_t t = 2; t < tokens.size();)
				{
					if (strcasecmp(tokens[t].c_str(), "name:") == 0)
					{
						name = tokens[t + 1].c_str();
						s32 sectorId = getSectorId(name, curItem->type == INF_ITEM_LINE, itemIndex);
						if (sectorId < 0)
						{
							DXL2_System::logWrite(LOG_WARNING, "INF", "Inf Item \"%s\" does not match a sector in the level.", name);
						}
						curItem->id |= sectorId < 0 ? 0xffff : u32(sectorId);
						t += 2;
					}
					else if (strcasecmp(tokens[t].c_str(), "num:") == 0)
					{
						wallNumFound = true;
						s32 wallNum = strtol(tokens[t + 1].c_str(), &endPtr, 10);
						if (wallNum < 0 && curItem->type == INF_ITEM_LINE)
						{
							DXL2_System::logWrite(LOG_WARNING, "INF", "Inf Item \"%s\" is a line type but does not have a valid line: %d", name ? name : "null", wallNum);
						}
						if (wallNum >= 0)
						{
							curItem->id &= 0x0000ffff;
							curItem->id |= (u32(wallNum) << 16u);
						}
						addExtraSectorsAsSlaves = false;
						t += 2;
					}
				}

				// In some cases no wall num: is specified but it is clearly a "line" type.
				// In this case, we have to search
				if (curItem->type == INF_ITEM_LINE && !wallNumFound)
				{
					// Search for the wall with a sign...
					s32 wallNum = findWallWithSign(curItem->id & 0xffffu);
					if (wallNum >= 0)
					{
						curItem->id &= 0x0000ffff;
						curItem->id |= (u32(wallNum) << 16u);
					}
				}

				if (name && strcasecmp(name, "complete") == 0 && curItem->type == INF_ITEM_SECTOR)
				{
					s_data.completeId = itemIndex - 1;
				}

				slaveToAddCount = 0;
				if (addExtraSectorsAsSlaves)
				{
					s32 matches[256];
					u32 matchCount = getSectorMatches(name, matches);
					if (matchCount > 1)
					{
						for (u32 i = 1; i < matchCount; i++)
						{
							slavesToAdd[slaveToAddCount++] = matches[i];
						}
					}
				}
			}
			else if (strcasecmp("seq", tokens[0].c_str()) == 0)
			{
				if (!curItem)
				{
					DXL2_System::logWrite(LOG_ERROR, "INF", "Beginning the sequence with no item.");
					continue;
				}

				inSequence = true;
				curClass = nullptr;
				addon = -1;
				classCount = 0;
			}
			else if (strcasecmp("seqend", tokens[0].c_str()) == 0)
			{
				inSequence = false;
				if (!curItem)
				{
					DXL2_System::logWrite(LOG_ERROR, "INF", "Ending the sequence with no item.");
					continue;
				}

				// Add extra slaves for other sectors with the same name.
				s32 mergeStart = slaveToAddCount ? slaveCount : -1;
				for (u32 i = 0; i < slaveToAddCount; i++)
				{
					slave[slaveCount++] = slavesToAdd[i];
				}
				finishClass(curItem, curClass, clients, slave, stops, func, clientIndex, slaveCount, stopCount, funcCount, mergeStart);
				
				// Now process all classes.
				curItem->classCount = classCount;
				curItem->classData = (InfClassData*)s_memoryPool.allocate(sizeof(InfClassData) * classCount);
				memcpy(curItem->classData, classes, sizeof(InfClassData) * classCount);
			}
			else if (!inSequence)
			{
				continue;
			}
			// Split out sequence elememts to limit if-defs.
			else if (strcasecmp("class:", tokens[0].c_str()) == 0 && tokens.size() >= 2)
			{
				//[1] = class
				//[2] = subclass
				if (curClass)
				{
					// Add extra slaves for other sectors with the same name.
					s32 mergeStart = slaveToAddCount ? slaveCount : -1;
					for (u32 i = 0; i < slaveToAddCount; i++)
					{
						slave[slaveCount++] = slavesToAdd[i];
					}
					finishClass(curItem, curClass, clients, slave, stops, func, clientIndex, slaveCount, stopCount, funcCount, mergeStart);
				}

				curClass = &classes[classCount];
				memset(curClass, 0, sizeof(InfClass));
				classCount++;

				curClass->iclass = getInfClass(tokens[1].c_str());
				curClass->isubclass = SUBCLASS_NONE;
				curClass->stateIndex = 0;
				if (tokens.size() >= 3)
				{
					curClass->isubclass = getInfSubClass(tokens[2].c_str(), (InfClass)curClass->iclass);
				}
				else if (curClass->iclass == INF_CLASS_TRIGGER)
				{
					curClass->isubclass = TRIGGER;
				}

				setupClassDefaults(curClass);
				curClass->stopCount = 0;
				clientIndex = 0;
				slaveCount = 0;
				stopCount = 0;
				funcCount = 0;
				curFunc = nullptr;
			}
			else if (strcasecmp("stop:", tokens[0].c_str()) == 0 && curClass != nullptr)
			{
				assert(tokens.size() >= 2);
				if (stopCount >= MAX_STOPS)
				{
					DXL2_System::logWrite(LOG_ERROR, "INF", "Too many stops %u of %u.", stopCount + 1, MAX_STOPS);
					continue;
				}
				//[1] = value0
				//[2] = value1
				u32 stopNum = stopCount;
				stopCount++;
				InfStop* stop = &stops[stopNum];
				stop->func = nullptr;
				stop->code = 0;
				stop->value0.iValue = 0;
				stop->time = 0.0f;
				
				// Is value0 a value, relative value or a sectorname?
				const char* value0 = tokens[1].c_str();
				if (value0[0] == '@')
				{
					stop->code |= STOP_VALUE0_TYPE(INF_STOP0_RELATIVE);
					stop->value0.fValue = (f32)strtod(&value0[1], &endPtr);
				}
				else if (value0[0] == '-' || (value0[0] >= '0' && value0[0] <= '9'))
				{
					stop->code |= STOP_VALUE0_TYPE(INF_STOP0_ABSOLUTE);
					stop->value0.fValue = (f32)strtod(value0, &endPtr);
				}
				else
				{
					stop->code |= STOP_VALUE0_TYPE(INF_STOP0_SECTORNAME);
					const s32 sectorId = getSectorId(value0);
					if (sectorId < 0)
					{
						DXL2_System::logWrite(LOG_ERROR, "INF", "Stop #%u has the sectorname type but \"%s\" has not been found in the level.", stopNum, value0);
					}
					stop->value0.iValue = sectorId;
				}

				// Is value1 a time, hold, terminate or complete?
				if (tokens.size() >= 3)
				{
					const char* value1 = tokens[2].c_str();
					if (strcasecmp(value1, "hold") == 0)
					{
						stop->code |= STOP_VALUE1_TYPE(INF_STOP1_HOLD);
					}
					else if (strcasecmp(value1, "terminate") == 0)
					{
						stop->code |= STOP_VALUE1_TYPE(INF_STOP1_TERMINATE);
					}
					else if (strcasecmp(value1, "complete") == 0)
					{
						stop->code |= STOP_VALUE1_TYPE(INF_STOP1_COMPLETE);
					}
					else
					{
						stop->code |= STOP_VALUE1_TYPE(INF_STOP1_TIME);
						stop->time = (f32)strtod(value1, &endPtr);
					}

					// Sometimes messages are on the same line as the stop, in this case restart the processing at the last token.
					if (tokens.size() > 3)
					{
						tokens.erase(tokens.begin(), tokens.begin() + 3);
						secondPass = true;
					}
				}
				else
				{
					// In this case, we wait a default amount of time.
					stop->code |= STOP_VALUE1_TYPE(INF_STOP1_TIME);
					stop->time = 3.0f;
				}
			}
			else if (strcasecmp("client:", tokens[0].c_str()) == 0 && tokens.size() >= 2 && curClass != nullptr)
			{
				char recStr[64];
				s32 recLine = -1;
				splitReceiver(tokens[1].c_str(), recStr, &recLine);

				const s32 sectorId = getSectorId(recStr);
				if (sectorId < 0)
				{
					DXL2_System::logWrite(LOG_ERROR, "INF", "Trigger client specified \"%s\" but cannot be found in the level.", recStr);
				}

				assert(clientIndex < MAX_CLIENTS);
				if (clientIndex >= MAX_CLIENTS)
				{
					DXL2_System::logWrite(LOG_ERROR, "INF", "Too many trigger clients %u, max is %u.", clientIndex+1, MAX_CLIENTS);
				}
				else
				{
					clients[clientIndex]  = sectorId < 0 ? 0xffff : u32(sectorId);
					clients[clientIndex] |= (recLine < 0 ? 0xffff : u32(recLine)) << 16u;
					clientIndex++;
				}
			}
			else if (strcasecmp("message:", tokens[0].c_str()) == 0 && curClass != nullptr)
			{
				// Message is a special type of function. Note that functions should be assigned to the correct stops, so it is written into temporary buffers
				// to be fixed up once the class is finished.
				assert(tokens.size() >= 2);
				if (funcCount >= MAX_FUNC)
				{
					DXL2_System::logWrite(LOG_ERROR, "INF", "Too many functions %u of %u.", funcCount + 1, MAX_FUNC);
					continue;
				}

				u32 funcNum = funcCount;
				funcCount++;
				curFunc = &func[funcNum];
				curFunc->stopNum = 0;
				curFunc->func.code = 0;
				curFunc->func.client = nullptr;
				curFunc->func.arg = nullptr;
				
				if (curClass->iclass == INF_CLASS_TRIGGER)
				{
					//[1] = message
					//[2] = parameters (optional)
					curFunc->func.code = FUNC_TYPE(getInfMessage(tokens[1].c_str()));
							
					u32 paramCount = (u32)std::max(0, (s32)tokens.size() - 2);
					curFunc->func.code |= FUNC_ARG_COUNT(paramCount);
					curFunc->func.arg = (InfArg*)s_memoryPool.allocate(sizeof(InfArg) * paramCount);

					for (u32 p = 0; p < paramCount; p++)
					{
						curFunc->func.arg[p].iValue = strtol(tokens[p + 2].c_str(), &endPtr, 10);
					}
				}
				else if (curClass->iclass == INF_CLASS_ELEVATOR)
				{
					//[1] = stop number
					//[2] = receiver
					//[3] = messasge
					//[4] = parameters (optional)
					curFunc->stopNum = strtol(tokens[1].c_str(), &endPtr, 10);
					curFunc->func.arg = nullptr;

					curFunc->func.code = FUNC_CLIENT_COUNT(1);
					curFunc->func.client = (u32*)s_memoryPool.allocate(sizeof(u32));

					char recStr[64];
					s32 recLine = -1;
					splitReceiver(tokens[2].c_str(), recStr, &recLine);
					const bool systemMsg = strcasecmp(recStr, "system") == 0;

					if (systemMsg)
					{
						curFunc->func.client[0] = 0xffff | ((recLine >= 0 ? u32(recLine) : 0xffffu) << 16u);
					}
					else
					{
						s32 sectorId = getSectorId(recStr);
						if (sectorId < 0)
						{
							DXL2_System::logWrite(LOG_ERROR, "INF", "Message client sector \"%s\" has not been found in the level.", recStr);
							sectorId = 0xffff;
						}
						curFunc->func.client[0] = u32(sectorId) | ((recLine >= 0 ? u32(recLine) : 0xffffu) << 16u);
					}

					if (tokens.size() >= 4)
					{
						curFunc->func.code |= FUNC_TYPE(getInfMessage(tokens[3].c_str()));

						u32 paramCount = (u32)std::max(0, (s32)tokens.size() - 4);
						curFunc->func.code |= FUNC_ARG_COUNT(paramCount);
						curFunc->func.arg = (InfArg*)s_memoryPool.allocate(sizeof(InfArg) * paramCount);
						for (u32 p = 0; p < paramCount; p++)
						{
							curFunc->func.arg[p].iValue = strtol(tokens[p + 4].c_str(), &endPtr, 10);
						}
					}
					else
					{
						// Default?
						curFunc->func.code |= FUNC_TYPE(INF_MSG_M_TRIGGER);
					}
				}
				else
				{
					DXL2_System::logWrite(LOG_ERROR, "INF", "Messages can only be sent from Elevators or Triggers.");
				}
			}
			else if (strcasecmp("adjoin:", tokens[0].c_str()) == 0)
			{
				assert(tokens.size() >= 5);
				// [1] stop number
				// [2] sector 1
				// [3] line 1
				// [4] sector 2
				// [5] line 2
				u32 funcNum = funcCount;
				funcCount++;
				curFunc = &func[funcNum];
				curFunc->stopNum = strtol(tokens[1].c_str(), &endPtr, 10);
				curFunc->func.client = nullptr;

				curFunc->func.code = FUNC_TYPE(INF_MSG_ADJOIN) | FUNC_CLIENT_COUNT(0u) | FUNC_ARG_COUNT(4u);
				curFunc->func.arg = (InfArg*)s_memoryPool.allocate(sizeof(InfArg) * 4);
				curFunc->func.arg[0].iValue = getSectorId(tokens[2].c_str());
				curFunc->func.arg[1].iValue = strtol(tokens[3].c_str(), &endPtr, 10);
				curFunc->func.arg[2].iValue = getSectorId(tokens[4].c_str());
				if (tokens.size() >= 6)
					curFunc->func.arg[3].iValue = strtol(tokens[5].c_str(), &endPtr, 10);
				else // default?
					curFunc->func.arg[3].iValue = 0;
			}
			else if (strcasecmp("page:", tokens[0].c_str()) == 0)
			{
				assert(tokens.size() >= 3);
				// [1] stop number
				// [2] voc file
				u32 funcNum = funcCount;
				funcCount++;
				curFunc = &func[funcNum];
				curFunc->stopNum = strtol(tokens[1].c_str(), &endPtr, 10);
				curFunc->func.client = nullptr;

				curFunc->func.code = FUNC_TYPE(INF_MSG_PAGE) | FUNC_CLIENT_COUNT(0u) | FUNC_ARG_COUNT(1u);
				curFunc->func.arg = (InfArg*)s_memoryPool.allocate(sizeof(InfArg) * 1);
				curFunc->func.arg[0].iValue = DXL2_VocAsset::getIndex(tokens[2].c_str());
			}
			else if (strcasecmp("text:", tokens[0].c_str()) == 0)
			{
				assert(tokens.size() >= 2);
				// [1] stop number
				// [2] text num
				u32 funcNum = funcCount;
				funcCount++;
				curFunc = &func[funcNum];
				curFunc->stopNum = tokens.size() >= 3 ? strtol(tokens[1].c_str(), &endPtr, 10) : 0;
				curFunc->func.client = nullptr;

				curFunc->func.code = FUNC_TYPE(INF_MSG_TEXT) | FUNC_CLIENT_COUNT(0u) | FUNC_ARG_COUNT(1u);
				curFunc->func.arg = (InfArg*)s_memoryPool.allocate(sizeof(InfArg) * 1);
				curFunc->func.arg[0].iValue = strtol(tokens.size() >= 3 ? tokens[2].c_str() : tokens[1].c_str(), &endPtr, 10);
			}
			else if (strcasecmp("texture:", tokens[0].c_str()) == 0)
			{
				assert(tokens.size() >= 4);
				// [1] stop number
				// [2] flag
				// [3] donor
				u32 funcNum = funcCount;
				funcCount++;
				curFunc = &func[funcNum];
				curFunc->stopNum = strtol(tokens[1].c_str(), &endPtr, 10);
				curFunc->func.client = (u32*)s_memoryPool.allocate(sizeof(u32));
				curFunc->func.client[0] = u32(curItem->id & 0xffffu) | ((0xffffu) << 16u);

				curFunc->func.code = FUNC_TYPE(INF_MSG_TEXTURE) | FUNC_CLIENT_COUNT(1u) | FUNC_ARG_COUNT(2u);
				curFunc->func.arg = (InfArg*)s_memoryPool.allocate(sizeof(InfArg) * 2);
				const char* flag = tokens[2].c_str();
				curFunc->func.arg[0].iValue = flag[0] >= '0' && flag[0] <= '9' ? 0 : 1;	// number = floor, letter = ceiling
				curFunc->func.arg[1].iValue = getSectorId(tokens[3].c_str());
			}
			else if (strcasecmp("addon:", tokens[0].c_str()) == 0 && tokens.size() >= 2)
			{
				addon = strtol(tokens[1].c_str(), &endPtr, 10);
			}
			// Variables
			else if (strcasecmp("master:", tokens[0].c_str()) == 0)
			{
				assert(tokens.size() >= 2);
				// Can I ignore addon here?
				if (!curClass) { DXL2_System::logWrite(LOG_ERROR, "INF", "Assigning the variable \"%s\" but there is no class.", tokens[0].c_str()); continue; }
				if (strcasecmp(tokens[1].c_str(), "off") == 0 || strcasecmp(tokens[1].c_str(), "0") == 0 || strcasecmp(tokens[1].c_str(), "false") == 0)
				{
					curClass->var.master = false;
				}
			}
			else if (strcasecmp("event_mask:", tokens[0].c_str()) == 0 || strcasecmp("event_mask", tokens[0].c_str()) == 0 || strcasecmp("eventmask:", tokens[0].c_str()) == 0)
			{
				assert(tokens.size() >= 2);
				// Can I ignore addon here?
				if (!curClass) { DXL2_System::logWrite(LOG_ERROR, "INF", "Assigning the variable \"%s\" but there is no class.", tokens[0].c_str()); continue; }
				curClass->var.event_mask = tokens[1].c_str()[0] == '*' ? 0xffffffff : strtoul(tokens[1].c_str(), &endPtr, 10);
			}
			else if (strcasecmp("event:", tokens[0].c_str()) == 0 && tokens.size() >= 2)
			{
				assert(tokens.size() >= 2);
				if (!curClass) { DXL2_System::logWrite(LOG_ERROR, "INF", "Assigning the variable \"%s\" but there is no class.", tokens[0].c_str()); continue; }
				// Can I ignore addon here?
				curClass->var.event = strtoul(tokens[1].c_str(), &endPtr, 10);
			}
			else if (strcasecmp("entity_mask:", tokens[0].c_str()) == 0)
			{
				assert(tokens.size() >= 2);
				if (!curClass) { DXL2_System::logWrite(LOG_ERROR, "INF", "Assigning the variable \"%s\" but there is no class.", tokens[0].c_str()); continue; }
				// Can I ignore addon here?
				curClass->var.entity_mask = tokens[1].c_str()[0] == '*' ? 0xffffffff : strtoul(tokens[1].c_str(), &endPtr, 10);
			}
			else if (strcasecmp("speed:", tokens[0].c_str()) == 0)
			{
				assert(tokens.size() >= 2);
				if (!curClass) { DXL2_System::logWrite(LOG_ERROR, "INF", "Assigning the variable \"%s\" but there is no class.", tokens[0].c_str()); continue; }
				if (addon >= 0)
				{
					curClass->var.speed_addon[addon] = (f32)strtod(tokens[1].c_str(), &endPtr);
				}
				else
				{
					curClass->var.speed = (f32)strtod(tokens[1].c_str(), &endPtr);
				}
			}
			else if (strcasecmp("start:", tokens[0].c_str()) == 0)
			{
				assert(tokens.size() >= 2);
				assert(addon < 0);
				if (!curClass) { DXL2_System::logWrite(LOG_ERROR, "INF", "Assigning the variable \"%s\" but there is no class.", tokens[0].c_str()); continue; }
				curClass->var.start = strtol(tokens[1].c_str(), &endPtr, 10);
			}
			else if (strcasecmp("center:", tokens[0].c_str()) == 0)
			{
				assert(tokens.size() >= 3);
				assert(addon < 0);
				if (!curClass) { DXL2_System::logWrite(LOG_ERROR, "INF", "Assigning the variable \"%s\" but there is no class.", tokens[0].c_str()); continue; }
				curClass->var.center.x = (f32)strtod(tokens[1].c_str(), &endPtr);
				curClass->var.center.z = (f32)strtod(tokens[2].c_str(), &endPtr);
			}
			else if (strcasecmp("angle:", tokens[0].c_str()) == 0 || strcasecmp("angle", tokens[0].c_str()) == 0)
			{
				assert(tokens.size() >= 2);
				if (!curClass) { DXL2_System::logWrite(LOG_ERROR, "INF", "Assigning the variable \"%s\" but there is no class.", tokens[0].c_str()); continue; }
				// Can I ignore addon here?
				curClass->var.angle = (f32)strtod(tokens[1].c_str(), &endPtr);
			}
			else if (strcasecmp("key:", tokens[0].c_str()) == 0)
			{
				assert(tokens.size() >= 2);
				if (!curClass) { DXL2_System::logWrite(LOG_ERROR, "INF", "Assigning the variable \"%s\" but there is no class.", tokens[0].c_str()); continue; }
				if (strcasecmp(tokens[1].c_str(), "red") == 0)
				{
					curClass->var.key = KEY_RED;
				}
				else if (strcasecmp(tokens[1].c_str(), "blue") == 0)
				{
					curClass->var.key = KEY_BLUE;
				}
				else if (strcasecmp(tokens[1].c_str(), "yellow") == 0)
				{
					curClass->var.key = KEY_YELLOW;
				}
			}
			else if (strcasecmp("flags:", tokens[0].c_str()) == 0)
			{
				assert(tokens.size() >= 2);
				if (!curClass) { DXL2_System::logWrite(LOG_ERROR, "INF", "Assigning the variable \"%s\" but there is no class.", tokens[0].c_str()); continue; }
				// Can I ignore addon here?
				curClass->var.flags = strtoul(tokens[1].c_str(), &endPtr, 10);
			}
			else if (strcasecmp("sound:", tokens[0].c_str()) == 0)
			{
				assert(tokens.size() >= 2);
				if (!curClass) { DXL2_System::logWrite(LOG_ERROR, "INF", "Assigning the variable \"%s\" but there is no class.", tokens[0].c_str()); continue; }
				// Can I ignore addon here?
				if (tokens.size() == 3)
				{
					s32 index = strtol(tokens[1].c_str(), &endPtr, 10) - 1;
					bool silent = (tokens[2].c_str()[0] == '0');
					curClass->var.sound[index] = index;
					if (silent)
					{
						curClass->var.sound[index] |= ((0xffffffu) << 8u);	// silent
					}
					else
					{
						curClass->var.sound[index] |= ((0x0u) << 8u);	// TODO: voc sound asset ID [tokens[2].c_str()]
					}
				}
				else
				{
					curClass->var.sound[0] = 0;
					curClass->var.sound[0] |= ((0x0u) << 8u);	// TODO: voc sound asset ID [tokens[1].c_str()]
				}
			}
			else if (strcasecmp("object_mask:", tokens[0].c_str()) == 0)
			{
				assert(tokens.size() >= 2);
				if (!curClass) { DXL2_System::logWrite(LOG_ERROR, "INF", "Assigning the variable \"%s\" but there is no class.", tokens[0].c_str()); continue; }
				// Can I ignore addon here?
				u32 mask = strtoul(tokens[1].c_str(), &endPtr, 10);
				if (curClass->iclass == INF_CLASS_ELEVATOR)
				{
					curClass->var.event_mask = mask;
				}
				else
				{
					curClass->var.entity_mask = mask;
				}
			}
			else if (strcasecmp("slave:", tokens[0].c_str()) == 0)
			{
				assert(tokens.size() >= 2);
				if (!curClass) { DXL2_System::logWrite(LOG_ERROR, "INF", "Assigning the variable \"%s\" but there is no class.", tokens[0].c_str()); continue; }
				// Can I ignore addon here?
				s32 sectorId = getSectorId(tokens[1].c_str());
				if (sectorId >= 0)
				{
					assert(slaveCount < MAX_SLAVES);
					if (slaveCount >= MAX_SLAVES)
					{
						DXL2_System::logWrite(LOG_ERROR, "INF", "Too many trigger slaves %u, max is %u.", slaveCount + 1, MAX_SLAVES);
					}
					else
					{
						slave[slaveCount++] = (u16)sectorId;
					}
				}
			}
			else if (strcasecmp("target:", tokens[0].c_str()) == 0)
			{
				assert(tokens.size() >= 2);
				if (!curClass) { DXL2_System::logWrite(LOG_ERROR, "INF", "Assigning the variable \"%s\" but there is no class.", tokens[0].c_str()); continue; }
				// Can I ignore addon here?
				curClass->var.target = getSectorId(tokens[1].c_str());
			}
			else
			{
				// Am I missing anything?
				// amb_sound: is missing - it doesn't seem to work in the original game though it might be fixed for Outlaws.
				// seed: is missing - they probably meant speed.
				// soudn: mispelling of sound.
				// entity_enter: a hidden function, needs to be cleared up.
				DXL2_System::logWrite(LOG_WARNING, "INF", "Unrecognized command \"%s\"", tokens[0].c_str());
			}
		}

		assert(s_data.completeId >= 0);
		s_data.itemCount = itemIndex;
				
		DXL2_System::logWrite(LOG_MSG, "INF", "Inf memory used: %u bytes.", s_memoryPool.getMemoryUsed());
		return true;
	}

	void setupDoors()
	{
		const LevelData* levelData = DXL2_LevelAsset::getLevelData();
		const s32 doorStart = s_data.itemCount;
		s_data.doorStart = -1;
		if (createDoors(s_data.itemCount, levelData))
		{
			s_data.doorStart = doorStart;
		}
	}

	void setupClassDefaults(InfClassData* classInfo)
	{
		classInfo->var.master = true;
		classInfo->var.event = 0;
		classInfo->var.entity_mask = INF_ENTITY_PLAYER;
		classInfo->var.start = 0;
		classInfo->var.center = { 0 };
		classInfo->var.key = 0;
		classInfo->var.target = -1;

		// Default event_mask
		classInfo->var.event_mask = 0;
		if (classInfo->iclass == INF_CLASS_ELEVATOR && (classInfo->isubclass == ELEVATOR_DOOR || classInfo->isubclass == ELEVATOR_DOOR_INV || classInfo->isubclass == ELEVATOR_DOOR_MID))
		{
			classInfo->var.event_mask = INF_EVENT_NUDGE_BACK;
		}
		else if (classInfo->iclass == INF_CLASS_ELEVATOR && (classInfo->isubclass == ELEVATOR_BASIC || classInfo->isubclass == ELEVATOR_INV || classInfo->isubclass == ELEVATOR_BASIC_AUTO))
		{
			classInfo->var.event_mask = INF_EVENT_NUDGE_BACK | INF_EVENT_NUDGE_FRONT | INF_EVENT_ENTER_SECTOR;
		}
		else if (classInfo->iclass == INF_CLASS_ELEVATOR && (classInfo->isubclass == ELEVATOR_MORPH_MOVE1 || classInfo->isubclass == ELEVATOR_MORPH_MOVE2 || classInfo->isubclass == ELEVATOR_MORPH_SPIN1 || classInfo->isubclass == ELEVATOR_MORPH_SPIN2))
		{
			classInfo->var.event_mask = INF_EVENT_NUDGE_BACK | INF_EVENT_NUDGE_FRONT | INF_EVENT_ENTER_SECTOR | INF_EVENT_LEAVE_SECTOR;
		}
		else if (classInfo->iclass == INF_CLASS_TRIGGER)
		{
			classInfo->var.event_mask = INF_EVENT_CROSS_LINE_FRONT | INF_EVENT_CROSS_LINE_BACK | INF_EVENT_ENTER_SECTOR | INF_EVENT_LEAVE_SECTOR | INF_EVENT_NUDGE_FRONT | INF_EVENT_NUDGE_BACK | INF_EVENT_LAND;
		}
		else if (classInfo->iclass == INF_CLASS_TELEPORTER)
		{
			classInfo->var.event_mask = INF_EVENT_LAND;
		}

		// TODO: Figure out the default speed for each type of elevator.
		if (classInfo->isubclass == ELEVATOR_DOOR || classInfo->isubclass == ELEVATOR_DOOR_INV || classInfo->isubclass == ELEVATOR_DOOR_MID)
		{
			classInfo->var.speed = 30.0f;
		}
		else if (classInfo->isubclass == ELEVATOR_MORPH_SPIN1 || classInfo->isubclass == ELEVATOR_MORPH_SPIN2)
		{
			classInfo->var.speed = 20.0f;
		}
		else if (classInfo->isubclass == ELEVATOR_MORPH_MOVE1)
		{
			classInfo->var.speed = 18.0f;
		}
		else if (classInfo->isubclass == ELEVATOR_MORPH_MOVE2)
		{
			classInfo->var.speed = 15.0f;
		}
		else
		{
			classInfo->var.speed = 10.0f;
		}

		// Flags
		classInfo->var.flags = 0;
		if (classInfo->iclass == INF_CLASS_ELEVATOR && (classInfo->isubclass == ELEVATOR_SCROLL_FLOOR || classInfo->isubclass == ELEVATOR_MORPH_MOVE2 || classInfo->isubclass == ELEVATOR_MORPH_SPIN2))
		{
			classInfo->var.flags = INF_MOVE_FLOOR | INF_MOVE_SECALT;
		}
	}

	void splitReceiver(const char* str, char* outSecName, s32* outLineId)
	{
		const s32 len = (s32)strlen(str);
		s32 paranBegin = -1;
		s32 paranEnd = -1;
		for (s32 i = 0; i < len; i++)
		{
			if (str[i] == '(')
			{
				paranBegin = i;
			}
			else if (str[i] == ')')
			{
				paranEnd = i;
			}
		}
		if (paranBegin >= 0)
		{
			strncpy(outSecName, str, paranBegin);
			outSecName[paranBegin] = 0;

			char* endPtr;
			*outLineId = strtol(&str[paranBegin + 1], &endPtr, 10);
		}
		else
		{
			strcpy(outSecName, str);
			*outLineId = -1;
		}
	}
		
	InfMessage getInfMessage(const char* type)
	{
		for (u32 i = 0; i < INF_MSG_COUNT; i++)
		{
			if (strcasecmp(type, c_infMessageTypes[i]) == 0)
			{
				return InfMessage(i);
			}
		}
		if (strcasecmp(type, "trigger") == 0)
		{
			return INF_MSG_M_TRIGGER;
		}
		DXL2_System::logWrite(LOG_ERROR, "INF", "Invalid message type: \"%s\"", type);
		return INF_MSG_INVALID;
	}

	InfClass getInfClass(const char* str)
	{
		if (strcasecmp(str, "elevator") == 0)
		{
			return INF_CLASS_ELEVATOR;
		}
		else if (strcasecmp(str, "trigger") == 0)
		{
			return INF_CLASS_TRIGGER;
		}
		else if (strcasecmp(str, "teleporter") == 0)
		{
			return INF_CLASS_TELEPORTER;
		}
		DXL2_System::logWrite(LOG_ERROR, "INF", "\"%s\" is not a valid item class.", str);
		return INF_CLASS_COUNT;
	}

	InfSubClass getInfSubClass(const char* str, InfClass iclass)
	{
		if (iclass == INF_CLASS_ELEVATOR)
		{
			for (u32 i = 0; i < ELEVATOR_COUNT; i++)
			{
				if (strcasecmp(str, c_elevatorSubClasses[i]) == 0)
				{
					return InfSubClass(i);
				}
			}
			DXL2_System::logWrite(LOG_ERROR, "INF", "\"%s\" is not a valid item subclass for \"Elevator.\"", str);
			return SUBCLASS_NONE;
		}
		else if (iclass == INF_CLASS_TRIGGER)
		{
			if (strcasecmp(str, "standard") == 0)
			{
				return TRIGGER_STANDARD;
			}
			else if (strcasecmp(str, "switch1") == 0)
			{
				return TRIGGER_SWITCH1;
			}
			else if (strcasecmp(str, "single") == 0)
			{
				return TRIGGER_SINGLE;
			}
			else if (strcasecmp(str, "toggle") == 0)
			{
				return TRIGGER_TOGGLE;
			}
			return TRIGGER;
		}
		else if (iclass == INF_CLASS_TELEPORTER)
		{
			if (strcasecmp(str, "chute") == 0)
			{
				return TELEPORTER_CHUTE;
			}
			DXL2_System::logWrite(LOG_ERROR, "INF", "\"%s\" is not a valid item subclass for \"Teleporter.\"", str);
			return SUBCLASS_NONE;
		}

		DXL2_System::logWrite(LOG_ERROR, "INF", "Invalid item class ID %u", iclass);
		return SUBCLASS_NONE;
	}
};
