#include "hitEffect.h"
#include "animLogic.h"
#include "projectile.h"
#include <TFE_Collision/collision.h>
#include <TFE_InfSystem/message.h>
#include <TFE_Level/level.h>
#include <TFE_Level/robject.h>
#include <TFE_Memory/allocator.h>
#include <TFE_JediSound/soundSystem.h>
#include <TFE_JediTask/task.h>

using namespace TFE_Collision;
using namespace TFE_JediSound;
using namespace TFE_Level;
using namespace TFE_Message;
using namespace TFE_Memory;
using namespace TFE_TaskSystem;

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

	enum HitEffectConstants
	{
		MAX_SPEC_EFFECT_COUNT = 5,
		EXPLOSION_TRIGGER_DIST = FIXED(10),
	};

	static Wax* s_concussion2;
	static SoundSourceID s_concussionExplodeSnd;
	static EffectData s_effectData[HEFFECT_COUNT];
	static Allocator* s_hitEffects;
	static EffectData* s_curEffectData = nullptr;
	static vec3_fixed s_explodePos;

	static Task* s_hitEffectTask = nullptr;

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

			// Make the hit effect task active since there is at least one effect to process.
			task_makeActive(s_hitEffectTask);
		}
	}

	////////////////////////////////////////////////////////////////////////
	// Hit effect logic function.
	// This handles all effects currently queued up.
	////////////////////////////////////////////////////////////////////////
	void hitEffectLogicFunc(s32 id)
	{
		struct LocalContext
		{
			s32 count;
		};
		task_begin_ctx;
		taskCtx->count = 0;
		while (1)
		{
			// The task sleeps until there are effects to process.
			task_yield(TASK_SLEEP);

			// When it is woken up, either it will be directly called with id = 20 after an animation is complete or
			// is called during the update because there are effects to process.
			if (id == 20)
			{
				taskCtx->count--;
			}
			else if (id == 0)
			{
				// There are no yields in this block, so using purely local variables is safe.
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
							if (taskCtx->count >= MAX_SPEC_EFFECT_COUNT)
							{
								createEffectObj = false;
							}
							else
							{
								taskCtx->count++;
							}
						}

						// Avoid creating a new effect object if there are too many.
						if (createEffectObj)
						{
							SecObject* obj = allocateObject();
							// If effect is Concussion, adjust y so it sits on the floor.
							if (effect->type == HEFFECT_CONCUSSION || effect->type == HEFFECT_CONCUSSION2)
							{
								fixed16_16 dummy;
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
								setAnimCompleteTask(animLogic, s_hitEffectTask);
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
			}  // if (id == 0)
		}  // while (1)
		task_end;
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
		message_sendToObj(obj, MSG_WAKEUP, hitEffectMsgFunc);
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

				s_msgArg1 = s_curEffectData->damage;
			}
			else
			{
				fixed16_16 damage = s_curEffectData->damage;
				fixed16_16 attenDmg = mul16(div16(leftOverRadius, radius), damage);
				s_msgArg1 = attenDmg;
			}

			fixed16_16 force = s_curEffectData->force;
			fixed16_16 attenForce = mul16(div16(leftOverRadius, radius), force);
			s_msgArg2 = attenForce;
			message_sendToObj(obj, MSG_EXPLOSION, hitEffectMsgFunc);
		}
	}
}  // TFE_DarkForces