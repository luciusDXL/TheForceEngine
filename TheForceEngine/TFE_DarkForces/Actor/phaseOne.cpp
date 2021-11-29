#include <cstring>

#include "phaseOne.h"
#include "aiActor.h"
#include "../logic.h"
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
	struct PhaseOne
	{
		Logic logic;
		PhysicsActor actor;

		SoundEffectID hitSndId;
		SoundEffectID reflectSndId;
		JBool canDamage;
	};

	static SoundSourceID s_phase1aSndID       = NULL_SOUND;
	static SoundSourceID s_phase1bSndID       = NULL_SOUND;
	static SoundSourceID s_phase1cSndID       = NULL_SOUND;
	static SoundSourceID s_phase1SwordSndID   = NULL_SOUND;
	static SoundSourceID s_phase1ReflectSndID = NULL_SOUND;

	static PhaseOne* s_curTrooper = nullptr;
	static s32 s_trooperNum = 0;

	JBool phaseOne_canSeePlayer(PhaseOne* trooper)
	{
		if (actor_canSeeObject(trooper->logic.obj, s_playerObject))
		{
			trooper->actor.lastPlayerPos.x = s_playerObject->posWS.x;
			trooper->actor.lastPlayerPos.z = s_playerObject->posWS.z;
			return JTRUE;
		}
		return JFALSE;
	}

	void phaseOne_reflectShot(MessageType msg)
	{
		struct LocalContext
		{
			PhaseOne* trooper;
			SecObject* obj;
			PhysicsActor* physicsActor;
			ActorTarget* target;
			LogicAnimation* anim;
			LogicAnimation tmp;
			JBool restoreAnim;
		};
		task_begin_ctx;

		local(trooper) = s_curTrooper;
		local(obj) = local(trooper)->logic.obj;
		local(physicsActor) = &local(trooper)->actor;
		local(target) = &local(physicsActor)->actor.target;
		local(anim) = &local(physicsActor)->anim;
		local(trooper)->canDamage = JTRUE;
		local(restoreAnim) = JFALSE;

		if (local(physicsActor)->alive)
		{
			if (random(100) <= 70)
			{
				task_localBlockBegin;
					ProjectileLogic* proj = (ProjectileLogic*)s_msgEntity;
					SecObject* projObj = proj->logic.obj;
					s_projReflectOverrideYaw = projObj->yaw + 0x1000;
					local(target)->flags |= 8;
					s_hitWallFlag = 4;

					// Save Animation.
					local(restoreAnim) = JTRUE;
					memcpy(&local(tmp), local(anim), sizeof(LogicAnimation) - 4);

					// Handle reflection.
					stopSound(local(trooper)->reflectSndId);
					local(trooper)->reflectSndId = playSound3D_oneshot(s_phase1ReflectSndID, local(obj)->posWS);
					local(anim)->flags |= 1;
					actor_setupAnimation2(local(obj), 13, local(anim));
				task_localBlockEnd;

				// Wait for animation to finish.
				do
				{
					task_yield(TASK_NO_DELAY);
					if (msg == MSG_DAMAGE)
					{
						ProjectileLogic* proj = (ProjectileLogic*)s_msgEntity;
						local(physicsActor)->hp -= proj->dmg;
					}
					else if (msg == MSG_EXPLOSION)
					{
						if (s_curEffectData->type == HEFFECT_THERMDET_EXP)
						{
							s_msgArg1 >>= 1;
						}
						local(physicsActor)->hp -= s_msgArg1;
					}
				} while (msg != MSG_RUN_TASK || !(local(anim)->flags & 2));
			}
			else
			{
				task_localBlockBegin;
					ProjectileLogic* proj = (ProjectileLogic*)s_msgEntity;
					local(physicsActor)->hp -= proj->dmg;
				task_localBlockEnd;

				if (local(physicsActor)->hp <= 0)
				{
					local(physicsActor)->state = 4;
					msg = MSG_RUN_TASK;
					task_setMessage(msg);
				}
				else
				{
					stopSound(local(trooper)->hitSndId);
					local(trooper)->hitSndId = playSound3D_oneshot(s_phase1bSndID, local(obj)->posWS);
					if (random(100) <= 20)
					{
						local(target)->flags |= 8;
						// Save Animation.
						local(restoreAnim) = JTRUE;
						memcpy(&local(tmp), local(anim), sizeof(LogicAnimation) - 4);

						local(anim)->flags |= 1;
						actor_setupAnimation2(local(obj), 12, local(anim));

						// Wait for animation to finish.
						do
						{
							task_yield(TASK_NO_DELAY);
							if (msg == MSG_DAMAGE)
							{
								ProjectileLogic* proj = (ProjectileLogic*)s_msgEntity;
								local(physicsActor)->hp -= proj->dmg;
							}
							else if (msg == MSG_EXPLOSION)
							{
								if (s_curEffectData->type == HEFFECT_THERMDET_EXP)
								{
									s_msgArg1 >>= 1;
								}
								local(physicsActor)->hp -= s_msgArg1;
							}
						} while (msg != MSG_RUN_TASK || !(local(anim)->flags & 2));
					}
				}
			}
			// Restore Animation.
			if (local(restoreAnim))
			{
				memcpy(local(anim), &local(tmp), sizeof(LogicAnimation) - 4);
				actor_setupAnimation2(local(obj), local(anim)->animId, local(anim));
				local(target)->flags &= 0xfffffff7;
			}
			msg = MSG_RUN_TASK;
			task_setMessage(msg);
		}
		local(trooper)->canDamage = JFALSE;
		task_end;
	}

	void phaseOne_handleExplosion(MessageType msg)
	{
		struct LocalContext
		{
			PhaseOne* trooper;
			SecObject* obj;
			PhysicsActor* physicsActor;
			ActorTarget* target;
			LogicAnimation* anim;
			LogicAnimation tmp;
			vec3_fixed vel;
		};
		task_begin_ctx;

		local(trooper) = s_curTrooper;
		local(obj) = local(trooper)->logic.obj;
		local(physicsActor) = &local(trooper)->actor;
		local(target) = &local(physicsActor)->actor.target;
		local(anim) = &local(physicsActor)->anim;
		local(trooper)->canDamage = JTRUE;

		if (local(physicsActor)->alive)
		{
			task_localBlockBegin;
				fixed16_16 dmg   = s_msgArg1;
				fixed16_16 force = s_msgArg2;

				local(physicsActor)->hp -= dmg;
				vec3_fixed pos = { local(obj)->posWS.x, local(obj)->posWS.y - local(obj)->worldHeight, local(obj)->posWS.z };
				vec3_fixed pushDir;

				computeExplosionPushDir(&pos, &pushDir);
				local(vel) = { mul16(pushDir.x, force), mul16(pushDir.y, force), mul16(pushDir.z, force) };
			task_localBlockEnd;

			if (local(physicsActor)->hp <= 0)
			{
				local(physicsActor)->state = 4;
				local(physicsActor)->vel = local(vel);
				msg = MSG_RUN_TASK;
				task_setMessage(msg);
			}
			else
			{
				local(vel).x -= (local(vel).x < 0 ? 1 : 0);
				local(vel).y -= (local(vel).y < 0 ? 1 : 0);
				local(vel).z -= (local(vel).z < 0 ? 1 : 0);
				local(physicsActor)->vel = { local(vel).x >> 1, local(vel).y >> 1, local(vel).z >> 1 };

				stopSound(local(trooper)->hitSndId);
				local(trooper)->hitSndId = playSound3D_oneshot(s_phase1bSndID, local(obj)->posWS);

				if (random(100) <= 20)
				{
					local(target)->flags |= 8;

					// Save Animation
					memcpy(&local(tmp), local(anim), sizeof(LogicAnimation) - 4);

					local(anim)->flags |= 1;
					actor_setupAnimation2(local(obj), 12, local(anim));

					// Wait for animation to finish.
					do
					{
						task_yield(TASK_NO_DELAY);
						if (msg == MSG_DAMAGE)
						{
							ProjectileLogic* proj = (ProjectileLogic*)s_msgEntity;
							local(physicsActor)->hp -= proj->dmg;
						}
						else if (msg == MSG_EXPLOSION)
						{
							if (s_curEffectData->type == HEFFECT_THERMDET_EXP)
							{
								s_msgArg1 >>= 1;
							}
							local(physicsActor)->hp -= s_msgArg1;
						}
					} while (msg != MSG_RUN_TASK || !(local(anim)->flags & 2));

					memcpy(local(anim), &local(tmp), sizeof(LogicAnimation) - 4);
					actor_setupAnimation2(local(obj), local(anim)->animId, local(anim));
					local(target)->flags &= 0xfffffff7;
				}
				msg = MSG_EXPLOSION;
				task_setMessage(msg);
			}
		}  // if (alive)
		local(trooper)->canDamage = JFALSE;
		task_end;
	}

	void phaseOne_handleMsg(MessageType msg)
	{
		struct LocalContext
		{
			PhaseOne* trooper;
		};
		task_begin_ctx;
		local(trooper) = s_curTrooper;
		
		if (msg == MSG_DAMAGE)
		{
			if (!local(trooper)->canDamage)
			{
				s_curTrooper = local(trooper);
				task_callTaskFunc(phaseOne_reflectShot);
			}
			else
			{
				ProjectileLogic* proj = (ProjectileLogic*)s_msgEntity;
				local(trooper)->actor.hp -= proj->dmg;
			}
		}
		else if (msg == MSG_EXPLOSION)
		{
			if (s_curEffectData->type == HEFFECT_THERMDET_EXP)
			{
				s_msgArg1 >>= 1;
			}
			if (!local(trooper)->canDamage)
			{
				s_curTrooper = local(trooper);
				task_callTaskFunc(phaseOne_handleExplosion);
			}
			else
			{
				local(trooper)->actor.hp -= s_msgArg1;
			}
		}
		task_end;
	}

	void phaseOne_handleCharge(MessageType msg)
	{
		struct LocalContext
		{
			PhaseOne* trooper;
			SecObject* obj;
			PhysicsActor* physicsActor;
			ActorTarget* target;
			LogicAnimation* anim;
			u32 prevColTick;
		};
		task_begin_ctx;

		local(trooper) = s_curTrooper;
		local(obj) = local(trooper)->logic.obj;
		local(physicsActor) = &local(trooper)->actor;
		local(target) = &local(physicsActor)->actor.target;
		local(anim) = &local(physicsActor)->anim;
		local(prevColTick) = 0;

		local(anim)->flags &= 0xfffffffe;
		actor_setupAnimation2(local(obj), 0, local(anim));

		while (local(physicsActor)->state == 1)
		{
			do
			{
				task_yield(TASK_NO_DELAY);
				s_curTrooper = local(trooper);
				task_callTaskFunc(phaseOne_handleMsg);
			} while (msg != MSG_RUN_TASK);
			if (local(physicsActor)->state != 1) { break; }

			if (!s_playerDying && phaseOne_canSeePlayer(local(trooper)))
			{
				fixed16_16 dy = TFE_Jedi::abs(local(obj)->posWS.y - s_playerObject->posWS.y);
				fixed16_16 dist = dy + distApprox(s_playerObject->posWS.x, s_playerObject->posWS.z, local(obj)->posWS.x, local(obj)->posWS.z);
				if (dist <= FIXED(15) && local(obj)->yaw == local(target)->yaw)
				{
					local(physicsActor)->state = 3;
				}
			}
			else if (!s_playerDying)
			{
				local(physicsActor)->state = 5;
			}
			if (local(physicsActor)->state != 1) { break; }

			local(target)->flags &= 0xfffffff7;
			if (actor_handleSteps(&local(physicsActor)->actor, local(target)))
			{
				// Change direction if the Trooper hits a wall or impassable ledge or drop.
				actor_changeDirFromCollision(&local(physicsActor)->actor, local(target), &local(prevColTick));
			}
			else
			{
				if (!s_playerDying)
				{
					fixed16_16 dx = s_playerObject->posWS.x - local(obj)->posWS.x;
					fixed16_16 dz = s_playerObject->posWS.z - local(obj)->posWS.z;
					local(target)->yaw = vec2ToAngle(dx, dz);
				}
				else
				{
					local(target)->yaw = random(16338);
				}

				local(target)->speedRotation = 0x3000;
				local(target)->flags |= 4;

				fixed16_16 sinYaw, cosYaw;
				sinCosFixed(local(obj)->yaw, &sinYaw, &cosYaw);
				local(target)->pos.x = local(obj)->posWS.x + mul16(sinYaw, FIXED(30));
				local(target)->pos.z = local(obj)->posWS.z + mul16(cosYaw, FIXED(30));
				local(target)->flags |= 1;
			}
		}

		local(anim)->flags |= 2;
		task_end;
	}

	void phaseOne_handleState2(MessageType msg)
	{
		struct LocalContext
		{
			PhaseOne* trooper;
			SecObject* obj;
			PhysicsActor* physicsActor;
			ActorTarget* target;
			LogicAnimation* anim;
			JBool odd;
		};
		task_begin_ctx;

		local(trooper) = s_curTrooper;
		local(obj) = local(trooper)->logic.obj;
		local(physicsActor) = &local(trooper)->actor;
		local(target) = &local(physicsActor)->actor.target;
		local(anim)   = &local(physicsActor)->anim;
		local(odd)    = (s_curTick & 1) ? JFALSE : JTRUE;
		local(anim)->flags &= 0xfffffffe;
		actor_setupAnimation2(local(obj), 0, local(anim));

		task_localBlockBegin;
			fixed16_16 dx = local(obj)->posWS.x - s_playerObject->posWS.x;
			fixed16_16 dz = local(obj)->posWS.z - s_playerObject->posWS.z;
			angle14_32 angle = vec2ToAngle(dx, dz) & ANGLE_MASK;
			angle14_32 moveAngle = local(odd) ? angle - 2048 : angle + 2048;

			fixed16_16 sinYaw, cosYaw;
			sinCosFixed(moveAngle, &sinYaw, &cosYaw);

			local(target)->flags &= 0xfffffff7;
			local(target)->pos.x = s_playerObject->posWS.x + mul16(FIXED(15), sinYaw);
			local(target)->pos.z = s_playerObject->posWS.z + mul16(FIXED(15), cosYaw);
			local(target)->flags |= 1;

			angle14_32 targetAngle = local(odd) ? angle - 4323 : angle + 4323;
			local(target)->yaw = targetAngle;
			local(target)->speedRotation = 0;
			local(target)->flags |= 4;
		task_localBlockEnd;

		while (local(physicsActor)->state == 2)
		{
			do
			{
				task_yield(TASK_NO_DELAY);
				s_curTrooper = local(trooper);
				task_callTaskFunc(phaseOne_handleMsg);
			} while (msg != MSG_RUN_TASK);
			if (local(physicsActor)->state != 2) { break; }

			task_localBlockBegin;
			CollisionInfo* collisionInfo = &local(physicsActor)->actor.physics;
			if (collisionInfo->wall || collisionInfo->collidedObj)
			{
				local(physicsActor)->state = 1;
				break;
			}
			
			fixed16_16 dist = distApprox(s_playerObject->posWS.x, s_playerObject->posWS.z, local(obj)->posWS.x, local(obj)->posWS.z);
			if (s_playerDying || !phaseOne_canSeePlayer(local(trooper)) || dist > FIXED(25))
			{
				local(physicsActor)->state = 1;
				break;
			}
			task_localBlockEnd;

			if (actor_arrivedAtTarget(local(target), local(obj)))
			{
				if (random(100) <= 70)
				{
					local(physicsActor)->state = 3;
					fixed16_16 dx = s_playerObject->posWS.x - local(obj)->posWS.x;
					fixed16_16 dz = s_playerObject->posWS.z - local(obj)->posWS.z;
					local(target)->yaw = vec2ToAngle(dx, dz);
					break;
				}
				else
				{
					if (random(100) <= 10)
					{
						local(odd) = ~local(odd);
					}
					fixed16_16 dx = local(obj)->posWS.x - s_playerObject->posWS.x;
					fixed16_16 dz = local(obj)->posWS.z - s_playerObject->posWS.z;
					angle14_32 angle = vec2ToAngle(dx, dz) & ANGLE_MASK;
					angle14_32 angleMove = local(odd) ? (angle - 2048) : (angle + 2048);

					fixed16_16 sinYaw, cosYaw;
					sinCosFixed(angleMove, &sinYaw, &cosYaw);

					local(target)->flags &= 0xfffffff7;
					local(target)->pos.x = s_playerObject->posWS.x + mul16(FIXED(15), sinYaw);
					local(target)->pos.z = s_playerObject->posWS.z + mul16(FIXED(15), cosYaw);
					local(target)->flags |= 1;

					angle14_32 angleTarget = local(odd) ? (angle - 4096) : (angle + 4096);
					local(target)->yaw = angleTarget;
					local(target)->speedRotation = 0;
					local(target)->flags |= 4;
				}
			}
		}

		local(anim)->flags |= 2;
		task_end;
	}

	void phaseOne_handleAttack(MessageType msg)
	{
		struct LocalContext
		{
			PhaseOne* trooper;
			SecObject* obj;
			PhysicsActor* physicsActor;
			ActorTarget* target;
			LogicAnimation* anim;
		};
		task_begin_ctx;

		local(trooper) = s_curTrooper;
		local(obj) = local(trooper)->logic.obj;
		local(physicsActor) = &local(trooper)->actor;
		local(target) = &local(physicsActor)->actor.target;
		local(anim) = &local(physicsActor)->anim;

		local(target)->flags |= 8;
		while (local(physicsActor)->state == 3)
		{
			do
			{
				task_yield(TASK_NO_DELAY);
				s_curTrooper = local(trooper);
				task_callTaskFunc(phaseOne_handleMsg);
			} while (msg != MSG_RUN_TASK);
			if (local(physicsActor)->state != 3) { break; }

			local(anim)->flags |= 1;
			actor_setupAnimation2(local(obj), 1, local(anim));

			// Wait for the animation to finish.
			do
			{
				task_yield(TASK_NO_DELAY);
				s_curTrooper = local(trooper);
				task_callTaskFunc(phaseOne_handleMsg);
			} while (msg != MSG_RUN_TASK || !(local(anim)->flags&2));

			// Attempt to attack.
			playSound3D_oneshot(s_phase1SwordSndID, local(obj)->posWS);
			fixed16_16 dy = TFE_Jedi::abs(local(obj)->posWS.y - s_playerObject->posWS.y);
			fixed16_16 dist = dy + distApprox(s_playerObject->posWS.x, s_playerObject->posWS.z, local(obj)->posWS.x, local(obj)->posWS.z);
			if (dist <= FIXED(15))
			{
				player_applyDamage(FIXED(20), 0, JTRUE);
				local(physicsActor)->state = 2;
			}
			else
			{
				local(physicsActor)->state = 1;
			}
		}
				
		task_end;
	}

	void phaseOne_handleDyingState(MessageType msg)
	{
		struct LocalContext
		{
			PhaseOne* trooper;
			SecObject* obj;
			PhysicsActor* physicsActor;
			ActorTarget* target;
			LogicAnimation* anim;
		};
		task_begin_ctx;

		local(trooper) = s_curTrooper;
		local(obj) = local(trooper)->logic.obj;
		local(physicsActor) = &local(trooper)->actor;
		local(target) = &local(physicsActor)->actor.target;
		local(anim) = &local(physicsActor)->anim;

		local(target)->flags |= 8;
		playSound3D_oneshot(s_phase1cSndID, local(obj)->posWS);

		local(anim)->flags |= 1;
		actor_setupAnimation2(local(obj), 2, local(anim));

		// Wait for the animation to finish.
		do
		{
			task_yield(TASK_NO_DELAY);
		} while (msg != MSG_RUN_TASK || !(local(anim)->flags & 2));

		// Wait until the actor can die.
		do
		{
			task_yield(TASK_NO_DELAY);
		} while (msg != MSG_RUN_TASK || !actor_canDie(local(physicsActor)));

		task_localBlockBegin;
			RSector* sector = local(obj)->sector;
			if (sector->secHeight - 1 < 0)
			{
				SecObject* corpse = allocateObject();
				sprite_setData(corpse, local(obj)->wax);
				corpse->frame = 0;
				corpse->anim = 4;
				corpse->posWS = local(obj)->posWS;
				corpse->worldWidth = 0;
				corpse->worldHeight = 0;
				// TODO: Break this apart.
				corpse->entityFlags |= (0x42 << 8);
				sector_addObject(sector, corpse);
			}
			local(physicsActor)->alive = JFALSE;
			actor_handleBossDeath(local(physicsActor));
		task_localBlockEnd;

		task_end;
	}

	void phaseOne_handleState5(MessageType msg)
	{
		struct LocalContext
		{
			PhaseOne* trooper;
			SecObject* obj;
			PhysicsActor* physicsActor;
			ActorTarget* target;
			LogicAnimation* anim;
			JBool updateTargetPos;
			Tick prevColTick;
			Tick nextTick;
			Tick nextTick2;
			Tick delay;
		};
		task_begin_ctx;

		local(trooper) = s_curTrooper;
		local(obj) = local(trooper)->logic.obj;
		local(physicsActor) = &local(trooper)->actor;
		local(target) = &local(physicsActor)->actor.target;
		local(anim) = &local(physicsActor)->anim;

		local(updateTargetPos) = JTRUE;
		local(prevColTick) = 0;
		local(nextTick2) = 0;
		local(nextTick) = s_curTick + 4369;
		local(delay) = 72;

		local(anim)->flags &= 0xfffffffe;
		actor_setupAnimation2(local(obj), 0, local(anim));
		local(target)->flags &= 0xfffffff7;

		while (local(physicsActor)->state == 5)
		{
			do
			{
				task_yield(TASK_NO_DELAY);
				s_curTrooper = local(trooper);
				task_callTaskFunc(phaseOne_handleMsg);
			} while (msg != MSG_RUN_TASK);
			if (local(physicsActor)->state != 5 || s_playerDying) { break; }

			JBool canSee = actor_canSeeObject(local(obj), s_playerObject);
			if (canSee)
			{
				local(physicsActor)->lastPlayerPos.x = s_playerObject->posWS.x;
				local(physicsActor)->lastPlayerPos.z = s_playerObject->posWS.z;
				local(physicsActor)->state = 1;
				break;
			}

			JBool arrivedAtTarget = actor_arrivedAtTarget(local(target), local(obj));
			if (local(nextTick2) < s_curTick || arrivedAtTarget)
			{
				local(updateTargetPos) = JTRUE;
				if (arrivedAtTarget)
				{
					local(nextTick) = 0;
				}
			}

			fixed16_16 targetX;
			fixed16_16 targetZ;
			if (local(updateTargetPos))
			{
				local(updateTargetPos) = JFALSE;
				if (s_curTick > local(nextTick))
				{
					targetX = s_eyePos.x;
					targetZ = s_eyePos.z;
				}
				else
				{
					targetX = local(physicsActor)->lastPlayerPos.x;
					targetZ = local(physicsActor)->lastPlayerPos.z;
				}

				fixed16_16 dx = TFE_Jedi::abs(s_playerObject->posWS.x - local(obj)->posWS.x);
				fixed16_16 offset = dx >> 2;
				angle14_32 angle = vec2ToAngle(local(obj)->posWS.x - targetX, local(obj)->posWS.z - targetZ);

				local(target)->pos.x = targetX;
				local(target)->pos.z = targetZ;
				actor_offsetTarget(&local(target)->pos.x, &local(target)->pos.z, offset, offset >> 1, angle, 0xfff);
				local(target)->flags |= 1;

				local(target)->yaw    = vec2ToAngle(local(target)->pos.x - local(obj)->posWS.x, local(target)->pos.z - local(obj)->posWS.z);
				local(target)->pitch  = 0;
				local(target)->roll   = 0;
				local(target)->flags |= 4;

				local(nextTick2) = s_curTick + local(delay);
			}

			if (actor_handleSteps(&local(physicsActor)->actor, local(target)))
			{
				actor_changeDirFromCollision(&local(physicsActor)->actor, local(target), &local(prevColTick));
				local(delay) += 72;
				if (local(delay) > 1165)	// ~8 seconds
				{
					local(delay) = 72;
				}
				local(nextTick2) = s_curTick + local(delay);
			}
		}  // while (state == 5)

		local(anim)->flags |= 2;
		task_end;
	}

	void phaseOneTaskFunc(MessageType msg)
	{
		struct LocalContext
		{
			PhaseOne* trooper;
			SecObject* obj;
			PhysicsActor* physicsActor;
			ActorTarget* target;
			LogicAnimation* anim;
		};
		task_begin_ctx;

		local(trooper) = (PhaseOne*)task_getUserData();
		local(obj) = local(trooper)->logic.obj;
		local(physicsActor) = &local(trooper)->actor;
		local(target) = &local(physicsActor)->actor.target;
		local(anim) = &local(physicsActor)->anim;

		while (local(physicsActor)->alive)
		{
			msg = MSG_RUN_TASK;
			task_setMessage(msg);

			if (local(physicsActor)->state == 1)
			{
				s_curTrooper = local(trooper);
				task_callTaskFunc(phaseOne_handleCharge);
			}
			else if (local(physicsActor)->state == 2)
			{
				s_curTrooper = local(trooper);
				task_callTaskFunc(phaseOne_handleState2);
			}
			else if (local(physicsActor)->state == 3)
			{
				s_curTrooper = local(trooper);
				task_callTaskFunc(phaseOne_handleAttack);
			}
			else if (local(physicsActor)->state == 4)
			{
				s_curTrooper = local(trooper);
				task_callTaskFunc(phaseOne_handleDyingState);
			}
			else if (local(physicsActor)->state == 5)
			{
				s_curTrooper = local(trooper);
				task_callTaskFunc(phaseOne_handleState5);
			}
			else
			{
				while (local(physicsActor)->state == 0)
				{
					do
					{
						task_yield(145);
						s_curTrooper = local(trooper);
						task_callTaskFunc(phaseOne_handleMsg);

						if (msg == MSG_DAMAGE || msg == MSG_EXPLOSION)
						{
							local(physicsActor)->state = 1;
							task_makeActive(local(physicsActor)->actorTask);
							task_yield(TASK_NO_DELAY);
						}
					} while (msg != MSG_RUN_TASK);

					if (local(physicsActor)->state == 0 && phaseOne_canSeePlayer(local(trooper)))
					{
						playSound3D_oneshot(s_phase1aSndID, local(obj)->posWS);
						local(physicsActor)->state = 1;
					}
				}  // while (state == 0)
			}
		}  // while (alive)

		// Dead
		while (msg != MSG_RUN_TASK)
		{
			task_yield(TASK_NO_DELAY);
		}

		actor_removePhysicsActorFromWorld(local(physicsActor));
		deleteLogicAndObject(&local(trooper)->logic);
		level_free(local(trooper));
		
		task_end;
	}

	void phaseOneCleanupFunc(Logic* logic)
	{
		PhaseOne* trooper = (PhaseOne*)logic;
		PhysicsActor* physicsActor = &trooper->actor;

		actor_removePhysicsActorFromWorld(physicsActor);
		deleteLogicAndObject(&trooper->logic);
		level_free(trooper);
		task_free(physicsActor->actorTask);
	}

	Logic* phaseOne_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		if (!s_phase1aSndID)
		{
			s_phase1aSndID = sound_Load("phase1a.voc");
		}
		if (!s_phase1bSndID)
		{
			s_phase1bSndID = sound_Load("phase1b.voc");
		}
		if (!s_phase1SwordSndID)
		{
			s_phase1SwordSndID = sound_Load("sword-1.voc");
		}
		if (!s_phase1ReflectSndID)
		{
			s_phase1ReflectSndID = sound_Load("bigrefl1.voc");
		}
		if (!s_phase1cSndID)
		{
			s_phase1cSndID = sound_Load("phase1c.voc");
		}
		// setSoundEffectVolume(s_phase1cSndID, 100);

		PhaseOne* trooper = (PhaseOne*)level_alloc(sizeof(PhaseOne));
		memset(trooper, 0, sizeof(PhaseOne));

		PhysicsActor* physicsActor = &trooper->actor;

		// Give the name of the task a number so I can tell them apart when debugging.
		char name[32];
		sprintf(name, "PhaseOne%d", s_trooperNum);
		s_trooperNum++;
		Task* task = createSubTask(name, phaseOneTaskFunc);
		task_setUserData(task, trooper);

		obj->entityFlags = ETFLAG_AI_ACTOR;
		obj->worldWidth >>= 1;

		physicsActor->alive = JTRUE;
		physicsActor->hp = FIXED(180);
		physicsActor->state = 0;
		physicsActor->actorTask = task;
		trooper->hitSndId = 0;
		trooper->reflectSndId = 0;
		trooper->canDamage = JFALSE;
		trooper->logic.obj = obj;
		actor_addPhysicsActorToWorld(physicsActor);

		physicsActor->actor.header.obj = obj;
		physicsActor->actor.physics.obj = obj;
		actor_setupSmartObj(&physicsActor->actor);

		physicsActor->actor.collisionFlags |= 3;
		physicsActor->actor.collisionFlags &= 0xfffffffb;

		ActorTarget* target = &physicsActor->actor.target;
		target->flags &= 0xfffffff0;
		target->speed = FIXED(30);
		target->speedRotation = 0x3000;

		LogicAnimation* anim = &physicsActor->anim;
		anim->frameRate = 5;
		anim->frameCount = ONE_16;
		anim->prevTick = 0;
		anim->flags |= 2;
		anim->flags &= 0xfffffffe;

		actor_setupAnimation2(obj, 5, anim);
		obj_addLogic(obj, (Logic*)trooper, task, phaseOneCleanupFunc);

		if (setupFunc)
		{
			*setupFunc = nullptr;
		}
		return (Logic*)trooper;
	}
}  // namespace TFE_DarkForces