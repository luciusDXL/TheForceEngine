#include "hitEffect.h"
#include "animLogic.h"
#include "projectile.h"
#include <TFE_Collision/collision.h>
#include <TFE_InfSystem/infSystem.h>
#include <TFE_Level/level.h>
#include <TFE_Level/robject.h>
#include <TFE_Memory/allocator.h>
#include <TFE_JediSound/soundSystem.h>

using namespace TFE_Collision;
using namespace TFE_InfSystem;
using namespace TFE_JediSound;
using namespace TFE_Level;
using namespace TFE_Memory;

namespace TFE_DarkForces
{
	struct HitEffect
	{
		HitEffectID type;
		RSector* sector;
		fixed16_16 x;
		fixed16_16 y;
		fixed16_16 z;
		SecObject* excludeObj;
	};

	struct EffectData
	{
		HitEffectID type;
		Wax* spriteData;
		fixed16_16 force;
		fixed16_16 damage;
		fixed16_16 explosiveRange;
		fixed16_16 wakeupRange;
		SoundSourceID soundEffect;
	};

	#define MAX_SPEC_EFFECT_COUNT 5
	#define EXPLOSION_TRIGGER_DIST FIXED(10)

	static Wax* s_concussion2;
	static SoundSourceID s_concussionExplodeSnd;
	static EffectData s_effectData[HEFFECT_COUNT];
	static Allocator* s_hitEffects;
	static EffectData* s_curEffectData = nullptr;
	static s32 s_specialEffectCnt = 0;
	static vec3_fixed s_explodePos;

	void hitEffectWakeupFunc(SecObject* obj);
	void hitEffectExplodeFunc(SecObject* obj);
	
	void spawnHitEffect(HitEffectID hitEffectId, RSector* sector, vec3_fixed pos, SecObject* excludeObj)
	{
		if (hitEffectId != HEFFECT_NONE)
		{
			HitEffect* effect = (HitEffect*)allocator_newItem(s_hitEffects);
			effect->type = hitEffectId;
			effect->sector = sector;
			effect->x = pos.x;
			effect->y = pos.y;
			effect->z = pos.z;
			effect->excludeObj = excludeObj;
		}
	}

	////////////////////////////////////////////////////////////////////////
	// RE Note:
	// The task update function has been split into two:
	// the main function (taskID = 0) and the count decrement (taskID = 20).
	// This is because TFE doesn't use coroutine-like task functions
	// since they don't have any practical benefit.
	////////////////////////////////////////////////////////////////////////
	void hitEffectLogicFunc()
	{
		HitEffect* effect = (HitEffect*)allocator_getHead(s_hitEffects);
		while (effect)
		{
			RSector* sector = effect->sector;
			fixed16_16 x = effect->x;
			fixed16_16 y = effect->y;
			fixed16_16 z = effect->z;

			EffectData* data = &s_effectData[effect->type];
			s_curEffectData = data;

			if (data->spriteData)
			{
				JBool createEffectObj = JTRUE;
				if (effect->type == HEFFECT_PLASMA_EXP || effect->type == HEFFECT_CANNON_EXP)
				{
					if (s_specialEffectCnt >= MAX_SPEC_EFFECT_COUNT)
					{
						createEffectObj = false;
					}
					else
					{
						s_specialEffectCnt++;
					}
				}

				// Avoid creating a new effect object if there are too many.
				if (createEffectObj)
				{
					SecObject* obj = allocateObject();
					// If effect is Concussion, adjust y so it sits on the floor.
					if (effect->type == HEFFECT_CONCUSSION || effect->type == HEFFECT_CONCUSSION2)
					{
						s32 dummy;
						sector_getObjFloorAndCeilHeight(sector, y, &y, &dummy);
					}

					setObjPos_AddToSector(obj, x, y, z, sector);
					if (effect->type != HEFFECT_SPLASH)
					{
						obj->flags |= OBJ_FLAG_FULLBRIGHT;
					}
					sprite_setData(obj, s_curEffectData->spriteData);
					SpriteAnimLogic* animLogic = (SpriteAnimLogic*)obj_setSpriteAnim(obj);

					// Setup to call this task when animation is finished.
					if (effect->type == HEFFECT_PLASMA_EXP || effect->type == HEFFECT_CANNON_EXP)
					{
						setAnimCompleteFunc(animLogic, hitEffectFinished);
					}

					setupAnimationFromLogic(animLogic, 0/*animIndex*/, 0/*firstFrame*/, 0xffffffff/*lastFrame*/, 1/*loopCount*/);
					obj->worldWidth = 0;
				}
			}
			if (s_curEffectData->soundEffect)
			{
				vec3_fixed soundPos = { x,y,z };
				playSound3D_oneshot(s_curEffectData->soundEffect, soundPos);
			}

			if (s_curEffectData->explosiveRange)
			{
				s_explodePos.x = x;
				s_explodePos.y = y;
				s_explodePos.z = z;
				collision_effectObjectsInRange3D(sector, s_curEffectData->explosiveRange, s_explodePos, hitEffectExplodeFunc, effect->excludeObj,
					ETFLAG_AI_ACTOR | ETFLAG_SCENERY | ETFLAG_LANDMINE | ETFLAG_LANDMINE_WPN | ETFLAG_PLAYER);
			}
			if (s_curEffectData->wakeupRange)
			{
				// Wakes up all objects with a valid collision path to (x,y,z) that are within s_curEffectData->wakeupRange units.
				vec3_fixed hitPos = { x, y, z };
				collision_effectObjectsInRangeXZ(sector, s_curEffectData->wakeupRange, hitPos, hitEffectWakeupFunc, effect->excludeObj, ETFLAG_AI_ACTOR);
			}
			allocator_deleteItem(s_hitEffects, effect);
			// Since the current item is deleted, "head" is the next item in the list.
			effect = (HitEffect*)allocator_getHead(s_hitEffects);
		}  // while (effect)
	}

	void hitEffectFinished()
	{
		s_specialEffectCnt--;
	}
		
	///////////////////////////////////////
	// Internal
	///////////////////////////////////////
	void hitEffectMsgFunc(void* logic)
	{
		// TODO
	}

	void hitEffectWakeupFunc(SecObject* obj)
	{
		inf_sendObjMessage(obj, IMSG_WAKEUP, hitEffectMsgFunc);
	}

	void hitEffectExplodeFunc(SecObject* obj)
	{
		u32 flags = (obj->entityFlags & ETFLAG_AI_ACTOR) | (ETFLAG_SCENERY | ETFLAG_LANDMINE | ETFLAG_LANDMINE_WPN | ETFLAG_PLAYER);
		// Since 'or' sets the zero flag and a non-zero literal value is used, this conditional should *never* hit.
		if (!flags)
		{
			assert(0);
			return;
		}

		fixed16_16 dy = TFE_CoreMath::abs(s_explodePos.y - obj->posWS.y);
		fixed16_16 approxDist = dy + distApprox(obj->posWS.x, obj->posWS.z, s_explodePos.x, s_explodePos.z);
		// This triggers when an explosive is hit with an explosion.
		if ((obj->entityFlags & ETFLAG_LANDMINE) || (obj->entityFlags & ETFLAG_LANDMINE_WPN))
		{
			if (approxDist >= EXPLOSION_TRIGGER_DIST)
			{
				return;
			}
			triggerLandMine((ProjectileLogic*)obj->projectileLogic, (obj->entityFlags & ETFLAG_LANDMINE) ? 87 : 145);
		}
		else
		{
			fixed16_16 radius = s_curEffectData->explosiveRange;
			fixed16_16 leftOverRadius = max(radius - approxDist, 0);
			HitEffectID type = s_curEffectData->type;
			if (type == HEFFECT_CONCUSSION || type == HEFFECT_CONCUSSION2)
			{
				fixed16_16 floor, dummy;
				RSector* sector = obj->sector;
				SecObject* newObj = allocateObject();

				sprite_setData(newObj, s_concussion2);
				sector_getObjFloorAndCeilHeight(sector, s_explodePos.y, &floor, &dummy);
				setObjPos_AddToSector(newObj, obj->posWS.x, floor, obj->posWS.z, sector);

				newObj->flags |= OBJ_FLAG_FULLBRIGHT;
				newObj->worldWidth = 0;

				SpriteAnimLogic* logic = (SpriteAnimLogic*)obj_setSpriteAnim(newObj);
				setupAnimationFromLogic(logic, 0/*animIndex*/, 0/*firstFrame*/, 0xffffffff/*lastFrame*/, 1/*loopCount*/);
				playSound3D_oneshot(s_concussionExplodeSnd, newObj->posWS);

				s_infMsgArg1 = s_curEffectData->damage;
			}
			else
			{
				fixed16_16 damage = s_curEffectData->damage;
				fixed16_16 attenDmg = mul16(div16(leftOverRadius, radius), damage);
				s_infMsgArg1 = attenDmg;
			}

			fixed16_16 force = s_curEffectData->force;
			fixed16_16 attenForce = mul16(div16(leftOverRadius, radius), force);
			s_infMsgArg2 = attenForce;
			inf_sendObjMessage(obj, IMSG_EXPLOSION, hitEffectMsgFunc);
		}
	}
}  // TFE_DarkForces