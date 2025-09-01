#include <cstring>

#include "logic.h"
#include "pickup.h"
#include "player.h"
#include "updateLogic.h"
#include "animLogic.h"
#include "vueLogic.h"
#include "projectile.h"
#include <TFE_DarkForces/Actor/actorSerialization.h>
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Jedi/Level/robjData.h>
#include <TFE_Jedi/Memory/allocator.h>
#include <TFE_Jedi/Collision/collision.h>
#include <TFE_Jedi/Level/rwall.h>
#include <TFE_Jedi/Serialization/serialization.h>
#include <TFE_DarkForces/generator.h>
#include <TFE_DarkForces/random.h>
#include <TFE_System/system.h>
#include <TFE_Settings/settings.h>

// Regular Enemies
#include <TFE_DarkForces/Actor/exploders.h>
#include <TFE_DarkForces/Actor/flyers.h>
#include <TFE_DarkForces/Actor/mousebot.h>
#include <TFE_DarkForces/Actor/scenery.h>
#include <TFE_DarkForces/Actor/troopers.h>
#include <TFE_DarkForces/Actor/enemies.h>
#include <TFE_DarkForces/Actor/sewer.h>
#include <TFE_DarkForces/Actor/turret.h>
#include <TFE_DarkForces/Actor/welder.h>

// Bosses
#include <TFE_DarkForces/Actor/dragon.h>
#include <TFE_DarkForces/Actor/phaseOne.h>
#include <TFE_DarkForces/Actor/phaseTwo.h>
#include <TFE_DarkForces/Actor/phaseThree.h>
#include <TFE_DarkForces/Actor/bobaFett.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	char s_objSeqArg0[256];
	char s_objSeqArg1[256];
	char s_objSeqArg2[256];
	char s_objSeqArg3[256];
	char s_objSeqArg4[256];
	char s_objSeqArg5[256];
	s32  s_objSeqArgCount;

	void obj_addLogic(SecObject* obj, Logic* logic, LogicType type, Task* task, LogicCleanupFunc cleanupFunc)
	{
		if (!obj->logic)
		{
			obj->logic = allocator_create(sizeof(Logic**));
		}

		Logic** logicItem = (Logic**)allocator_newItem((Allocator*)obj->logic);
		if (!logicItem)
			return;
		*logicItem = logic;
		logic->obj = obj;
		logic->type = type;
		logic->parent = logicItem;
		logic->task = task;
		logic->cleanupFunc = cleanupFunc;
	}

	void deleteLogicAndObject(Logic* logic)
	{
		if (s_freeObjLock) { return; }

		SecObject* obj = logic->obj;
		Logic** parent = logic->parent;
		Logic** logics = (Logic**)allocator_getHead_noIterUpdate((Allocator*)obj->logic);

		allocator_deleteItem((Allocator*)obj->logic, parent);
		if (parent == logics)
		{
			freeObject(obj);
		}
	}

	JBool logic_defaultSetupFunc(SecObject* obj, KEYWORD key)
	{
		char* endPtr;
		JBool retValue = JTRUE;
		if (key == KW_EYE)
		{
			player_setupEyeObject(obj);
		}
		else if (key == KW_BOSS)
		{
			obj->flags |= OBJ_FLAG_BOSS;
		}
		else if (key == KW_HEIGHT)
		{
			obj->worldHeight = floatToFixed16(strtof(s_objSeqArg1, &endPtr));
		}
		else if (key == KW_RADIUS)
		{
			obj->worldWidth = floatToFixed16(strtof(s_objSeqArg1, &endPtr));
		}
		// TFE - scripting
		else if (key == KW_NAME)
		{
			TFE_Jedi::obj_addName(s_objSeqArg1, obj);
		}
		// New in TFE
		else if (key == KW_CAMERA)
		{
			obj->flags |= OBJ_FLAG_CAMERA;
		}
		else  // Invalid key.
		{
			retValue = JFALSE;
		}
		return retValue;
	}

	JBool object_parseSeq(SecObject* obj, TFE_Parser* parser, size_t* bufferPos)
	{
		LogicSetupFunc setupFunc = nullptr;

		const char* line = parser->readLine(*bufferPos);
		if (!line || !strstr(line, "SEQ"))
		{
			return JFALSE;
		}

		Logic* newLogic = nullptr;
		while (1)
		{
			line = parser->readLine(*bufferPos);
			if (!line) { return JFALSE; }

			s_objSeqArgCount = sscanf(line, " %s %s %s %s %s %s", s_objSeqArg0, s_objSeqArg1, s_objSeqArg2, s_objSeqArg3, s_objSeqArg4, s_objSeqArg5);
			KEYWORD key = getKeywordIndex(s_objSeqArg0);
			if (key == KW_TYPE || key == KW_LOGIC)
			{
				KEYWORD logicId = getKeywordIndex(s_objSeqArg1);
				
				// First, search the externally defined logics for a match (if the setting is enabled)
				TFE_ExternalData::CustomActorLogic* customLogic = (logicId != KW_PLAYER)
					? tryFindCustomActorLogic(s_objSeqArg1)
					: nullptr;		// do not allow "LOGIC: PLAYER" to be overridden !!

				if (TFE_Settings::jsonAiLogics() && customLogic)
				{
					newLogic = obj_setCustomActorLogic(obj, customLogic);
					setupFunc = nullptr;
				}
				// Then go to the hardcoded logics
				else if (logicId == KW_PLAYER)  // Player Logic.
				{
					player_setupObject(obj);
					setupFunc = nullptr;
				}
				else if (logicId == KW_ANIM)	// Animated Sprites Logic.
				{
					newLogic = obj_setSpriteAnim(obj);
					setupFunc = nullptr;
				}
				else if (logicId == KW_UPDATE)	// "Update" logic is usually used for rotating 3D objects, like the Death Star.
				{
					newLogic = obj_setUpdate(obj, &setupFunc);
				}
				else if (logicId >= KW_TROOP && logicId <= KW_SCENERY)	// Enemies, explosives barrels, land mines, and scenery.
				{
					newLogic = obj_setEnemyLogic(obj, logicId);
					setupFunc = nullptr;
				}
				else if (logicId == KW_KEY)         // Vue animation logic.
				{
					newLogic = obj_createVueLogic(obj, &setupFunc);
				}
				else if (logicId == KW_GENERATOR)	// Enemy generator, used for in-level enemy spawning.
				{
					KEYWORD genType = getKeywordIndex(s_objSeqArg2);
					newLogic = obj_createGenerator(obj, &setupFunc, genType, s_objSeqArg2);
				}
				else if (logicId == KW_DISPATCH)
				{
					newLogic = (Logic*)actor_createDispatch(obj, &setupFunc);
				}
				else if ((logicId >= KW_BATTERY && logicId <= KW_AUTOGUN) || logicId == KW_ITEM)
				{
					if (logicId >= KW_BATTERY && logicId <= KW_AUTOGUN)
					{
						strcpy(s_objSeqArg2, s_objSeqArg1);
					}
					ItemId itemId = getPickupItemId(s_objSeqArg2);
					obj_createPickup(obj, itemId);
					setupFunc = nullptr;
				}
				else if (TFE_Settings::enableUnusedItem() && strcasecmp(s_objSeqArg1, "ITEM10") == 0)
				{
					obj_createPickup(obj, ITEM_UNUSED);
					setupFunc = nullptr;
				}
			}
			else if (key == KW_SEQEND)
			{
				return JTRUE;
			}
			else
			{
				if (setupFunc && setupFunc(newLogic, key))
				{
					continue;
				}
				logic_defaultSetupFunc(obj, key);
			}
		}

		return JTRUE;
	}

	Logic* obj_setEnemyLogic(SecObject* obj, KEYWORD logicId)
	{
		obj->flags |= OBJ_FLAG_AIM;
		LogicSetupFunc* setupFunc = nullptr;

		switch (logicId)
		{
			case KW_TROOP:
			case KW_STORM1:
			{
				obj->entityFlags = ETFLAG_AI_ACTOR | ETFLAG_HAS_GRAVITY;
				return trooper_setup(obj, setupFunc);
			} break;
			case KW_INT_DROID:
			{
				obj->entityFlags = (ETFLAG_AI_ACTOR | ETFLAG_FLYING);
				return intDroid_setup(obj, setupFunc);
			} break;
			case KW_PROBE_DROID:
			{
				obj->entityFlags = (ETFLAG_AI_ACTOR | ETFLAG_FLYING);
				return probeDroid_setup(obj, setupFunc);
			} break;
			case KW_D_TROOP1:
			{
				return phaseOne_setup(obj, setupFunc);
			} break;
			case KW_D_TROOP2:
			{
				return phaseTwo_setup(obj, setupFunc);
			} break;
			case KW_D_TROOP3:
			{
				return phaseThree_setup(obj, setupFunc);
			} break;
			case KW_BOBA_FETT:
			{
				return bobaFett_setup(obj, setupFunc);
			} break;
			case KW_COMMANDO:
			{
				obj->entityFlags = ETFLAG_AI_ACTOR | ETFLAG_HAS_GRAVITY;
				return commando_setup(obj, setupFunc);
			} break;
			case KW_I_OFFICER:
			case KW_I_OFFICER1:
			case KW_I_OFFICER2:
			case KW_I_OFFICER3:
			case KW_I_OFFICER4:
			case KW_I_OFFICER5:
			case KW_I_OFFICER6:
			case KW_I_OFFICER7:
			case KW_I_OFFICER8:
			case KW_I_OFFICER9:
			case KW_I_OFFICERR:
			case KW_I_OFFICERY:
			case KW_I_OFFICERB:
			{
				obj->entityFlags = ETFLAG_AI_ACTOR | ETFLAG_HAS_GRAVITY;
				return officer_setup(obj, setupFunc, logicId);
			} break;
			case KW_G_GUARD:
			{
				obj->entityFlags = ETFLAG_AI_ACTOR | ETFLAG_HAS_GRAVITY;
				return gamor_setup(obj, setupFunc);
			} break;
			case KW_REE_YEES:
			{
				obj->entityFlags = ETFLAG_AI_ACTOR | ETFLAG_HAS_GRAVITY;
				return reeyees_setup(obj, setupFunc);
			} break;
			case KW_REE_YEES2:
			{
				obj->entityFlags = ETFLAG_AI_ACTOR | ETFLAG_HAS_GRAVITY;
				return reeyees2_setup(obj, setupFunc);
			} break;
			case KW_BOSSK:
			{
				obj->entityFlags = ETFLAG_AI_ACTOR | ETFLAG_HAS_GRAVITY;
				return bossk_setup(obj, setupFunc);
			} break;
			case KW_BARREL:
			{
				obj->entityFlags = ETFLAG_AI_ACTOR;
				return barrel_setup(obj, setupFunc);
			} break;
			case KW_LAND_MINE:
			{
				ProjectileLogic* mineLogic = (ProjectileLogic*)createProjectile(PROJ_LAND_MINE_PLACED, obj->sector, obj->posWS.x, obj->posWS.y, obj->posWS.z, obj);
				freeObject(obj);

				SecObject* mineObj = mineLogic->logic.obj;
				mineObj->entityFlags |= ETFLAG_LANDMINE;
				mineObj->projectileLogic = (Logic*)mineLogic;

				landmine_setup(mineObj, setupFunc);
				return (Logic*)mineLogic;
			} break;
			case KW_KELL:
			{
				return kellDragon_setup(obj, setupFunc);
			} break;
			case KW_SEWER1:
			{
				return sewerCreature_setup(obj, setupFunc);
			} break;
			case KW_REMOTE:
			{
				return remote_setup(obj, setupFunc);
			} break;
			case KW_TURRET:
			{
				return turret_setup(obj, setupFunc);
			} break;
			case KW_MOUSEBOT:
			{
				return mousebot_setup(obj, setupFunc);
			} break;
			case KW_WELDER:
			{
				return welder_setup(obj, setupFunc);
			} break;
			case KW_SCENERY:
			{
				obj->flags &= ~OBJ_FLAG_AIM;
				obj->entityFlags = ETFLAG_SCENERY;
				return scenery_setup(obj, setupFunc);
			} break;
		}

		TFE_System::logWrite(LOG_ERROR, "Logic", "Unknown logic type - %d.", logicId);
		return nullptr;
	}

	SecObject* logic_spawnEnemy(const char* waxName, const char* typeName)
	{
		Wax* wax = TFE_Sprite_Jedi::getWax(waxName);
		if (!wax)
		{
			return nullptr;
		}
		KEYWORD type = getKeywordIndex(typeName);
		if (type == KW_UNKNOWN || type < KW_TROOP || type > KW_SCENERY)
		{
			return nullptr;
		}

		vec3_fixed pos = s_playerObject->posWS;
		fixed16_16 sinYaw, cosYaw;
		sinCosFixed(s_playerObject->yaw, &sinYaw, &cosYaw);

		vec3_fixed spawnPos;
		fixed16_16 dist = FIXED(8);
		spawnPos.x = pos.x + mul16(sinYaw, dist);
		spawnPos.y = pos.y;
		spawnPos.z = pos.z + mul16(cosYaw, dist);

		RSector* sector = s_playerObject->sector;
		RWall* wall = collision_wallCollisionFromPath(sector, pos.x, pos.z, spawnPos.x, spawnPos.z);
		fixed16_16 floorHeight = sector->floorHeight;
		fixed16_16 ceilingHeight = sector->ceilingHeight;
		while (wall)
		{
			if (wall->nextSector)
			{
				RSector* next = wall->nextSector;
				floorHeight = min(floorHeight, next->floorHeight);
				ceilingHeight = max(ceilingHeight, next->ceilingHeight);
				fixed16_16 opening = floorHeight - ceilingHeight;
				if (opening < FIXED(5) || TFE_Jedi::abs(sector->floorHeight - next->floorHeight) > FIXED(2))
				{
					break;
				}

				sector = next;
				wall = collision_wallCollisionFromPath(sector, pos.x, pos.z, spawnPos.x, spawnPos.z);
			}
			else
			{
				break;
			}
		}

		if (wall)
		{
			return nullptr;
		}

		SecObject* spawn = allocateObject();
		spawn->posWS = spawnPos;
		spawn->yaw = random_next() & ANGLE_MASK;
		sector_addObject(sector, spawn);

		sprite_setData(spawn, wax);
		obj_setEnemyLogic(spawn, type);

		obj_addToRefList(spawn, ObjRefType_ConsoleSpawn);	// scripting
		return spawn;
	}

	///////////////////////////////////////////////////
	// Logic Serialization
	// TODO: It probably makes sense to move this to
	// its own file at some point.
	///////////////////////////////////////////////////
	// Type -> Serialization function tables.
	typedef void(*LogicSerializationFunc)(Logic*&, SecObject* obj, Stream*);
	LogicSerializationFunc c_serializeFn[] =
	{
		actorDispatch_serialize,      // LOGIC_DISPATCH
		bobaFett_serialize,           // LOGIC_BOBA_FETT
		kellDragon_serialize,         // LOGIC_DRAGON
		mousebot_serialize,           // LOGIC_MOUSEBOT
		phaseOne_serialize,           // LOGIC_PHASE_ONE
		phaseTwo_serialize,           // LOGIC_PHASE_TWO
		phaseThree_serialize,         // LOGIC_PHASE_THREE
		turret_serialize,             // LOGIC_TURRET
		welder_serialize,             // LOGIC_WELDER
		animLogic_serialize,          // LOGIC_ANIM
		updateLogic_serialize,        // LOGIC_UPDATE
		generatorLogic_serialize,     // LOGIC_GENERATOR
		pickupLogic_serialize,        // LOGIC_PICKUP
		playerLogic_serialize,        // LOGIC_PLAYER
		projLogic_serialize,          // LOGIC_PROJECTILE
		vueLogic_serialize,           // LOGIC_VUE
		nullptr,                      // LOGIC_UNKNOWN
	};

	// Root serialization functions for Logics.
	void logic_serialize(Logic*& logic, SecObject* obj, Stream* stream)
	{
		LogicType type;
		if (serialization_getMode() == SMODE_WRITE)
		{
			type = logic->type;
		}

		SERIALIZE(ObjState_InitVersion, type, LOGIC_UNKNOWN);
		const u32 index = type < LOGIC_UNKNOWN ? type : LOGIC_UNKNOWN;

		if (c_serializeFn[index])
		{
			c_serializeFn[index](logic, obj, stream);
			if (logic) { logic->type = type; }
		}
		else
		{
			TFE_System::logWrite(LOG_ERROR, "Logic", "Serialize - invalid logic type: %d.", type);
			if (logic) { logic->type = LOGIC_UNKNOWN; }
			assert(0);
		}
	}


	///////////////////////////////////////////////////
	// Custom logics
	///////////////////////////////////////////////////
	Logic* obj_setCustomActorLogic(SecObject* obj, TFE_ExternalData::CustomActorLogic* customLogic)
	{
		obj->flags |= OBJ_FLAG_AIM;
		obj->entityFlags = ETFLAG_AI_ACTOR;

		if (customLogic->isFlying) { obj->entityFlags |= ETFLAG_FLYING; }
		if (customLogic->hasGravity) { obj->entityFlags |= ETFLAG_HAS_GRAVITY; }

		LogicSetupFunc* setupFunc = nullptr;
		return custom_actor_setup(obj, customLogic, setupFunc);
	}

	TFE_ExternalData::CustomActorLogic* tryFindCustomActorLogic(const char* logicName)
	{
		TFE_ExternalData::ExternalLogics* externalLogics = TFE_ExternalData::getExternalLogics();
		u32 actorCount = (u32)externalLogics->actorLogics.size();

		for (u32 a = 0; a < actorCount; a++)
		{
			if (strcasecmp(logicName, externalLogics->actorLogics[a].logicName) == 0)
			{
				return &externalLogics->actorLogics[a];
			}
		}

		return nullptr;
	}
}  // TFE_DarkForces
