#include "weaponSystem.h"
#include <DXL2_Asset/spriteAsset.h>
#include <DXL2_Asset/textureAsset.h>
#include <DXL2_Asset/modelAsset.h>
#include <DXL2_Asset/vocAsset.h>
#include <DXL2_LogicSystem/logicSystem.h>
#include <DXL2_ScriptSystem/scriptSystem.h>
#include <DXL2_System/system.h>
#include <DXL2_System/math.h>
#include <DXL2_System/memoryPool.h>
#include <DXL2_Game/physics.h>
#include <DXL2_Game/gameObject.h>
#include <DXL2_Game/gameConstants.h>
#include <DXL2_Game/level.h>
#include <DXL2_InfSystem/infSystem.h>
#include <DXL2_FileSystem/filestream.h>
#include <DXL2_FileSystem/paths.h>
#include <DXL2_Renderer/renderer.h>
#include <DXL2_Game/player.h>
#include <DXL2_Audio/audioSystem.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <algorithm>

// TEMP
#include <DXL2_Game/modelRendering.h>
#include <DXL2_Game/renderCommon.h>

using namespace DXL2_GameConstants;

namespace DXL2_WeaponSystem
{
	static const char* c_weaponScriptName[WEAPON_COUNT] =
	{
		"weapon_punch",			// WEAPON_PUNCH
		"weapon_pistol",		// WEAPON_PISTOL
		"weapon_rifle",			// WEAPON_RIFLE
		"weapon_thermal_det",	// WEAPON_THERMAL_DETONATOR
	};
	#define MAX_WEAPON_FRAMES 16
	#define MAX_POOL_COUNT 32
	#define PROJECTILE_POOL_SIZE 64
	#define MAX_ACTIVE_PROJECTILES 256
	#define MAX_ACTIVE_EFFECTS 256

	enum ProjPoolType
	{
		PROJ_FRAME = 0,
		PROJ_SPRITE,
		PROJ_3D,
	};

	struct ProjectilePool
	{
		ProjPoolType type;
		u32 count;
		u16 freeCount;
		u16 activeCount;
		u32 startIndex;
		union
		{
			void*   asset;
			Frame*  frame;
			Sprite* sprite;
			Model*  model;
		};
		GameObject* obj;
		u32* freeObj;
		u32* activeObj;
	};

	struct WeaponObject
	{
		s32 x, y;
		s32 yOffset;	//yOffset for things like pitch that shouldn't affect animations.
		s32 state;
		s32 frame;
		s32 scaledWidth;
		s32 scaledHeight;

		f32 motion;
		f32 pitch;
		f32 time;

		bool hold;
		f32  holdTime;

		const TextureFrame* frames[MAX_WEAPON_FRAMES];
		ProjectilePool* projPool;
		ProjectilePool* hitEffectPool;
		const SoundBuffer* hitEffectSound = nullptr;
	};

	struct ScreenObject
	{
		s32 width;
		s32 height;
		s32 scaleX;
		s32 scaleY;
	};

	enum ProjectileFlags
	{
		PFLAG_HAS_GRAVITY = (1 << 0),
		PFLAG_EXPLODE_ON_IMPACT = (1 << 1),
		// Later...
		PFLAG_TIMED_EXPLOSION = (1 << 2),
		PFLAG_EXPLODE_ON_RANGE = (1 << 3),
	};

	struct Projectile
	{
		GameObject* obj;
		ProjectilePool* pool;
		ProjectilePool* effectPool;
		const SoundBuffer* hitEffectSound = nullptr;
		Vec3f pos;
		Vec3f vel;
		u32 flags;	// ProjectileFlags
		u32 damage;
		f32 value0;
		f32 value1;
	};

	struct HitEffect
	{
		GameObject* obj;
		ProjectilePool* pool;
		Vec3f pos;
		f32 time;
		f32 frameRate;
		f32 animLength;
		u32 frame;
	};
	
	struct ScriptWeapon
	{
		// Logic Name
		char name[64];

		// Script functions (may be NULL)
		SCRIPT_FUNC_PTR switchTo;
		SCRIPT_FUNC_PTR switchFrom;
		SCRIPT_FUNC_PTR tick;
		SCRIPT_FUNC_PTR firePrimary;
		SCRIPT_FUNC_PTR fireSecondary;
	};

	static WeaponObject s_weapon = { 0 };
	static ScreenObject s_screen = { 0 };
	static WeaponObject* s_weaponPtr = &s_weapon;
	static ScreenObject* s_screenPtr = &s_screen;
	static bool s_initialized = false;
	static ScriptWeapon* s_scriptWeapons = nullptr;

	static f32 s_accum = 0.0f;

	static s32 s_activeWeapon = -1;
	static s32 s_nextWeapon = -1;
	static bool s_callSwitchTo = false;

	static DXL2_Renderer* s_renderer;
	static MemoryPool* s_memoryPool;

	static bool s_shot = false;
	static s32 s_primeTimer = 0;
	static Vec3f s_shotLoc;
	static s32 s_shotSectorId;
	static f32 s_shotYaw;
	static f32 s_shotPitch;
	static Model* s_model;
	static Player* s_player;

	static bool s_showExplosion = false;
	static Vec3f s_explosionLoc;
	static f32 s_explosionAnimTime;
	static Sprite* s_explosionSprite = nullptr;

	static Vec3f s_shootDir;
	static u32 s_projPoolCount = 0;
	static ProjectilePool s_projectilePool[MAX_POOL_COUNT];

	static u32 s_projCount = 0;
	static u32 s_effectCount = 0;
	static Projectile s_proj[MAX_ACTIVE_PROJECTILES];
	static HitEffect s_effect[MAX_ACTIVE_EFFECTS];

	void DXL2_WeaponFire();
	void DXL2_WeaponPrimed(s32 primeCount);
	void DXL2_WeaponLightEnd();

	void clearWeapons()
	{
		s_activeWeapon = -1;
		s_nextWeapon = -1;
		s_callSwitchTo = false;
		s_showExplosion = false;
		s_primeTimer = 0;
		s_model = nullptr;
		s_explosionSprite = nullptr;
		s_explosionAnimTime = 0.0f;
		s_projPoolCount = 0;
		s_projCount = 0;
		s_effectCount = 0;
		s_memoryPool->clear();
	}

	bool registerWeapon(Weapon weapon, s32 id)
	{
		const size_t index = id;
		ScriptWeapon& newWeapon = s_scriptWeapons[index];

		strcpy(newWeapon.name, c_weaponScriptName[id]);
		if (DXL2_ScriptSystem::loadScriptModule(SCRIPT_TYPE_WEAPON, newWeapon.name))
		{
			newWeapon.switchTo      = DXL2_ScriptSystem::getScriptFunction(newWeapon.name, "void switchTo()");
			newWeapon.switchFrom    = DXL2_ScriptSystem::getScriptFunction(newWeapon.name, "void switchFrom()");
			newWeapon.tick		    = DXL2_ScriptSystem::getScriptFunction(newWeapon.name, "void tick()");
			newWeapon.firePrimary   = DXL2_ScriptSystem::getScriptFunction(newWeapon.name, "void firePrimary()");
			newWeapon.fireSecondary = DXL2_ScriptSystem::getScriptFunction(newWeapon.name, "void fireSecondary()");
		}
		
		return true;
	}
	
	void DXL2_LoadWeaponFrame(s32 frameIndex, std::string& textureName)
	{
		if (frameIndex < 0 || frameIndex >= MAX_WEAPON_FRAMES) { return; }

		const Texture* texture = DXL2_Texture::get(textureName.c_str());
		if (!texture) { return; }
		s_weapon.frames[frameIndex] = &texture->frames[0];
	}
		
	void DXL2_NextWeapon()
	{
		memset(&s_weapon, 0, sizeof(WeaponObject));
		s_weapon.state = 0; // had a previous weapon.

		s_callSwitchTo = true;
		s_activeWeapon = s_nextWeapon;
		s_nextWeapon = -1;
		s_primeTimer = 0;
		s_player->m_shooting = false;
	}

	void DXL2_Sound_PlayOneShot(f32 volume, const std::string& soundName)
	{
		const SoundBuffer* buffer = DXL2_VocAsset::get(soundName.c_str());
		if (buffer)
		{
			// One shot, play and forget. Only do this if the client needs no control until stopAllSounds() is called.
			// Note that looping one shots are valid though may generate too many sound sources if not used carefully.
			DXL2_Audio::playOneShot(SOUND_2D, volume, MONO_SEPERATION, buffer, false);
		}
	}

	GameObject* getProjectile(ProjectilePool* projPool)
	{
		if (!projPool->freeCount)
		{
			return nullptr;
		}

		const u32 freeIndex   = projPool->freeCount - 1;
		const u32 activeIndex = projPool->activeCount;
		projPool->freeCount--;
		projPool->activeCount++;

		projPool->activeObj[activeIndex] = projPool->freeObj[freeIndex];
		GameObject* obj = &projPool->obj[projPool->activeObj[activeIndex]];
		memset(obj, 0, sizeof(GameObject));
		obj->show = true;
		if (projPool->type == PROJ_FRAME)
		{
			obj->oclass = CLASS_FRAME;
			obj->frame = projPool->frame;
		}
		else if (projPool->type == PROJ_SPRITE)
		{
			obj->oclass = CLASS_SPRITE;
			obj->sprite = projPool->sprite;
		}
		else if (projPool->type == PROJ_3D)
		{
			obj->oclass = CLASS_3D;
			obj->model = projPool->model;
		}

		return obj;
	}

	void returnProjectile(ProjectilePool* projPool, GameObject* obj)
	{
		u32 index = (u32)(size_t((u8*)obj - (u8*)projPool->obj) / sizeof(GameObject));
		// search the active list.
		for (u32 i = 0; i < projPool->activeCount; i++)
		{
			if (projPool->activeObj[i] == index)
			{
				projPool->activeCount--;
				if (projPool->activeCount)
				{
					projPool->activeObj[i] = projPool->activeObj[projPool->activeCount];
				}
				projPool->freeObj[projPool->freeCount++] = index;
				break;
			}
		}
	}
		
	ProjectilePool* createProjectilePool(ProjPoolType type, void* asset)
	{
		// Does this pool already exist?
		const u32 count = s_projPoolCount;
		ProjectilePool* pool = s_projectilePool;
		for (size_t i = 0; i < count; i++, pool++)
		{
			if (pool->type == type && pool->asset == asset)
			{
				return pool;
			}
		}
		// If not create it now.
		ProjectilePool* newPool = &s_projectilePool[s_projPoolCount];
		s_projPoolCount++;

		newPool->type = type;
		newPool->asset = asset;
		newPool->count = PROJECTILE_POOL_SIZE;

		GameObjectList* objects = LevelGameObjects::getGameObjectList();
		size_t startIndex = objects->size();
		objects->resize(startIndex + newPool->count);

		newPool->obj = objects->data() + startIndex;
		newPool->freeObj = (u32*)s_memoryPool->allocate(sizeof(u32) * newPool->count);
		newPool->activeObj = (u32*)s_memoryPool->allocate(sizeof(u32) * newPool->count);
		newPool->freeCount = newPool->count;
		newPool->activeCount = 0;
		newPool->startIndex = startIndex;
		for (u32 i = 0; i < newPool->freeCount; i++)
		{
			newPool->freeObj[i] = i;
			newPool->obj[i].show = false;
		}

		return newPool;
	}

	void DXL2_LoadProjectile(std::string& projectileName)
	{
		size_t len = projectileName.length();
		if (!len) { return; }

		const char* ext = &projectileName.c_str()[len - 3];
		void* asset = nullptr;

		ProjPoolType type;
		if (strcasecmp(ext, "fme") == 0)
		{
			asset = (void*)DXL2_Sprite::getFrame(projectileName.c_str());
			type = PROJ_FRAME;
		}
		else if (strcasecmp(ext, "wax") == 0)
		{
			asset = (void*)DXL2_Sprite::getSprite(projectileName.c_str());
			type = PROJ_SPRITE;
		}
		else
		{
			asset = (void*)DXL2_Model::get(projectileName.c_str());
			type = PROJ_3D;
		}
		if (!asset) { return; }

		s_weapon.projPool = createProjectilePool(type, asset);
	}

	void DXL2_LoadHitEffect(std::string& effectName)
	{
		Sprite* effect = DXL2_Sprite::getSprite(effectName.c_str());
		if (!effect) { return; }

		s_weapon.hitEffectPool = createProjectilePool(PROJ_SPRITE, effect);
	}

	void DXL2_LoadHitSound(std::string& soundName)
	{
		const SoundBuffer* buffer = DXL2_VocAsset::get(soundName.c_str());
		s_weapon.hitEffectSound = buffer;
	}

	void DXL2_PlayerSpawnProjectile(s32 x, s32 y, f32 fwdSpeed, f32 upwardSpeed, u32 flags, u32 damage, f32 value0, f32 value1)
	{
		// Convert from screen pixel offset to world position.
		u32 w, h;
		s_renderer->getResolution(&w, &h);

		Vec3f origin = DXL2_RenderCommon::unproject({ x, y }, 1.0f);
		Vec3f target = DXL2_RenderCommon::unproject({ s32(w/2), s32(h/2) }, 1000.0f);
		Vec3f dir = { target.x - origin.x, target.y - origin.y, target.z - origin.z };
		dir = DXL2_Math::normalize(&dir);

		Vec3f playerStart = { s_player->pos.x, origin.y, s_player->pos.z };
		Vec3f playerToOrigin = { origin.x - s_player->pos.x, 0.0f, origin.z - s_player->pos.z };
		f32 playerToOriginDist = DXL2_Math::distance(&playerStart, &origin);
		f32 scale = 1.0f / playerToOriginDist;
		playerToOrigin.x *= scale;
		playerToOrigin.y *= scale;
		playerToOrigin.z *= scale;

		// Trace a ray from the origin to the target so the sector can be updated.
		const Ray ray =
		{
			playerStart,
			playerToOrigin,
			s_player->m_sectorId,
			playerToOriginDist,
		};
		RayHitInfo hitInfo;
		if (DXL2_Physics::traceRay(&ray, &hitInfo))
		{
			// We hit something right away so no point in spawning.
			return;
		}

		GameObject* projObj = getProjectile(s_weapon.projPool);
		if (!projObj) { return; }

		Projectile* proj = nullptr;
		if (s_projCount == MAX_ACTIVE_PROJECTILES)
		{
			// Steal it from the beginning of the list.
			proj = &s_proj[0];
			returnProjectile(proj->pool, proj->obj);
		}
		else
		{
			proj = &s_proj[s_projCount];
			s_projCount++;
		}

		proj->obj = projObj;
		proj->pool = s_weapon.projPool;
		proj->effectPool = s_weapon.hitEffectPool;
		proj->hitEffectSound = s_weapon.hitEffectSound;

		SectorObjectList* secList = LevelGameObjects::getSectorObjectList();
		std::vector<u32>& list = (*secList)[hitInfo.hitSectorId].list;
		u32 index = (u32)((u8*)projObj - (u8*)s_weapon.projPool->obj) / sizeof(GameObject);
		list.push_back(index + s_weapon.projPool->startIndex);

		proj->obj->sectorId = hitInfo.hitSectorId;

		proj->pos = origin;
		proj->vel = { dir.x * fwdSpeed, dir.y * fwdSpeed + upwardSpeed, dir.z * fwdSpeed };
		proj->flags = flags;
		proj->damage = damage;
		proj->value0 = value0;
		proj->value1 = value1;

		projObj->pos = origin;
	}

	void spawnEffect(ProjectilePool* effectPool, const Vec3f* pos, s32 sectorId)
	{
		GameObject* effectObj = getProjectile(effectPool);
		if (!effectObj) { return; }

		HitEffect* effect = nullptr;
		if (s_effectCount == MAX_ACTIVE_EFFECTS)
		{
			// Steal it from the beginning of the list.
			effect = &s_effect[0];
			returnProjectile(effect->pool, effect->obj);
		}
		else
		{
			effect = &s_effect[s_effectCount];
			s_effectCount++;
		}

		effect->time = 0.0f;
		effect->frame = 0;
		effect->frameRate = (f32)effectObj->sprite->anim[0].frameRate;
		effect->animLength = (f32)effectObj->sprite->anim[0].angles[0].frameCount / effect->frameRate;
		effect->obj = effectObj;
		effect->pool = effectPool;
		effect->pos = *pos;

		effectObj->pos = *pos;
		effectObj->sectorId = sectorId;
		effectObj->animId = 0;
		effectObj->frameIndex = 0;
		effectObj->fullbright = true;

		SectorObjectList* secList = LevelGameObjects::getSectorObjectList();
		std::vector<u32>& list = (*secList)[sectorId].list;
		u32 index = (u32)((u8*)effectObj - (u8*)effectPool->obj) / sizeof(GameObject);
		list.push_back(index + effectPool->startIndex);
	}

	void updateEffects()
	{
		HitEffect* effect = s_effect;
		s32 delCount = 0;
		s32 delList[256];
		for (u32 i = 0; i < s_effectCount; i++, effect++)
		{
			effect->time += c_step;
			if (effect->time >= effect->animLength)
			{
				delList[delCount++] = i;
				continue;
			}
			effect->frame = u32(effect->time * effect->frameRate);
			effect->obj->frameIndex = u16(effect->frame);
		}

		SectorObjectList* secList = LevelGameObjects::getSectorObjectList();
		for (s32 i = delCount - 1; i >= 0; i--)
		{
			s32 index = delList[i];
			HitEffect* effect = &s_effect[index];
			returnProjectile(effect->pool, effect->obj);
			effect->obj->show = false;

			// Delete from the sector.
			u32 objIndex = (u32)((u8*)effect->obj - (u8*)effect->pool->obj) / sizeof(GameObject);
			u32 startIndex = effect->pool->startIndex;

			// Remove from the old sector.
			std::vector<u32>& prevlist = (*secList)[effect->obj->sectorId].list;
			const size_t prevCount = prevlist.size();
			for (u32 j = 0; j < prevCount; j++)
			{
				if (prevlist[j] == objIndex + startIndex)
				{
					prevlist.erase(prevlist.begin() + j);
					break;
				}
			}

			for (s32 j = index; j < s_effectCount - 1; j++)
			{
				s_effect[j] = s_effect[j + 1];
			}
			s_effectCount--;
		}
	}

	void simulateProjectiles()
	{
		SectorObjectList* secList = LevelGameObjects::getSectorObjectList();
		
		Projectile* proj = s_proj;
		s32 delCount = 0;
		s32 delList[256];
		for (u32 i = 0; i < s_projCount; i++, proj++)
		{
			const Vec3f p0 = proj->pos;
			const Vec3f p1 = { p0.x + proj->vel.x * c_step, p0.y + proj->vel.y * c_step, p0.z + proj->vel.z * c_step };

			f32 stepLen = DXL2_Math::distance(&p0, &p1);
			f32 scale = 1.0f / stepLen;
			Vec3f dir = { (p1.x - p0.x) * scale, (p1.y - p0.y) * scale, (p1.z - p0.z) * scale };

			// Collision
			const Ray ray=
			{
				p0,
				dir,
				proj->obj->sectorId,
				stepLen,
			};
			RayHitInfo hitInfo;
			if (DXL2_Physics::traceRay(&ray, &hitInfo))
			{
				if (proj->flags & PFLAG_EXPLODE_ON_IMPACT)
				{
					// Explode here.
					if (proj->effectPool)
					{
						spawnEffect(proj->effectPool, &hitInfo.hitPoint, hitInfo.hitSectorId);
						if (proj->hitEffectSound)
						{
							DXL2_Audio::playOneShot(SOUND_3D, 1.0f, MONO_SEPERATION, proj->hitEffectSound, false, &proj->pos, true);
						}
					}
					// Damage.
					const f32 radius = proj->value0;
					const u32 damage = proj->damage;
					if (radius == 0.0f)	// Radius of 0 means only the target is damaged.
					{
						DXL2_LogicSystem::damageObject((GameObject*)hitInfo.obj, damage);
					}
					else  // Otherwise effect all objects in range.
					{
						GameObjectList* objects = LevelGameObjects::getGameObjectList();
						GameObject* obj = objects->data();

						const u32 count = (u32)objects->size();
						const f32 rSq = radius * radius;
						for (u32 j = 0; j < count; j++, obj++)
						{
							if (!obj->show) { continue; }
							const f32 distSq = DXL2_Math::distanceSq(&obj->pos, &hitInfo.hitPoint);
							if (distSq <= rSq)
							{
								// TODO: Damage falloff and explosion effect.
								DXL2_LogicSystem::damageObject(obj, damage, DMG_EXPLOSION);
							}
						}

						DXL2_InfSystem::explosion(&hitInfo.hitPoint, hitInfo.hitSectorId, radius);
					}
				}
				// Handle bouncing, etc.

				// Remove object
				returnProjectile(proj->pool, proj->obj);
				proj->obj->show = false;

				// Delete from the sector.
				u32 index = (u32)((u8*)proj->obj - (u8*)proj->pool->obj) / sizeof(GameObject);
				u32 startIndex = proj->pool->startIndex;

				// Remove from the old sector.
				std::vector<u32>& prevlist = (*secList)[proj->obj->sectorId].list;
				const size_t prevCount = prevlist.size();
				for (u32 j = 0; j < prevCount; j++)
				{
					if (prevlist[j] == index + startIndex)
					{
						prevlist.erase(prevlist.begin() + j);
						break;
					}
				}
				delList[delCount++] = i;

				continue;
			}
			if (proj->obj->sectorId != hitInfo.hitSectorId && hitInfo.hitSectorId > -1)
			{
				u32 index = (u32)((u8*)proj->obj - (u8*)proj->pool->obj) / sizeof(GameObject);
				u32 startIndex = proj->pool->startIndex;

				// Remove from the old sector.
				std::vector<u32>& prevlist = (*secList)[proj->obj->sectorId].list;
				const size_t prevCount = prevlist.size();
				for (u32 i = 0; i < prevCount; i++)
				{
					if (prevlist[i] == index + startIndex)
					{
						prevlist.erase(prevlist.begin() + i);
						break;
					}
				}

				// Add to the new sector.
				std::vector<u32>& newlist = (*secList)[hitInfo.hitSectorId].list;
				newlist.push_back(index + startIndex);

				proj->obj->sectorId = hitInfo.hitSectorId;
			}
			
			// Assuming no collision.
			proj->pos = p1;
			proj->obj->pos = p1;

			// Physics
			if (proj->flags & PFLAG_HAS_GRAVITY)
			{
				proj->vel.y += c_gravityAccelStep;
			}
		}

		for (s32 i = delCount - 1; i >= 0; i--)
		{
			s32 index = delList[i];
			for (s32 j = index; j < s_projCount - 1; j++)
			{
				s_proj[j] = s_proj[j + 1];
			}
			s_projCount--;
		}
	}

	f32 DXL2_GetSineMotion(f32 time)
	{
		return sinf(time * 2.0f * PI);
	}

	f32 DXL2_GetCosMotion(f32 time)
	{
		return cosf(time * 2.0f * PI);
	}

	void registerWeapons()
	{
		s_scriptWeapons = (ScriptWeapon*)s_memoryPool->allocate(sizeof(ScriptWeapon)*WEAPON_COUNT);
		for (u32 i = 0; i < WEAPON_COUNT; i++)
		{
			registerWeapon(Weapon(i), i);
		}
	}

// 256Kb
#define DXL2_WEAPON_MEMORY_POOL 256 * 1024

	bool init(DXL2_Renderer* renderer)
	{
		s_renderer = renderer;
		s_memoryPool = new MemoryPool();
		s_memoryPool->init(DXL2_WEAPON_MEMORY_POOL, "Weapon_Memory");
		// Set the warning watermark at 75% capacity.
		s_memoryPool->setWarningWatermark(DXL2_WEAPON_MEMORY_POOL * 3 / 4);

		clearWeapons();
		s_accum = 0.0f;

		if (s_initialized)
		{
			registerWeapons();
			return true;
		}
		s_initialized = true;
						
		// register script functions.
		std::vector<ScriptTypeProp> prop;
		prop.reserve(16);

		// WeaponObject type
		prop.clear();
		prop.push_back({ "int x",				offsetof(WeaponObject, x) });
		prop.push_back({ "int y",				offsetof(WeaponObject, y) });
		prop.push_back({ "int yOffset",         offsetof(WeaponObject, yOffset) });
		prop.push_back({ "int state",			offsetof(WeaponObject, state) });
		prop.push_back({ "int frame",			offsetof(WeaponObject, frame) });
		prop.push_back({ "int scaledWidth",		offsetof(WeaponObject, scaledWidth) });
		prop.push_back({ "int scaledHeight",	offsetof(WeaponObject, scaledHeight) });
		prop.push_back({ "const float motion",	offsetof(WeaponObject, motion) });
		prop.push_back({ "const float pitch",	offsetof(WeaponObject, pitch) });
		prop.push_back({ "const bool hold",		offsetof(WeaponObject, hold) });
		prop.push_back({ "float time",			offsetof(WeaponObject, time) });
		prop.push_back({ "float holdTime",		offsetof(WeaponObject, holdTime) });
		DXL2_ScriptSystem::registerRefType("WeaponObject", prop);

		// Screen Object
		prop.clear();
		prop.push_back({"const int width",  offsetof(ScreenObject, width) });
		prop.push_back({"const int height", offsetof(ScreenObject, height) });
		prop.push_back({"const int scaleX", offsetof(ScreenObject, scaleX) });
		prop.push_back({"const int scaleY", offsetof(ScreenObject, scaleY) });
		DXL2_ScriptSystem::registerRefType("ScreenObject", prop);

		// Functions
		DXL2_ScriptSystem::registerFunction("void DXL2_LoadWeaponFrame(int frameIndex, const string &in)", SCRIPT_FUNCTION(DXL2_LoadWeaponFrame));
		DXL2_ScriptSystem::registerFunction("void DXL2_NextWeapon()", SCRIPT_FUNCTION(DXL2_NextWeapon));
		DXL2_ScriptSystem::registerFunction("void DXL2_LoadProjectile(const string &in)", SCRIPT_FUNCTION(DXL2_LoadProjectile));
		DXL2_ScriptSystem::registerFunction("void DXL2_LoadHitEffect(const string &in)", SCRIPT_FUNCTION(DXL2_LoadHitEffect));
		DXL2_ScriptSystem::registerFunction("void DXL2_LoadHitSound(const string &in)", SCRIPT_FUNCTION(DXL2_LoadHitSound));
		DXL2_ScriptSystem::registerFunction("float DXL2_GetSineMotion(float time)", SCRIPT_FUNCTION(DXL2_GetSineMotion));
		DXL2_ScriptSystem::registerFunction("float DXL2_GetCosMotion(float time)", SCRIPT_FUNCTION(DXL2_GetCosMotion));
		DXL2_ScriptSystem::registerFunction("void DXL2_WeaponFire()", SCRIPT_FUNCTION(DXL2_WeaponFire));
		DXL2_ScriptSystem::registerFunction("void DXL2_WeaponPrimed(int primeCount)", SCRIPT_FUNCTION(DXL2_WeaponPrimed));
		DXL2_ScriptSystem::registerFunction("void DXL2_WeaponLightEnd()", SCRIPT_FUNCTION(DXL2_WeaponLightEnd));
		DXL2_ScriptSystem::registerFunction("void DXL2_PlayerSpawnProjectile(int x, int y, float fwdSpeed, float upwardSpeed, uint flags, uint damage, float value0, float value1)",
											SCRIPT_FUNCTION(DXL2_PlayerSpawnProjectile));

		DXL2_ScriptSystem::registerFunction("void DXL2_Sound_PlayOneShot(float volume, const string &in)", SCRIPT_FUNCTION(DXL2_Sound_PlayOneShot));

		// Register "self" and Logic parameters.
		DXL2_ScriptSystem::registerGlobalProperty("WeaponObject @weapon", &s_weaponPtr);
		DXL2_ScriptSystem::registerGlobalProperty("ScreenObject @screen", &s_screenPtr);

		// Register global constants.
		// Register enums
		DXL2_ScriptSystem::registerEnumType("ProjectileFlags");
		DXL2_ScriptSystem::registerEnumValue("ProjectileFlags", "PFLAG_HAS_GRAVITY", PFLAG_HAS_GRAVITY);
		DXL2_ScriptSystem::registerEnumValue("ProjectileFlags", "PFLAG_EXPLODE_ON_IMPACT", PFLAG_EXPLODE_ON_IMPACT);
		DXL2_ScriptSystem::registerEnumValue("ProjectileFlags", "PFLAG_TIMED_EXPLOSION", PFLAG_TIMED_EXPLOSION);
		DXL2_ScriptSystem::registerEnumValue("ProjectileFlags", "PFLAG_EXPLODE_ON_RANGE", PFLAG_EXPLODE_ON_RANGE);

		u32 width, height;
		s_renderer->getResolution(&width, &height);
		s_screen.width = width;
		s_screen.height = height;
		s_screen.scaleX = 256 * width / 320;
		s_screen.scaleY = 256 * height / 200;

		registerWeapons();

		return true;
	}

	void shutdown()
	{
		delete s_memoryPool;
	}
		
	void switchToWeapon(Weapon weapon)
	{
		if (weapon == s_activeWeapon) { return; }

		if (s_activeWeapon < 0)
		{
			memset(&s_weapon, 0, sizeof(WeaponObject));
			s_weapon.state = -1; // no previous weapon.

			if (s_scriptWeapons[weapon].switchTo)
			{
				DXL2_ScriptSystem::executeScriptFunction(SCRIPT_TYPE_WEAPON, s_scriptWeapons[weapon].switchTo);
			}
			s_activeWeapon = weapon;
			s_nextWeapon = -1;
		}
		else
		{
			s_nextWeapon = weapon;
			if (s_scriptWeapons[s_activeWeapon].switchFrom)
			{
				DXL2_ScriptSystem::executeScriptFunction(SCRIPT_TYPE_WEAPON, s_scriptWeapons[s_activeWeapon].switchFrom);
			}
			else
			{
				DXL2_NextWeapon();
			}
		}
	}

	void update(f32 motion, Player* player)
	{
		if (!player) { return; }

		s_weapon.motion = motion;
		s_weapon.pitch = player->m_pitch / PITCH_LIMIT;

		// First call the "switch to" function for the next weapon if needed.
		if (s_callSwitchTo && s_activeWeapon >= 0 && s_scriptWeapons[s_activeWeapon].switchTo)
		{
			DXL2_ScriptSystem::executeScriptFunction(SCRIPT_TYPE_WEAPON, s_scriptWeapons[s_activeWeapon].switchTo);
		}
		s_callSwitchTo = false;

		// Then call the fixed-rate tick().
		s_accum += (f32)DXL2_System::getDeltaTime();
		while (s_accum >= c_step)
		{
			updateEffects();
			simulateProjectiles();
			
			if (s_scriptWeapons[s_activeWeapon].tick)
			{
				DXL2_ScriptSystem::executeScriptFunction(SCRIPT_TYPE_WEAPON, s_scriptWeapons[s_activeWeapon].tick);
			}
			if (s_primeTimer) { s_primeTimer--; }

			s_accum -= c_step;
		}
	}
	
	void DXL2_WeaponFire()
	{
		s_shot = true;
	}

	void DXL2_WeaponPrimed(s32 primeCount)
	{
		s_primeTimer = primeCount;
	}

	void DXL2_WeaponLightEnd()
	{
		if (s_player) { s_player->m_shooting = false; }
	}

	// HACKY DEBUG CODE, REPLACE WITH REAL SHOOTING CODE!
	void release()
	{
		s_weapon.hold = false;
	}

	void shoot(Player* player, const Vec2f* dir)
	{
		if ((s_activeWeapon != WEAPON_PISTOL && s_activeWeapon != WEAPON_THERMAL_DETONATOR) || s_nextWeapon >= 0) { return; }
		if (s_primeTimer > 0) { return; }

		s_weapon.hold = true;

		u32 w, h;
		s_renderer->getResolution(&w, &h);

		Vec3f origin = DXL2_RenderCommon::unproject({ s_weapon.x + 16 * s_screen.scaleX / 256, s_weapon.y + s_weapon.yOffset }, 1.0f);
		Vec3f target = DXL2_RenderCommon::unproject({ s32(w) / 2, s32(h) / 2 }, 10000.0f);

		s_player = player;
		s_shotLoc = origin;
		s_shotSectorId = player->m_sectorId;
		s_shotYaw = -player->m_yaw * 180.0f / PI;
		s_shotPitch = 1.0f * player->m_pitch * 180.0f / PI;
		s_model = DXL2_Model::get("WRBOLT.3DO");

		s_shootDir = { target.x - origin.x, target.y - origin.y, target.z - origin.z };
				
		f32 magSq = s_shootDir.x*s_shootDir.x + s_shootDir.z*s_shootDir.z + s_shootDir.y*s_shootDir.y;
		f32 dirScale = 1.0f / sqrtf(magSq);
		s_shootDir.x *= dirScale;
		s_shootDir.y *= dirScale;
		s_shootDir.z *= dirScale;

		// get Yaw and pitch from direction.
		s_shotYaw = -atan2f(s_shootDir.z, s_shootDir.x) * 180.0f / PI + 90.0f;
		//s_shotPitch = -atan2f(s_shootDir.y, s_shootDir.z) * 180.0f / PI * 0.5f;

		if (s_scriptWeapons[s_activeWeapon].firePrimary)
		{
			DXL2_ScriptSystem::executeScriptFunction(SCRIPT_TYPE_WEAPON, s_scriptWeapons[s_activeWeapon].firePrimary);
		}
		s_primeTimer = INT_MAX;
		if (s_activeWeapon != WEAPON_THERMAL_DETONATOR)
		{
			if (s_player) { s_player->m_shooting = true; }

			// Do an initial trace from the player to the weapon start, to catch cases where sectors should change.
			Vec3f playerOrigin = { s_player->pos.x, origin.y, s_player->pos.z };
			f32 dist = DXL2_Math::distance(&playerOrigin, &s_shotLoc);
			Vec3f initDir = { s_shotLoc.x - playerOrigin.x, s_shotLoc.y - playerOrigin.y, s_shotLoc.z - playerOrigin.z };
			initDir = DXL2_Math::normalize(&initDir);

			const Ray ray =
			{
				playerOrigin,
				initDir,
				s_shotSectorId,
				dist
			};
			RayHitInfo hitInfo;
			if (DXL2_Physics::traceRay(&ray, &hitInfo))
			{
				// Stop the projectile.
				s_shot = false;
				s_showExplosion = true;
				s_explosionLoc = hitInfo.hitPoint;
				s_explosionAnimTime = 0.0f;
				s_explosionSprite = DXL2_Sprite::getSprite("EXPTINY.WAX");

				if (hitInfo.obj)
				{
					DXL2_LogicSystem::damageObject((GameObject*)hitInfo.obj, 1);
				}
				else if (hitInfo.hitWallId >= 0)
				{
					DXL2_InfSystem::shootWall(&hitInfo.hitPoint, hitInfo.hitSectorId, hitInfo.hitWallId);
				}
				const SoundBuffer* expTiny = DXL2_VocAsset::get("EX-TINY1.VOC");
				DXL2_Audio::playOneShot(SOUND_3D, 1.0f, MONO_SEPERATION, expTiny, false, &hitInfo.hitPoint, true);
			}
			else
			{
				s_shotSectorId = hitInfo.hitSectorId;
			}
		}
	}

	void draw(Player* player, Vec3f* cameraPos, u8 ambient)
	{
		if (s_activeWeapon < 0) { return; }

		const TextureFrame* frame = s_weapon.frames[s_weapon.frame];
		if (!frame) { return; }

		// TEST RAYTRACE
		u32 w, h;
		s_renderer->getResolution(&w, &h);
		Ray ray =
		{
			*cameraPos,
			DXL2_RenderCommon::unproject({ s32(w) / 2, s32(h) / 2 }, 10000.0f),
			player->m_sectorId,
			1000.0f
		};
		f32 rayLen = sqrtf(ray.dir.x*ray.dir.x + ray.dir.y*ray.dir.y + ray.dir.z*ray.dir.z);
		f32 rayScale = 1.0f / rayLen;
		ray.dir.x *= rayScale; ray.dir.y *= rayScale; ray.dir.z *= rayScale;

		RayHitInfo hitInfo;
		if (0 && DXL2_Physics::traceRay(&ray, &hitInfo))
		{
			Vec3f screenPos = DXL2_RenderCommon::project(&hitInfo.hitPoint);
			if (screenPos.x >= 0 && screenPos.y >= 0 && screenPos.x < w && screenPos.y < h)
			{
				s32 sz = (s32)std::max(screenPos.z * 100.0f, 1.0f);
				s_renderer->drawColoredQuad(s32(screenPos.x)-sz/2, s32(screenPos.y)-sz/2, sz, sz, 6);
			}
		}

		// DEBUG
		if (s_shot && s_model)
		{
			Vec3f angles = { 0.0f, s_shotYaw, s_shotPitch };
			const f32 ca = cosf(s_player->m_yaw);
			const f32 sa = sinf(s_player->m_yaw);
			f32 rot[] = {ca, sa};
			s32 pitchOffset = s32(f32(h/2) * s_player->m_pitch * PITCH_PIXEL_SCALE / PITCH_LIMIT) + (h/2);
						
			Vec3f modelScale = { 1.0f, 1.0f, 0.05f }, defaultScale = { 1.0f, 1.0f, 1.0f };
			DXL2_ModelRender::setModelScale(&modelScale);
			DXL2_ModelRender::setClip(0, s32(w)-1, nullptr, nullptr);
			DXL2_ModelRender::draw(s_model, &angles, &s_shotLoc, nullptr, cameraPos, rot, (f32)pitchOffset, nullptr);
			DXL2_ModelRender::setModelScale(&defaultScale);

			// Trace a ray between the current location and the next. Then check if it hit anything.
			const Ray ray =
			{
				s_shotLoc,
				s_shootDir,
				s_shotSectorId,
				10.0f
			};
			RayHitInfo hitInfo;
			if (DXL2_Physics::traceRay(&ray, &hitInfo))
			{
				// Stop the projectile.
				s_shot = false;
				s_showExplosion = true;
				s_explosionLoc = hitInfo.hitPoint;
				s_explosionAnimTime = 0.0f;
				s_explosionSprite = DXL2_Sprite::getSprite("EXPTINY.WAX");

				if (hitInfo.obj)
				{
					DXL2_LogicSystem::damageObject((GameObject*)hitInfo.obj, 1);
				}
				else if (hitInfo.hitWallId >= 0)
				{
					DXL2_InfSystem::shootWall(&hitInfo.hitPoint, hitInfo.hitSectorId, hitInfo.hitWallId);
				}
				const SoundBuffer* expTiny = DXL2_VocAsset::get("EX-TINY1.VOC");
				DXL2_Audio::playOneShot(SOUND_3D, 1.0f, MONO_SEPERATION, expTiny, false, &hitInfo.hitPoint, true);
			}
			else
			{
				s_shotSectorId = hitInfo.hitSectorId;
				s_shotLoc.x += s_shootDir.x * 10.0f;
				s_shotLoc.y += s_shootDir.y * 10.0f;
				s_shotLoc.z += s_shootDir.z * 10.0f;
			}
		}
		else if (s_showExplosion && s_explosionSprite)
		{
			f32 frameTime = s_explosionSprite->anim[0].frameRate * s_explosionAnimTime;
			s32 frame = s32(frameTime);
			if (frame >= s_explosionSprite->anim[0].angles[0].frameCount)
			{
				s_showExplosion = false;
				s_explosionAnimTime = 0.0f;
			}
			else
			{
				s32 frameIndex = s_explosionSprite->anim[0].angles[0].frameIndex[frame];
				Frame* frame = &s_explosionSprite->frames[frameIndex];

				s_explosionAnimTime += (f32)DXL2_System::getDeltaTime();
				TextureFrame texFrame = { 0 };
				texFrame.width = frame->width;
				texFrame.height = frame->height;
				texFrame.image = frame->image;

				Vec3f eProj = DXL2_RenderCommon::project(&s_explosionLoc);
				if (eProj.x >= 0.0f && eProj.y >= 0.0f)
				{
					f32 focalLength = DXL2_RenderCommon::getFocalLength() / 8.0f;
					f32 heightScale = DXL2_RenderCommon::getHeightScale() / 8.0f;

					f32 ew = frame->width  * focalLength * eProj.z;
					f32 eh = frame->height * heightScale * eProj.z;
					s32 sx = s32(256 * focalLength * eProj.z);
					s32 sy = s32(256 * heightScale * eProj.z);

					s_renderer->blitImage(&texFrame, s32(eProj.x - ew/2), s32(eProj.y - eh/2), sx, sy, 31);
				}
			}
		}

		if (s_player && s_player->m_shooting) { ambient = 31; }
		s_renderer->blitImage(frame, s_weapon.x, s_weapon.y + s_weapon.yOffset, s_screen.scaleX, s_screen.scaleY, ambient);
	}
}
