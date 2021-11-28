#include <cstring>

#include "welder.h"
#include "aiActor.h"
#include "../logic.h"
#include <TFE_DarkForces/player.h>
#include <TFE_DarkForces/hitEffect.h>
#include <TFE_DarkForces/projectile.h>
#include <TFE_DarkForces/item.h>
#include <TFE_DarkForces/random.h>
#include <TFE_DarkForces/pickup.h>
#include <TFE_DarkForces/weapon.h>
#include <TFE_DarkForces/animLogic.h>
#include <TFE_Game/igame.h>
#include <TFE_Asset/modelAsset_jedi.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_Jedi/Sound/soundSystem.h>
#include <TFE_Jedi/Memory/list.h>
#include <TFE_Jedi/Memory/allocator.h>

namespace TFE_DarkForces
{
	enum WelderState
	{
		WSTATE_DEFAULT = 0,
		WSTATE_ACTIVE  = 1,
		WSTATE_RESET   = 2,
		WSTATE_DYING   = 3,
		WSTATE_COUNT
	};

	struct Welder
	{
		Logic logic;
		PhysicsActor actor;

		SoundEffectID sound2Id;
		SoundEffectID hurtSndId;
		angle14_16 yaw;
		angle14_16 pitch;
	};

	static Wax* s_welderSpark = nullptr;
	static SoundSourceID s_weld2SoundSrcId     = NULL_SOUND;
	static SoundSourceID s_weld1SoundSrcId     = NULL_SOUND;
	static SoundSourceID s_weldSparkSoundSrcId = NULL_SOUND;
	static SoundSourceID s_weldHurtSoundSrcId  = NULL_SOUND;
	static SoundSourceID s_weldDieSoundSrcId   = NULL_SOUND;

	static Welder* s_curWelder = nullptr;
	static s32 s_welderNum = 0;

	MessageType welder_handleDamage(MessageType msg, Welder* welder)
	{
		PhysicsActor* physicsActor = &welder->actor;
		SecObject* obj = welder->logic.obj;
		if (physicsActor->alive)
		{
			ProjectileLogic* proj = (ProjectileLogic*)s_msgEntity;
			physicsActor->hp -= proj->dmg;
			if (physicsActor->hp <= 0)
			{
				physicsActor->state = WSTATE_DYING;
				msg = MSG_RUN_TASK;
			}
			else
			{
				if (welder->hurtSndId)
				{
					stopSound(welder->hurtSndId);
				}
				welder->hurtSndId = playSound3D_oneshot(s_weldHurtSoundSrcId, obj->posWS);
				msg = MSG_DAMAGE;
			}
		}
		return msg;
	}

	MessageType welder_handleExplosion(MessageType msg, Welder* welder)
	{
		PhysicsActor* physicsActor = &welder->actor;
		SecObject* obj = welder->logic.obj;
		if (physicsActor->alive)
		{
			fixed16_16 dmg = s_msgArg1;
			physicsActor->hp -= dmg;
			if (physicsActor->hp <= 0)
			{
				physicsActor->state = WSTATE_DYING;
				msg = MSG_RUN_TASK;
			}
			else
			{
				if (welder->hurtSndId)
				{
					stopSound(welder->hurtSndId);
				}
				welder->hurtSndId = playSound3D_oneshot(s_weldHurtSoundSrcId, obj->posWS);
				msg = MSG_EXPLOSION;
			}
		}
		return msg;
	}

	void welder_handleDefaultState(MessageType msg)
	{
		struct LocalContext
		{
			Welder* welder;
			SecObject* obj;
			PhysicsActor* physicsActor;
		};
		task_begin_ctx;

		local(welder) = s_curWelder;
		local(obj) = local(welder)->logic.obj;
		local(physicsActor) = &local(welder)->actor;
		while (local(physicsActor)->state == WSTATE_DEFAULT)
		{
			do
			{
				task_yield(145);
				if (msg == MSG_DAMAGE)
				{
					msg = welder_handleDamage(msg, local(welder));
				}
				else if (msg == MSG_EXPLOSION)
				{
					msg = welder_handleExplosion(msg, local(welder));
				}
				if (msg == MSG_DAMAGE || msg == MSG_EXPLOSION)
				{
					local(physicsActor)->state = WSTATE_ACTIVE;
					task_makeActive(local(physicsActor)->actorTask);
					task_yield(TASK_NO_DELAY);
				}
			} while (msg != MSG_RUN_TASK);
			if (local(physicsActor)->state != WSTATE_DEFAULT || s_playerDying) { break; }

			if (actor_isObjectVisible(local(obj), s_playerObject, ANGLE_MAX, FIXED(25)))
			{
				fixed16_16 dy = TFE_Jedi::abs(local(obj)->posWS.y - s_playerObject->posWS.y);
				fixed16_16 dist = dy + distApprox(s_playerObject->posWS.x, s_playerObject->posWS.z, local(obj)->posWS.x, local(obj)->posWS.z);
				// Switch to the attack state if the player is close enough.
				if (dist <= FIXED(40))
				{
					local(physicsActor)->state = WSTATE_ACTIVE;
				}
			}
		}  // while (state == WSTATE_DEFAULT)

		task_end;
	}

	void welder_handleActiveState(MessageType msg)
	{
		struct LocalContext
		{
			Welder* welder;
			SecObject* obj;
			PhysicsActor* physicsActor;
			ActorTarget* target;
			RSector* sector;
			JBool attack;
			fixed16_16 dist;
			fixed16_16 dy;
		};
		task_begin_ctx;

		local(welder) = s_curWelder;
		local(obj) = local(welder)->logic.obj;
		local(physicsActor) = &local(welder)->actor;
		local(target) = &local(physicsActor)->actor.target;
		local(sector) = local(obj)->sector;
		local(attack) = JFALSE;

		while (local(physicsActor)->state == WSTATE_ACTIVE)
		{
			do
			{
				task_yield(TASK_NO_DELAY);
				if (msg == MSG_DAMAGE)
				{
					msg = welder_handleDamage(msg, local(welder));
				}
				else if (msg == MSG_EXPLOSION)
				{
					msg = welder_handleExplosion(msg, local(welder));
				}
			} while (msg != MSG_RUN_TASK);
			if (local(physicsActor)->state != WSTATE_ACTIVE) { break; }

			// Is the yaw aligned to the target?
			if ((local(obj)->yaw & ANGLE_MASK) == (local(target)->yaw & ANGLE_MASK))
			{
				stopSound(local(welder)->sound2Id);
				if (local(attack))
				{
					playSound3D_oneshot(s_weld1SoundSrcId, local(obj)->posWS);
				}
				local(dy) = TFE_Jedi::abs(local(obj)->posWS.y - s_playerObject->posWS.y);
				local(dist) = local(dy) + distApprox(s_playerObject->posWS.x, s_playerObject->posWS.z, local(obj)->posWS.x, local(obj)->posWS.z);
				if (local(dist) <= FIXED(40) && actor_isObjectVisible(local(obj), s_playerObject, ANGLE_MAX, FIXED(25)))
				{
					if (local(attack))
					{
						fixed16_16 sinYaw, cosYaw;
						fixed16_16 sinPitch, cosPitch;
						sinCosFixed(local(obj)->yaw, &sinYaw, &cosYaw);
						sinCosFixed(local(obj)->pitch, &sinPitch, &cosPitch);

						fixed16_16 armTipX = local(obj)->posWS.x + mul16(FIXED(19), sinYaw);
						fixed16_16 armTipZ = local(obj)->posWS.z + mul16(FIXED(19), cosYaw);
						fixed16_16 armTipY = FIXED(3) + local(obj)->posWS.y + mul16(FIXED(19), sinPitch);

						SecObject* spark = allocateObject();
						setObjPos_AddToSector(spark, armTipX, armTipY, armTipZ, local(sector));
						spark->flags |= OBJ_FLAG_FULLBRIGHT;
						sprite_setData(spark, s_welderSpark);

						SpriteAnimLogic* animLogic = (SpriteAnimLogic*)obj_setSpriteAnim(spark);
						setupAnimationFromLogic(animLogic, 0, 0, 0xffffffff, 1);
						playSound3D_oneshot(s_weldSparkSoundSrcId, spark->posWS);

						spark->worldWidth = 0;
						local(dy) = TFE_Jedi::abs(armTipY - s_playerObject->posWS.y);
						local(dist) = local(dy) + distApprox(s_playerObject->posWS.x, s_playerObject->posWS.z, armTipX, armTipZ);
						if (local(dist) <= FIXED(35))
						{
							player_applyDamage(FIXED(20), 0, JTRUE);
						}
					}

					do
					{
						task_yield(72);
						if (msg == MSG_DAMAGE)
						{
							msg = welder_handleDamage(msg, local(welder));
						}
						else if (msg == MSG_EXPLOSION)
						{
							msg = welder_handleExplosion(msg, local(welder));
						}
					} while (msg != MSG_RUN_TASK);

					fixed16_16 dz = s_playerObject->posWS.z - local(obj)->posWS.z;
					fixed16_16 dx = s_playerObject->posWS.x - local(obj)->posWS.x;
					local(dy) = local(obj)->posWS.y - s_eyePos.y + ONE_16;
					
					local(target)->yaw   = vec2ToAngle(dx, dz);
					local(target)->pitch = vec2ToAngle(local(dy), FIXED(19));

					if (local(target)->yaw != local(obj)->yaw || local(target)->pitch != local(obj)->pitch)
					{
						local(target)->flags |= 4;
						local(attack) = JTRUE;
						local(welder)->sound2Id = playSound3D_oneshot(s_weld2SoundSrcId, local(obj)->posWS);
					}
					else
					{
						local(target)->flags &= 0xfffffffb;
						local(attack) = JFALSE;
					}
				}
				else
				{
					local(physicsActor)->state = WSTATE_RESET;
				}
			}
		}  // while (state == WSTATE_ACTIVE)

		task_end;
	}

	void welder_handleReset(MessageType msg)
	{
		struct LocalContext
		{
			Welder* welder;
			SecObject* obj;
			PhysicsActor* physicsActor;
			ActorTarget* target;
		};
		task_begin_ctx;

		local(welder) = s_curWelder;
		local(obj) = local(welder)->logic.obj;
		local(physicsActor) = &local(welder)->actor;
		local(target) = &local(physicsActor)->actor.target;

		local(target)->yaw = local(welder)->yaw;
		local(target)->pitch = local(welder)->pitch;
		local(target)->flags = (local(target)->flags | 4) & 0xfffffffe;
		local(welder)->sound2Id = playSound3D_oneshot(s_weld2SoundSrcId, local(obj)->posWS);

		while (local(physicsActor)->state == WSTATE_RESET)
		{
			do
			{
				task_yield(TASK_NO_DELAY);
				if (msg == MSG_DAMAGE)
				{
					msg = welder_handleDamage(msg, local(welder));
				}
				else if (msg == MSG_EXPLOSION)
				{
					msg = welder_handleExplosion(msg, local(welder));
				}
			} while (msg != MSG_RUN_TASK);
			if (local(physicsActor)->state != WSTATE_RESET) { break; }

			if (local(target)->yaw == local(obj)->yaw)
			{
				stopSound(local(welder)->sound2Id);
				playSound3D_oneshot(s_weld1SoundSrcId, local(obj)->posWS);
				local(physicsActor)->state = WSTATE_DEFAULT;
			}
		}  // while (state == WSTATE_RESET)

		task_end;
	}

	void welderTaskFunc(MessageType msg)
	{
		struct LocalContext
		{
			Welder* welder;
			PhysicsActor* physicsActor;
			SecObject* obj;
			ActorTarget* target;
		};
		task_begin_ctx;

		local(welder) = (Welder*)task_getUserData();
		local(physicsActor) = &local(welder)->actor;
		local(obj) = local(welder)->logic.obj;
		local(target) = &local(physicsActor)->actor.target;
		while (local(physicsActor)->alive)
		{
			msg = MSG_RUN_TASK;
			task_setMessage(MSG_RUN_TASK);

			if (local(physicsActor)->state == WSTATE_ACTIVE)
			{
				s_curWelder = local(welder);
				task_callTaskFunc(welder_handleActiveState);
			}
			else if (local(physicsActor)->state == WSTATE_RESET)
			{
				s_curWelder = local(welder);
				task_callTaskFunc(welder_handleReset);
			}
			else if (local(physicsActor)->state == WSTATE_DYING)
			{
				local(target)->flags |= 8;
				stopSound(local(welder)->sound2Id);
				playSound3D_oneshot(s_weldDieSoundSrcId, local(obj)->posWS);

				local(target)->pitch = 64171 & ANGLE_MASK;
				local(target)->yaw = local(obj)->yaw;
				local(target)->flags |= 4;

				do
				{
					task_yield(TASK_NO_DELAY);
				} while ((local(obj)->yaw & 0x3fff) != (local(target)->yaw & 0x3fff) || msg != MSG_RUN_TASK);

				spawnHitEffect(HEFFECT_EXP_NO_DMG, local(obj)->sector, local(obj)->posWS, local(obj));
				local(physicsActor)->alive = JFALSE;
			}
			else
			{
				s_curWelder = local(welder);
				task_callTaskFunc(welder_handleDefaultState);
			}
		}  // while (alive)

		// Dead:
		while (msg != MSG_RUN_TASK)
		{
			task_yield(TASK_NO_DELAY);
		}
		actor_removePhysicsActorFromWorld(local(physicsActor));
		deleteLogicAndObject((Logic*)local(welder));
		level_free(local(welder));

		task_end;
	}

	void welderCleanupFunc(Logic* logic)
	{
		Welder* welder = (Welder*)logic;

		actor_removePhysicsActorFromWorld(&welder->actor);
		deleteLogicAndObject(logic);
		level_free(welder);
		task_free(welder->actor.actorTask);
	}

	Logic* welder_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		if (!s_welderSpark)
		{
			s_welderSpark = TFE_Sprite_Jedi::getWax("spark.wax");
		}
		if (!s_weld2SoundSrcId)
		{
			s_weld2SoundSrcId = sound_Load("weld-2.voc");
		}
		if (!s_weld1SoundSrcId)
		{
			s_weld1SoundSrcId = sound_Load("weld-1.voc");
		}
		if (!s_weldSparkSoundSrcId)
		{
			s_weldSparkSoundSrcId = sound_Load("weldsht1.voc");
		}
		if (!s_weldHurtSoundSrcId)
		{
			s_weldHurtSoundSrcId = sound_Load("weldhrt.voc");
		}
		if (!s_weldDieSoundSrcId)
		{
			s_weldDieSoundSrcId = sound_Load("weld-die.voc");
		}

		Welder* welder = (Welder*)level_alloc(sizeof(Welder));
		memset(welder, 0, sizeof(Welder));

		// Give the name of the task a number so I can tell them apart when debugging.
		char name[32];
		sprintf(name, "Welder%d", s_welderNum);
		s_welderNum++;
		Task* task = createSubTask(name, welderTaskFunc);
		task_setUserData(task, welder);

		obj->entityFlags = ETFLAG_AI_ACTOR;
		PhysicsActor* physicsActor = &welder->actor;
		physicsActor->alive = JTRUE;
		physicsActor->hp = FIXED(130);
		physicsActor->state = WSTATE_DEFAULT;
		physicsActor->actorTask = task;
		welder->logic.obj = obj;
		welder->yaw = obj->yaw;
		welder->pitch = obj->pitch;
		welder->hurtSndId = NULL_SOUND;
		welder->sound2Id  = NULL_SOUND;
		actor_addPhysicsActorToWorld(physicsActor);
		CollisionInfo* physics = &physicsActor->actor.physics;

		ActorTarget* target = &physicsActor->actor.target;
		obj->worldWidth  = FIXED(10);
		obj->worldHeight = FIXED(4);
		physicsActor->actor.header.obj = obj;
		physics->obj = obj;
		physics->width = obj->worldWidth;
		physics->wall = nullptr;
		physics->u24 = 0;
		physics->botOffset = 0;
		physics->yPos = 0;
		physics->height = obj->worldHeight + HALF_16;

		physicsActor->actor.delta = { 0, 0, 0 };
		physicsActor->actor.collisionFlags &= 0xfffffff8;

		target->speed = 0;
		target->speedRotation = 4551;	// ~100 degrees/second.
		target->flags &= 0xfffffff0;
		target->pitch = obj->pitch;
		target->yaw   = obj->yaw;
		target->roll  = obj->roll;
		obj_addLogic(obj, &welder->logic, task, welderCleanupFunc);

		if (setupFunc)
		{
			*setupFunc = nullptr;
		}
		return (Logic*)welder;
	}
}  // namespace TFE_DarkForces