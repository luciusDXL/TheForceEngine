#include "sewer.h"
#include "aiActor.h"
#include "../logic.h"
#include "../gameMusic.h"
#include <TFE_DarkForces/player.h>
#include <TFE_DarkForces/hitEffect.h>
#include <TFE_DarkForces/projectile.h>
#include <TFE_DarkForces/item.h>
#include <TFE_DarkForces/random.h>
#include <TFE_DarkForces/pickup.h>
#include <TFE_DarkForces/weapon.h>
#include <TFE_Game/igame.h>
#include <TFE_Asset/modelAsset_jedi.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_Jedi/Sound/soundSystem.h>
#include <TFE_Jedi/Memory/list.h>
#include <TFE_Jedi/Memory/allocator.h>

namespace TFE_DarkForces
{
	static const s32 s_sewerCreatureAnimTable[] =
	{ -1, 1, 2, 3, 4, 5, 6, -1, -1, -1, -1, -1, -1, -1, 12, 13 };

	u32 sewerCreatureDie(AiActor* aiActor, Actor* actor);

	JBool sewerCreatureAiFunc(AiActor* aiActor, Actor* actor)
	{
		ActorEnemy* enemy = &aiActor->enemy;
		SecObject* obj = enemy->header.obj;
		RSector* sector = obj->sector;
		LogicAnimation* anim = &enemy->anim;

		if (!(anim->flags & 2))
		{
			if (obj->type == OBJ_TYPE_SPRITE)
			{
				actor_setCurAnimation(&enemy->anim);
			}
			actor->updateTargetFunc(actor, &enemy->target);
			return 0;
		}
		else if (aiActor->hp > 0)
		{
			return 0xffffffff;
		}

		// Creature die.
		s32 animIndex = actor_getAnimationIndex(4);
		if (animIndex != -1)
		{
			SecObject* corpse = allocateObject();
			sprite_setData(corpse, obj->wax);
			corpse->frame = 0;
			corpse->anim = animIndex;
			corpse->posWS.x = obj->posWS.x;
			corpse->posWS.z = obj->posWS.z;
			corpse->posWS.y = sector->floorHeight;
			corpse->entityFlags |= ETFLAG_CORPSE;
			corpse->worldWidth = 0;
			sector_addObject(sector, corpse);
		}
		actor_kill();
		return 0;
	}
	
	JBool sewerCreatureAiMsgFunc(s32 msg, AiActor* aiActor, Actor* actor)
	{
		ActorEnemy* enemy = &aiActor->enemy;
		SecObject* obj = enemy->header.obj;
		RSector* sector = obj->sector;
		LogicAnimation* anim = &enemy->anim;

		if (msg == MSG_DAMAGE)
		{
			if (aiActor->hp <= 0)
			{
				if (obj->type == OBJ_TYPE_SPRITE)
				{
					actor_setCurAnimation(&enemy->anim);
				}
				actor->updateTargetFunc(actor, &enemy->target);
				return 0;
			}

			ProjectileLogic* proj = (ProjectileLogic*)s_msgEntity;
			aiActor->hp -= proj->dmg;
			if (aiActor->hp <= 0)
			{
				return sewerCreatureDie(aiActor, actor);
			}

			stopSound(aiActor->hurtSndID);
			aiActor->hurtSndID = playSound3D_oneshot(aiActor->hurtSndSrc, obj->posWS);
			return 0xffffffff;
		}
		else if (msg == MSG_EXPLOSION)
		{
			if (aiActor->hp <= 0)
			{
				if (obj->type == OBJ_TYPE_SPRITE)
				{
					actor_setCurAnimation(&enemy->anim);
				}
				actor->updateTargetFunc(actor, &enemy->target);
				return 0;
			}

			fixed16_16 dmg = s_msgArg1;
			aiActor->hp -= dmg;
			if (aiActor->hp > 0)
			{
				stopSound(aiActor->hurtSndID);
				aiActor->hurtSndID = playSound3D_oneshot(aiActor->hurtSndSrc, obj->posWS);
				return 0xffffffff;
			}
			return sewerCreatureDie(aiActor, actor);
		}
		return enemy->header.nextTick;
	}

	JBool sewerCreatureEnemyFunc(AiActor* aiActor, Actor* actor)
	{
		ActorEnemy* enemy = &aiActor->enemy;
		SecObject* obj = enemy->header.obj;
		RSector* sector = obj->sector;

		switch (enemy->anim.state)
		{
			case 0:
			{
				if (enemy->anim.flags & 2)
				{
					obj->flags &= ~(OBJ_FLAG_NEEDS_TRANSFORM | OBJ_FLAG_HAS_COLLISION);
					obj->worldWidth = 0;
					obj->posWS.y = sector->floorHeight + sector->secHeight;

					actor->collisionFlags |= 1;
					enemy->target.flags &= ~(1|2|4|8);
					enemy->anim.state = 1;
					return s_curTick + random(enemy->timing.delay);
				}
			} break;
			case 1:
			{
				gameMusic_sustainFight();
				if (!actor_canSeeObjFromDist(obj, s_playerObject))
				{
					actor_updatePlayerVisiblity(JFALSE, 0, 0);
					enemy->anim.flags |= 2;
					enemy->anim.state = 0;
					if (s_curTick > enemy->timing.nextTick)
					{
						enemy->timing.delay = enemy->timing.state0Delay;
						actor_setupInitAnimation();
					}
					return enemy->timing.delay;
				}

				actor_updatePlayerVisiblity(JTRUE, s_eyePos.x, s_eyePos.z);
				actor->collisionFlags &= ~1;

				obj->posWS.y = sector->floorHeight;
				fixed16_16 dy = TFE_Jedi::abs(obj->posWS.y - s_playerObject->posWS.y);
				fixed16_16 dist = dy + distApprox(s_playerObject->posWS.x, s_playerObject->posWS.z, obj->posWS.x, obj->posWS.z);
				if (dist <= enemy->meleeRange) 
				{
					enemy->anim.state = 2;
					enemy->timing.delay = enemy->timing.state2Delay;
					enemy->target.pos.x = obj->posWS.x;
					enemy->target.pos.z = obj->posWS.z;
					enemy->target.flags |= 1;

					fixed16_16 dx = s_eyePos.x - obj->posWS.x;
					fixed16_16 dz = s_eyePos.z - obj->posWS.z;
					enemy->target.yaw   = vec2ToAngle(dx, dz);
					enemy->target.pitch = obj->pitch;
					enemy->target.roll  = obj->roll;
					enemy->target.flags |= 4;
				}
				else
				{
					enemy->anim.state = 0;
					enemy->timing.delay = enemy->timing.state0Delay;
				}

				enemy->timing.nextTick = s_curTick + enemy->timing.state1Delay;
				enemy->target.flags |= 8;
				obj->worldWidth = FIXED(3);
				obj->flags |= (OBJ_FLAG_NEEDS_TRANSFORM | OBJ_FLAG_HAS_COLLISION);
				if (obj->type == OBJ_TYPE_SPRITE)
				{
					if (enemy->anim.state == 2)  // Attack animation
					{
						actor_setupAnimation(1, &enemy->anim);
					}
					else // Look around animation
					{
						actor_setupAnimation(14, &enemy->anim);
					}
				}
			} break;
			case 2:
			{
				if (enemy->anim.flags & 2)
				{
					enemy->anim.state = 3;
					playSound3D_oneshot(enemy->attackSecSndSrc, obj->posWS);

					fixed16_16 dy = TFE_Jedi::abs(obj->posWS.y - s_playerObject->posWS.y);
					fixed16_16 dist = dy + distApprox(s_playerObject->posWS.x, s_playerObject->posWS.z, obj->posWS.x, obj->posWS.z);
					if (dist <= enemy->meleeRange)
					{
						player_applyDamage(enemy->meleeDmg, 0, JTRUE);
					}
				}
			} break;
			case 3:
			{
				actor_setupAnimation(6, &enemy->anim);
				enemy->anim.state = 0;
			} break;
		}

		actor_setCurAnimation(&enemy->anim);
		actor->updateTargetFunc(actor, &enemy->target);
		return enemy->timing.delay;
	}

	Logic* sewerCreature_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		obj->flags &= ~(OBJ_FLAG_NEEDS_TRANSFORM | OBJ_FLAG_HAS_COLLISION);
		obj->entityFlags = ETFLAG_AI_ACTOR;
		obj->worldWidth >>= 1;

		ActorLogic* logic = actor_setupActorLogic(obj, setupFunc);
		logic->alertSndSrc = s_alertSndSrc[ALERT_CREATURE];
		logic->fov = ANGLE_MAX;

		AiActor* aiActor = actor_createAiActor((Logic*)logic);
		aiActor->enemy.header.func = sewerCreatureAiFunc;
		aiActor->enemy.header.msgFunc = sewerCreatureAiMsgFunc;
		aiActor->hp = FIXED(36);
		aiActor->hurtSndSrc = s_agentSndSrc[AGENTSND_CREATURE_HURT];
		aiActor->dieSndSrc = s_agentSndSrc[AGENTSND_CREATURE_DIE];
		actorLogic_addActor(logic, (AiActor*)aiActor, SAT_AI_ACTOR);

		ActorEnemy* enemyActor = actor_createEnemyActor((Logic*)logic);
		s_actorState.curEnemyActor = enemyActor;
		enemyActor->header.func = sewerCreatureEnemyFunc;
		enemyActor->timing.state0Delay = 1240;
		enemyActor->timing.state2Delay = 1240;
		enemyActor->meleeRange = FIXED(13);
		enemyActor->meleeDmg = FIXED(20);
		enemyActor->ua4 = FIXED(360);
		enemyActor->attackSecSndSrc = s_agentSndSrc[AGENTSND_CREATURE2];
		enemyActor->attackFlags = (enemyActor->attackFlags | 1) & 0xfffffffd;
		actorLogic_addActor(logic, (AiActor*)enemyActor, SAT_ENEMY);

		ActorSimple* actorSimple = actor_createSimpleActor((Logic*)logic);
		actorSimple->target.speedRotation = 0x7fff;
		actorSimple->target.speed = FIXED(18);
		actorSimple->u3c = 58;
		actorSimple->startDelay = 72;
		actorSimple->anim.flags &= 0xfffffffe;
		actorLogic_addActor(logic, (AiActor*)actorSimple, SAT_SIMPLE);

		Actor* actor = actor_create((Logic*)logic);	// eax
		logic->actor = actor;
		logic->animTable = s_sewerCreatureAnimTable;
		obj->entityFlags &= ~ETFLAG_SMART_OBJ;

		actor->collisionFlags = (actor->collisionFlags | 1) & 0xfffffffd;
		actor->physics.yPos = 0;
		actor->physics.botOffset = 0;
		actor->physics.width = obj->worldWidth;
		actor_setupInitAnimation();

		return (Logic*)logic;
	}

	u32 sewerCreatureDie(AiActor* aiActor, Actor* actor)
	{
		ActorEnemy* enemy = &aiActor->enemy;
		SecObject* obj = enemy->header.obj;
		RSector* sector = obj->sector;

		actor_setDeathCollisionFlags();
		ActorLogic* logic = (ActorLogic*)s_actorState.curLogic;
		stopSound(logic->alertSndID);
		playSound3D_oneshot(aiActor->dieSndSrc, obj->posWS);
		enemy->target.flags |= 8;

		if ((obj->anim == 1 || obj->anim == 6) && obj->type == OBJ_TYPE_SPRITE)
		{
			actor_setupAnimation(2, &enemy->anim);
		}
		else if (obj->type == OBJ_TYPE_SPRITE)
		{
			actor_setupAnimation(3, &enemy->anim);
		}

		obj->posWS.y = sector->floorHeight;
		actor->collisionFlags &= 0xfffffffd;

		if (obj->type == OBJ_TYPE_SPRITE)
		{
			actor_setCurAnimation(&enemy->anim);
		}
		actor->updateTargetFunc(actor, &enemy->target);
		return 0;
	}
}  // namespace TFE_DarkForces