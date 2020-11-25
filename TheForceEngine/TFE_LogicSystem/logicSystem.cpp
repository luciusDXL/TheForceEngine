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
#include <TFE_JediRenderer/jediRenderer.h>
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
		if (s_scriptObjects.size() <= gameObject->objectId)
		{
			s_scriptObjects.resize(gameObject->objectId + 1);
		}

		const size_t index = gameObject->objectId;
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
		if (gameObject->objectId >= s_scriptObjects.size()) { return; }

		ScriptObject* obj = &s_scriptObjects[gameObject->objectId];
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
					gameObject->position.y -= 0.2f;
					gameObject->verticalVel -= 32.0f;
					gameObject->update = true;
				}
			}
		}
	}

	void sendPlayerCollisionTrigger(const GameObject* gameObject)
	{
		ScriptObject* obj = &s_scriptObjects[gameObject->objectId];
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
		obj->gameObj->update = true;
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

			const Vec3f prevPos = obj->gameObj->position;
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
					if (prevlist[i] == obj->gameObj->objectId)
					{
						prevlist.erase(prevlist.begin() + i);
						break;
					}
				}

				// Add to the new sector.
				std::vector<u32>& newlist = (*secList)[newSectorId].list;
				newlist.push_back(obj->gameObj->objectId);
			}
			obj->gameObj->sectorId = newSectorId;

			// update the object position.
			obj->gameObj->position = newPos;
			// set the new transform.
			obj->gameObj->vueTransform = &newTransform->rotScale;

			obj->gameObj->update = true;
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
		obj->gameObj->update = true;
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
		obj->gameObj->update = true;
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
		obj->objectId = objectId;
		obj->sectorId = sectorId;
		obj->angles = angles;
		obj->scale = { 1.0f,1.0f, 1.0f };
		obj->position = pos;

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

		TFE_JediRenderer::addObject(objectName.c_str(), objectId, sectorId);

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
			TFE_Audio::playOneShot(SoundType(type), volume, MONO_SEPERATION, buffer, false, &s_self->position);
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
		SCRIPT_STRUCT_START;
		SCRIPT_STRUCT_MEMBER(Vec2f, float, x);
		SCRIPT_STRUCT_MEMBER(Vec2f, float, z);
		SCRIPT_STRUCT_VALUE(Vec2f);

		SCRIPT_STRUCT_START;
		SCRIPT_STRUCT_MEMBER(Vec2i, int, x);
		SCRIPT_STRUCT_MEMBER(Vec2i, int, z);
		SCRIPT_STRUCT_VALUE(Vec2i);

		SCRIPT_STRUCT_START;
		SCRIPT_STRUCT_MEMBER(Vec3f, float, x);
		SCRIPT_STRUCT_MEMBER(Vec3f, float, y);
		SCRIPT_STRUCT_MEMBER(Vec3f, float, z);
		SCRIPT_STRUCT_VALUE(Vec3f);

		SCRIPT_STRUCT_START;
		SCRIPT_STRUCT_MEMBER(Vec3i, int, x);
		SCRIPT_STRUCT_MEMBER(Vec3i, int, y);
		SCRIPT_STRUCT_MEMBER(Vec3i, int, z);
		SCRIPT_STRUCT_VALUE(Vec3i);
		
		// GameObject type.
		SCRIPT_STRUCT_START;
		SCRIPT_STRUCT_MEMBER_CONST(GameObject, int, objectId);
		SCRIPT_STRUCT_MEMBER_CONST(GameObject, int, sectorId);
		SCRIPT_STRUCT_MEMBER_CONST(GameObject, Vec3f, position);
		SCRIPT_STRUCT_MEMBER_CONST(GameObject, Vec3f, angles);
		SCRIPT_STRUCT_MEMBER(GameObject, uint, flags);
		SCRIPT_STRUCT_MEMBER(GameObject, uint, messageMask);
		SCRIPT_STRUCT_MEMBER(GameObject, int, state);
		SCRIPT_STRUCT_MEMBER(GameObject, int, hp);
		SCRIPT_STRUCT_MEMBER(GameObject, float, time);
		SCRIPT_STRUCT_MEMBER(GameObject, uint, commonFlags);
		SCRIPT_STRUCT_MEMBER_CONST(GameObject, float, radius);
		SCRIPT_STRUCT_MEMBER_CONST(GameObject, float, height);
		SCRIPT_STRUCT_REF(GameObject);

		// Logic Parameters
		SCRIPT_STRUCT_START;
		SCRIPT_STRUCT_MEMBER_CONST(LogicParam, uint, flags);
		SCRIPT_STRUCT_MEMBER_CONST(LogicParam, float, frameRate);
		SCRIPT_STRUCT_MEMBER_CONST(LogicParam, Vec3f, rotation);
		SCRIPT_STRUCT_REF(LogicParam);

		SCRIPT_STRUCT_START;
		SCRIPT_STRUCT_MEMBER_CONST(GenParam, uint, type);
		SCRIPT_STRUCT_MEMBER_CONST(GenParam, float, delay);
		SCRIPT_STRUCT_MEMBER_CONST(GenParam, float, interval);
		SCRIPT_STRUCT_MEMBER_CONST(GenParam, float, minDist);
		SCRIPT_STRUCT_MEMBER_CONST(GenParam, float, maxDist);
		SCRIPT_STRUCT_MEMBER_CONST(GenParam, int, maxAlive);
		SCRIPT_STRUCT_MEMBER_CONST(GenParam, int, numTerminate);
		SCRIPT_STRUCT_MEMBER_CONST(GenParam, float, wanderTime);
		SCRIPT_STRUCT_REF(GenParam);

		// Functions
		SCRIPT_NATIVE_FUNC(TFE_PrintVec3f,        void TFE_Print(const string &in, Vec3f value));
		SCRIPT_NATIVE_FUNC(TFE_ChangeAngles,      void TFE_ChangeAngles(int objectId, Vec3f angleChange));
		SCRIPT_NATIVE_FUNC(TFE_GetAnimFramerate,  float TFE_GetAnimFramerate(int objectId, int animationId));
		SCRIPT_NATIVE_FUNC(TFE_GetAnimFrameCount, uint TFE_GetAnimFrameCount(int objectId, int animationId));
		SCRIPT_NATIVE_FUNC(TFE_SetAnimFrame,      void TFE_SetAnimFrame(int objectId, int animationId, int frameIndex));
		SCRIPT_NATIVE_FUNC(TFE_SetCollision,      void TFE_SetCollision(int objectId, float radius, float height, uint collisionFlags));
		SCRIPT_NATIVE_FUNC(TFE_SetPhysics,        void TFE_SetPhysics(int objectId, uint physicsFlags));
		SCRIPT_NATIVE_FUNC(TFE_Hide,              void TFE_Hide(int objectId));
		
		SCRIPT_NATIVE_FUNC(TFE_BiasTowardsPlayer, Vec3f TFE_BiasTowardsPlayer(Vec3f pos, float bias));
		SCRIPT_NATIVE_FUNC(TFE_SpawnObject,       int TFE_SpawnObject(Vec3f pos, Vec3f angles, int sectorId, int objectClass, const string &in));
		SCRIPT_NATIVE_FUNC(TFE_AddLogic,          void TFE_AddLogic(int objectId, int logicId));
		SCRIPT_NATIVE_FUNC(TFE_GivePlayerKey,     void TFE_GivePlayerKey(int keyType));
		SCRIPT_NATIVE_FUNC(TFE_AddGoalItem,       void TFE_AddGoalItem());
		SCRIPT_NATIVE_FUNC(TFE_BossKilled,        void TFE_BossKilled());
		SCRIPT_NATIVE_FUNC(TFE_MohcKilled,        void TFE_MohcKilled());
		SCRIPT_NATIVE_FUNC(TFE_GiveGear,          void TFE_GiveGear());

		SCRIPT_NATIVE_FUNC(TFE_PrintMessage,      void TFE_PrintMessage(uint msgId));

		SCRIPT_NATIVE_FUNC(TFE_GetVueLength,      float TFE_GetVueLength(int objectId, int viewIndex));
		SCRIPT_NATIVE_FUNC(TFE_GetVueCount,       uint TFE_GetVueCount(int objectId));
		SCRIPT_NATIVE_FUNC(TFE_SetVueAnimTime,    void TFE_SetVueAnimTime(int objectId, int viewIndex, float time));

		SCRIPT_NATIVE_FUNC(TFE_Sound_PlayOneShot, void TFE_Sound_PlayOneShot(uint type, float volume, const string &in));

		// Register "self" and Logic parameters.
		SCRIPT_GLOBAL_PROPERTY(GameObject, self,  s_self);
		SCRIPT_GLOBAL_PROPERTY(LogicParam, param, s_param);
		SCRIPT_GLOBAL_PROPERTY(GenParam,   genParam, s_genParam);

		// Register enums
		SCRIPT_ENUM(LogicMsg);
		SCRIPT_ENUM_VALUE(LogicMsg, LOGIC_MSG_DAMAGE);
		SCRIPT_ENUM_VALUE(LogicMsg, LOGIC_MSG_PLAYER_COLLISION);

		SCRIPT_ENUM(ObjectClass);
		SCRIPT_ENUM_VALUE(ObjectClass, CLASS_SPIRIT);
		SCRIPT_ENUM_VALUE(ObjectClass, CLASS_SAFE);
		SCRIPT_ENUM_VALUE(ObjectClass, CLASS_SPRITE);
		SCRIPT_ENUM_VALUE(ObjectClass, CLASS_FRAME);
		SCRIPT_ENUM_VALUE(ObjectClass, CLASS_3D);
		SCRIPT_ENUM_VALUE(ObjectClass, CLASS_SOUND);

		SCRIPT_ENUM(CollisionFlags);
		SCRIPT_ENUM_VALUE(CollisionFlags, COLLIDE_NONE);
		SCRIPT_ENUM_VALUE(CollisionFlags, COLLIDE_TRIGGER);
		SCRIPT_ENUM_VALUE(CollisionFlags, COLLIDE_PLAYER);
		SCRIPT_ENUM_VALUE(CollisionFlags, COLLIDE_ENEMY);

		SCRIPT_ENUM(PhysicsFlags);
		SCRIPT_ENUM_VALUE(PhysicsFlags, PHYSICS_NONE);
		SCRIPT_ENUM_VALUE(PhysicsFlags, PHYSICS_GRAVITY);
		SCRIPT_ENUM_VALUE(PhysicsFlags, PHYSICS_BOUNCE);

		SCRIPT_ENUM(LogicCommonFlags);
		SCRIPT_ENUM_VALUE(LogicCommonFlags, LCF_EYE);
		SCRIPT_ENUM_VALUE(LogicCommonFlags, LCF_BOSS);
		SCRIPT_ENUM_VALUE(LogicCommonFlags, LCF_PAUSE);
		SCRIPT_ENUM_VALUE(LogicCommonFlags, LCF_LOOP);

		SCRIPT_ENUM(DamageType);
		SCRIPT_ENUM_VALUE(DamageType, DMG_SHOT);
		SCRIPT_ENUM_VALUE(DamageType, DMG_FIST);
		SCRIPT_ENUM_VALUE(DamageType, DMG_EXPLOSION);

		SCRIPT_ENUM(SoundType);
		SCRIPT_ENUM_VALUE(SoundType, SOUND_2D);
		SCRIPT_ENUM_VALUE(SoundType, SOUND_3D);

		registerKeyTypes();
		registerLogicTypes();
			   
		// Register global constants.
		SCRIPT_GLOBAL_PROPERTY_CONST(float, timeStep, c_step);

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
		SCRIPT_ENUM(KeyType);
		SCRIPT_ENUM_VALUE(KeyType, KEY_RED);
		SCRIPT_ENUM_VALUE(KeyType, KEY_YELLOW);
		SCRIPT_ENUM_VALUE(KeyType, KEY_BLUE);
		SCRIPT_ENUM_VALUE(KeyType, KEY_CODE_1);
		SCRIPT_ENUM_VALUE(KeyType, KEY_CODE_2);
		SCRIPT_ENUM_VALUE(KeyType, KEY_CODE_3);
		SCRIPT_ENUM_VALUE(KeyType, KEY_CODE_4);
		SCRIPT_ENUM_VALUE(KeyType, KEY_CODE_5);
		SCRIPT_ENUM_VALUE(KeyType, KEY_CODE_6);
		SCRIPT_ENUM_VALUE(KeyType, KEY_CODE_7);
		SCRIPT_ENUM_VALUE(KeyType, KEY_CODE_8);
		SCRIPT_ENUM_VALUE(KeyType, KEY_CODE_9);
	}

	void registerLogicTypes()
	{
		SCRIPT_ENUM(LogicType);
		// Player
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_PLAYER);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_SHIELD);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_BATTERY);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_CLEATS);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_GOGGLES);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_MASK);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_MEDKIT);
		// Weapons -
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_RIFLE);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_AUTOGUN);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_FUSION);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_MORTAR);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_CONCUSSION);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_CANNON);
		// Ammo -
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_ENERGY);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_DETONATOR);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_DETONATORS);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_POWER);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_MINE);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_MINES);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_SHELL);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_SHELLS);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_PLASMA);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_MISSILE);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_MISSILES);
		// Bonuses -
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_SUPERCHARGE);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_INVINCIBLE);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_LIFE);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_REVIVE);
		// Keys -
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_BLUE);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_RED);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_YELLOW);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_CODE1);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_CODE2);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_CODE3);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_CODE4);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_CODE5);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_CODE6);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_CODE7);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_CODE8);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_CODE9);
		// Goal items -
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_DATATAPE);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_PLANS);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_DT_WEAPON);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_NAVA);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_PHRIK);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_PILE);
		// Enemy logics
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_GENERATOR);
		// Imperials -
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_I_OFFICER);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_I_OFFICERR);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_I_OFFICERB);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_I_OFFICERY);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_I_OFFICER1);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_I_OFFICER2);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_I_OFFICER3);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_I_OFFICER4);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_I_OFFICER5);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_I_OFFICER6);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_I_OFFICER7);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_I_OFFICER8);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_I_OFFICER9);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_TROOP);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_STORM1);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_COMMANDO);
		// Aliens -
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_BOSSK);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_G_GUARD);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_REE_YEES);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_REE_YEES2);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_SEWER1);
		// Robots -
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_INT_DROID);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_PROBE_DROID);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_REMOTE);
		// Bosses -
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_BOBA_FETT);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_KELL);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_D_TROOP1);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_D_TROOP2);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_D_TROOP3);
		// Special sprite logics
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_SCENERY);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_ANIM);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_BARREL);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_LAND_MINE);
		// 3D object logics
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_TURRET);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_MOUSEBOT);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_WELDER);
		// 3D object motion logics
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_UPDATE);
		SCRIPT_ENUM_VALUE(LogicType, LOGIC_KEY);
	}
}
