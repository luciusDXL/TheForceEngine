#include "logicSystem.h"
#include <TFE_Asset/spriteAsset.h>
#include <TFE_Asset/modelAsset.h>
#include <TFE_Asset/gameMessages.h>
#include <TFE_Asset/vocAsset.h>
#include <TFE_Game/gameHud.h>
#include <TFE_Game/gameConstants.h>
#include <TFE_ScriptSystem/scriptSystem.h>
#include <TFE_System/system.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Game/gameObject.h>
#include <TFE_Game/player.h>
#include <TFE_Game/physics.h>
#include <TFE_Game/geometry.h>
#include <TFE_InfSystem/infSystem.h>
#include <TFE_Audio/audioSystem.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <algorithm>

using namespace TFE_GameConstants;

void TFE_PrintVec3f(std::string& msg, Vec3f value0)
{
	TFE_System::logWrite(LOG_MSG, "Scripting", "%s (%f, %f, %f)", msg.c_str(), value0.x, value0.y, value0.z);
}

namespace TFE_LogicSystem
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
			if (TFE_ScriptSystem::loadScriptModule(SCRIPT_TYPE_LOGIC, scriptLogic->name.c_str()))
			{
				scriptLogic->param = logics[i];
				scriptLogic->start = TFE_ScriptSystem::getScriptFunction(scriptLogic->name.c_str(), "void start()");
				scriptLogic->stop  = TFE_ScriptSystem::getScriptFunction(scriptLogic->name.c_str(), "void stop()");
				scriptLogic->tick  = TFE_ScriptSystem::getScriptFunction(scriptLogic->name.c_str(), "void tick()");
				scriptLogic->handleMessage = TFE_ScriptSystem::getScriptFunction(scriptLogic->name.c_str(), "void handleMessage(int, int, int)");

				// Execute the start function if it exists.
				if (scriptLogic->start)
				{
					s_self = gameObject;
					s_param = &scriptLogic->param;
					TFE_ScriptSystem::executeScriptFunction(SCRIPT_TYPE_LOGIC, scriptLogic->start);
				}
			}
		}

		ScriptGenerator* scriptGen = newObject.generator.data();
		for (size_t i = 0; i < genCount; i++, scriptGen++)
		{
			const LogicType type = generators[i].type;
			scriptGen->name = c_logicScriptName[LOGIC_GENERATOR];
			if (TFE_ScriptSystem::loadScriptModule(SCRIPT_TYPE_LOGIC, scriptGen->name.c_str()))
			{
				scriptGen->param = generators[i];
				scriptGen->start = TFE_ScriptSystem::getScriptFunction(scriptGen->name.c_str(), "void start()");
				scriptGen->stop = TFE_ScriptSystem::getScriptFunction(scriptGen->name.c_str(), "void stop()");
				scriptGen->tick = TFE_ScriptSystem::getScriptFunction(scriptGen->name.c_str(), "void tick()");
				scriptGen->handleMessage = TFE_ScriptSystem::getScriptFunction(scriptGen->name.c_str(), "void handleMessage(int, int, int)");

				// Execute the start function if it exists.
				if (scriptGen->start)
				{
					s_self = gameObject;
					s_genParam = &scriptGen->param;
					TFE_ScriptSystem::executeScriptFunction(SCRIPT_TYPE_LOGIC, scriptGen->start);
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
				TFE_ScriptSystem::executeScriptFunction(SCRIPT_TYPE_LOGIC, logic[i].handleMessage, LOGIC_MSG_DAMAGE, damage, type);

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
				TFE_ScriptSystem::executeScriptFunction(SCRIPT_TYPE_LOGIC, logic[i].handleMessage, LOGIC_MSG_PLAYER_COLLISION, 0, 0);
			}
		}
	}

	void TFE_ChangeAngles(s32 objectId, Vec3f angleChange)
	{
		if (objectId < 0 || objectId >= s_scriptObjects.size()) { return; }

		ScriptObject* obj = &s_scriptObjects[objectId];
		obj->gameObj->angles.x += angleChange.x;
		obj->gameObj->angles.y += angleChange.y;
		obj->gameObj->angles.z += angleChange.z;
	}
		
	f32 TFE_GetVueLength(s32 objectId, s32 viewIndex)
	{
		if (objectId < 0 || objectId >= s_scriptObjects.size()) { return 0.0f; }

		ScriptObject* obj = &s_scriptObjects[objectId];
		if (viewIndex == 0 && s_param->vue)
		{
			return f32(s_param->vue->frameCount) / s_param->frameRate;
		}
		else if (viewIndex == 1 && s_param->vueAppend)
		{
			return f32(s_param->vueAppend->frameCount) / s_param->frameRate;
		}
		else
		{
			TFE_System::logWrite(LOG_ERROR, "Scripting", "Object %d VUE index %d out of range.\n", objectId, viewIndex);
		}

		return 0.0f;
	}

	u32 TFE_GetVueCount(s32 objectId)
	{
		if (objectId < 0 || objectId >= s_scriptObjects.size()) { return 0; }

		ScriptObject* obj = &s_scriptObjects[objectId];
		u32 vueCount = 0;
		if (s_param->vueAppend) { vueCount = 2; }
		else if (s_param->vue) { vueCount = 1; }
		
		return vueCount;
	}

	void TFE_SetVueAnimTime(s32 objectId, s32 viewIndex, f32 time)
	{
		if (objectId < 0 || objectId >= s_scriptObjects.size()) { return; }

		ScriptObject* obj = &s_scriptObjects[objectId];
		const VueAsset* vue = nullptr;
		if (viewIndex == 0 && s_param->vue)
		{
			vue = s_param->vue;
		}
		else if (viewIndex == 1 && s_param->vueAppend)
		{
			vue = s_param->vueAppend;
		}
		else
		{
			TFE_System::logWrite(LOG_ERROR, "Scripting", "Object %d VUE index %d out of range.\n", objectId, viewIndex);
			return;
		}

		const s32 frameIndex = std::min(s32(time * s_param->frameRate), (s32)vue->frameCount - 1);
		const s32 transformIndex = s_param->vueId;
		if (obj->gameObj)
		{
			const VueTransform* newTransform = &vue->transforms[frameIndex * vue->transformCount + transformIndex];

			const Vec3f prevPos = obj->gameObj->pos;
			const Vec3f newPos = newTransform->translation;

			// Trace a ray to update the new sectorId.
			// For this we will force our way through any adjoins.
			// Upon failure we will have to search for the new sector id.
			const RayIgnoreHeight ray=
			{
				{prevPos.x, prevPos.z}, {newPos.x, newPos.z},
				obj->gameObj->sectorId,
			};

			// TODO: Simplifiy this code since it can call findSector() twice in failure cases.
			s32 newSectorId = ray.originalSectorId;
			if (TFE_Physics::traceRayIgnoreHeight(&ray, &newSectorId))
			{
				// We hit something... which is bad. Now we have to search for the correct sector...
				s32 findSectorId = TFE_Physics::findSector(&newPos);
				if (findSectorId >= 0)
				{
					newSectorId = findSectorId;
				}
			}

			// Make sure the object is in the new sector.
			bool objInside = false;
			if (newSectorId >= 0)
			{
				const LevelData* level = TFE_LevelAsset::getLevelData();
				const Sector* sector = level->sectors.data() + newSectorId;
				const Vec2f* vtx = level->vertices.data() + sector->vtxOffset;
				const SectorWall* walls = level->walls.data() + sector->wallOffset;
				const Vec2f posXZ = { newPos.x, newPos.z };
				objInside = Geometry::pointInSector(&posXZ, sector->vtxCount, vtx, sector->wallCount, walls);
			}
			if (!objInside)
			{
				s32 findSectorId = TFE_Physics::findSector(&newPos);
				if (findSectorId >= 0)
				{
					newSectorId = findSectorId;
				}
			}

			const s32 prevSectorId = obj->gameObj->sectorId;
			if (prevSectorId != newSectorId)
			{
				SectorObjectList* secList = LevelGameObjects::getSectorObjectList();

				// Remove from the old sector.
				std::vector<u32>& prevlist = (*secList)[prevSectorId].list;
				const size_t prevCount = prevlist.size();
				for (u32 i = 0; i < prevCount; i++)
				{
					if (prevlist[i] == obj->gameObj->id)
					{
						prevlist.erase(prevlist.begin() + i);
						break;
					}
				}

				// Add to the new sector.
				std::vector<u32>& newlist = (*secList)[newSectorId].list;
				newlist.push_back(obj->gameObj->id);
			}
			obj->gameObj->sectorId = newSectorId;

			// update the object position.
			obj->gameObj->pos = newPos;
			// set the new transform.
			obj->gameObj->vueTransform = &newTransform->rotScale;
		}
	}
	
	f32 TFE_GetAnimFramerate(s32 objectId, s32 animationId)
	{
		if (objectId < 0 || objectId >= s_scriptObjects.size()) { return 0.0f; }

		ScriptObject* obj = &s_scriptObjects[objectId];
		if (obj->gameObj->oclass != CLASS_SPRITE) { return 0.0f; }
		return (f32)obj->gameObj->sprite->anim[animationId].frameRate;
	}

	u32 TFE_GetAnimFrameCount(s32 objectId, s32 animationId)
	{
		if (objectId < 0 || objectId >= s_scriptObjects.size()) { return 0; }

		ScriptObject* obj = &s_scriptObjects[objectId];
		if (obj->gameObj->oclass != CLASS_SPRITE) { return 0; }
		return obj->gameObj->sprite->anim[animationId].angles[0].frameCount;
	}

	void TFE_SetAnimFrame(s32 objectId, s32 animationId, s32 frameIndex)
	{
		if (objectId < 0 || objectId >= s_scriptObjects.size()) { return; }

		ScriptObject* obj = &s_scriptObjects[objectId];
		if (obj->gameObj->oclass != CLASS_SPRITE) { return; }

		obj->gameObj->animId = animationId;
		obj->gameObj->frameIndex = frameIndex;
	}

	void TFE_SetCollision(s32 objectId, f32 radius, f32 height, u32 collisionFlags)
	{
		if (objectId < 0 || objectId >= s_scriptObjects.size()) { return; }
		ScriptObject* obj = &s_scriptObjects[objectId];

		obj->gameObj->collisionRadius = radius;
		obj->gameObj->collisionHeight = height;
		obj->gameObj->collisionFlags  = collisionFlags;
	}

	void TFE_SetPhysics(s32 objectId, u32 physicsFlags)
	{
		if (objectId < 0 || objectId >= s_scriptObjects.size()) { return; }
		ScriptObject* obj = &s_scriptObjects[objectId];

		obj->gameObj->physicsFlags = physicsFlags;
		obj->gameObj->verticalVel = 0.0f;
	}

	void TFE_Hide(s32 objectId)
	{
		if (objectId < 0 || objectId >= s_scriptObjects.size()) { return; }
		ScriptObject* obj = &s_scriptObjects[objectId];

		obj->gameObj->show = false;
	}

	Vec3f TFE_BiasTowardsPlayer(Vec3f pos, f32 bias)
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

	s32 TFE_SpawnObject(Vec3f pos, Vec3f angles, s32 sectorId, s32 objectClass, std::string& objectName)
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
		obj->zbias = 0.0f;
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
		obj->scale = { 1.0f,1.0f, 1.0f };
		obj->pos = pos;

		if (objectClass == CLASS_SPRITE)
		{
			obj->sprite = TFE_Sprite::getSprite(objectName.c_str());
		}
		else if (objectClass == CLASS_FRAME)
		{
			obj->frame = TFE_Sprite::getFrame(objectName.c_str());
		}
		else if (objectClass == CLASS_3D)
		{
			obj->model = TFE_Model::get(objectName.c_str());
		}

		(*sectorObjList)[sectorId].list.push_back(objectId);
		return objectId;
	}
		
	// TODO: Support setting parameters.
	void TFE_AddLogic(s32 objectId, s32 logicId)
	{
		if (objectId < 0) { return; }
		GameObjectList* gameObjList = LevelGameObjects::getGameObjectList();
		GameObject* obj = &(*gameObjList)[objectId];

		s_logicsToAdd.push_back({obj, LogicType(logicId)});
	}

	void TFE_GivePlayerKey(s32 keyType)
	{
		s_player->m_keys |= keyType;
	}

	void TFE_AddGoalItem()
	{
		TFE_InfSystem::advanceCompleteElevator();
	}

	void TFE_BossKilled()
	{
		TFE_InfSystem::advanceBossElevator();
	}

	void TFE_MohcKilled()
	{
		TFE_InfSystem::advanceMohcElevator();
	}

	void TFE_GiveGear()
	{
		// TODO: Give gear back...
	}

	void TFE_PrintMessage(u32 msgId)
	{
		TFE_GameHud::setMessage(TFE_GameMessages::getMessage(msgId));
	}

	void TFE_Sound_PlayOneShot(u32 type, f32 volume, const std::string& soundName)
	{
		const SoundBuffer* buffer = TFE_VocAsset::get(soundName.c_str());
		if (buffer)
		{
			// One shot, play and forget. Only do this if the client needs no control until stopAllSounds() is called.
			// Note that looping one shots are valid though may generate too many sound sources if not used carefully.
			TFE_Audio::playOneShot(SoundType(type), volume, MONO_SEPERATION, buffer, false, &s_self->pos);
		}
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
		TFE_ScriptSystem::registerValueType("Vec2f", sizeof(Vec2f), TFE_ScriptSystem::getTypeTraits<Vec2f>(), prop);

		prop.clear();
		prop.push_back({ "int x", offsetof(Vec2i, x) });
		prop.push_back({ "int z", offsetof(Vec2i, z) });
		TFE_ScriptSystem::registerValueType("Vec2i", sizeof(Vec2i), TFE_ScriptSystem::getTypeTraits<Vec2i>(), prop);

		prop.clear();
		prop.push_back({"float x", offsetof(Vec3f, x)});
		prop.push_back({"float y", offsetof(Vec3f, y)});
		prop.push_back({"float z", offsetof(Vec3f, z)});
		TFE_ScriptSystem::registerValueType("Vec3f", sizeof(Vec3f), TFE_ScriptSystem::getTypeTraits<Vec3f>(), prop);

		prop.clear();
		prop.push_back({"int x", offsetof(Vec3i, x)});
		prop.push_back({"int y", offsetof(Vec3i, y)});
		prop.push_back({"int z", offsetof(Vec3i, z)});
		TFE_ScriptSystem::registerValueType("Vec3i", sizeof(Vec3i), TFE_ScriptSystem::getTypeTraits<Vec3i>(), prop);

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
		prop.push_back({ "uint commonFlags", offsetof(GameObject, comFlags) });
		prop.push_back({ "const float radius", offsetof(GameObject, radius) });
		prop.push_back({ "const float height", offsetof(GameObject, height) });
		TFE_ScriptSystem::registerRefType("GameObject", prop);

		// Logic Parameters
		prop.clear();
		prop.push_back({"const uint flags", offsetof(Logic, flags) });
		prop.push_back({"const float frameRate", offsetof(Logic, frameRate) });
		prop.push_back({"const Vec3f rotation", offsetof(Logic, rotation) });
		TFE_ScriptSystem::registerRefType("LogicParam", prop);

		prop.clear();
		prop.push_back({ "const uint type", offsetof(EnemyGenerator, type) });
		prop.push_back({ "const float delay", offsetof(EnemyGenerator, delay) });
		prop.push_back({ "const float interval", offsetof(EnemyGenerator, interval) });
		prop.push_back({ "const float minDist", offsetof(EnemyGenerator, minDist) });
		prop.push_back({ "const float maxDist", offsetof(EnemyGenerator, maxDist) });
		prop.push_back({ "const int maxAlive", offsetof(EnemyGenerator, maxAlive) });
		prop.push_back({ "const int numTerminate", offsetof(EnemyGenerator, numTerminate) });
		prop.push_back({ "const float wanderTime", offsetof(EnemyGenerator, wanderTime) });
		TFE_ScriptSystem::registerRefType("GenParam", prop);

		// Functions
		TFE_ScriptSystem::registerFunction("void TFE_Print(const string &in, Vec3f value)", SCRIPT_FUNCTION(TFE_PrintVec3f));
		TFE_ScriptSystem::registerFunction("void TFE_ChangeAngles(int objectId, Vec3f angleChange)", SCRIPT_FUNCTION(TFE_ChangeAngles));
		TFE_ScriptSystem::registerFunction("float TFE_GetAnimFramerate(int objectId, int animationId)", SCRIPT_FUNCTION(TFE_GetAnimFramerate));
		TFE_ScriptSystem::registerFunction("uint TFE_GetAnimFrameCount(int objectId, int animationId)", SCRIPT_FUNCTION(TFE_GetAnimFrameCount));
		TFE_ScriptSystem::registerFunction("void TFE_SetAnimFrame(int objectId, int animationId, int frameIndex)", SCRIPT_FUNCTION(TFE_SetAnimFrame));
		TFE_ScriptSystem::registerFunction("void TFE_SetCollision(int objectId, float radius, float height, uint collisionFlags)", SCRIPT_FUNCTION(TFE_SetCollision));
		TFE_ScriptSystem::registerFunction("void TFE_SetPhysics(int objectId, uint physicsFlags)", SCRIPT_FUNCTION(TFE_SetPhysics));
		TFE_ScriptSystem::registerFunction("void TFE_Hide(int objectId)", SCRIPT_FUNCTION(TFE_Hide));
		
		TFE_ScriptSystem::registerFunction("Vec3f TFE_BiasTowardsPlayer(Vec3f pos, float bias)", SCRIPT_FUNCTION(TFE_BiasTowardsPlayer));
		TFE_ScriptSystem::registerFunction("int TFE_SpawnObject(Vec3f pos, Vec3f angles, int sectorId, int objectClass, const string &in)", SCRIPT_FUNCTION(TFE_SpawnObject));
		TFE_ScriptSystem::registerFunction("void TFE_AddLogic(int objectId, int logicId)", SCRIPT_FUNCTION(TFE_AddLogic));
		TFE_ScriptSystem::registerFunction("void TFE_GivePlayerKey(int keyType)", SCRIPT_FUNCTION(TFE_GivePlayerKey));
		TFE_ScriptSystem::registerFunction("void TFE_AddGoalItem()", SCRIPT_FUNCTION(TFE_AddGoalItem));
		TFE_ScriptSystem::registerFunction("void TFE_BossKilled()", SCRIPT_FUNCTION(TFE_BossKilled));
		TFE_ScriptSystem::registerFunction("void TFE_MohcKilled()", SCRIPT_FUNCTION(TFE_MohcKilled));
		TFE_ScriptSystem::registerFunction("void TFE_GiveGear()", SCRIPT_FUNCTION(TFE_GiveGear));

		TFE_ScriptSystem::registerFunction("void TFE_PrintMessage(uint msgId)", SCRIPT_FUNCTION(TFE_PrintMessage));

		TFE_ScriptSystem::registerFunction("float TFE_GetVueLength(int objectId, int viewIndex)", SCRIPT_FUNCTION(TFE_GetVueLength));
		TFE_ScriptSystem::registerFunction("uint TFE_GetVueCount(int objectId)", SCRIPT_FUNCTION(TFE_GetVueCount));
		TFE_ScriptSystem::registerFunction("void TFE_SetVueAnimTime(int objectId, int viewIndex, float time)", SCRIPT_FUNCTION(TFE_SetVueAnimTime));

		TFE_ScriptSystem::registerFunction("void TFE_Sound_PlayOneShot(uint type, float volume, const string &in)", SCRIPT_FUNCTION(TFE_Sound_PlayOneShot));

		// Register "self" and Logic parameters.
		TFE_ScriptSystem::registerGlobalProperty("GameObject @self",  &s_self);
		TFE_ScriptSystem::registerGlobalProperty("LogicParam @param", &s_param);
		TFE_ScriptSystem::registerGlobalProperty("GenParam   @genParam", &s_genParam);

		// Register enums
		TFE_ScriptSystem::registerEnumType("LogicMsg");
		TFE_ScriptSystem::registerEnumValue("LogicMsg", "LOGIC_MSG_DAMAGED", LOGIC_MSG_DAMAGE);
		TFE_ScriptSystem::registerEnumValue("LogicMsg", "LOGIC_MSG_PLAYER_COLLISION", LOGIC_MSG_PLAYER_COLLISION);

		TFE_ScriptSystem::registerEnumType("ObjectClass");
		TFE_ScriptSystem::registerEnumValue("ObjectClass", "CLASS_SPIRIT", CLASS_SPIRIT);
		TFE_ScriptSystem::registerEnumValue("ObjectClass", "CLASS_SAFE",	CLASS_SAFE);
		TFE_ScriptSystem::registerEnumValue("ObjectClass", "CLASS_SPRITE", CLASS_SPRITE);
		TFE_ScriptSystem::registerEnumValue("ObjectClass", "CLASS_FRAME",	CLASS_FRAME);
		TFE_ScriptSystem::registerEnumValue("ObjectClass", "CLASS_3D",		CLASS_3D);
		TFE_ScriptSystem::registerEnumValue("ObjectClass", "CLASS_SOUND",	CLASS_SOUND);

		TFE_ScriptSystem::registerEnumType("CollisionFlags");
		TFE_ScriptSystem::registerEnumValue("CollisionFlags", "COLLIDE_NONE", COLLIDE_NONE);
		TFE_ScriptSystem::registerEnumValue("CollisionFlags", "COLLIDE_TRIGGER", COLLIDE_TRIGGER);
		TFE_ScriptSystem::registerEnumValue("CollisionFlags", "COLLIDE_PLAYER", COLLIDE_PLAYER);
		TFE_ScriptSystem::registerEnumValue("CollisionFlags", "COLLIDE_ENEMY", COLLIDE_ENEMY);

		TFE_ScriptSystem::registerEnumType("PhysicsFlags");
		TFE_ScriptSystem::registerEnumValue("PhysicsFlags", "PHYSICS_NONE", PHYSICS_NONE);
		TFE_ScriptSystem::registerEnumValue("PhysicsFlags", "PHYSICS_GRAVITY", PHYSICS_GRAVITY);
		TFE_ScriptSystem::registerEnumValue("PhysicsFlags", "PHYSICS_BOUNCE", PHYSICS_BOUNCE);

		TFE_ScriptSystem::registerEnumType("LogicCommonFlags");
		TFE_ScriptSystem::registerEnumValue("LogicCommonFlags", "LCF_EYE",   LCF_EYE);
		TFE_ScriptSystem::registerEnumValue("LogicCommonFlags", "LCF_BOSS",  LCF_BOSS);
		TFE_ScriptSystem::registerEnumValue("LogicCommonFlags", "LCF_PAUSE", LCF_PAUSE);
		TFE_ScriptSystem::registerEnumValue("LogicCommonFlags", "LCF_LOOP",  LCF_LOOP);

		TFE_ScriptSystem::registerEnumType("DamageType");
		TFE_ScriptSystem::registerEnumValue("DamageType", "DMG_SHOT", DMG_SHOT);
		TFE_ScriptSystem::registerEnumValue("DamageType", "DMG_FIST", DMG_FIST);
		TFE_ScriptSystem::registerEnumValue("DamageType", "DMG_EXPLOSION", DMG_EXPLOSION);

		TFE_ScriptSystem::registerEnumType("SoundType");
		TFE_ScriptSystem::registerEnumValue("SoundType", "SOUND_2D", SOUND_2D);
		TFE_ScriptSystem::registerEnumValue("SoundType", "SOUND_3D", SOUND_3D);
				
		registerKeyTypes();
		registerLogicTypes();
			   
		// Register global constants.
		TFE_ScriptSystem::registerGlobalProperty("const float timeStep", &c_step);

		return true;
	}

	void shutdown()
	{
	}
		
	void update()
	{
		s_accum += (f32)TFE_System::getDeltaTime();
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
						TFE_ScriptSystem::executeScriptFunction(SCRIPT_TYPE_LOGIC, logic->tick);
					}
				}

				ScriptGenerator* gen = obj->generator.data();
				for (size_t l = 0; l < genCount; l++, gen++)
				{
					if (gen->tick)
					{
						s_self = obj->gameObj;
						s_genParam = &gen->param;
						TFE_ScriptSystem::executeScriptFunction(SCRIPT_TYPE_LOGIC, gen->tick);
					}
				}
			}

			s_accum -= c_step;
		}
	}

	void registerKeyTypes()
	{
		TFE_ScriptSystem::registerEnumType("KeyType");
		TFE_ScriptSystem::registerEnumValue("KeyType", "KEY_RED", KEY_RED);
		TFE_ScriptSystem::registerEnumValue("KeyType", "KEY_YELLOW", KEY_YELLOW);
		TFE_ScriptSystem::registerEnumValue("KeyType", "KEY_BLUE", KEY_BLUE);
		TFE_ScriptSystem::registerEnumValue("KeyType", "KEY_CODE_1", KEY_CODE_1);
		TFE_ScriptSystem::registerEnumValue("KeyType", "KEY_CODE_2", KEY_CODE_2);
		TFE_ScriptSystem::registerEnumValue("KeyType", "KEY_CODE_3", KEY_CODE_3);
		TFE_ScriptSystem::registerEnumValue("KeyType", "KEY_CODE_4", KEY_CODE_4);
		TFE_ScriptSystem::registerEnumValue("KeyType", "KEY_CODE_5", KEY_CODE_5);
		TFE_ScriptSystem::registerEnumValue("KeyType", "KEY_CODE_6", KEY_CODE_6);
		TFE_ScriptSystem::registerEnumValue("KeyType", "KEY_CODE_7", KEY_CODE_7);
		TFE_ScriptSystem::registerEnumValue("KeyType", "KEY_CODE_8", KEY_CODE_8);
		TFE_ScriptSystem::registerEnumValue("KeyType", "KEY_CODE_9", KEY_CODE_9);
	}

	void registerLogicTypes()
	{
		TFE_ScriptSystem::registerEnumType("LogicType");
		// Player
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_PLAYER", LOGIC_PLAYER);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_SHIELD", LOGIC_SHIELD);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_BATTERY", LOGIC_BATTERY);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_CLEATS", LOGIC_CLEATS);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_GOGGLES", LOGIC_GOGGLES);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_MASK", LOGIC_MASK);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_MEDKIT", LOGIC_MEDKIT);
					// Weapons -
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_RIFLE", LOGIC_RIFLE);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_AUTOGUN", LOGIC_AUTOGUN);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_FUSION", LOGIC_FUSION);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_MORTAR", LOGIC_MORTAR);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_CONCUSSION", LOGIC_CONCUSSION);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_CANNON", LOGIC_CANNON);
			// Ammo -
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_ENERGY", LOGIC_ENERGY);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_DETONATOR", LOGIC_DETONATOR);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_DETONATORS", LOGIC_DETONATORS);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_POWER", LOGIC_POWER);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_MINE", LOGIC_MINE);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_MINES", LOGIC_MINES);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_SHELL", LOGIC_SHELL);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_SHELLS", LOGIC_SHELLS);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_PLASMA", LOGIC_PLASMA);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_MISSILE", LOGIC_MISSILE);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_MISSILES", LOGIC_MISSILES);
		// Bonuses -
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_SUPERCHARGE", LOGIC_SUPERCHARGE);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_INVINCIBLE", LOGIC_INVINCIBLE);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_LIFE", LOGIC_LIFE);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_REVIVE", LOGIC_REVIVE);
		// Keys -
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_BLUE", LOGIC_BLUE);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_RED", LOGIC_RED);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_YELLOW", LOGIC_YELLOW);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_CODE1", LOGIC_CODE1);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_CODE2", LOGIC_CODE2);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_CODE3", LOGIC_CODE3);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_CODE4", LOGIC_CODE4);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_CODE5", LOGIC_CODE5);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_CODE6", LOGIC_CODE6);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_CODE7", LOGIC_CODE7);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_CODE8", LOGIC_CODE8);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_CODE9", LOGIC_CODE9);
		// Goal items -
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_DATATAPE", LOGIC_DATATAPE);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_PLANS", LOGIC_PLANS);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_DT_WEAPON", LOGIC_DT_WEAPON);
		TFE_ScriptSystem::registerEnumValue("LogicType", "CLASLOGIC_NAVAS_SPIRIT", LOGIC_NAVA);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_PHRIK", LOGIC_PHRIK);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_PILE", LOGIC_PILE);
		// Enemy logics
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_GENERATOR", LOGIC_GENERATOR);
		// Imperials -
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_I_OFFICER", LOGIC_I_OFFICER);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_I_OFFICERR", LOGIC_I_OFFICERR);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_I_OFFICERB", LOGIC_I_OFFICERB);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_I_OFFICERY", LOGIC_I_OFFICERY);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_I_OFFICER1", LOGIC_I_OFFICER1);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_I_OFFICER2", LOGIC_I_OFFICER2);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_I_OFFICER3", LOGIC_I_OFFICER3);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_I_OFFICER4", LOGIC_I_OFFICER4);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_I_OFFICER5", LOGIC_I_OFFICER5);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_I_OFFICER6", LOGIC_I_OFFICER6);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_I_OFFICER7", LOGIC_I_OFFICER7);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_I_OFFICER8", LOGIC_I_OFFICER8);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_I_OFFICER9", LOGIC_I_OFFICER9);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_TROOP", LOGIC_TROOP);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_STORM1", LOGIC_STORM1);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_COMMANDO", LOGIC_COMMANDO);
		// Aliens -
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_BOSSK", LOGIC_BOSSK);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_G_GUARD", LOGIC_G_GUARD);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_REE_YEES", LOGIC_REE_YEES);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_REE_YEES2", LOGIC_REE_YEES2);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_SEWER1", LOGIC_SEWER1);
		// Robots -
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_INT_DROID", LOGIC_INT_DROID);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_PROBE_DROID", LOGIC_PROBE_DROID);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_REMOTE", LOGIC_REMOTE);
		// Bosses -
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_BOBA_FETT", LOGIC_BOBA_FETT);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_KELL", LOGIC_KELL);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_D_TROOP1", LOGIC_D_TROOP1);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_D_TROOP2", LOGIC_D_TROOP2);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_D_TROOP3", LOGIC_D_TROOP3);
		// Special sprite logics
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_SCENERY", LOGIC_SCENERY);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_ANIM", LOGIC_ANIM);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_BARREL", LOGIC_BARREL);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_LAND_MINE", LOGIC_LAND_MINE);
		// 3D object logics
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_TURRET", LOGIC_TURRET);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_MOUSEBOT", LOGIC_MOUSEBOT);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_WELDER", LOGIC_WELDER);
		// 3D object motion logics
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_UPDATE", LOGIC_UPDATE);
		TFE_ScriptSystem::registerEnumValue("LogicType", "LOGIC_KEY", LOGIC_KEY);
	}
}
