#include <cstring>

#include "phaseThree.h"
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
	enum PhaseThreeStates
	{
		P3STATE_DEFAULT = 0,
		P3STATE_CHARGE,
		P3STATE_WANDER,
		P3STATE_FIRE_MISSILES,
		P3STATE_FIRE_PLASMA,
		P3STATE_DYING,
		P3STATE_SEARCH,
		P3STATE_COUNT
	};

	static const angle14_16 c_phaseThreeMissileOffsets[] =
	{
		-2275, -1365, -455, 455, 1365, 2275
	};

	struct PhaseThree
	{
		Logic logic;
		PhysicsActor actor;

		SoundEffectID rocketSndId;
		SoundEffectID hitSndId;
		JBool noDeath;
	};

	static SoundSourceID s_phase3aSndID = NULL_SOUND;
	static SoundSourceID s_phase3bSndID = NULL_SOUND;
	static SoundSourceID s_phase3cSndID = NULL_SOUND;
	static SoundSourceID s_phase3RocketSndID = NULL_SOUND;

	static PhaseThree* s_curTrooper = nullptr;
	static s32 s_trooperNum = 0;

	JBool phaseThree_updatePlayerPos(PhaseThree* trooper)
	{
		if (actor_canSeeObject(trooper->logic.obj, s_playerObject))
		{
			trooper->actor.lastPlayerPos.x = s_playerObject->posWS.x;
			trooper->actor.lastPlayerPos.z = s_playerObject->posWS.z;
			return JTRUE;
		}
		return JFALSE;
	}

	void phaseThree_handleDamage(MessageType msg)
	{
		struct LocalContext
		{
			PhaseThree* trooper;
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
		local(trooper)->noDeath = JTRUE;

		if (local(physicsActor)->alive)
		{
			task_localBlockBegin;
			ProjectileLogic* proj = (ProjectileLogic*)s_msgEntity;
			local(physicsActor)->hp -= proj->dmg;
			task_localBlockEnd;

			if (local(physicsActor)->hp <= 0)
			{
				local(physicsActor)->state = P3STATE_DYING;
				msg = MSG_RUN_TASK;
				task_setMessage(msg);
			}
			else
			{
				stopSound(local(trooper)->hitSndId);
				local(trooper)->hitSndId = playSound3D_oneshot(s_phase3bSndID, local(obj)->posWS);

				if (random(100) <= 10)
				{
					local(target)->flags |= 8;

					// Save Animation
					memcpy(&local(tmp), local(anim), sizeof(LogicAnimation) - 4);

					local(anim)->flags |= 1;
					actor_setupAnimation2(local(obj), 10, local(anim));

					// Wait for animation to finish.
					do
					{
						task_yield(TASK_NO_DELAY);
					} while (msg != MSG_RUN_TASK || !(local(anim)->flags & 2));

					memcpy(local(anim), &local(tmp), sizeof(LogicAnimation) - 4);
					actor_setupAnimation2(local(obj), local(anim)->animId, local(anim));
					local(target)->flags &= 0xfffffff7;
				}
				msg = MSG_DAMAGE;
				task_setMessage(msg);
			}
		}  // if (alive)
		local(trooper)->noDeath = JFALSE;
		task_end;
	}

	void phaseThree_handleExplosion(MessageType msg)
	{
		struct LocalContext
		{
			PhaseThree* trooper;
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
		local(trooper)->noDeath = JTRUE;

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
				local(physicsActor)->state = P3STATE_DYING;
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
				local(trooper)->hitSndId = playSound3D_oneshot(s_phase3bSndID, local(obj)->posWS);

				if (random(100) <= 10)
				{
					local(target)->flags |= 8;

					// Save Animation
					memcpy(&local(tmp), local(anim), sizeof(LogicAnimation) - 4);

					local(anim)->flags |= 1;
					actor_setupAnimation2(local(obj), 10, local(anim));

					// Wait for animation to finish.
					do
					{
						task_yield(TASK_NO_DELAY);
					} while (msg != MSG_RUN_TASK || !(local(anim)->flags & 2));

					memcpy(local(anim), &local(tmp), sizeof(LogicAnimation) - 4);
					actor_setupAnimation2(local(obj), local(anim)->animId, local(anim));
					local(target)->flags &= 0xfffffff7;
				}
				msg = MSG_EXPLOSION;
				task_setMessage(msg);
			}
		}  // if (alive)
		local(trooper)->noDeath = JFALSE;
		task_end;
	}

	void phaseThree_handleMsg(MessageType msg)
	{
		struct LocalContext
		{
			PhaseThree* trooper;
		};
		task_begin_ctx;
		local(trooper) = s_curTrooper;
		
		if (msg == MSG_DAMAGE)
		{
			if (!local(trooper)->noDeath)
			{
				s_curTrooper = local(trooper);
				task_callTaskFunc(phaseThree_handleDamage);
			}
			else
			{
				ProjectileLogic* proj = (ProjectileLogic*)s_msgEntity;
				local(trooper)->actor.hp -= proj->dmg;
			}
		}
		else if (msg == MSG_EXPLOSION)
		{
			if (!local(trooper)->noDeath)
			{
				s_curTrooper = local(trooper);
				task_callTaskFunc(phaseThree_handleExplosion);
			}
			else
			{
				local(trooper)->actor.hp -= s_msgArg1;
			}
		}
		task_end;
	}

	void phaseThree_chargePlayer(MessageType msg)
	{
		struct LocalContext
		{
			PhaseThree* trooper;
			SecObject* obj;
			PhysicsActor* physicsActor;
			ActorTarget* target;
			LogicAnimation* anim;
			u32 prevColTick;
			JBool flying;
		};
		task_begin_ctx;

		local(trooper) = s_curTrooper;
		local(obj) = local(trooper)->logic.obj;
		local(physicsActor) = &local(trooper)->actor;
		local(target) = &local(physicsActor)->actor.target;
		local(anim) = &local(physicsActor)->anim;
		local(prevColTick) = 0;
		local(target)->flags &= 0xfffffff7;

		if (random(100) <= 40)
		{
			local(trooper)->rocketSndId = playSound3D_oneshot(s_phase3RocketSndID, local(obj)->posWS);
			local(anim)->flags |= 1;
			actor_setupAnimation2(local(obj), 13, local(anim));
			actor_setAnimFrameRange(local(anim), 0, 1);

			// Wait for the animation to finish.
			do
			{
				task_yield(0);
				s_curTrooper = local(trooper);
				task_callTaskFunc(phaseThree_handleMsg);
			} while (!(local(anim)->flags & 2) || msg != MSG_RUN_TASK);

			actor_setupAnimation2(local(obj), 13, local(anim));
			actor_setAnimFrameRange(local(anim), 2, 2);

			local(target)->speed = FIXED(70);
			local(flying) = JTRUE;
			local(physicsActor)->actor.collisionFlags &= 0xfffffffc;
			local(physicsActor)->actor.physics.yPos = FIXED(9999);
		}
		else
		{
			local(physicsActor)->actor.collisionFlags |= 3;
			local(target)->speed = FIXED(25);
			local(flying) = JFALSE;
			local(anim)->flags &= 0xfffffffe;
			actor_setupAnimation2(local(obj), 0, local(anim));
		}

		while (local(physicsActor)->state == P3STATE_CHARGE)
		{
			do
			{
				task_yield(TASK_NO_DELAY);
				s_curTrooper = local(trooper);
				task_callTaskFunc(phaseThree_handleMsg);
			} while (msg != MSG_RUN_TASK);
			
			if (random(100) <= 10)
			{
				if (!s_playerDying && phaseThree_updatePlayerPos(local(trooper)))
				{
					if (random(player_getSquaredDistance(local(obj))) <= FIXED(100) && local(obj)->yaw == local(target)->yaw)	// essentially <= (dist = 10)
					{
						local(physicsActor)->state = (random(100) > 40) ? P3STATE_FIRE_PLASMA : P3STATE_FIRE_MISSILES;
						if (local(flying))
						{
							local(anim)->flags |= 1;
							actor_setupAnimation2(local(obj), 13, local(anim));
							actor_setAnimFrameRange(local(anim), 3, 3);
							do
							{
								task_yield(0);
								s_curTrooper = local(trooper);
								task_callTaskFunc(phaseThree_handleMsg);
							} while (msg != MSG_RUN_TASK || !(local(anim)->flags & 2));

							local(physicsActor)->actor.collisionFlags |= 3;
							local(physicsActor)->actor.physics.yPos = FIXED(9999);
							local(target)->speed = FIXED(25);
							local(target)->flags &= 0xfffffffd;
							stopSound(local(trooper)->rocketSndId);
						}
					}
				}
				else if (!s_playerDying)
				{
					stopSound(local(trooper)->rocketSndId);
					local(physicsActor)->state = P3STATE_SEARCH;
				}
			}
			if (local(physicsActor)->state != P3STATE_CHARGE) { break; }

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
				local(target)->pos.x = local(obj)->posWS.x + mul16(sinYaw, FIXED(25));
				local(target)->pos.z = local(obj)->posWS.z + mul16(cosYaw, FIXED(25));
				local(target)->flags |= 1;

				if (local(flying))
				{
					local(target)->pos.y = s_playerObject->posWS.y - FIXED(2);
					local(target)->flags |= 2;
				}
			}
		}

		local(anim)->flags |= 2;
		task_end;
	}

	void phaseThree_wander(MessageType msg)
	{
		struct LocalContext
		{
			PhaseThree* trooper;
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
			local(target)->pos.x = s_playerObject->posWS.x + mul16(FIXED(100), sinYaw);
			local(target)->pos.z = s_playerObject->posWS.z + mul16(FIXED(100), cosYaw);
			local(target)->flags |= 1;

			angle14_32 targetAngle = local(odd) ? angle - 4323 : angle + 4323;
			local(target)->yaw = targetAngle;
			local(target)->speedRotation = 0;
			local(target)->flags |= 4;
		task_localBlockEnd;

		while (local(physicsActor)->state == P3STATE_WANDER)
		{
			do
			{
				task_yield(TASK_NO_DELAY);
				s_curTrooper = local(trooper);
				task_callTaskFunc(phaseThree_handleMsg);
			} while (msg != MSG_RUN_TASK);
			if (local(physicsActor)->state != P3STATE_WANDER) { break; }

			task_localBlockBegin;
			CollisionInfo* collisionInfo = &local(physicsActor)->actor.physics;
			if (collisionInfo->wall || collisionInfo->collidedObj)
			{
				local(physicsActor)->state = P3STATE_CHARGE;
				break;
			}
			
			fixed16_16 dist = distApprox(s_playerObject->posWS.x, s_playerObject->posWS.z, local(obj)->posWS.x, local(obj)->posWS.z);
			if (s_playerDying || !phaseThree_updatePlayerPos(local(trooper)) || dist > FIXED(20))
			{
				local(physicsActor)->state = P3STATE_CHARGE;
				break;
			}
			task_localBlockEnd;

			if (actor_arrivedAtTarget(local(target), local(obj)))
			{
				if (random(100) <= 10)
				{
					local(physicsActor)->state = (random(100) > 40) ? P3STATE_FIRE_PLASMA : P3STATE_FIRE_MISSILES;
					fixed16_16 dx = s_playerObject->posWS.x - local(obj)->posWS.x;
					fixed16_16 dz = s_playerObject->posWS.z - local(obj)->posWS.z;
					local(target)->yaw = vec2ToAngle(dx, dz);
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
					local(target)->pos.x = s_playerObject->posWS.x + mul16(FIXED(100), sinYaw);
					local(target)->pos.z = s_playerObject->posWS.z + mul16(FIXED(100), cosYaw);
					local(target)->flags |= 1;

					angle14_32 angleTarget = local(odd) ? (angle - 4096) : (angle + 4096);
					local(target)->yaw = angleTarget;
					// local(target)->speedRotation = 0;
					local(target)->flags |= 4;
				}
			}
		}

		local(anim)->flags |= 2;
		task_end;
	}
		
	void phaseThree_fireMissiles(MessageType msg)
	{
		struct LocalContext
		{
			PhaseThree* trooper;
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

		local(target)->flags &= 0xfffffffe;
		local(physicsActor)->actor.collisionFlags |= 3;
		while (local(physicsActor)->state == P3STATE_FIRE_MISSILES)
		{
			do
			{
				task_yield(TASK_NO_DELAY);
				s_curTrooper = local(trooper);
				task_callTaskFunc(phaseThree_handleMsg);
			} while (msg != MSG_RUN_TASK);
			if (local(physicsActor)->state != P3STATE_FIRE_MISSILES) { break; }

			local(anim)->flags |= 1;
			actor_setupAnimation2(local(obj), 11, local(anim));

			// Wait for the animation to finish.
			do
			{
				task_yield(TASK_NO_DELAY);
				s_curTrooper = local(trooper);
				task_callTaskFunc(phaseThree_handleMsg);
			} while (msg != MSG_RUN_TASK || !(local(anim)->flags&2));

			// Attempt to attack.
			task_localBlockBegin;
				local(obj)->flags |= OBJ_FLAG_FULLBRIGHT;
				playSound3D_oneshot(s_missile1SndSrc, local(obj)->posWS);

				// Fire 6 missiles with different yaw offsets.
				for (s32 i = 0; i < 6; i++)
				{
					ProjectileLogic* proj = (ProjectileLogic*)createProjectile(PROJ_HOMING_MISSILE, local(obj)->sector, local(obj)->posWS.x, local(obj)->posWS.y - FIXED(11), local(obj)->posWS.z, local(obj));
					proj->prevColObj = local(obj);
					proj->excludeObj = local(obj);

					SecObject* projObj = proj->logic.obj;
					projObj->yaw = local(target)->yaw + c_phaseThreeMissileOffsets[i];
					proj_setTransform(proj, 0, projObj->yaw);
				}

				local(anim)->flags |= 1;
				actor_setupAnimation2(local(obj), 12, local(anim));
			task_localBlockEnd;

			// Wait for the animation to finish.
			do
			{
				task_yield(TASK_NO_DELAY);
				s_curTrooper = local(trooper);
				task_callTaskFunc(phaseThree_handleMsg);
			} while (msg != MSG_RUN_TASK || !(local(anim)->flags & 2));

			local(obj)->flags &= ~OBJ_FLAG_FULLBRIGHT;
			local(physicsActor)->state = P3STATE_WANDER;
		}
				
		task_end;
	}

	void phaseThree_firePlasma(MessageType msg)
	{
		struct LocalContext
		{
			PhaseThree* trooper;
			SecObject* obj;
			PhysicsActor* physicsActor;
			ActorTarget* target;
			LogicAnimation* anim;
			s32 shotCount;
		};
		task_begin_ctx;

		local(trooper) = s_curTrooper;
		local(obj) = local(trooper)->logic.obj;
		local(physicsActor) = &local(trooper)->actor;
		local(target) = &local(physicsActor)->actor.target;
		local(anim) = &local(physicsActor)->anim;

		local(target)->flags &= 0xfffffffe;
		local(physicsActor)->actor.collisionFlags |= 3;

		local(anim)->flags |= 1;
		actor_setupAnimation2(local(obj), 1, local(anim));

		// Wait for the animation to finish.
		do
		{
			task_yield(TASK_NO_DELAY);
			s_curTrooper = local(trooper);
			task_callTaskFunc(phaseThree_handleMsg);
		} while (msg != MSG_RUN_TASK || !(local(anim)->flags & 2));

		local(shotCount) = random(20) + 1;
		local(obj)->flags |= OBJ_FLAG_FULLBRIGHT;
		local(target)->flags |= 8;

		while (local(physicsActor)->state == P3STATE_FIRE_PLASMA)
		{
			do
			{
				task_yield(29);
				s_curTrooper = local(trooper);
				task_callTaskFunc(phaseThree_handleMsg);
			} while (msg != MSG_RUN_TASK);
			if (local(physicsActor)->state != P3STATE_FIRE_PLASMA) { break; }

			playSound3D_oneshot(s_plasma4SndSrc, local(obj)->posWS);

			ProjectileLogic* proj = (ProjectileLogic*)createProjectile(PROJ_CANNON, local(obj)->sector, local(obj)->posWS.x, local(obj)->posWS.y - FIXED(9), local(obj)->posWS.z, local(obj));
			proj->prevColObj = local(obj);
			proj->excludeObj = local(obj);

			SecObject* projObj = proj->logic.obj;
			projObj->yaw = local(target)->yaw;
			actor_leadTarget(proj);
			local(shotCount)--;
			if (local(shotCount) == 0)
			{
				local(physicsActor)->state = P3STATE_WANDER;
			}
		}

		local(anim)->flags |= 1;
		actor_setupAnimation2(local(obj), 6, local(anim));

		// Wait for the animation to finish.
		do
		{
			task_yield(TASK_NO_DELAY);
			s_curTrooper = local(trooper);
			task_callTaskFunc(phaseThree_handleMsg);
		} while (msg != MSG_RUN_TASK || !(local(anim)->flags & 2));

		local(obj)->flags &= ~OBJ_FLAG_FULLBRIGHT;
		task_end;
	}

	void phaseThree_handleDying(MessageType msg)
	{
		struct LocalContext
		{
			PhaseThree* trooper;
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
		stopSound(local(trooper)->rocketSndId);
		playSound3D_oneshot(s_phase3cSndID, local(obj)->posWS);

		local(anim)->flags |= 1;
		actor_setupAnimation2(local(obj), 2, local(anim));
		local(physicsActor)->actor.collisionFlags |= 3;

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
				corpse->entityFlags |= (ETFLAG_CORPSE | ETFLAG_16384);
				sector_addObject(sector, corpse);
			}
			local(physicsActor)->alive = JFALSE;
			actor_handleBossDeath(local(physicsActor));
		task_localBlockEnd;

		task_end;
	}

	void phaseThree_lookForPlayer(MessageType msg)
	{
		struct LocalContext
		{
			PhaseThree* trooper;
			SecObject* obj;
			PhysicsActor* physicsActor;
			ActorTarget* target;
			LogicAnimation* anim;
			JBool updateTargetPos;
			JBool forceContinue;
			Tick prevColTick;
			Tick nextTick;
			Tick retargetTick;
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
		local(retargetTick) = 0;
		local(nextTick) = s_curTick + 4369;
		local(delay) = 72;

		local(anim)->flags &= 0xfffffffe;
		local(forceContinue) = JFALSE;
		actor_setupAnimation2(local(obj), 13, local(anim));

		local(target)->flags &= 0xfffffff7;
		local(target)->speed = FIXED(30);
		local(physicsActor)->actor.collisionFlags &= 0xfffffffc;

		while (local(physicsActor)->state == P3STATE_SEARCH || local(forceContinue))
		{
			do
			{
				task_yield(TASK_NO_DELAY);
				s_curTrooper = local(trooper);
				task_callTaskFunc(phaseThree_handleMsg);
			} while (msg != MSG_RUN_TASK);
			if (s_playerDying) { continue; }

			JBool canSee = actor_canSeeObject(local(obj), s_playerObject);
			if (canSee)
			{
				local(physicsActor)->lastPlayerPos.x = s_playerObject->posWS.x;
				local(physicsActor)->lastPlayerPos.z = s_playerObject->posWS.z;
				local(physicsActor)->state = P3STATE_CHARGE;
			}

			if (local(retargetTick) < s_curTick || actor_arrivedAtTarget(local(target), local(obj)))
			{
				local(updateTargetPos) = JTRUE;
				if (actor_arrivedAtTarget(local(target), local(obj)))
				{
					local(nextTick) = 0;
				}
			}

			fixed16_16 targetX;
			fixed16_16 targetZ;
			if (local(updateTargetPos))
			{
				local(updateTargetPos) = JFALSE;
				if (local(nextTick) > s_curTick)
				{
					targetX = local(physicsActor)->lastPlayerPos.x;
					targetZ = local(physicsActor)->lastPlayerPos.z;
				}
				else
				{
					targetX = s_eyePos.x;
					targetZ = s_eyePos.z;
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

				local(retargetTick) = s_curTick + local(delay);
			}

			if (actor_handleSteps(&local(physicsActor)->actor, local(target)))
			{
				actor_changeDirFromCollision(&local(physicsActor)->actor, local(target), &local(prevColTick));
				local(delay) += 36;
				local(forceContinue) = JTRUE;
				if (local(delay) > 582)
				{
					local(delay) = 72;
				}
				local(retargetTick) = s_curTick + local(delay);
			}
			else
			{
				local(forceContinue) = JFALSE;
			}
		}  // while (state == 6 || forceContinue)

		local(anim)->flags |= 2;
		task_end;
	}

	void phaseThreeTaskFunc(MessageType msg)
	{
		struct LocalContext
		{
			PhaseThree* trooper;
			SecObject* obj;
			PhysicsActor* physicsActor;
			ActorTarget* target;
			LogicAnimation* anim;
		};
		task_begin_ctx;

		local(trooper) = (PhaseThree*)task_getUserData();
		local(obj) = local(trooper)->logic.obj;
		local(physicsActor) = &local(trooper)->actor;
		local(target) = &local(physicsActor)->actor.target;
		local(anim) = &local(physicsActor)->anim;

		while (local(physicsActor)->alive)
		{
			msg = MSG_RUN_TASK;
			task_setMessage(msg);

			if (local(physicsActor)->state == P3STATE_CHARGE)
			{
				s_curTrooper = local(trooper);
				task_callTaskFunc(phaseThree_chargePlayer);
			}
			else if (local(physicsActor)->state == P3STATE_WANDER)
			{
				s_curTrooper = local(trooper);
				task_callTaskFunc(phaseThree_wander);
			}
			else if (local(physicsActor)->state == P3STATE_FIRE_MISSILES)
			{
				s_curTrooper = local(trooper);
				task_callTaskFunc(phaseThree_fireMissiles);
			}
			else if (local(physicsActor)->state == P3STATE_FIRE_PLASMA)
			{
				s_curTrooper = local(trooper);
				task_callTaskFunc(phaseThree_firePlasma);
			}
			else if (local(physicsActor)->state == P3STATE_DYING)
			{
				s_curTrooper = local(trooper);
				task_callTaskFunc(phaseThree_handleDying);
			}
			else if (local(physicsActor)->state == P3STATE_SEARCH)
			{
				s_curTrooper = local(trooper);
				task_callTaskFunc(phaseThree_lookForPlayer);
			}
			else
			{
				while (local(physicsActor)->state == P3STATE_DEFAULT)
				{
					do
					{
						task_yield(145);
						s_curTrooper = local(trooper);
						task_callTaskFunc(phaseThree_handleMsg);

						if (msg == MSG_DAMAGE || msg == MSG_EXPLOSION)
						{
							local(physicsActor)->state = P3STATE_CHARGE;
							task_makeActive(local(physicsActor)->actorTask);
							task_yield(TASK_NO_DELAY);
						}
					} while (msg != MSG_RUN_TASK);

					if (local(physicsActor)->state == 0 && phaseThree_updatePlayerPos(local(trooper)))
					{
						playSound3D_oneshot(s_phase3aSndID, local(obj)->posWS);
						local(physicsActor)->state = P3STATE_CHARGE;
					}
				}  // while (state == P3STATE_DEFAULT)
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

	void phaseThreeCleanupFunc(Logic* logic)
	{
		PhaseThree* trooper = (PhaseThree*)logic;
		PhysicsActor* physicsActor = &trooper->actor;

		actor_removePhysicsActorFromWorld(physicsActor);
		deleteLogicAndObject(&trooper->logic);
		level_free(trooper);
		task_free(physicsActor->actorTask);
	}

	Logic* phaseThree_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		if (!s_phase3aSndID)
		{
			s_phase3aSndID = sound_Load("phase3a.voc");
			assert(s_phase3aSndID);
		}
		if (!s_phase3bSndID)
		{
			s_phase3bSndID = sound_Load("phase3b.voc");
			assert(s_phase3bSndID);
		}
		if (!s_phase3RocketSndID)
		{
			s_phase3RocketSndID = sound_Load("rocket-1.voc");
			assert(s_phase3RocketSndID);
		}
		if (!s_phase3cSndID)
		{
			s_phase3cSndID = sound_Load("phase3c.voc");
			assert(s_phase3cSndID);
		}
		// setSoundEffectVolume(s_phase3cSndID, 120);

		PhaseThree* trooper = (PhaseThree*)level_alloc(sizeof(PhaseThree));
		memset(trooper, 0, sizeof(PhaseThree));

		PhysicsActor* physicsActor = &trooper->actor;

		// Give the name of the task a number so I can tell them apart when debugging.
		char name[32];
		sprintf(name, "PhaseThree%d", s_trooperNum);
		s_trooperNum++;
		Task* task = createSubTask(name, phaseThreeTaskFunc);
		task_setUserData(task, trooper);

		obj->entityFlags = ETFLAG_AI_ACTOR | ETFLAG_GENERAL_MOHC;
		obj->worldWidth >>= 1;

		physicsActor->alive = JTRUE;
		physicsActor->hp = FIXED(750);
		physicsActor->state = 0;
		physicsActor->actorTask = task;
		trooper->hitSndId = 0;
		trooper->rocketSndId = 0;
		trooper->noDeath = JFALSE;
		trooper->logic.obj = obj;
		actor_addPhysicsActorToWorld(physicsActor);

		physicsActor->actor.header.obj = obj;
		physicsActor->actor.physics.obj = obj;
		actor_setupSmartObj(&physicsActor->actor);

		physicsActor->actor.collisionFlags |= 7;
		physicsActor->actor.physics.yPos = FIXED(9999);

		ActorTarget* target = &physicsActor->actor.target;
		target->flags &= 0xfffffff0;
		target->speed = FIXED(25);
		target->speedVert = FIXED(10);
		target->speedRotation = 0x3000;

		LogicAnimation* anim = &physicsActor->anim;
		anim->frameRate = 5;
		anim->frameCount = ONE_16;
		anim->prevTick = 0;
		anim->flags |= 2;
		anim->flags &= 0xfffffffe;

		actor_setupAnimation2(obj, 3, anim);
		obj_addLogic(obj, (Logic*)trooper, task, phaseThreeCleanupFunc);

		if (setupFunc)
		{
			*setupFunc = nullptr;
		}
		return (Logic*)trooper;
	}
}  // namespace TFE_DarkForces