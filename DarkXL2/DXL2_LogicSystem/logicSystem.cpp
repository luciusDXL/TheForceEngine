#include "logicSystem.h"
#include <DXL2_Asset/spriteAsset.h>
#include <DXL2_Asset/modelAsset.h>
#include <DXL2_Asset/gameMessages.h>
#include <DXL2_Game/gameHud.h>
#include <DXL2_Game/gameConstants.h>
#include <DXL2_ScriptSystem/scriptSystem.h>
#include <DXL2_System/system.h>
#include <DXL2_FileSystem/filestream.h>
#include <DXL2_FileSystem/paths.h>
#include <DXL2_Game/gameObject.h>
#include <DXL2_Game/player.h>
#include <DXL2_InfSystem/infSystem.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <algorithm>

using namespace DXL2_GameConstants;

void DXL2_PrintVec3f(std::string& msg, Vec3f value0)
{
	DXL2_System::logWrite(LOG_MSG, "Scripting", "%s (%f, %f, %f)", msg.c_str(), value0.x, value0.y, value0.z);
}

namespace DXL2_LogicSystem
{
	static const char* c_logicScriptName[LOGIC_COUNT] =
	{
		// Player
		"logic_player",		// LOGIC_PLAYER
		// General
		"logic_shield",		// LOGIC_SHIELD
		"logic_battery",	// LOGIC_BATTERY
		"logic_cleats",		// LOGIC_CLEATS
		"logic_goggles",	// LOGIC_GOGGLES
		"logic_mask",		// LOGIC_MASK
		"logic_medkit",		// LOGIC_MEDKIT
		// Weapons -
		"logic_rifle",		// LOGIC_RIFLE
		"logic_autogun",	// LOGIC_AUTOGUN
		"logic_fusion",		// LOGIC_FUSION
		"logic_mortar",		// LOGIC_MORTAR
		"logic_concussion",	// LOGIC_CONCUSSION
		"logic_cannon",		// LOGIC_CANNON
		// Ammo -
		"logic_energy",		// LOGIC_ENERGY
		"logic_detonator",	// LOGIC_DETONATOR
		"logic_detonators",	// LOGIC_DETONATORS
		"logic_power",		// LOGIC_POWER
		"logic_mine",		// LOGIC_MINE
		"logic_mines",		// LOGIC_MINES
		"logic_shell",		// LOGIC_SHELL
		"logic_shells",		// LOGIC_SHELLS
		"logic_plasma",		// LOGIC_PLASMA
		"logic_missile",	// LOGIC_MISSILE
		"logic_missiles",	// LOGIC_MISSILES
		// Bonuses -
		"logic_supercharge",// LOGIC_SUPERCHARGE
		"logic_invincible",	// LOGIC_INVINCIBLE
		"logic_life",		// LOGIC_LIFE
		"logic_revive",		// LOGIC_REVIVE
		// Keys -
		"logic_blue",		// LOGIC_BLUE
		"logic_red",		// LOGIC_RED
		"logic_yellow",		// LOGIC_YELLOW
		"logic_code1",		// LOGIC_CODE1
		"logic_code2",		// LOGIC_CODE2
		"logic_code3",		// LOGIC_CODE3
		"logic_code4",		// LOGIC_CODE4
		"logic_code5",		// LOGIC_CODE5
		"logic_code6",		// LOGIC_CODE6
		"logic_code7",		// LOGIC_CODE7
		"logic_code8",		// LOGIC_CODE8
		"logic_code9",		// LOGIC_CODE9
		// Goal items -
		"logic_datatape",	// LOGIC_DATATAPE
		"logic_plans",		// LOGIC_PLANS
		"logic_dt_weapon",	// LOGIC_DT_WEAPON
		"logic_nava",		// LOGIC_NAVA
		"logic_phrik",		// LOGIC_PHRIK
		"logic_pile",		// LOGIC_PILE
		// Enemy logics
		"logic_generator",	// LOGIC_GENERATOR
		// Imperials -
		"logic_i_officer",	// LOGIC_I_OFFICER
		"logic_i_officerr",	// LOGIC_I_OFFICERR
		"logic_i_officerb",	// LOGIC_I_OFFICERB
		"logic_i_officery",	// LOGIC_I_OFFICERY
		"logic_i_officer1",	// LOGIC_I_OFFICER1
		"logic_i_officer2",	// LOGIC_I_OFFICER2
		"logic_i_officer3",	// LOGIC_I_OFFICER3
		"logic_i_officer4",	// LOGIC_I_OFFICER4
		"logic_i_officer5",	// LOGIC_I_OFFICER5
		"logic_i_officer6",	// LOGIC_I_OFFICER6
		"logic_i_officer7",	// LOGIC_I_OFFICER7
		"logic_i_officer8",	// LOGIC_I_OFFICER8
		"logic_i_officer9",	// LOGIC_I_OFFICER9
		"logic_troop",		// LOGIC_TROOP
		"logic_storm1",		// LOGIC_STORM1
		"logic_commando",	// LOGIC_COMMANDO
		// Aliens -
		"logic_bossk",		// LOGIC_BOSSK
		"logic_g_guard",	// LOGIC_G_GUARD
		"logic_ree_yees",	// LOGIC_REE_YEES
		"logic_ree_yees2",	// LOGIC_REE_YEES2
		"logic_sewer1",		// LOGIC_SEWER1
		// Robots -
		"logic_int_droid",	// LOGIC_INT_DROID
		"logic_probe_droid",// LOGIC_PROBE_DROID
		"logic_remote",		// LOGIC_REMOTE
		// Bosses -
		"logic_boba_fett",	// LOGIC_BOBA_FETT
		"logic_kell",		// LOGIC_KELL
		"logic_d_troop1",	// LOGIC_D_TROOP1
		"logic_d_troop2",	// LOGIC_D_TROOP2
		"logic_d_troop3",	// LOGIC_D_TROOP3
		// Special sprite logics
		"logic_scenery",	// LOGIC_SCENERY
		"logic_anim",		// LOGIC_ANIM
		"logic_barrel",		// LOGIC_BARREL
		"logic_land_mine",	// LOGIC_LAND_MINE
		// 3D object logics
		"logic_turret",		// LOGIC_TURRET
		"logic_mousebot",	// LOGIC_MOUSEBOT
		"logic_welder",		// LOGIC_WELDER
		// 3D object motion logics
		"logic_update",		// LOGIC_UPDATE
		"logic_key",		// LOGIC_KEY
	};

	struct ScriptLogic
	{
		// Logic Name
		std::string name;

		// Script functions (may be NULL)
		SCRIPT_FUNC_PTR start;
		SCRIPT_FUNC_PTR stop;
		SCRIPT_FUNC_PTR tick;
		SCRIPT_FUNC_PTR handleMessage;

		// Logic Parameters, these come from the level data or spawn.
		Logic param;
	};

	struct ScriptGenerator
	{
		// Logic Name
		std::string name;

		// Script functions (may be NULL)
		SCRIPT_FUNC_PTR start;
		SCRIPT_FUNC_PTR stop;
		SCRIPT_FUNC_PTR tick;
		SCRIPT_FUNC_PTR handleMessage;

		// Logic Parameters, these come from the level data or spawn.
		EnemyGenerator param;
	};
	
	struct ScriptObject
	{
		GameObject* gameObj = nullptr;
		std::vector<ScriptLogic> logic;
		std::vector<ScriptGenerator> generator;
	};

	enum LogicMsg
	{
		LOGIC_MSG_DAMAGE = 0,
		LOGIC_MSG_PLAYER_COLLISION,
	};

	struct LogicToAdd
	{
		GameObject* obj;
		LogicType logicType;
	};
	std::vector<LogicToAdd> s_logicsToAdd;

	static GameObject* s_self = nullptr;
	static Logic* s_param = nullptr;
	static EnemyGenerator* s_genParam = nullptr;
	static bool s_initialized = false;
	static std::vector<ScriptObject> s_scriptObjects;

	static Player* s_player;

	static f32 s_accum = 0.0f;

	void registerKeyTypes();
	void registerLogicTypes();

	void clearObjectLogics()
	{
		s_scriptObjects.clear();
		s_logicsToAdd.clear();
	}

	bool registerObjectLogics(GameObject* gameObject, const std::vector<Logic>& logics, const std::vector<EnemyGenerator>& generators)
	{
		if (s_scriptObjects.size() <= gameObject->id)
		{
			s_scriptObjects.resize(gameObject->id + 1);
		}

		const size_t index = gameObject->id;
		const size_t logicCount = logics.size();
		const size_t genCount = generators.size();

		ScriptObject& newObject = s_scriptObjects[index];
		newObject.gameObj = gameObject;
		newObject.logic.resize(logicCount);
		newObject.generator.resize(genCount);

		ScriptLogic* scriptLogic = newObject.logic.data();
		for (size_t i = 0; i < logicCount; i++, scriptLogic++)
		{
			const LogicType type = logics[i].type;
			scriptLogic->name = c_logicScriptName[type];
			if (DXL2_ScriptSystem::loadScriptModule(SCRIPT_TYPE_LOGIC, scriptLogic->name.c_str()))
			{
				scriptLogic->param = logics[i];
				scriptLogic->start = DXL2_ScriptSystem::getScriptFunction(scriptLogic->name.c_str(), "void start()");
				scriptLogic->stop  = DXL2_ScriptSystem::getScriptFunction(scriptLogic->name.c_str(), "void stop()");
				scriptLogic->tick  = DXL2_ScriptSystem::getScriptFunction(scriptLogic->name.c_str(), "void tick()");
				scriptLogic->handleMessage = DXL2_ScriptSystem::getScriptFunction(scriptLogic->name.c_str(), "void handleMessage(int, int, int)");

				// Execute the start function if it exists.
				if (scriptLogic->start)
				{
					s_self = gameObject;
					s_param = &scriptLogic->param;
					DXL2_ScriptSystem::executeScriptFunction(SCRIPT_TYPE_LOGIC, scriptLogic->start);
				}
			}
		}

		ScriptGenerator* scriptGen = newObject.generator.data();
		for (size_t i = 0; i < genCount; i++, scriptGen++)
		{
			const LogicType type = generators[i].type;
			scriptGen->name = c_logicScriptName[LOGIC_GENERATOR];
			if (DXL2_ScriptSystem::loadScriptModule(SCRIPT_TYPE_LOGIC, scriptGen->name.c_str()))
			{
				scriptGen->param = generators[i];
				scriptGen->start = DXL2_ScriptSystem::getScriptFunction(scriptGen->name.c_str(), "void start()");
				scriptGen->stop = DXL2_ScriptSystem::getScriptFunction(scriptGen->name.c_str(), "void stop()");
				scriptGen->tick = DXL2_ScriptSystem::getScriptFunction(scriptGen->name.c_str(), "void tick()");
				scriptGen->handleMessage = DXL2_ScriptSystem::getScriptFunction(scriptGen->name.c_str(), "void handleMessage(int, int, int)");

				// Execute the start function if it exists.
				if (scriptGen->start)
				{
					s_self = gameObject;
					s_genParam = &scriptGen->param;
					DXL2_ScriptSystem::executeScriptFunction(SCRIPT_TYPE_LOGIC, scriptGen->start);
				}
			}
		}

		return true;
	}

	void damageObject(GameObject* gameObject, s32 damage, DamageType type)
	{
		if (gameObject->id >= s_scriptObjects.size()) { return; }

		ScriptObject* obj = &s_scriptObjects[gameObject->id];
		size_t count = obj->logic.size();
		ScriptLogic* logic = obj->logic.data();
		for (size_t i = 0; i < count; i++)
		{
			if (logic[i].handleMessage && gameObject->collisionFlags)
			{
				s_self = (GameObject*)gameObject;
				s_param = &logic[i].param;
				DXL2_ScriptSystem::executeScriptFunction(SCRIPT_TYPE_LOGIC, logic[i].handleMessage, LOGIC_MSG_DAMAGE, damage, type);

				if (type == DMG_EXPLOSION)
				{
					gameObject->pos.y -= 0.2f;
					gameObject->verticalVel -= 32.0f;
				}
			}
		}
	}

	void sendPlayerCollisionTrigger(const GameObject* gameObject)
	{
		ScriptObject* obj = &s_scriptObjects[gameObject->id];
		size_t count = obj->logic.size();
		ScriptLogic* logic = obj->logic.data();
		for (size_t i = 0; i < count; i++)
		{
			if (logic[i].handleMessage)
			{
				s_self = (GameObject*)gameObject;
				s_param = &logic[i].param;
				DXL2_ScriptSystem::executeScriptFunction(SCRIPT_TYPE_LOGIC, logic[i].handleMessage, LOGIC_MSG_PLAYER_COLLISION, 0, 0);
			}
		}
	}

	void DXL2_ChangeAngles(s32 objectId, Vec3f angleChange)
	{
		if (objectId < 0 || objectId >= s_scriptObjects.size()) { return; }

		ScriptObject* obj = &s_scriptObjects[objectId];
		obj->gameObj->angles.x += angleChange.x;
		obj->gameObj->angles.y += angleChange.y;
		obj->gameObj->angles.z += angleChange.z;
	}
	
	f32 DXL2_GetAnimFramerate(s32 objectId, s32 animationId)
	{
		if (objectId < 0 || objectId >= s_scriptObjects.size()) { return 0.0f; }

		ScriptObject* obj = &s_scriptObjects[objectId];
		if (obj->gameObj->oclass != CLASS_SPRITE) { return 0.0f; }
		return (f32)obj->gameObj->sprite->anim[animationId].frameRate;
	}

	u32 DXL2_GetAnimFrameCount(s32 objectId, s32 animationId)
	{
		if (objectId < 0 || objectId >= s_scriptObjects.size()) { return 0; }

		ScriptObject* obj = &s_scriptObjects[objectId];
		if (obj->gameObj->oclass != CLASS_SPRITE) { return 0; }
		return obj->gameObj->sprite->anim[animationId].angles[0].frameCount;
	}

	void DXL2_SetAnimFrame(s32 objectId, s32 animationId, s32 frameIndex)
	{
		if (objectId < 0 || objectId >= s_scriptObjects.size()) { return; }

		ScriptObject* obj = &s_scriptObjects[objectId];
		if (obj->gameObj->oclass != CLASS_SPRITE) { return; }

		obj->gameObj->animId = animationId;
		obj->gameObj->frameIndex = frameIndex;
	}

	void DXL2_SetCollision(s32 objectId, f32 radius, f32 height, u32 collisionFlags)
	{
		if (objectId < 0 || objectId >= s_scriptObjects.size()) { return; }
		ScriptObject* obj = &s_scriptObjects[objectId];

		obj->gameObj->collisionRadius = radius;
		obj->gameObj->collisionHeight = height;
		obj->gameObj->collisionFlags  = collisionFlags;
	}

	void DXL2_SetPhysics(s32 objectId, u32 physicsFlags)
	{
		if (objectId < 0 || objectId >= s_scriptObjects.size()) { return; }
		ScriptObject* obj = &s_scriptObjects[objectId];

		obj->gameObj->physicsFlags = physicsFlags;
		obj->gameObj->verticalVel = 0.0f;
	}

	void DXL2_Hide(s32 objectId)
	{
		if (objectId < 0 || objectId >= s_scriptObjects.size()) { return; }
		ScriptObject* obj = &s_scriptObjects[objectId];

		obj->gameObj->show = false;
	}

	Vec3f DXL2_BiasTowardsPlayer(Vec3f pos, f32 bias)
	{
		Vec2f dir = { pos.x - s_player->pos.x, pos.z - s_player->pos.z };
		f32 distSq = dir.x*dir.x + dir.z*dir.z;
		if (distSq > FLT_EPSILON)
		{
			f32 scale = bias / sqrtf(distSq);
			dir.x *= scale;
			dir.z *= scale;
		}
		pos.x -= dir.x;
		pos.z -= dir.z;
		return pos;
	}

	s32 DXL2_SpawnObject(Vec3f pos, Vec3f angles, s32 sectorId, s32 objectClass, std::string& objectName)
	{
		GameObjectList* gameObjList = LevelGameObjects::getGameObjectList();
		SectorObjectList* sectorObjList = LevelGameObjects::getSectorObjectList();

		// Add a new game object.
		s32 objectId = (s32)gameObjList->size();
		gameObjList->push_back({});

		GameObject* obj = &(*gameObjList)[objectId];
		obj->animId = 0;
		obj->collisionHeight = 0.0f;
		obj->collisionRadius = 0.0f;
		obj->physicsFlags = PHYSICS_GRAVITY;	// Gravity is the default.
		obj->verticalVel = 0.0f;
		obj->flags = 0;
		obj->frameIndex = 0;
		obj->fullbright = false;
		obj->hp = 0;
		obj->state = 0;
		obj->time = 0.0f;
		obj->show = true;

		obj->oclass = ObjectClass(objectClass);
		obj->id = objectId;
		obj->sectorId = sectorId;
		obj->angles = angles;
		obj->pos = pos;

		if (objectClass == CLASS_SPRITE)
		{
			obj->sprite = DXL2_Sprite::getSprite(objectName.c_str());
		}
		else if (objectClass == CLASS_FRAME)
		{
			obj->frame = DXL2_Sprite::getFrame(objectName.c_str());
		}
		else if (objectClass == CLASS_3D)
		{
			obj->model = DXL2_Model::get(objectName.c_str());
		}

		(*sectorObjList)[sectorId].list.push_back(objectId);
		return objectId;
	}
		
	// TODO: Support setting parameters.
	void DXL2_AddLogic(s32 objectId, s32 logicId)
	{
		if (objectId < 0) { return; }
		GameObjectList* gameObjList = LevelGameObjects::getGameObjectList();
		GameObject* obj = &(*gameObjList)[objectId];

		s_logicsToAdd.push_back({obj, LogicType(logicId)});
	}

	void DXL2_GivePlayerKey(s32 keyType)
	{
		s_player->m_keys |= keyType;
	}

	void DXL2_AddGoalItem()
	{
		DXL2_InfSystem::advanceCompleteElevator();
	}

	void DXL2_BossKilled()
	{
		DXL2_InfSystem::advanceBossElevator();
	}

	void DXL2_MohcKilled()
	{
		DXL2_InfSystem::advanceMohcElevator();
	}

	void DXL2_GiveGear()
	{
		// TODO: Give gear back...
	}

	void DXL2_PrintMessage(u32 msgId)
	{
		DXL2_GameHud::setMessage(DXL2_GameMessages::getMessage(msgId));
	}

	bool init(Player* player)
	{
		clearObjectLogics();
		s_accum = 0.0f;
		s_player = player;

		if (s_initialized) { return true; }
		s_initialized = true;
		// register script functions.

		// register types.
		// Math types (move into math)
		std::vector<ScriptTypeProp> prop;
		prop.reserve(16);

		prop.clear();
		prop.push_back({ "float x", offsetof(Vec2f, x) });
		prop.push_back({ "float z", offsetof(Vec2f, z) });
		DXL2_ScriptSystem::registerValueType("Vec2f", sizeof(Vec2f), DXL2_ScriptSystem::getTypeTraits<Vec2f>(), prop);

		prop.clear();
		prop.push_back({ "int x", offsetof(Vec2i, x) });
		prop.push_back({ "int z", offsetof(Vec2i, z) });
		DXL2_ScriptSystem::registerValueType("Vec2i", sizeof(Vec2i), DXL2_ScriptSystem::getTypeTraits<Vec2i>(), prop);

		prop.clear();
		prop.push_back({"float x", offsetof(Vec3f, x)});
		prop.push_back({"float y", offsetof(Vec3f, y)});
		prop.push_back({"float z", offsetof(Vec3f, z)});
		DXL2_ScriptSystem::registerValueType("Vec3f", sizeof(Vec3f), DXL2_ScriptSystem::getTypeTraits<Vec3f>(), prop);

		prop.clear();
		prop.push_back({"int x", offsetof(Vec3i, x)});
		prop.push_back({"int y", offsetof(Vec3i, y)});
		prop.push_back({"int z", offsetof(Vec3i, z)});
		DXL2_ScriptSystem::registerValueType("Vec3i", sizeof(Vec3i), DXL2_ScriptSystem::getTypeTraits<Vec3i>(), prop);

		// GameObject type.
		prop.clear();
		prop.push_back({ "const int objectId",	offsetof(GameObject, id) });
		prop.push_back({ "const int sectorId",	offsetof(GameObject, sectorId) });
		prop.push_back({ "const Vec3f position",offsetof(GameObject, pos) });
		prop.push_back({ "const Vec3f angles",	offsetof(GameObject, angles) });
		prop.push_back({ "uint flags",		offsetof(GameObject, flags) });
		prop.push_back({ "uint messageMask",offsetof(GameObject, messageMask) });
		prop.push_back({ "int state",		offsetof(GameObject, state) });
		prop.push_back({ "int hp",			offsetof(GameObject, hp) });
		prop.push_back({ "float time",		offsetof(GameObject, time) });
		prop.push_back({ "const uint commonFlags", offsetof(GameObject, comFlags) });
		prop.push_back({ "const float radius", offsetof(GameObject, radius) });
		prop.push_back({ "const float height", offsetof(GameObject, height) });
		DXL2_ScriptSystem::registerRefType("GameObject", prop);

		// Logic Parameters
		prop.clear();
		prop.push_back({"const uint flags", offsetof(Logic, flags) });
		prop.push_back({"const float frameRate", offsetof(Logic, frameRate) });
		prop.push_back({"const Vec3f rotation", offsetof(Logic, rotation) });
		DXL2_ScriptSystem::registerRefType("LogicParam", prop);

		prop.clear();
		prop.push_back({ "const uint type", offsetof(EnemyGenerator, type) });
		prop.push_back({ "const float delay", offsetof(EnemyGenerator, delay) });
		prop.push_back({ "const float interval", offsetof(EnemyGenerator, interval) });
		prop.push_back({ "const float minDist", offsetof(EnemyGenerator, minDist) });
		prop.push_back({ "const float maxDist", offsetof(EnemyGenerator, maxDist) });
		prop.push_back({ "const int maxAlive", offsetof(EnemyGenerator, maxAlive) });
		prop.push_back({ "const int numTerminate", offsetof(EnemyGenerator, numTerminate) });
		prop.push_back({ "const float wanderTime", offsetof(EnemyGenerator, wanderTime) });
		DXL2_ScriptSystem::registerRefType("GenParam", prop);

		// Functions
		DXL2_ScriptSystem::registerFunction("void DXL2_Print(const string &in, Vec3f value)", SCRIPT_FUNCTION(DXL2_PrintVec3f));
		DXL2_ScriptSystem::registerFunction("void DXL2_ChangeAngles(int objectId, Vec3f angleChange)", SCRIPT_FUNCTION(DXL2_ChangeAngles));
		DXL2_ScriptSystem::registerFunction("float DXL2_GetAnimFramerate(int objectId, int animationId)", SCRIPT_FUNCTION(DXL2_GetAnimFramerate));
		DXL2_ScriptSystem::registerFunction("uint DXL2_GetAnimFrameCount(int objectId, int animationId)", SCRIPT_FUNCTION(DXL2_GetAnimFrameCount));
		DXL2_ScriptSystem::registerFunction("void DXL2_SetAnimFrame(int objectId, int animationId, int frameIndex)", SCRIPT_FUNCTION(DXL2_SetAnimFrame));
		DXL2_ScriptSystem::registerFunction("void DXL2_SetCollision(int objectId, float radius, float height, uint collisionFlags)", SCRIPT_FUNCTION(DXL2_SetCollision));
		DXL2_ScriptSystem::registerFunction("void DXL2_SetPhysics(int objectId, uint physicsFlags)", SCRIPT_FUNCTION(DXL2_SetPhysics));
		DXL2_ScriptSystem::registerFunction("void DXL2_Hide(int objectId)", SCRIPT_FUNCTION(DXL2_Hide));
		
		DXL2_ScriptSystem::registerFunction("Vec3f DXL2_BiasTowardsPlayer(Vec3f pos, float bias)", SCRIPT_FUNCTION(DXL2_BiasTowardsPlayer));
		DXL2_ScriptSystem::registerFunction("int DXL2_SpawnObject(Vec3f pos, Vec3f angles, int sectorId, int objectClass, const string &in)", SCRIPT_FUNCTION(DXL2_SpawnObject));
		DXL2_ScriptSystem::registerFunction("void DXL2_AddLogic(int objectId, int logicId)", SCRIPT_FUNCTION(DXL2_AddLogic));
		DXL2_ScriptSystem::registerFunction("void DXL2_GivePlayerKey(int keyType)", SCRIPT_FUNCTION(DXL2_GivePlayerKey));
		DXL2_ScriptSystem::registerFunction("void DXL2_AddGoalItem()", SCRIPT_FUNCTION(DXL2_AddGoalItem));
		DXL2_ScriptSystem::registerFunction("void DXL2_BossKilled()", SCRIPT_FUNCTION(DXL2_BossKilled));
		DXL2_ScriptSystem::registerFunction("void DXL2_MohcKilled()", SCRIPT_FUNCTION(DXL2_MohcKilled));
		DXL2_ScriptSystem::registerFunction("void DXL2_GiveGear()", SCRIPT_FUNCTION(DXL2_GiveGear));

		DXL2_ScriptSystem::registerFunction("void DXL2_PrintMessage(uint msgId)", SCRIPT_FUNCTION(DXL2_PrintMessage));

		// Register "self" and Logic parameters.
		DXL2_ScriptSystem::registerGlobalProperty("GameObject @self",  &s_self);
		DXL2_ScriptSystem::registerGlobalProperty("LogicParam @param", &s_param);
		DXL2_ScriptSystem::registerGlobalProperty("GenParam   @genParam", &s_genParam);

		// Register enums
		DXL2_ScriptSystem::registerEnumType("LogicMsg");
		DXL2_ScriptSystem::registerEnumValue("LogicMsg", "LOGIC_MSG_DAMAGED", LOGIC_MSG_DAMAGE);
		DXL2_ScriptSystem::registerEnumValue("LogicMsg", "LOGIC_MSG_PLAYER_COLLISION", LOGIC_MSG_PLAYER_COLLISION);

		DXL2_ScriptSystem::registerEnumType("ObjectClass");
		DXL2_ScriptSystem::registerEnumValue("ObjectClass", "CLASS_SPIRIT", CLASS_SPIRIT);
		DXL2_ScriptSystem::registerEnumValue("ObjectClass", "CLASS_SAFE",	CLASS_SAFE);
		DXL2_ScriptSystem::registerEnumValue("ObjectClass", "CLASS_SPRITE", CLASS_SPRITE);
		DXL2_ScriptSystem::registerEnumValue("ObjectClass", "CLASS_FRAME",	CLASS_FRAME);
		DXL2_ScriptSystem::registerEnumValue("ObjectClass", "CLASS_3D",		CLASS_3D);
		DXL2_ScriptSystem::registerEnumValue("ObjectClass", "CLASS_SOUND",	CLASS_SOUND);

		DXL2_ScriptSystem::registerEnumType("CollisionFlags");
		DXL2_ScriptSystem::registerEnumValue("CollisionFlags", "COLLIDE_NONE", COLLIDE_NONE);
		DXL2_ScriptSystem::registerEnumValue("CollisionFlags", "COLLIDE_TRIGGER", COLLIDE_TRIGGER);
		DXL2_ScriptSystem::registerEnumValue("CollisionFlags", "COLLIDE_PLAYER", COLLIDE_PLAYER);
		DXL2_ScriptSystem::registerEnumValue("CollisionFlags", "COLLIDE_ENEMY", COLLIDE_ENEMY);

		DXL2_ScriptSystem::registerEnumType("PhysicsFlags");
		DXL2_ScriptSystem::registerEnumValue("PhysicsFlags", "PHYSICS_NONE", PHYSICS_NONE);
		DXL2_ScriptSystem::registerEnumValue("PhysicsFlags", "PHYSICS_GRAVITY", PHYSICS_GRAVITY);
		DXL2_ScriptSystem::registerEnumValue("PhysicsFlags", "PHYSICS_BOUNCE", PHYSICS_BOUNCE);

		DXL2_ScriptSystem::registerEnumType("LogicCommonFlags");
		DXL2_ScriptSystem::registerEnumValue("LogicCommonFlags", "LCF_EYE",   LCF_EYE);
		DXL2_ScriptSystem::registerEnumValue("LogicCommonFlags", "LCF_BOSS",  LCF_BOSS);
		DXL2_ScriptSystem::registerEnumValue("LogicCommonFlags", "LCF_PAUSE", LCF_PAUSE);

		DXL2_ScriptSystem::registerEnumType("DamageType");
		DXL2_ScriptSystem::registerEnumValue("DamageType", "DMG_SHOT", DMG_SHOT);
		DXL2_ScriptSystem::registerEnumValue("DamageType", "DMG_FIST", DMG_FIST);
		DXL2_ScriptSystem::registerEnumValue("DamageType", "DMG_EXPLOSION", DMG_EXPLOSION);
		
		registerKeyTypes();
		registerLogicTypes();
			   
		// Register global constants.
		DXL2_ScriptSystem::registerGlobalProperty("const float timeStep", &c_step);

		return true;
	}

	void shutdown()
	{
	}
		
	void update()
	{
		s_accum += (f32)DXL2_System::getDeltaTime();
		const size_t count = s_scriptObjects.size();

		// Add any new logics
		const size_t addCount = s_logicsToAdd.size();
		for (size_t i = 0; i < addCount; i++)
		{
			std::vector<Logic> logic;
			std::vector<EnemyGenerator> gen;
			logic.push_back({});
			logic[0].type = s_logicsToAdd[i].logicType;

			registerObjectLogics(s_logicsToAdd[i].obj, logic, gen);
		}
		s_logicsToAdd.clear();
		
		while (s_accum >= c_step)
		{
			ScriptObject* obj = s_scriptObjects.data();
			
			for (size_t i = 0; i < count; i++, obj++)
			{
				if (!obj->gameObj) { continue; }

				const size_t logicCount = obj->logic.size();
				const size_t genCount = obj->generator.size();

				ScriptLogic* logic = obj->logic.data();
				for (size_t l = 0; l < logicCount; l++, logic++)
				{
					if (logic->tick)
					{
						s_self = obj->gameObj;
						s_param = &logic->param;
						DXL2_ScriptSystem::executeScriptFunction(SCRIPT_TYPE_LOGIC, logic->tick);
					}
				}

				ScriptGenerator* gen = obj->generator.data();
				for (size_t l = 0; l < genCount; l++, gen++)
				{
					if (gen->tick)
					{
						s_self = obj->gameObj;
						s_genParam = &gen->param;
						DXL2_ScriptSystem::executeScriptFunction(SCRIPT_TYPE_LOGIC, gen->tick);
					}
				}
			}

			s_accum -= c_step;
		}
	}

	void registerKeyTypes()
	{
		DXL2_ScriptSystem::registerEnumType("KeyType");
		DXL2_ScriptSystem::registerEnumValue("KeyType", "KEY_RED", KEY_RED);
		DXL2_ScriptSystem::registerEnumValue("KeyType", "KEY_YELLOW", KEY_YELLOW);
		DXL2_ScriptSystem::registerEnumValue("KeyType", "KEY_BLUE", KEY_BLUE);
		DXL2_ScriptSystem::registerEnumValue("KeyType", "KEY_CODE_1", KEY_CODE_1);
		DXL2_ScriptSystem::registerEnumValue("KeyType", "KEY_CODE_2", KEY_CODE_2);
		DXL2_ScriptSystem::registerEnumValue("KeyType", "KEY_CODE_3", KEY_CODE_3);
		DXL2_ScriptSystem::registerEnumValue("KeyType", "KEY_CODE_4", KEY_CODE_4);
		DXL2_ScriptSystem::registerEnumValue("KeyType", "KEY_CODE_5", KEY_CODE_5);
		DXL2_ScriptSystem::registerEnumValue("KeyType", "KEY_CODE_6", KEY_CODE_6);
		DXL2_ScriptSystem::registerEnumValue("KeyType", "KEY_CODE_7", KEY_CODE_7);
		DXL2_ScriptSystem::registerEnumValue("KeyType", "KEY_CODE_8", KEY_CODE_8);
		DXL2_ScriptSystem::registerEnumValue("KeyType", "KEY_CODE_9", KEY_CODE_9);
	}

	void registerLogicTypes()
	{
		DXL2_ScriptSystem::registerEnumType("LogicType");
		// Player
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_PLAYER", LOGIC_PLAYER);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_SHIELD", LOGIC_SHIELD);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_BATTERY", LOGIC_BATTERY);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_CLEATS", LOGIC_CLEATS);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_GOGGLES", LOGIC_GOGGLES);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_MASK", LOGIC_MASK);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_MEDKIT", LOGIC_MEDKIT);
					// Weapons -
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_RIFLE", LOGIC_RIFLE);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_AUTOGUN", LOGIC_AUTOGUN);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_FUSION", LOGIC_FUSION);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_MORTAR", LOGIC_MORTAR);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_CONCUSSION", LOGIC_CONCUSSION);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_CANNON", LOGIC_CANNON);
			// Ammo -
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_ENERGY", LOGIC_ENERGY);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_DETONATOR", LOGIC_DETONATOR);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_DETONATORS", LOGIC_DETONATORS);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_POWER", LOGIC_POWER);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_MINE", LOGIC_MINE);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_MINES", LOGIC_MINES);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_SHELL", LOGIC_SHELL);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_SHELLS", LOGIC_SHELLS);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_PLASMA", LOGIC_PLASMA);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_MISSILE", LOGIC_MISSILE);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_MISSILES", LOGIC_MISSILES);
		// Bonuses -
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_SUPERCHARGE", LOGIC_SUPERCHARGE);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_INVINCIBLE", LOGIC_INVINCIBLE);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_LIFE", LOGIC_LIFE);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_REVIVE", LOGIC_REVIVE);
		// Keys -
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_BLUE", LOGIC_BLUE);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_RED", LOGIC_RED);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_YELLOW", LOGIC_YELLOW);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_CODE1", LOGIC_CODE1);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_CODE2", LOGIC_CODE2);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_CODE3", LOGIC_CODE3);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_CODE4", LOGIC_CODE4);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_CODE5", LOGIC_CODE5);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_CODE6", LOGIC_CODE6);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_CODE7", LOGIC_CODE7);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_CODE8", LOGIC_CODE8);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_CODE9", LOGIC_CODE9);
		// Goal items -
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_DATATAPE", LOGIC_DATATAPE);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_PLANS", LOGIC_PLANS);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_DT_WEAPON", LOGIC_DT_WEAPON);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "CLASLOGIC_NAVAS_SPIRIT", LOGIC_NAVA);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_PHRIK", LOGIC_PHRIK);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_PILE", LOGIC_PILE);
		// Enemy logics
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_GENERATOR", LOGIC_GENERATOR);
		// Imperials -
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_I_OFFICER", LOGIC_I_OFFICER);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_I_OFFICERR", LOGIC_I_OFFICERR);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_I_OFFICERB", LOGIC_I_OFFICERB);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_I_OFFICERY", LOGIC_I_OFFICERY);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_I_OFFICER1", LOGIC_I_OFFICER1);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_I_OFFICER2", LOGIC_I_OFFICER2);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_I_OFFICER3", LOGIC_I_OFFICER3);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_I_OFFICER4", LOGIC_I_OFFICER4);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_I_OFFICER5", LOGIC_I_OFFICER5);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_I_OFFICER6", LOGIC_I_OFFICER6);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_I_OFFICER7", LOGIC_I_OFFICER7);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_I_OFFICER8", LOGIC_I_OFFICER8);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_I_OFFICER9", LOGIC_I_OFFICER9);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_TROOP", LOGIC_TROOP);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_STORM1", LOGIC_STORM1);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_COMMANDO", LOGIC_COMMANDO);
		// Aliens -
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_BOSSK", LOGIC_BOSSK);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_G_GUARD", LOGIC_G_GUARD);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_REE_YEES", LOGIC_REE_YEES);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_REE_YEES2", LOGIC_REE_YEES2);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_SEWER1", LOGIC_SEWER1);
		// Robots -
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_INT_DROID", LOGIC_INT_DROID);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_PROBE_DROID", LOGIC_PROBE_DROID);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_REMOTE", LOGIC_REMOTE);
		// Bosses -
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_BOBA_FETT", LOGIC_BOBA_FETT);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_KELL", LOGIC_KELL);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_D_TROOP1", LOGIC_D_TROOP1);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_D_TROOP2", LOGIC_D_TROOP2);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_D_TROOP3", LOGIC_D_TROOP3);
		// Special sprite logics
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_SCENERY", LOGIC_SCENERY);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_ANIM", LOGIC_ANIM);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_BARREL", LOGIC_BARREL);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_LAND_MINE", LOGIC_LAND_MINE);
		// 3D object logics
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_TURRET", LOGIC_TURRET);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_MOUSEBOT", LOGIC_MOUSEBOT);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_WELDER", LOGIC_WELDER);
		// 3D object motion logics
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_UPDATE", LOGIC_UPDATE);
		DXL2_ScriptSystem::registerEnumValue("LogicType", "LOGIC_KEY", LOGIC_KEY);
	}
}
