#include <cstring>

#include "bobaFett.h"
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
#include <TFE_Jedi/Level/rwall.h>
#include <TFE_Jedi/Sound/soundSystem.h>
#include <TFE_Jedi/Memory/list.h>
#include <TFE_Jedi/Memory/allocator.h>
#include <TFE_Settings/settings.h>

namespace TFE_DarkForces
{
	enum BobaFettStates
	{
		BOBASTATE_DEFAULT         = 0,
		BOBASTATE_MOVE_AND_ATTACK = 1,
		BOBASTATE_DYING           = 2,
		BOBASTATE_SEARCH          = 3,
		BOBASTATE_COUNT
	};

	struct BobaFettMoveState
	{
		vec3_fixed target;
		fixed16_16 yAccel;
		SoundSourceID soundSrc;
		angle14_32 vertAngleRange;
		fixed16_16 vertAngle;
		fixed16_16 yVelOffset;
		fixed16_16 accel;
	};

	struct BobaFett
	{
		Logic logic;
		PhysicsActor actor;

		s32 unused;
		BobaFettMoveState moveState;
		SoundEffectID hitSndId;
	};

	static SoundSourceID s_boba1SndID = NULL_SOUND;
	static SoundSourceID s_boba3SndID = NULL_SOUND;
	static SoundSourceID s_boba2SndID = NULL_SOUND;
	static SoundSourceID s_boba4SndID = NULL_SOUND;
	static SoundSourceID s_bobaRocket2SndID = NULL_SOUND;

	static BobaFett* s_curBobaFett = nullptr;
	static s32 s_bobaFettNum = 0;
	static s32 s_bobaFett_pitchScale = 0;

	void bobaFett_handleDamage(MessageType msg)
	{
		struct LocalContext
		{
			BobaFett* bobaFett;
			SecObject* obj;
			PhysicsActor* physicsActor;
			LogicAnimation* anim;
			LogicAnimation tmp;
		};
		task_begin_ctx;

		local(bobaFett) = s_curBobaFett;
		local(obj) = local(bobaFett)->logic.obj;
		local(physicsActor) = &local(bobaFett)->actor;
		local(anim) = &local(physicsActor)->anim;

		if (local(physicsActor)->alive)
		{
			task_localBlockBegin;
			ProjectileLogic* proj = (ProjectileLogic*)s_msgEntity;
			local(physicsActor)->hp -= proj->dmg;
			task_localBlockEnd;

			if (local(physicsActor)->hp <= 0)
			{
				local(physicsActor)->state = BOBASTATE_DYING;
				msg = MSG_RUN_TASK;
				task_setMessage(msg);
			}
			else
			{
				stopSound(local(bobaFett)->hitSndId);
				local(bobaFett)->hitSndId = playSound3D_oneshot(s_boba3SndID, local(obj)->posWS);

				// Save Animation
				memcpy(&local(tmp), local(anim), sizeof(LogicAnimation) - 4);

				local(anim)->flags |= 1;
				local(anim)->frameRate = 6;
				actor_setupAnimation2(local(obj), 12, local(anim));

				// Wait for animation to finish.
				do
				{
					task_yield(TASK_NO_DELAY);
				} while (msg != MSG_RUN_TASK || !(local(anim)->flags & 2));

				memcpy(local(anim), &local(tmp), sizeof(LogicAnimation) - 4);
				actor_setupAnimation2(local(obj), local(anim)->animId, local(anim));

				msg = MSG_DAMAGE;
				task_setMessage(msg);
			}
		}  // if (alive)
		task_end;
	}

	void bobaFett_handleExplosion(MessageType msg)
	{
		struct LocalContext
		{
			BobaFett* bobaFett;
			SecObject* obj;
			PhysicsActor* physicsActor;
			LogicAnimation* anim;
			LogicAnimation tmp;
		};
		task_begin_ctx;

		local(bobaFett) = s_curBobaFett;
		local(obj) = local(bobaFett)->logic.obj;
		local(physicsActor) = &local(bobaFett)->actor;
		local(anim) = &local(physicsActor)->anim;

		if (local(physicsActor)->alive)
		{
			task_localBlockBegin;
				fixed16_16 dmg = s_msgArg1;
				local(physicsActor)->hp -= dmg;
			task_localBlockEnd;

			if (local(physicsActor)->hp <= 0)
			{
				local(physicsActor)->state = BOBASTATE_DYING;
				msg = MSG_RUN_TASK;
				task_setMessage(msg);
			}
			else
			{
				stopSound(local(bobaFett)->hitSndId);
				local(bobaFett)->hitSndId = playSound3D_oneshot(s_boba3SndID, local(obj)->posWS);

				// Save Animation
				memcpy(&local(tmp), local(anim), sizeof(LogicAnimation) - 4);

				local(anim)->flags |= 1;
				local(anim)->frameRate = 6;
				actor_setupAnimation2(local(obj), 12, local(anim));

				// Wait for animation to finish.
				do
				{
					task_yield(TASK_NO_DELAY);
				} while (msg != MSG_RUN_TASK || !(local(anim)->flags & 2));

				memcpy(local(anim), &local(tmp), sizeof(LogicAnimation) - 4);
				actor_setupAnimation2(local(obj), local(anim)->animId, local(anim));

				msg = MSG_EXPLOSION;
				task_setMessage(msg);
			}
		}  // if (alive)
		task_end;
	}

	void bobaFett_handleMsg(MessageType msg)
	{
		struct LocalContext
		{
			BobaFett* bobaFett;
		};
		task_begin_ctx;
		local(bobaFett) = s_curBobaFett;
		
		if (msg == MSG_DAMAGE)
		{
			s_curBobaFett = local(bobaFett);
			task_callTaskFunc(bobaFett_handleDamage);
		}
		else if (msg == MSG_EXPLOSION)
		{
			s_curBobaFett = local(bobaFett);
			task_callTaskFunc(bobaFett_handleExplosion);
		}
		task_end;
	}

	fixed16_16 bobaFett_changeYVel(fixed16_16 yTarget, fixed16_16 yPos, fixed16_16 yVel, fixed16_16 yVelOffset, angle14_32* vertAngleRange)
	{
		fixed16_16 target = -yTarget;
		fixed16_16 pos = -yPos;
		fixed16_16 vel = -yVel;
		if (pos != target)
		{
			vel += mul16(s_gravityAccel, s_deltaTime);
			fixed16_16 dy = target - pos;
			if (vel != dy)
			{
				vel = max(0, dy - vel);
				*vertAngleRange = clamp(ONE_16 - (vel - 0x13333), 0, ONE_16);
				vel = min(ONE_16, vel);

				fixed16_16 maxChange = mul16(TFE_Jedi::abs(dy) + FIXED(4), s_deltaTime);
				return clamp(vel - yVelOffset, -maxChange, maxChange) + yVelOffset;
			}
		}
		return yTarget;
	}

	void bobaFett_setRocketPitch(u32 sndId, u32 pitch)
	{
		if (sndId)
		{
			// TODO
			// sound_changePitch(sndId, 0x600, pitch & 0x7f);
		}
	}

	void bobaFett_handleVerticalMove(PhysicsActor* physicsActor, fixed16_16 yVelOffset, angle14_32 vertAngle, fixed16_16 yAccel, SoundSourceID soundSrc)
	{
		fixed16_16 yVelDelta, hVelDelta;
		SecObject* obj = physicsActor->actor.header.obj;
		if (yVelOffset)
		{
			physicsActor->moveSndId = playSound3D_looping(soundSrc, physicsActor->moveSndId, obj->posWS);

			if (s_bobaFett_pitchScale)
			{
				intToFixed16(s_bobaFett_pitchScale);
				u32 sndPitch = (u32)floor16(mul16(mul16(yVelOffset, yVelOffset), intToFixed16(s_bobaFett_pitchScale)));
				bobaFett_setRocketPitch(physicsActor->moveSndId, sndPitch);
			}
			fixed16_16 sinPitch, cosPitch;
			yVelDelta = mul16(mul16(yVelOffset, yAccel), s_deltaTime);
			sinCosFixed(vertAngle, &sinPitch, &cosPitch);

			fixed16_16 prevYVelDelta = yVelDelta;
			yVelDelta =  mul16(cosPitch, yVelDelta);
			hVelDelta = -mul16(sinPitch, prevYVelDelta);

			fixed16_16 sinYaw, cosYaw;
			sinCosFixed(obj->yaw, &sinYaw, &cosYaw);

			physicsActor->vel.x += mul16(hVelDelta, sinYaw);
			physicsActor->vel.y -= yVelDelta;
			physicsActor->vel.z += mul16(hVelDelta, cosYaw);
		}
		else if (physicsActor->moveSndId)
		{
			stopSound(physicsActor->moveSndId);
			physicsActor->moveSndId = 0;
		}
	}

	void bobaFett_handleFlying(PhysicsActor* physicsActor, BobaFettMoveState* moveState)
	{
		SecObject* obj = physicsActor->actor.header.obj;

		fixed16_16 dx = moveState->target.z - obj->posWS.z;
		fixed16_16 dz = moveState->target.x - obj->posWS.x;
		angle14_32 targetAngle = vec2ToAngle(dx, dz);
		angle14_32 velAngle = vec2ToAngle(physicsActor->vel.x, physicsActor->vel.z);

		angle14_32 adjustedTargetAngle = targetAngle - velAngle;
		if (adjustedTargetAngle != 0)
		{
			angle14_32 maxFrameRotation = floor16(mul16(FIXED(4551), s_deltaTime));
			angle14_32 angleDelta = clamp(getAngleDifference(obj->yaw, targetAngle), -maxFrameRotation, maxFrameRotation);
			obj->yaw += angleDelta;
			obj->yaw &= ANGLE_MASK;
		}

		// Adjust the vertical movement angle.
		fixed16_16 dist = distApprox(obj->posWS.x, obj->posWS.z, moveState->target.x, moveState->target.z);
		fixed16_16 scaledDist = min(ONE_16, dist >> 7);
		fixed16_16 newVertAngle = mul16(scaledDist, FIXED(100));

		fixed16_16 cosVelAngle, sinVelAngle;
		sinCosFixed(velAngle, &cosVelAngle, &sinVelAngle);

		newVertAngle -= (mul16(physicsActor->vel.z, cosVelAngle) + mul16(physicsActor->vel.x, sinVelAngle));
		newVertAngle = clamp(div16(newVertAngle, FIXED(100)), moveState->vertAngleRange, -moveState->vertAngleRange);

		fixed16_16 targetVertAngle   = floor16(mul16(newVertAngle, FIXED(2048)));
		fixed16_16 maxVertAngleDelta = floor16(mul16(FIXED(1365), s_deltaTime));
		fixed16_16 vertAngleDelta    = clamp(targetVertAngle - moveState->vertAngle, -maxVertAngleDelta, maxVertAngleDelta);
		moveState->vertAngle        += vertAngleDelta;

		// Change the Y velocity offset and vertical ange range.
		moveState->yVelOffset = bobaFett_changeYVel(moveState->target.y, obj->posWS.y, physicsActor->vel.y, moveState->yVelOffset, &moveState->vertAngleRange);
		if (moveState->accel)
		{
			fixed16_16 sinYaw, cosYaw;
			sinCosFixed(obj->yaw + 4095, &sinYaw, &cosYaw);

			fixed16_16 velDelta = mul16(moveState->accel, s_deltaTime);
			physicsActor->vel.x += mul16(velDelta, sinYaw);
			physicsActor->vel.z += mul16(velDelta, cosYaw);
		}
		// Handle the vertical move based on the y velocity and vertical angle.
		bobaFett_handleVerticalMove(physicsActor, moveState->yVelOffset, moveState->vertAngle, moveState->yAccel, moveState->soundSrc);
	}

	void bobaFett_handleMovement(BobaFett* bobaFett)
	{
		PhysicsActor* physicsActor = &bobaFett->actor;
		SecObject* obj = bobaFett->logic.obj;
		RSector* sector = obj->sector;
		fixed16_16 targetHeight = sector->ceilingHeight + obj->worldHeight;
		fixed16_16 minHeight = sector->floorHeight;
		RWall* wall = collision_wallCollisionFromPath(sector, obj->posWS.x, obj->posWS.z, bobaFett->moveState.target.x, bobaFett->moveState.target.z);
		if (wall)
		{
			RSector* nextSector = wall->nextSector;
			if (nextSector)
			{
				targetHeight = max(targetHeight, nextSector->ceilingHeight + obj->worldHeight);
				minHeight = min(minHeight, nextSector->floorHeight);
			}
		}

		if (targetHeight < bobaFett->moveState.target.y)
		{
			targetHeight = bobaFett->moveState.target.y;
		}
		if (targetHeight >= minHeight)
		{
			targetHeight = minHeight;
		}
		bobaFett->moveState.target.y = targetHeight;

		bobaFett_handleFlying(physicsActor, &bobaFett->moveState);
	}

	void bobaFett_handleMoveAndAttack(MessageType msg)
	{
		struct LocalContext
		{
			BobaFett* bobaFett;
			SecObject* obj;
			PhysicsActor* physicsActor;
			ActorTarget* target;
			LogicAnimation* anim;
			BobaFettMoveState* moveState;
			
			u32 phase;
			Tick nextAimedShotTick;
			Tick nextShootTick;
			Tick nextChangeHeightTick;
			Tick nextPhaseChangeTick;
			Tick nextSwapAccelTick;
			fixed16_16 accel;
			fixed16_16 heightTarget;
		};
		task_begin_ctx;

		local(bobaFett) = s_curBobaFett;
		local(obj) = local(bobaFett)->logic.obj;
		local(physicsActor) = &local(bobaFett)->actor;
		local(target) = &local(physicsActor)->actor.target;
		local(anim) = &local(physicsActor)->anim;
		local(moveState) = &local(bobaFett)->moveState;
		
		local(phase) = 4;
		local(nextAimedShotTick) = s_curTick + floor16(random(FIXED(291)));
		local(nextChangeHeightTick) = 0;
		local(nextShootTick) = 0;
		local(nextPhaseChangeTick) = 0;
		local(nextSwapAccelTick) = 0;
		local(heightTarget) = 0;
		local(accel) = 0;

		while (local(physicsActor)->state == BOBASTATE_MOVE_AND_ATTACK)
		{
			do
			{
				task_yield(TASK_NO_DELAY);
				s_curBobaFett = local(bobaFett);
				task_callTaskFunc(bobaFett_handleMsg);
			} while (msg != MSG_RUN_TASK);
			if (local(physicsActor)->state != BOBASTATE_MOVE_AND_ATTACK) { break; }
			
			task_localBlockBegin;
				fixed16_16 dx = s_playerObject->posWS.x - local(obj)->posWS.x;
				fixed16_16 dz = s_playerObject->posWS.z - local(obj)->posWS.z;
				angle14_32 angle = vec2ToAngle(dx, dz) & ANGLE_MASK;

				if (local(phase) == 0 || local(phase) == 5)
				{
					local(target)->yaw = angle;
					local(target)->flags |= 4;

					fixed16_16 sinYaw, cosYaw;
					sinCosFixed(local(obj)->yaw, &sinYaw, &cosYaw);

					if (local(phase) == 0)
					{
						local(nextPhaseChangeTick) = s_curTick + 728;
						local(moveState)->vertAngle = ONE_16;
						local(moveState)->yVelOffset = ONE_16;
						local(phase) = 5;

						local(physicsActor)->vel.y -= FIXED(15);
						local(physicsActor)->vel.x += mul16(FIXED(80), sinYaw);
						local(physicsActor)->vel.z += mul16(FIXED(80), cosYaw);
					}
					// Phase == 5
					local(moveState)->target.x = s_playerObject->posWS.x + mul16(FIXED(10), sinYaw);
					local(moveState)->target.y = s_playerObject->posWS.y - FIXED(5);
					local(moveState)->target.z = s_playerObject->posWS.z + mul16(FIXED(10), cosYaw);
				}
				else if (local(phase) == 1 || local(phase) == 6)
				{
					if (local(phase) == 1)
					{
						local(nextPhaseChangeTick) = s_curTick + 1456;
						local(accel) = (s_curTick & 1) ? -FIXED(15) : FIXED(15);
						local(phase) = 6;
					}
					// Phase == 6
					local(moveState)->accel    = FIXED(15);
					local(moveState)->target.x = s_playerObject->posWS.x;
					local(moveState)->target.y = local(heightTarget);
					local(moveState)->target.z = s_playerObject->posWS.z;
				}
				else if (local(phase) == 2 || local(phase) == 7)
				{
					if (local(phase) == 2)
					{
						local(nextPhaseChangeTick) = s_curTick + 2184;
						local(nextSwapAccelTick)   = s_curTick + 291;
						local(phase) = 7;
						local(accel) = FIXED(50);
					}
					// Phase == 7
					local(moveState)->accel    = local(accel);
					local(moveState)->target.x = s_playerObject->posWS.x;
					local(moveState)->target.y = local(heightTarget);
					local(moveState)->target.z = s_playerObject->posWS.z;

					if (local(nextSwapAccelTick) < s_curTick)
					{
						local(accel) = -local(accel);
						local(nextSwapAccelTick) = s_curTick + 291;
					}
				}
				else if (local(phase) == 4)
				{
					local(phase) = (s_curTick & 1) ? 0 : floor16(random(FIXED(3)));
					local(nextPhaseChangeTick) = 0xffffffff;
				}

				bobaFett_handleMovement(local(bobaFett));
				local(moveState)->accel = 0;
				if (local(nextPhaseChangeTick) < s_curTick)
				{
					local(phase) = 4;
				}

				angle14_32 angleDiff = getAngleDifference(angle, local(obj)->yaw);
				// The original DOS code has a bug where it only checks the angle difference in one direction.
				// The 'df_fixBobaFettFireDir' will cause the negative direction to be tested if true.
				JBool canShoot = (angleDiff < 1592) ? JTRUE : JFALSE;	// ~35 degrees
				if (TFE_Settings::getGameSettings()->df_fixBobaFettFireDir && angleDiff <= -1592)
				{
					canShoot = JFALSE;
				}

				if (canShoot)
				{
					if (local(nextAimedShotTick) < s_curTick && actor_canSeeObject(local(obj), s_playerObject))
					{
						local(nextAimedShotTick) = s_curTick + floor16(random(FIXED(291)));
						ProjectileLogic* proj = (ProjectileLogic*)createProjectile(PROJ_BOBAFET_BALL, local(obj)->sector, local(obj)->posWS.x, local(obj)->posWS.y - FIXED(4), local(obj)->posWS.z, local(obj));
						proj->prevColObj = local(obj);
						proj->excludeObj = local(obj);

						playSound3D_oneshot(s_boba2SndID, local(obj)->posWS);
						SecObject* projObj = proj->logic.obj;
						projObj->yaw = angle;
						actor_leadTarget(proj);
					}

					fixed16_16 dist = distApprox(s_playerObject->posWS.x, s_playerObject->posWS.z, local(obj)->posWS.x, local(obj)->posWS.z);
					if (dist < FIXED(25) && local(nextShootTick) < s_curTick && actor_canSeeObject(local(obj), s_playerObject))
					{
						local(nextShootTick) = s_curTick + 36;
						ProjectileLogic* proj = (ProjectileLogic*)createProjectile(PROJ_BOBAFET_BALL, local(obj)->sector, local(obj)->posWS.x, local(obj)->posWS.y - FIXED(4), local(obj)->posWS.z, local(obj));
						proj->prevColObj = local(obj);
						proj->excludeObj = local(obj);

						playSound3D_oneshot(s_boba2SndID, local(obj)->posWS);
						SecObject* projObj = proj->logic.obj;
						projObj->yaw = angle;
						proj_aimAtTarget(proj, s_playerObject->posWS);
						proj->duration = s_curTick + 145;
					}
				}  // if (angleDiff < 1592)

				if (local(nextChangeHeightTick) < s_curTick)  // Pick a target height
				{
					local(nextChangeHeightTick) = s_curTick + floor16(random(FIXED(728)));
					RSector* sector = local(obj)->sector;
					fixed16_16 height = sector->floorHeight - sector->ceilingHeight;
					local(heightTarget) = sector->floorHeight - random(height);
				}

				fixed16_16 top = s_playerObject->posWS.y - s_playerObject->worldHeight;
				if (top > local(heightTarget))
				{
					top = local(heightTarget);
				}
				if (top <= s_playerObject->posWS.y - FIXED(25))
				{
					top = s_playerObject->posWS.y - FIXED(25);
				}
				local(heightTarget) = top;
			task_localBlockEnd;
		}  // while (state == BOBASTATE_MOVE_AND_ATTACK)
		task_end;
	}

	void bobaFett_handleDyingState(MessageType msg)
	{
		struct LocalContext
		{
			SecObject* obj;
			PhysicsActor* physicsActor;
			ActorTarget* target;
			LogicAnimation* anim;
		};
		task_begin_ctx;

		local(obj) = s_curBobaFett->logic.obj;
		local(physicsActor) = &s_curBobaFett->actor;
		local(target) = &local(physicsActor)->actor.target;
		local(anim) = &local(physicsActor)->anim;

		local(target)->flags |= 8;
		stopSound(local(physicsActor)->moveSndId);
		playSound3D_oneshot(s_boba4SndID, local(obj)->posWS);

		local(anim)->flags |= 1;
		local(anim)->frameRate = 8;
		actor_setupAnimation2(local(obj), 2, local(anim));

		// Wait for the animation to finish.
		do
		{
			task_yield(TASK_NO_DELAY);
		} while (msg != MSG_RUN_TASK || !(local(anim)->flags & 2));

		task_localBlockBegin;
			SecObject* corpse = allocateObject();
			sprite_setData(corpse, local(obj)->wax);
			corpse->frame = 0;
			corpse->anim  = 4;
			corpse->posWS = local(obj)->posWS;
			corpse->worldWidth  = 0;
			corpse->worldHeight = 0;
			corpse->entityFlags |= (ETFLAG_CORPSE | ETFLAG_16384);
			sector_addObject(local(obj)->sector, corpse);

			local(physicsActor)->alive = JFALSE;
			actor_handleBossDeath(local(physicsActor));
		task_localBlockEnd;

		task_end;
	}

	void bobaFett_handleSearch(MessageType msg)
	{
		struct LocalContext
		{
			BobaFett* bobaFett;
			SecObject* obj;
			PhysicsActor* physicsActor;
			ActorTarget* target;
			LogicAnimation* anim;
			u32 phase;
			Tick nextCheckForPlayerTick;
			Tick changeStateTick;
			Tick nextChangePhaseTick;
		};
		task_begin_ctx;

		local(bobaFett) = s_curBobaFett;
		local(obj) = local(bobaFett)->logic.obj;
		local(physicsActor) = &local(bobaFett)->actor;
		local(target) = &local(physicsActor)->actor.target;
		local(anim) = &local(physicsActor)->anim;

		local(phase) = 0;
		local(nextCheckForPlayerTick) = 0;
		local(changeStateTick) = s_curTick + 8739;
		local(nextChangePhaseTick) = s_curTick + 1456;
		local(physicsActor)->actor.collisionFlags |= 4;

		while (local(physicsActor)->state == BOBASTATE_SEARCH)
		{
			do
			{
				task_yield(TASK_NO_DELAY);
				s_curBobaFett = local(bobaFett);
				task_callTaskFunc(bobaFett_handleMsg);
			} while (msg != MSG_RUN_TASK);
			if (local(physicsActor)->state != BOBASTATE_SEARCH) { break; }

			if (local(phase) == 0)
			{
				local(bobaFett)->moveState.target.x = s_playerObject->posWS.x;
				local(bobaFett)->moveState.target.y = s_eyePos.y + FIXED(3);
				local(bobaFett)->moveState.target.z = s_playerObject->posWS.z;
			}
			else if (local(phase) == 1)
			{
				local(bobaFett)->moveState.target.x = local(obj)->posWS.x;
				local(bobaFett)->moveState.target.z = local(obj)->posWS.z;
				actor_offsetTarget(&local(bobaFett)->moveState.target.x, &local(bobaFett)->moveState.target.z, FIXED(80), FIXED(40), random_next(), 0x1fff);
				local(phase) = 2;
			}

			bobaFett_handleMovement(local(bobaFett));

			if (local(nextChangePhaseTick) < s_curTick)
			{
				local(phase) = (s_curTick & 1) ? 0 : 1;
				local(nextChangePhaseTick) = s_curTick + 1456;
			}

			if (local(nextCheckForPlayerTick) < s_curTick)
			{
				if (!s_playerDying && actor_canSeeObject(local(obj), s_playerObject))
				{
					local(physicsActor)->state = BOBASTATE_MOVE_AND_ATTACK;
				}
				else
				{
					local(nextCheckForPlayerTick) = s_curTick + 145;
				}
			}
			if (local(changeStateTick) < s_curTick)
			{
				local(physicsActor)->state = BOBASTATE_DEFAULT;
			}
		}  // while (state == BOBASTATE_SEARCH)

		local(physicsActor)->actor.collisionFlags |= 4;
		task_end;
	}

	void bobaFett_handleDefault(MessageType msg)
	{
		struct LocalContext
		{
			BobaFett* bobaFett;
			SecObject* obj;
			PhysicsActor* physicsActor;
			ActorTarget* target;
			LogicAnimation* anim;
			u32 phase;
			Tick nextCheckForPlayerTick;
			Tick changeStateTick;
		};
		task_begin_ctx;

		local(bobaFett) = s_curBobaFett;
		local(obj) = local(bobaFett)->logic.obj;
		local(physicsActor) = &local(bobaFett)->actor;
		local(target) = &local(physicsActor)->actor.target;
		local(anim) = &local(physicsActor)->anim;

		while (local(physicsActor)->state == BOBASTATE_DEFAULT)
		{
			do
			{
				task_yield(145);
				s_curBobaFett = local(bobaFett);
				task_callTaskFunc(bobaFett_handleMsg);
			} while (msg != MSG_RUN_TASK);
			if (local(physicsActor)->state != BOBASTATE_DEFAULT) { break; }

			if (msg == MSG_DAMAGE || msg == MSG_EXPLOSION)
			{
				task_makeActive(local(physicsActor)->actorTask);
				local(physicsActor)->state = BOBASTATE_MOVE_AND_ATTACK;
				task_yield(0);
			}
			if (local(physicsActor)->state != BOBASTATE_DEFAULT) { break; }

			if (actor_canSeeObject(local(obj), s_playerObject))
			{
				playSound3D_oneshot(s_boba1SndID, local(obj)->posWS);
				local(physicsActor)->state = BOBASTATE_SEARCH;
				actor_setupAnimation2(local(obj), 0, local(anim));
			}
		}  // while (state == BOBASTATE_DEFAULT)
		task_end;
	}

	void bobaFettTaskFunc(MessageType msg)
	{
		struct LocalContext
		{
			BobaFett* bobaFett;
			SecObject* obj;
			PhysicsActor* physicsActor;
			ActorTarget* target;
			LogicAnimation* anim;
		};
		task_begin_ctx;

		local(bobaFett) = (BobaFett*)task_getUserData();
		local(obj) = local(bobaFett)->logic.obj;
		local(physicsActor) = &local(bobaFett)->actor;
		local(target) = &local(physicsActor)->actor.target;
		local(anim) = &local(physicsActor)->anim;

		while (local(physicsActor)->alive)
		{
			msg = MSG_RUN_TASK;
			task_setMessage(msg);

			if (local(physicsActor)->state == BOBASTATE_MOVE_AND_ATTACK)
			{
				s_curBobaFett = local(bobaFett);
				task_callTaskFunc(bobaFett_handleMoveAndAttack);
			}
			else if (local(physicsActor)->state == BOBASTATE_DYING)
			{
				s_curBobaFett = local(bobaFett);
				task_callTaskFunc(bobaFett_handleDyingState);
			}
			else if (local(physicsActor)->state == BOBASTATE_SEARCH)
			{
				s_curBobaFett = local(bobaFett);
				task_callTaskFunc(bobaFett_handleSearch);
			}
			else  // Default
			{
				s_curBobaFett = local(bobaFett);
				task_callTaskFunc(bobaFett_handleDefault);
			}
		}  // while (alive)

		// Dead
		while (msg != MSG_RUN_TASK)
		{
			task_yield(TASK_NO_DELAY);
		}

		actor_removePhysicsActorFromWorld(local(physicsActor));
		deleteLogicAndObject(&local(bobaFett)->logic);
		level_free(local(bobaFett));
		
		task_end;
	}

	void bobaFettCleanupFunc(Logic* logic)
	{
		BobaFett* bobaFett = (BobaFett*)logic;
		PhysicsActor* physicsActor = &bobaFett->actor;

		actor_removePhysicsActorFromWorld(physicsActor);
		deleteLogicAndObject(&bobaFett->logic);
		level_free(bobaFett);
		task_free(physicsActor->actorTask);
	}

	Logic* bobaFett_setup(SecObject* obj, LogicSetupFunc* setupFunc)
	{
		if (!s_boba1SndID)
		{
			s_boba1SndID = sound_Load("boba-1.voc");
		}
		if (!s_boba3SndID)
		{
			s_boba3SndID = sound_Load("boba-3.voc");
		}
		if (!s_boba2SndID)
		{
			s_boba2SndID = sound_Load("boba-2.voc");
		}
		if (!s_boba4SndID)
		{
			s_boba4SndID = sound_Load("boba-4.voc");
		}
		if (!s_bobaRocket2SndID)
		{
			s_bobaRocket2SndID = sound_Load("rocket-2.voc");
		}

		BobaFett* bobaFett = (BobaFett*)level_alloc(sizeof(BobaFett));
		memset(bobaFett, 0, sizeof(BobaFett));

		PhysicsActor* physicsActor = &bobaFett->actor;

		// Give the name of the task a number so I can tell them apart when debugging.
		char name[32];
		sprintf(name, "BobaFett%d", s_bobaFettNum);
		s_bobaFettNum++;
		Task* task = createSubTask(name, bobaFettTaskFunc);
		task_setUserData(task, bobaFett);

		obj->entityFlags = ETFLAG_AI_ACTOR;
		obj->worldWidth >>= 1;
		obj->worldHeight = FIXED(7);

		physicsActor->alive = JTRUE;
		physicsActor->hp    = FIXED(200);
		physicsActor->state = BOBASTATE_DEFAULT;
		physicsActor->actorTask = task;

		bobaFett->unused   = 0;
		bobaFett->hitSndId = 0;
		bobaFett->moveState.yAccel = FIXED(220);
		bobaFett->moveState.vertAngleRange = ONE_16;
		bobaFett->moveState.vertAngle = 0;
		bobaFett->moveState.yVelOffset = 0;
		bobaFett->moveState.soundSrc   = s_bobaRocket2SndID;
		bobaFett->moveState.accel = 0;
		bobaFett->logic.obj = obj;
		actor_addPhysicsActorToWorld(physicsActor);

		physicsActor->actor.header.obj  = obj;
		physicsActor->actor.physics.obj = obj;
		actor_setupSmartObj(&physicsActor->actor);

		physicsActor->actor.physics.width = FIXED(2);
		physicsActor->actor.physics.botOffset = 0;

		physicsActor->actor.collisionFlags &= 0xfffffff8;
		physicsActor->actor.collisionFlags |= 6;
		physicsActor->actor.physics.yPos = FIXED(9999);
		physicsActor->actor.physics.height = obj->worldHeight;

		LogicAnimation* anim = &physicsActor->anim;
		anim->frameRate = 5;
		anim->frameCount = ONE_16;
		anim->prevTick = 0;
		anim->flags |= 2;
		anim->flags &= 0xfffffffe;
		actor_setupAnimation2(obj, 5, anim);

		ActorTarget* target = &physicsActor->actor.target;
		target->speedRotation = 6826;

		obj_addLogic(obj, (Logic*)bobaFett, task, bobaFettCleanupFunc);

		if (setupFunc)
		{
			*setupFunc = nullptr;
		}
		return (Logic*)bobaFett;
	}
}  // namespace TFE_DarkForces