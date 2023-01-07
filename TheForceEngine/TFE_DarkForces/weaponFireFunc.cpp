#include "weaponFireFunc.h"
#include "weapon.h"
#include "random.h"
#include "player.h"
#include "pickup.h"
#include "projectile.h"
#include "hitEffect.h"
#include <TFE_Jedi/Collision/collision.h>
#include <TFE_Jedi/Renderer/jediRenderer.h>
// TFE
#include <TFE_Settings/settings.h>

namespace TFE_DarkForces
{
	enum WeaponFireConst
	{
		MAX_AUTOAIM_DIST = COL_INFINITY,
	};

	static SoundEffectId s_punchSwingSndId = 0;
	static SoundEffectId s_pistolSndId = 0;
	static SoundEffectId s_rifleSndId = 0;
	static SoundEffectId s_mortarFireSndID = 0;
	static SoundEffectId s_mortarFireSndID2 = 0;
	static SoundEffectId s_outOfAmmoSndId = 0;
	static SoundEffectId s_mortarOutofAmmoSndId = 0;
	static SoundEffectId s_repeaterFireSndID1 = 0;
	static SoundEffectId s_repeaterOutOfAmmoSndId = 0;
	static SoundEffectId s_fusionFireSndID = 0;
	static SoundEffectId s_fusionOutOfAmmoSndID = 0;
	static SoundEffectId s_mineSndId = 0;
	static SoundEffectId s_concussionFireSndID = 0;
	static SoundEffectId s_concussionFireSndID1 = 0;
	static SoundEffectId s_concussionOutOfAmmoSndID = 0;
	static SoundEffectId s_cannonOutOfAmmoSndID = 0;
	static SoundEffectId s_cannonFireSndID = 0;
	static SoundEffectId s_cannonFireSndID1 = 0;
	static fixed16_16 s_autoAimDirX;
	static fixed16_16 s_autoAimDirZ;
	static fixed16_16 s_wpnPitchSin;
	static fixed16_16 s_wpnPitchCos;
	static angle14_32 s_weaponFirePitch;
	static angle14_32 s_weaponFireYaw;

	extern WeaponAnimState s_weaponAnimState;
	extern SoundEffectId s_repeaterFireSndID;

	extern JBool s_weaponAutoMount2;
	extern JBool s_secondaryFire;
	extern JBool s_weaponOffAnim;
	extern JBool s_isShooting;
	extern s32 s_canFireWeaponSec;
	extern s32 s_canFireWeaponPrim;
	extern u32 s_fireFrame;

	extern s32 s_prevWeapon;
	extern s32 s_curWeapon;
	extern s32 s_nextWeapon;
	extern s32 s_lastWeapon;

	struct WeaponAnimFrame
	{
		s32 waxFrame;
		s32 weaponLight;
		Tick delaySupercharge;
		Tick delayNormal;
	};
	static const WeaponAnimFrame c_punchAnim[4] =
	{
		{ 1,  0,   5, 10 },
		{ 2,  0,  10, 21 },
		{ 3,  0,  10, 21 },
		{ 0,  0,  10, 21 },
	};
	static const WeaponAnimFrame c_pistolAnim[3] =
	{
		{ 1, 40,  7, 14 },
		{ 2,  0,  7, 14 },
		{ 0,  0, 21, 43 },
	};
	static const WeaponAnimFrame c_rifleAnim[2] =
	{
		{ 1, 40,  3,  7 },
		{ 0,  0,  7, 14 },
	};
	static const WeaponAnimFrame c_mortarAnim[4] =
	{
		{ 1, 36,  29, 58 },
		{ 2, 36,   7, 14 },
		{ 3, 36,   7, 14 },
		{ 0,  0,   7, 14 },
	};
	static const WeaponAnimFrame c_thermalDetAnim[5] =
	{
		{ 1, 0, 11, 11 },
		{ 2, 0, 58, 58 },
		{ 1, 0, 29, 29 },
		{ 0, 0, 29, 29 },
		{ 3, 0, 29, 29 },
	};
	static const WeaponAnimFrame c_repeaterAnim[4] =
	{
		{ 1,  0,  2,  5 },	// primary
		{ 1,  0,  4,  8 },	// secondary
		{ 2, 33,  5, 11 },
		{ 0,  0, 14, 29 },
	};
	static const WeaponAnimFrame c_concussionAnim[] =
	{
		{ 1, 36, 21, 43 },
		{ 2,  0,  7, 14 },
		{ 0,  0, 29, 58 },
	};
	static const angle14_32 c_repeaterYawOffset[3] = { 0, 136, -136 };
	static const angle14_32 c_repeaterPitchOffset[3] = { 136, -136, -136 };
	// Deltas for repeater - note the order is ZXY
	static fixed16_16 c_repeaterDelta[3 * 3] =
	{
		0x50000, 0xe666,  0xfae1,   // 5.0, 0.9, 0.98
		0x50000, 0x8000,  0x18000,  // 5.0, 0.5, 1.5
		0x50000, 0x18000, 0x18000,  // 5.0, 1.5, 1.5
	};
	static s32 s_fusionCylinder = 1;
	static fixed16_16 s_fusionOffset[] =
	{
		FIXED(3), -ONE_16, 0x8000,	// 3.0, -1.0, 0.5
		FIXED(3),  -19660, 0xcccc,	// 3.0, -0.3, 0.8
		FIXED(3),  0x4ccc, 0x11999,	// 3.0,  0.3, 1.1
		FIXED(3),  0xcccc, 0x14ccc, // 3.0,  0.8, 1.3
	};
	static angle14_32 s_fusionYawOffset[] =
	{
		-375, -125, 125, 375
	};
	static JBool s_fusionCycleForward = JTRUE;

	extern void weapon_handleState(MessageType msg);
	extern void weapon_handleState2(MessageType msg);
	extern void weapon_handleOffAnimation(MessageType msg);
	extern void weapon_handleOnAnimation(MessageType msg);
	extern void weapon_animateOnOrOffscreen(MessageType msg);
	JBool computeAutoaim(fixed16_16 xPos, fixed16_16 yPos, fixed16_16 zPos, angle14_32 pitch, angle14_32 yaw, s32 variation);

	// Adjust the speed - in TFE the framerate may be higher than expected by the original code, which means that this
	// step (proj->delta) is too large for the 'speed'.
	void tfe_adjustWeaponCollisionSpeed(ProjectileLogic* proj)
	{
		fixed16_16 initSpeed = div16(vec3Length(proj->delta.x, proj->delta.y, proj->delta.z), max(1, s_deltaTime));
		proj->col_speed = proj->speed;
		if (initSpeed > proj->speed)
		{
			proj->col_speed = initSpeed;
		}
	}

	void tfe_restoreWeaponCollisionSpeed(ProjectileLogic* proj)
	{
		proj->col_speed = proj->speed;
	}

	void weaponFire_fist(MessageType msg)
	{
		struct LocalContext
		{
			Tick delay;
			s32  iFrame;
		};
		task_begin_ctx;

		taskCtx->delay = (s_superCharge) ? c_punchAnim[0].delaySupercharge : c_punchAnim[0].delayNormal;
		s_curPlayerWeapon->frame = c_punchAnim[0].waxFrame;
		s_weaponLight = c_punchAnim[0].weaponLight;
		do
		{
			task_yield(taskCtx->delay);
			task_callTaskFunc(weapon_handleState);
		} while (msg != MSG_RUN_TASK);

		if (s_punchSwingSndId)
		{
			sound_stop(s_punchSwingSndId);
		}
		s_punchSwingSndId = sound_play(s_punchSwingSndSrc);

		if (s_curPlayerWeapon->wakeupRange)
		{
			vec3_fixed origin = { s_playerObject->posWS.x, s_playerObject->posWS.y, s_playerObject->posWS.z };
			collision_effectObjectsInRangeXZ(s_playerObject->sector, s_curPlayerWeapon->wakeupRange, origin, hitEffectWakeupFunc, s_playerObject, ETFLAG_AI_ACTOR);
		}

		task_localBlockBegin;
		fixed16_16 mtx[9];
		weapon_computeMatrix(mtx, -s_playerObject->pitch, -s_playerObject->yaw);

		fixed16_16 xOffset = -114688;	// -1.75
		angle14_32 yawOffset = -910;
		for (s32 i = 0; i < 3; i++, xOffset += 0x1c000, yawOffset += 910)
		{
			ProjectileLogic* proj = (ProjectileLogic*)createProjectile(ProjectileType::PROJ_PUNCH, s_playerObject->sector, s_playerObject->posWS.x, s_playerObject->posWS.y - s_playerObject->worldHeight + s_headwaveVerticalOffset, s_playerObject->posWS.z, s_playerObject);

			s_weaponFirePitch = s_playerObject->pitch + 0x638;
			s_weaponFireYaw   = s_playerObject->yaw + yawOffset;
			proj_setTransform(proj, s_weaponFirePitch, s_weaponFireYaw);

			proj->hitEffectId = HEFFECT_PUNCH;
			proj->prevColObj = s_playerObject;
			proj->excludeObj = s_playerObject;

			vec3_fixed inVec =
			{
				xOffset, // -1.75, 0.0, +1.75
				0x18000, // 1.5
				0x60000  // 6.0
			};
			rotateVectorM3x3(&inVec, &proj->delta, mtx);

			tfe_adjustWeaponCollisionSpeed(proj);

			ProjectileHitType hitType = proj_handleMovement(proj);
			handleProjectileHit(proj, hitType);

			tfe_restoreWeaponCollisionSpeed(proj);
		}
		task_localBlockEnd;

		// Animation.
		for (taskCtx->iFrame = 1; taskCtx->iFrame < TFE_ARRAYSIZE(c_punchAnim); taskCtx->iFrame++)
		{
			s_curPlayerWeapon->frame = c_punchAnim[taskCtx->iFrame].waxFrame;
			s_weaponLight = c_punchAnim[taskCtx->iFrame].weaponLight;
			taskCtx->delay = (s_superCharge) ? c_punchAnim[taskCtx->iFrame].delaySupercharge : c_punchAnim[taskCtx->iFrame].delayNormal;

			do
			{
				task_yield(taskCtx->delay);
				task_callTaskFunc(weapon_handleState);
			} while (msg != MSG_RUN_TASK);
		}
		s_canFireWeaponPrim = 1;
		s_canFireWeaponSec = 1;

		task_end;
	}

	void weaponFire_pistol(MessageType msg)
	{
		struct LocalContext
		{
			Tick delay;
			s32  iFrame;
		};
		task_begin_ctx;

		if (*s_curPlayerWeapon->ammo)
		{
			task_localBlockBegin;
			if (s_pistolSndId)
			{
				sound_stop(s_pistolSndId);
			}
			s_pistolSndId = sound_play(s_pistolSndSrc);

			if (s_curPlayerWeapon->wakeupRange)
			{
				vec3_fixed origin = { s_playerObject->posWS.x, s_playerObject->posWS.y, s_playerObject->posWS.z };
				collision_effectObjectsInRangeXZ(s_playerObject->sector, s_curPlayerWeapon->wakeupRange, origin, hitEffectWakeupFunc, s_playerObject, ETFLAG_AI_ACTOR);
			}

			fixed16_16 mtx[9];
			weapon_computeMatrix(mtx, -s_playerObject->pitch, -s_playerObject->yaw);

			vec3_fixed inVec =
			{
				0x6666,	// 0.4
				0x9999, // 0.6
				0x33333 // 3.2
			};
			vec3_fixed outVec;
			rotateVectorM3x3(&inVec, &outVec, mtx);

			fixed16_16 xPos = s_playerObject->posWS.x + outVec.x;
			fixed16_16 yPos = outVec.y + s_playerObject->posWS.y - s_playerObject->worldHeight + s_headwaveVerticalOffset;
			fixed16_16 zPos = s_playerObject->posWS.z + outVec.z;

			JBool targetFound = computeAutoaim(xPos, yPos, zPos, s_playerObject->pitch, s_playerObject->yaw, s_curPlayerWeapon->variation & 0xffff);
			if (!targetFound)
			{
				s32 variation = s_curPlayerWeapon->variation & 0xffff;
				variation = random(variation * 2) - variation;
				s_weaponFirePitch = s_playerObject->pitch + variation;

				variation = s_curPlayerWeapon->variation & 0xffff;
				variation = random(variation * 2) - variation;
				s_weaponFireYaw = s_playerObject->yaw + variation;
			}

			s32 canFire = s_canFireWeaponPrim;
			fixed16_16 mtx2[9];
			s_canFireWeaponPrim = 0;
			if (canFire > 1)
			{
				weapon_computeMatrix(mtx2, -s_weaponFirePitch, -s_weaponFireYaw);
			}

			// Spawn projectiles.
			fixed16_16 fire = intToFixed16(canFire);
			while (canFire)
			{
				if (*s_curPlayerWeapon->ammo == 0)
				{
					break;
				}
				fire -= ONE_16;
				canFire--;
				s32 superChargeFrame = s_superCharge ? 0 : 1;
				// This is always true if super charge is *not* active otherwise
				// it is true every other frame.
				if (superChargeFrame | (s_fireFrame & 1))
				{
					*s_curPlayerWeapon->ammo = pickup_addToValue(s_playerInfo.ammoEnergy, -1, 500);
				}

				ProjectileLogic* projLogic;
				fixed16_16 yPos = s_playerObject->posWS.y - s_playerObject->worldHeight + s_headwaveVerticalOffset;
				projLogic = (ProjectileLogic*)createProjectile(PROJ_PISTOL_BOLT, s_playerObject->sector, s_playerObject->posWS.x, yPos, s_playerObject->posWS.z, s_playerObject);
				projLogic->flags &= ~PROJFLAG_CAMERA_PASS_SOUND;
				projLogic->prevColObj = s_playerObject;

				if (targetFound)
				{
					proj_setYawPitch(projLogic, s_wpnPitchSin, s_wpnPitchCos, s_autoAimDirX, s_autoAimDirZ);

					SecObject* projObj = projLogic->logic.obj;
					projObj->pitch = s_weaponFirePitch;
					projObj->yaw = s_weaponFireYaw;
				}
				else
				{
					proj_setTransform(projLogic, s_weaponFirePitch, s_weaponFireYaw);
				}

				if (canFire)
				{
					vec3_fixed inVec2 = { 0 };
					if (canFire > 1)
					{
						// TODO: This doesn't seem to be hit in the base game.
						assert(0);
					}

					Tick v0 = (s_superCharge) ? 21 : 43;
					Tick v1 = (s_superCharge) ? 7 : 14;
					Tick v2 = (s_superCharge) ? 7 : 14;
					inVec2.z = intToFixed16(s32(float(v0 + v1 + v2) * 1.71f));

					vec3_fixed outVec2;
					rotateVectorM3x3(&inVec2, &outVec2, mtx2);
					// First try using the outVec value computed above.
					projLogic->delta = outVec;
					tfe_adjustWeaponCollisionSpeed(projLogic);

					ProjectileHitType hitType = proj_handleMovement(projLogic);
					if (!handleProjectileHit(projLogic, hitType))
					{
						// If that fails, then try the new value.
						projLogic->delta = outVec2;
						tfe_adjustWeaponCollisionSpeed(projLogic);

						hitType = proj_handleMovement(projLogic);
						fire = handleProjectileHit(projLogic, hitType);
					}
				}
				else
				{
					projLogic->delta = outVec;
					tfe_adjustWeaponCollisionSpeed(projLogic);

					ProjectileHitType hitType = proj_handleMovement(projLogic);
					handleProjectileHit(projLogic, hitType);
				}
				tfe_restoreWeaponCollisionSpeed(projLogic);
			}
			task_localBlockEnd;
			
			// Animation.
			for (taskCtx->iFrame = 0; taskCtx->iFrame < TFE_ARRAYSIZE(c_pistolAnim); taskCtx->iFrame++)
			{
				s_curPlayerWeapon->frame = c_pistolAnim[taskCtx->iFrame].waxFrame;
				s_weaponLight = c_pistolAnim[taskCtx->iFrame].weaponLight;
				taskCtx->delay = (s_superCharge) ? c_pistolAnim[taskCtx->iFrame].delaySupercharge : c_pistolAnim[taskCtx->iFrame].delayNormal;

				do
				{
					task_yield(taskCtx->delay);
					task_callTaskFunc(weapon_handleState);
				} while (msg != MSG_RUN_TASK);
			}

			s_canFireWeaponPrim = 1;
			s_canFireWeaponSec = 1;
		}
		else  // Out of Ammo
		{
			if (s_outOfAmmoSndId)
			{
				sound_stop(s_outOfAmmoSndId);
			}
			s_outOfAmmoSndId = sound_play(s_pistolEmptySndSrc);

			taskCtx->delay = (s_superCharge) ? 3 : 7;
			s_curPlayerWeapon->frame = 0;
			do
			{
				task_yield(taskCtx->delay);
				task_callTaskFunc(weapon_handleState);
			} while (msg != MSG_RUN_TASK);

			s_canFireWeaponPrim = 0;
			s_canFireWeaponSec = 0;
			// if (s_weaponAutoMount2)
			// {
				// func_1ece78();
			// }
		}

		task_end;
	}

	void weaponFire_rifle(MessageType msg)
	{
		struct LocalContext
		{
			Tick delay;
			s32  iFrame;
		};
		task_begin_ctx;

		if (*s_curPlayerWeapon->ammo)
		{
			task_localBlockBegin;
			if (s_rifleSndId)
			{
				sound_stop(s_rifleSndId);
			}
			s_rifleSndId = sound_play(s_rifleSndSrc);

			if (s_curPlayerWeapon->wakeupRange)
			{
				vec3_fixed origin = { s_playerObject->posWS.x, s_playerObject->posWS.y, s_playerObject->posWS.z };
				collision_effectObjectsInRangeXZ(s_playerObject->sector, s_curPlayerWeapon->wakeupRange, origin, hitEffectWakeupFunc, s_playerObject, ETFLAG_AI_ACTOR);
			}

			fixed16_16 mtx[9];
			weapon_computeMatrix(mtx, -s_playerObject->pitch, -s_playerObject->yaw);

			vec3_fixed inVec =
			{
				0x6666,	// 0.4
				0xc000, // 0.75
				0x40000 // 4.0
			};
			vec3_fixed outVec;
			rotateVectorM3x3(&inVec, &outVec, mtx);

			fixed16_16 xPos = s_playerObject->posWS.x + outVec.x;
			fixed16_16 yPos = outVec.y + s_playerObject->posWS.y - s_playerObject->worldHeight + s_headwaveVerticalOffset;
			fixed16_16 zPos = s_playerObject->posWS.z + outVec.z;

			JBool targetFound = computeAutoaim(xPos, yPos, zPos, s_playerObject->pitch, s_playerObject->yaw, s_curPlayerWeapon->variation & 0xffff);
			if (!targetFound)
			{
				s32 variation = s_curPlayerWeapon->variation & 0xffff;
				variation = random(variation * 2) - variation;
				s_weaponFirePitch = s_playerObject->pitch + variation;

				variation = s_curPlayerWeapon->variation & 0xffff;
				variation = random(variation * 2) - variation;
				s_weaponFireYaw = s_playerObject->yaw + variation;
			}

			s32 canFire = s_canFireWeaponPrim;
			fixed16_16 mtx2[9];
			s_canFireWeaponPrim = 0;
			if (canFire > 1)
			{
				weapon_computeMatrix(mtx2, -s_weaponFirePitch, -s_weaponFireYaw);
			}

			// Spawn projectiles.
			fixed16_16 fire = intToFixed16(canFire);
			while (canFire)
			{
				if (*s_curPlayerWeapon->ammo == 0)
				{
					break;
				}
				fire -= ONE_16;
				canFire--;
				s32 superChargeFrame = s_superCharge ? 0 : 1;
				// This is always true if super charge is *not* active otherwise
				// it is true every other frame.
				if (superChargeFrame | (s_fireFrame & 1))
				{
					*s_curPlayerWeapon->ammo = pickup_addToValue(s_playerInfo.ammoEnergy, -2, 500);
				}

				ProjectileLogic* projLogic;
				fixed16_16 yPos = s_playerObject->posWS.y - s_playerObject->worldHeight + s_headwaveVerticalOffset;
				projLogic = (ProjectileLogic*)createProjectile(PROJ_RIFLE_BOLT, s_playerObject->sector, s_playerObject->posWS.x, yPos, s_playerObject->posWS.z, s_playerObject);
				projLogic->flags &= ~PROJFLAG_CAMERA_PASS_SOUND;
				projLogic->prevColObj = s_playerObject;

				if (targetFound)
				{
					proj_setYawPitch(projLogic, s_wpnPitchSin, s_wpnPitchCos, s_autoAimDirX, s_autoAimDirZ);

					SecObject* projObj = projLogic->logic.obj;
					projObj->pitch = s_weaponFirePitch;
					projObj->yaw = s_weaponFireYaw;
				}
				else
				{
					proj_setTransform(projLogic, s_weaponFirePitch, s_weaponFireYaw);
				}

				if (canFire)
				{
					vec3_fixed inVec2 = { 0 };
					if (canFire > 1)
					{
						// TODO: This doesn't seem to be hit in the base game.
						assert(0);
					}

					Tick v0 = (s_superCharge) ? 7 : 14;
					Tick v1 = (s_superCharge) ? 3 : 7;
					Tick v2 = (s_superCharge) ? 3 : 7;
					inVec2.z = intToFixed16(s32(float(v0 + v1 + v2) * 1.71f));

					vec3_fixed outVec2;
					rotateVectorM3x3(&inVec2, &outVec2, mtx2);
					// First try using the outVec value computed above.
					projLogic->delta = outVec;
					tfe_adjustWeaponCollisionSpeed(projLogic);

					ProjectileHitType hitType = proj_handleMovement(projLogic);
					if (!handleProjectileHit(projLogic, hitType))
					{
						// If that fails, then try the new value.
						projLogic->delta = outVec2;
						tfe_adjustWeaponCollisionSpeed(projLogic);

						hitType = proj_handleMovement(projLogic);
						fire = handleProjectileHit(projLogic, hitType);
					}
				}
				else
				{
					projLogic->delta = outVec;
					tfe_adjustWeaponCollisionSpeed(projLogic);

					ProjectileHitType hitType = proj_handleMovement(projLogic);
					handleProjectileHit(projLogic, hitType);
				}
				tfe_restoreWeaponCollisionSpeed(projLogic);
			}
			task_localBlockEnd;

			// Animation.
			for (taskCtx->iFrame = 0; taskCtx->iFrame < TFE_ARRAYSIZE(c_rifleAnim); taskCtx->iFrame++)
			{
				s_curPlayerWeapon->frame = c_rifleAnim[taskCtx->iFrame].waxFrame;
				s_weaponLight = c_rifleAnim[taskCtx->iFrame].weaponLight;
				taskCtx->delay = (s_superCharge) ? c_rifleAnim[taskCtx->iFrame].delaySupercharge : c_rifleAnim[taskCtx->iFrame].delayNormal;

				do
				{
					task_yield(taskCtx->delay);
					task_callTaskFunc(weapon_handleState);
				} while (msg != MSG_RUN_TASK);
			}

			s_canFireWeaponPrim = 1;
			s_canFireWeaponSec = 1;
		}
		else  // Out of Ammo
		{
			if (s_outOfAmmoSndId)
			{
				sound_stop(s_outOfAmmoSndId);
			}
			s_outOfAmmoSndId = sound_play(s_pistolEmptySndSrc);

			taskCtx->delay = (s_superCharge) ? 3 : 7;
			s_curPlayerWeapon->frame = 0;
			do
			{
				task_yield(taskCtx->delay);
				task_callTaskFunc(weapon_handleState);
			} while (msg != MSG_RUN_TASK);

			s_canFireWeaponPrim = 0;
			s_canFireWeaponSec = 0;
			// if (s_weaponAutoMount2)
			// {
				// func_1ece78();
			// }
		}

		task_end;
	}

	void weaponFire_thermalDetonator(MessageType msg)
	{
		struct LocalContext
		{
			Tick delay;
			Tick startTick;
			s32  iFrame;
		};
		task_begin_ctx;

		if (*s_curPlayerWeapon->ammo)
		{
			s_canFireWeaponPrim = 0;
			s_canFireWeaponSec = 0;
			*s_curPlayerWeapon->ammo = pickup_addToValue(s_playerInfo.ammoDetonator, -1, 50);

			// Weapon Frame 0.
			s_curPlayerWeapon->frame = c_thermalDetAnim[0].waxFrame;
			do
			{
				task_yield(c_thermalDetAnim[0].delayNormal);
				task_callTaskFunc(weapon_handleState);
			} while (msg != MSG_RUN_TASK);

			s_curPlayerWeapon->xOffset = 100;
			s_curPlayerWeapon->yOffset = 100;
			taskCtx->startTick = s_curTick;
			task_callTaskFunc(weapon_handleState2);

			// Wait until the player is no longer "shooting"
			// This holds the thermal detonator in place.
			while (s_isShooting)
			{
				task_yield(TASK_NO_DELAY);
				task_callTaskFunc(weapon_handleState);
			};

			task_localBlockBegin;
			task_callTaskFunc(weapon_handleState2);
			fixed16_16 dt = intToFixed16(s_curTick - taskCtx->startTick);

			if (s_curPlayerWeapon->wakeupRange)
			{
				vec3_fixed origin = { s_playerObject->posWS.x, s_playerObject->posWS.y, s_playerObject->posWS.z };
				collision_effectObjectsInRangeXZ(s_playerObject->sector, s_curPlayerWeapon->wakeupRange, origin, hitEffectWakeupFunc, s_playerObject, ETFLAG_AI_ACTOR);
			}

			fixed16_16 yPos = s_playerObject->posWS.y - s_playerObject->worldHeight + s_headwaveVerticalOffset;
			ProjectileLogic* proj = (ProjectileLogic*)createProjectile(PROJ_THERMAL_DET, s_playerObject->sector, s_playerObject->posWS.x, yPos, s_playerObject->posWS.z, s_playerObject);
			proj->flags &= ~PROJFLAG_CAMERA_PASS_SOUND;
			proj->prevColObj = s_playerObject;

			// Calculate projectile speed and duration.
			if (s_secondaryFire)
			{
				dt = max(dt, FIXED(23));
			}
			else
			{
				proj->duration = 0xffffffff;
				proj->bounceCnt = 0;
				dt = max(dt, FIXED(58));
			}
			dt = min(dt, FIXED(116));
			proj->speed = mul16(div16(dt, FIXED(116)), FIXED(100));

			// Calculate projectile transform.
			s32 baseVariation = s_curPlayerWeapon->variation & 0xffff;
			s32 pitchVariation = random(baseVariation * 2) - baseVariation;
			s_weaponFirePitch = (s_playerObject->pitch >> 1) + 0x638 + pitchVariation;

			s32 yawVariation = random(baseVariation * 2) - baseVariation;
			s_weaponFireYaw = s_playerObject->yaw + yawVariation;

			proj_setTransform(proj, s_weaponFirePitch, s_weaponFireYaw);

			// Calculate initial projectile delta and handle up close collision.
			fixed16_16 mtx[9];
			weapon_computeMatrix(mtx, -(s_playerObject->pitch + 0x638), -s_playerObject->yaw);

			vec3_fixed inVec = { FIXED(2), FIXED(2), FIXED(2) };
			rotateVectorM3x3(&inVec, &proj->delta, mtx);
			tfe_adjustWeaponCollisionSpeed(proj);

			ProjectileHitType hitType = proj_handleMovement(proj);
			handleProjectileHit(proj, hitType);

			tfe_restoreWeaponCollisionSpeed(proj);
			task_localBlockEnd;

			// Handle animation
			s_curPlayerWeapon->xOffset = 0;
			s_curPlayerWeapon->yOffset = 0;

			// The initial frame is the same regardless of ammo.
			s_curPlayerWeapon->frame = c_thermalDetAnim[1].waxFrame;
			s_weaponLight = c_thermalDetAnim[1].weaponLight;
			taskCtx->delay = c_thermalDetAnim[1].delayNormal;
			do
			{
				task_yield(taskCtx->delay);
				task_callTaskFunc(weapon_handleState);
			} while (msg != MSG_RUN_TASK);

			if (*s_curPlayerWeapon->ammo)
			{
				for (taskCtx->iFrame = 2; taskCtx->iFrame <= 3; taskCtx->iFrame++)
				{
					s_curPlayerWeapon->frame = c_thermalDetAnim[taskCtx->iFrame].waxFrame;
					s_weaponLight = c_thermalDetAnim[taskCtx->iFrame].weaponLight;
					taskCtx->delay = c_thermalDetAnim[taskCtx->iFrame].delayNormal;
					do
					{
						task_yield(taskCtx->delay);
						task_callTaskFunc(weapon_handleState);
					} while (msg != MSG_RUN_TASK);
				}
			}
			else
			{
				s_curPlayerWeapon->frame = c_thermalDetAnim[4].waxFrame;
				s_weaponLight = c_thermalDetAnim[4].weaponLight;
				taskCtx->delay = c_thermalDetAnim[4].delayNormal;
				do
				{
					task_yield(taskCtx->delay);
					task_callTaskFunc(weapon_handleState);
				} while (msg != MSG_RUN_TASK);
			}

			s_canFireWeaponPrim = 1;
			s_canFireWeaponSec = 1;
		}
		else  // Out of Ammo
		{
			if (s_weaponAutoMount2)
			{
				//func_1ece78();
				//return;
			}
			
			s_lastWeapon = s_curWeapon;
			s_curWeapon = WPN_FIST;
			s_playerInfo.curWeapon = s_prevWeapon;
			if (s_weaponOffAnim)
			{
				task_callTaskFunc(weapon_handleOffAnimation);
			}
			weapon_setNext(s_curWeapon);
			task_callTaskFunc(weapon_handleOnAnimation);
		}

		task_end;
	}

	void weaponFire_repeater(MessageType msg)
	{
		struct LocalContext
		{
			Tick delay;
			s32  iFrame;
		};
		task_begin_ctx;

		if (s_secondaryFire)
		{
			if (*s_curPlayerWeapon->ammo)
			{
				if (s_repeaterFireSndID1)
				{
					sound_stop(s_repeaterFireSndID1);
				}
				s_repeaterFireSndID1 = sound_play(s_repeater1SndSrc);

				if (s_curPlayerWeapon->wakeupRange)
				{
					vec3_fixed origin = { s_playerObject->posWS.x, s_playerObject->posWS.y, s_playerObject->posWS.z };
					collision_effectObjectsInRangeXZ(s_playerObject->sector, s_curPlayerWeapon->wakeupRange, origin, hitEffectWakeupFunc, s_playerObject, ETFLAG_AI_ACTOR);
				}

				taskCtx->delay = s_superCharge ? c_repeaterAnim[1].delaySupercharge : c_repeaterAnim[1].delayNormal;
				s_curPlayerWeapon->frame = c_repeaterAnim[1].waxFrame;
				do
				{
					task_yield(taskCtx->delay);
					task_callTaskFunc(weapon_handleState);
				} while (msg != MSG_RUN_TASK);

				task_localBlockBegin;
				fixed16_16 mtx[9];
				weapon_computeMatrix(mtx, -s_playerObject->pitch, -s_playerObject->yaw);

				s32 baseVariation = s_curPlayerWeapon->variation & 0xffff;
				s32 variation = random(baseVariation * 2) - baseVariation;
				s_weaponFirePitch = s_playerObject->pitch + variation;

				variation = random(variation * 2) - variation;
				s_weaponFireYaw = s_playerObject->yaw + variation;

				s32 canFire = s_canFireWeaponSec;
				s_canFireWeaponSec = 0;

				fixed16_16 mtx2[9];
				if (canFire > 1)
				{
					weapon_computeMatrix(mtx2, -s_weaponFirePitch, -s_weaponFireYaw);
				}

				fixed16_16 fire = intToFixed16(canFire);
				while (canFire)
				{
					if (!*s_curPlayerWeapon->ammo)
					{
						break;
					}
					canFire--;
					fire -= ONE_16;
					s32 superChargeFrame = s_superCharge ? 0 : 1;
					// This is always true if super charge is *not* active otherwise
					// it is true every other frame.
					if (superChargeFrame | (s_fireFrame & 1))
					{
						*s_curPlayerWeapon->ammo = pickup_addToValue(s_playerInfo.ammoPower, -3, 500);
					}
					
					fixed16_16 yPos = s_playerObject->posWS.y - s_playerObject->worldHeight + s_headwaveVerticalOffset;
					ProjectileLogic* proj[3];
					for (s32 i = 0; i < 3; i++)
					{
						proj[i] = (ProjectileLogic*)createProjectile(PROJ_REPEATER, s_playerObject->sector, s_playerObject->posWS.x, yPos, s_playerObject->posWS.z, s_playerObject);
						proj[i]->flags &= ~PROJFLAG_CAMERA_PASS_SOUND;
						proj_setTransform(proj[i], s_weaponFirePitch + c_repeaterPitchOffset[i], s_weaponFireYaw + c_repeaterYawOffset[i]);
						proj[i]->prevColObj = s_playerObject;
					}

					for (s32 i = 0; i < 3; i++)
					{
						vec3_fixed inVec = { c_repeaterDelta[1 + i*3], c_repeaterDelta[2 + i*3], c_repeaterDelta[i*3] };
						vec3_fixed outVec;
						rotateVectorM3x3(&inVec, &outVec, mtx);

						if (canFire)
						{
							// TODO
							assert(0);
						}
						else
						{
							proj[i]->delta = outVec;
							tfe_adjustWeaponCollisionSpeed(proj[i]);

							// Initial movement to make sure the player isn't too close to a wall or adjoin.
							ProjectileHitType hitType = proj_handleMovement(proj[i]);
							handleProjectileHit(proj[i], hitType);
							tfe_restoreWeaponCollisionSpeed(proj[i]);
						}
					}
				}
				task_localBlockEnd;

				// Animation
				for (taskCtx->iFrame = 2; taskCtx->iFrame < TFE_ARRAYSIZE(c_repeaterAnim); taskCtx->iFrame++)
				{
					s_curPlayerWeapon->frame = c_repeaterAnim[taskCtx->iFrame].waxFrame;
					s_weaponLight = c_repeaterAnim[taskCtx->iFrame].weaponLight;
					taskCtx->delay = (s_superCharge) ? c_repeaterAnim[taskCtx->iFrame].delaySupercharge : c_repeaterAnim[taskCtx->iFrame].delayNormal;
					do
					{
						task_yield(taskCtx->delay);
						task_callTaskFunc(weapon_handleState);
					} while (msg != MSG_RUN_TASK);
				}
				s_canFireWeaponSec = 1;
			}
			else
			{
				if (s_repeaterFireSndID)
				{
					sound_stop(s_repeaterFireSndID);
					s_repeaterFireSndID = 0;
				}
				if (s_repeaterOutOfAmmoSndId)
				{
					sound_stop(s_repeaterOutOfAmmoSndId);
				}
				s_repeaterOutOfAmmoSndId = sound_play(s_repeaterEmptySndSrc);

				taskCtx->delay = (s_superCharge) ? 3 : 7;
				s_curPlayerWeapon->frame = 0;
				do
				{
					task_yield(taskCtx->delay);
					task_callTaskFunc(weapon_handleState);
				} while (msg != MSG_RUN_TASK);

				s_canFireWeaponPrim = 0;
				s_canFireWeaponSec = 0;

				/* TODO:
				if (s_weaponAutoMount2)
				{
					func_1ece78();
				}*/
			}
		}
		else  // Primary Fire.
		{
			if (*s_curPlayerWeapon->ammo)
			{
				if (!s_repeaterFireSndID)
				{
					if (s_repeaterFireSndID1)
					{
						sound_stop(s_repeaterFireSndID1);
					}
					s_repeaterFireSndID1 = sound_play(s_repeater1SndSrc);
					s_repeaterFireSndID  = sound_play(s_repeaterSndSrc);
				}

				if (s_curPlayerWeapon->wakeupRange)
				{
					vec3_fixed origin = { s_playerObject->posWS.x, s_playerObject->posWS.y, s_playerObject->posWS.z };
					collision_effectObjectsInRangeXZ(s_playerObject->sector, s_curPlayerWeapon->wakeupRange, origin, hitEffectWakeupFunc, s_playerObject, ETFLAG_AI_ACTOR);
				}

				taskCtx->delay = s_superCharge ? c_repeaterAnim[0].delaySupercharge : c_repeaterAnim[0].delayNormal;
				s_curPlayerWeapon->frame = c_repeaterAnim[0].waxFrame;
				do
				{
					task_yield(taskCtx->delay);
					task_callTaskFunc(weapon_handleState);
				} while (msg != MSG_RUN_TASK);

				task_localBlockBegin;
				fixed16_16 mtx[9];
				weapon_computeMatrix(mtx, -s_playerObject->pitch, -s_playerObject->yaw);

				vec3_fixed inVec =
				{
					0xcccc,	// 0.8
					0xfae1, // 0.98
					0x50000 // 5.0
				};
				vec3_fixed outVec;
				rotateVectorM3x3(&inVec, &outVec, mtx);

				fixed16_16 xPos = s_playerObject->posWS.x + outVec.x;
				fixed16_16 yPos = outVec.y + s_playerObject->posWS.y - s_playerObject->worldHeight + s_headwaveVerticalOffset;
				fixed16_16 zPos = s_playerObject->posWS.z + outVec.z;

				s32 baseVariation = s_curPlayerWeapon->variation & 0xffff;
				JBool targetFound = computeAutoaim(xPos, yPos, zPos, s_playerObject->pitch, s_playerObject->yaw, baseVariation);
				if (!targetFound)
				{
					s32 variation = random(baseVariation * 2) - baseVariation;
					s_weaponFirePitch = s_playerObject->pitch + variation;

					variation = random(variation * 2) - variation;
					s_weaponFireYaw = s_playerObject->yaw + variation;
				}

				s32 canFire = s_canFireWeaponPrim;
				fixed16_16 mtx2[9];
				s_canFireWeaponPrim = 0;
				if (canFire > 1)
				{
					weapon_computeMatrix(mtx2, -s_weaponFirePitch, -s_weaponFireYaw);
				}

				fixed16_16 fire = intToFixed16(canFire);
				while (canFire)
				{
					if (!*s_curPlayerWeapon->ammo)
					{
						break;
					}
					canFire--;
					fire -= ONE_16;
					s32 superChargeFrame = s_superCharge ? 0 : 1;
					// This is always true if super charge is *not* active otherwise
					// it is true every other frame.
					if (superChargeFrame | (s_fireFrame & 1))
					{
						*s_curPlayerWeapon->ammo = pickup_addToValue(s_playerInfo.ammoPower, -1, 500);
					}

					fixed16_16 yPos = s_playerObject->posWS.y - s_playerObject->worldHeight + s_headwaveVerticalOffset;
					ProjectileLogic* projLogic = (ProjectileLogic*)createProjectile(PROJ_REPEATER, s_playerObject->sector, s_playerObject->posWS.x, yPos, s_playerObject->posWS.z, s_playerObject);
					projLogic->flags &= ~PROJFLAG_CAMERA_PASS_SOUND;
					projLogic->prevColObj = s_playerObject;
					if (targetFound)
					{
						proj_setYawPitch(projLogic, s_wpnPitchSin, s_wpnPitchCos, s_autoAimDirX, s_autoAimDirZ);

						SecObject* projObj = projLogic->logic.obj;
						projObj->pitch = s_weaponFirePitch;
						projObj->yaw = s_weaponFireYaw;
					}
					else
					{
						proj_setTransform(projLogic, s_weaponFirePitch, s_weaponFireYaw);
					}

					if (canFire)
					{
						// TODO
						assert(0);
					}
					else
					{
						projLogic->delta = outVec;
						tfe_adjustWeaponCollisionSpeed(projLogic);

						// Initial movement to make sure the player isn't too close to a wall or adjoin.
						ProjectileHitType hitType = proj_handleMovement(projLogic);
						handleProjectileHit(projLogic, hitType);
						tfe_restoreWeaponCollisionSpeed(projLogic);
					}
				}
				task_localBlockEnd;

				taskCtx->delay = s_superCharge ? c_repeaterAnim[2].delaySupercharge : c_repeaterAnim[2].delayNormal;
				s_weaponLight = c_repeaterAnim[2].weaponLight;
				s_curPlayerWeapon->frame = c_repeaterAnim[2].waxFrame;
				do
				{
					task_yield(taskCtx->delay);
					task_callTaskFunc(weapon_handleState);
				} while (msg != MSG_RUN_TASK);

				s_weaponLight = 0;

				// The repeater has an extra delay before it can be fired again.
				if (s_isShooting)
				{
					taskCtx->delay = (s_superCharge) ? 5 : 10;
					do
					{
						task_yield(taskCtx->delay);
						task_callTaskFunc(weapon_handleState);
					} while (msg != MSG_RUN_TASK);
				}

				s_canFireWeaponPrim = 1;
			}
			else
			{
				if (s_repeaterFireSndID)
				{
					sound_stop(s_repeaterFireSndID);
					s_repeaterFireSndID = 0;
				}
				if (s_repeaterOutOfAmmoSndId)
				{
					sound_stop(s_repeaterOutOfAmmoSndId);
				}
				s_repeaterOutOfAmmoSndId = sound_play(s_repeaterEmptySndSrc);

				taskCtx->delay = (s_superCharge) ? 3 : 7;
				s_curPlayerWeapon->frame = 0;
				do
				{
					task_yield(taskCtx->delay);
					task_callTaskFunc(weapon_handleState);
				} while (msg != MSG_RUN_TASK);

				s_canFireWeaponPrim = 0;
				s_canFireWeaponSec = 0;

				/* TODO:
				if (s_weaponAutoMount2)
				{
					func_1ece78();
				}*/
			}
		}

		task_end;
	}

	void weaponFire_fusion(MessageType msg)
	{
		struct LocalContext
		{
			Tick delay;
			s32  iFrame;
		};
		task_begin_ctx;

		if (s_secondaryFire)
		{
			if (*s_curPlayerWeapon->ammo)
			{
				if (s_fusionFireSndID)
				{
					sound_stop(s_fusionFireSndID);
				}
				s_fusionFireSndID = sound_play(s_fusion1SndSrc);

				if (s_curPlayerWeapon->wakeupRange)
				{
					vec3_fixed origin = { s_playerObject->posWS.x, s_playerObject->posWS.y, s_playerObject->posWS.z };
					collision_effectObjectsInRangeXZ(s_playerObject->sector, s_curPlayerWeapon->wakeupRange, origin, hitEffectWakeupFunc, s_playerObject, ETFLAG_AI_ACTOR);
				}

				task_localBlockBegin;
				fixed16_16 mtx[9];
				weapon_computeMatrix(mtx, -s_playerObject->pitch, -s_playerObject->yaw);

				s32 baseVariation = s_curPlayerWeapon->variation & 0xffff;
				s32 variation = random(baseVariation * 2) - baseVariation;
				s_weaponFirePitch = s_playerObject->pitch + variation;

				variation = random(variation * 2) - variation;
				s_weaponFireYaw = s_playerObject->yaw + variation;

				s32 canFire = s_canFireWeaponSec;
				s_canFireWeaponSec = 0;

				fixed16_16 mtx2[9];
				if (canFire > 1)
				{
					weapon_computeMatrix(mtx2, -s_weaponFirePitch, -s_weaponFireYaw);
				}

				fixed16_16 fire = intToFixed16(canFire);
				while (canFire)
				{
					if (!*s_curPlayerWeapon->ammo)
					{
						break;
					}
					canFire--;
					fire -= ONE_16;
					s32 superChargeFrame = s_superCharge ? 0 : 1;
					// This is always true if super charge is *not* active otherwise
					// it is true every other frame.
					if (superChargeFrame | (s_fireFrame & 1))
					{
						*s_curPlayerWeapon->ammo = pickup_addToValue(s_playerInfo.ammoPower, -8, 500);
					}

					fixed16_16 yPos = s_playerObject->posWS.y - s_playerObject->worldHeight + s_headwaveVerticalOffset;
					ProjectileLogic* proj[4];
					for (s32 i = 0; i < 4; i++)
					{
						proj[i] = (ProjectileLogic*)createProjectile(PROJ_PLASMA, s_playerObject->sector, s_playerObject->posWS.x, yPos, s_playerObject->posWS.z, s_playerObject);
						proj[i]->flags &= ~PROJFLAG_CAMERA_PASS_SOUND;
						proj_setTransform(proj[i], s_weaponFirePitch, s_weaponFireYaw + s_fusionYawOffset[i]);
						proj[i]->prevColObj = s_playerObject;
					}

					for (s32 i = 0; i < 4; i++)
					{
						vec3_fixed inVec = { s_fusionOffset[1 + i*3], s_fusionOffset[2 + i*3], s_fusionOffset[i*3] };
						vec3_fixed outVec;
						rotateVectorM3x3(&inVec, &outVec, mtx);

						if (canFire)
						{
							// TODO
							assert(0);
						}
						else
						{
							proj[i]->delta = outVec;
							tfe_adjustWeaponCollisionSpeed(proj[i]);

							// Initial movement to make sure the player isn't too close to a wall or adjoin.
							ProjectileHitType hitType = proj_handleMovement(proj[i]);
							handleProjectileHit(proj[i], hitType);
							tfe_restoreWeaponCollisionSpeed(proj[i]);
						}
					}
				}
				task_localBlockEnd;

				// Animation
				// TODO: not using the tables for this due to differences - refactor later.
				s_curPlayerWeapon->frame = 5;
				s_weaponLight = 34;
				taskCtx->delay = (s_superCharge) ? 14 : 29;
				do
				{
					task_yield(taskCtx->delay);
					task_callTaskFunc(weapon_handleState);
				} while (msg != MSG_RUN_TASK);

				s_curPlayerWeapon->frame = 0;
				s_weaponLight = 0;
				taskCtx->delay = (s_superCharge) ? 19 : 39;
				do
				{
					task_yield(taskCtx->delay);
					task_callTaskFunc(weapon_handleState);
				} while (msg != MSG_RUN_TASK);

				s_canFireWeaponSec = 1;
			}
			else
			{
				if (s_fusionOutOfAmmoSndID)
				{
					sound_stop(s_fusionOutOfAmmoSndID);
				}
				s_fusionOutOfAmmoSndID = sound_play(s_fusion2SndSrc);

				taskCtx->delay = (s_superCharge) ? 3 : 7;
				s_curPlayerWeapon->frame = 0;
				do
				{
					task_yield(taskCtx->delay);
					task_callTaskFunc(weapon_handleState);
				} while (msg != MSG_RUN_TASK);

				s_canFireWeaponPrim = 0;
				s_canFireWeaponSec = 0;

				/* TODO:
				if (s_weaponAutoMount2)
				{
					func_1ece78();
				}*/
			}
		}
		else  // Primary Fire.
		{
			if (*s_curPlayerWeapon->ammo)
			{
				if (s_fusionFireSndID)
				{
					sound_stop(s_fusionFireSndID);
				}
				s_fusionFireSndID = sound_play(s_fusion1SndSrc);

				if (s_curPlayerWeapon->wakeupRange)
				{
					vec3_fixed origin = { s_playerObject->posWS.x, s_playerObject->posWS.y, s_playerObject->posWS.z };
					collision_effectObjectsInRangeXZ(s_playerObject->sector, s_curPlayerWeapon->wakeupRange, origin, hitEffectWakeupFunc, s_playerObject, ETFLAG_AI_ACTOR);
				}

				task_localBlockBegin;
				fixed16_16 mtx[9];
				weapon_computeMatrix(mtx, -s_playerObject->pitch, -s_playerObject->yaw);

				s32 canFire = s_canFireWeaponPrim;
				fixed16_16 mtx2[9];
				s_canFireWeaponPrim = 0;
				if (canFire > 1)
				{
					weapon_computeMatrix(mtx2, -s_weaponFirePitch, -s_weaponFireYaw);
				}

				fixed16_16 fire = intToFixed16(canFire);
				while (canFire)
				{
					if (!*s_curPlayerWeapon->ammo)
					{
						break;
					}
					canFire--;
					fire -= ONE_16;

					vec3_fixed inVec =
					{
						s_fusionOffset[(s_fusionCylinder-1)*3 + 1],
						s_fusionOffset[(s_fusionCylinder-1)*3 + 2],
						s_fusionOffset[(s_fusionCylinder-1)*3 + 0]
					};
					vec3_fixed outVec;
					rotateVectorM3x3(&inVec, &outVec, mtx);

					fixed16_16 xPos = s_playerObject->posWS.x + outVec.x;
					fixed16_16 yPos = outVec.y + s_playerObject->posWS.y - s_playerObject->worldHeight + s_headwaveVerticalOffset;
					fixed16_16 zPos = s_playerObject->posWS.z + outVec.z;

					s32 baseVariation = s_curPlayerWeapon->variation & 0xffff;
					JBool targetFound = computeAutoaim(xPos, yPos, zPos, s_playerObject->pitch, s_playerObject->yaw, baseVariation);
					if (!targetFound)
					{
						s32 variation = random(baseVariation * 2) - baseVariation;
						s_weaponFirePitch = s_playerObject->pitch + variation;

						variation = random(baseVariation * 2) - baseVariation;
						s_weaponFireYaw = s_playerObject->yaw + variation;
					}

					s32 superChargeFrame = s_superCharge ? 0 : 1;
					// This is always true if super charge is *not* active otherwise
					// it is true every other frame.
					if (superChargeFrame | (s_fireFrame & 1))
					{
						*s_curPlayerWeapon->ammo = pickup_addToValue(s_playerInfo.ammoPower, -1, 500);
					}

					fixed16_16 yPlayerPos = s_playerObject->posWS.y - s_playerObject->worldHeight + s_headwaveVerticalOffset;
					ProjectileLogic* projLogic = (ProjectileLogic*)createProjectile(PROJ_PLASMA, s_playerObject->sector, s_playerObject->posWS.x, yPlayerPos, s_playerObject->posWS.z, s_playerObject);
					projLogic->flags &= ~PROJFLAG_CAMERA_PASS_SOUND;
					projLogic->prevColObj = s_playerObject;
					if (targetFound)
					{
						proj_setYawPitch(projLogic, s_wpnPitchSin, s_wpnPitchCos, s_autoAimDirX, s_autoAimDirZ);

						SecObject* projObj = projLogic->logic.obj;
						projObj->pitch = s_weaponFirePitch;
						projObj->yaw = s_weaponFireYaw;
					}
					else
					{
						proj_setTransform(projLogic, s_weaponFirePitch, s_weaponFireYaw);
					}

					if (canFire)
					{
						// TODO
						assert(0);
					}
					else
					{
						projLogic->delta = outVec;
						tfe_adjustWeaponCollisionSpeed(projLogic);

						// Initial movement to make sure the player isn't too close to a wall or adjoin.
						ProjectileHitType hitType = proj_handleMovement(projLogic);
						handleProjectileHit(projLogic, hitType);
						tfe_restoreWeaponCollisionSpeed(projLogic);
					}
				}
				task_localBlockEnd;

				taskCtx->delay = s_superCharge ? 10 : 20;
				s_weaponLight = 34;
				s_curPlayerWeapon->frame = s_fusionCylinder;
				do
				{
					task_yield(taskCtx->delay);
					task_callTaskFunc(weapon_handleState);
				} while (msg != MSG_RUN_TASK);

				if (s_fusionCycleForward)
				{
					s_fusionCylinder++;
					if (s_fusionCylinder > 4)
					{
						s_fusionCylinder = 3;
						s_fusionCycleForward = JFALSE;
					}
				}
				else
				{
					s_fusionCylinder--;
					if (s_fusionCylinder < 1)
					{
						s_fusionCylinder = 2;
						s_fusionCycleForward = JTRUE;
					}
				}

				taskCtx->delay = s_superCharge ? 7 : 14;
				s_weaponLight = 0;
				s_curPlayerWeapon->frame = 0;
				do
				{
					task_yield(taskCtx->delay);
					task_callTaskFunc(weapon_handleState);
				} while (msg != MSG_RUN_TASK);

				s_canFireWeaponPrim = 1;
			}
			else
			{
				if (s_fusionOutOfAmmoSndID)
				{
					sound_stop(s_fusionOutOfAmmoSndID);
				}
				s_fusionOutOfAmmoSndID = sound_play(s_fusion2SndSrc);

				taskCtx->delay = (s_superCharge) ? 3 : 7;
				s_curPlayerWeapon->frame = 0;
				do
				{
					task_yield(taskCtx->delay);
					task_callTaskFunc(weapon_handleState);
				} while (msg != MSG_RUN_TASK);

				s_canFireWeaponPrim = 0;
				s_canFireWeaponSec = 0;

				/* TODO:
				if (s_weaponAutoMount2)
				{
					func_1ece78();
				}*/
			}
		}

		task_end;
	}

	void weaponFire_mortar(MessageType msg)
	{
		struct LocalContext
		{
			Tick delay;
			s32  iFrame;
		};
		task_begin_ctx;

		if (*s_curPlayerWeapon->ammo)
		{
			task_localBlockBegin;
			if (s_mortarFireSndID)
			{
				sound_stop(s_mortarFireSndID);
			}
			s_mortarFireSndID = sound_play(s_mortarFireSndSrc);

			if (s_curPlayerWeapon->wakeupRange)
			{
				vec3_fixed origin = { s_playerObject->posWS.x, s_playerObject->posWS.y, s_playerObject->posWS.z };
				collision_effectObjectsInRangeXZ(s_playerObject->sector, s_curPlayerWeapon->wakeupRange, origin, hitEffectWakeupFunc, s_playerObject, ETFLAG_AI_ACTOR);
			}

			fixed16_16 mtx[9];
			weapon_computeMatrix(mtx, -s_playerObject->pitch, -s_playerObject->yaw);

			s32 canFire = s_canFireWeaponPrim;
			s_canFireWeaponPrim = 0;

			s32 wpnVariation = s_curPlayerWeapon->variation & 0xffff;
			s32 variation;
			if (canFire > 1)
			{
				variation = random(wpnVariation * 2) - wpnVariation + 0x333;
				s_weaponFirePitch = s_playerObject->pitch + (variation >> 2);
			}
			else
			{
				variation = random(wpnVariation * 2) - wpnVariation + 0x333;
				s_weaponFirePitch = s_playerObject->pitch + variation;
			}

			variation = random(wpnVariation * 2) - wpnVariation;
			s_weaponFireYaw = s_playerObject->yaw + variation;

			vec3_fixed inVec =
			{
				0xb333,	 // 0.7
				0x1b333, // 1.7
				0x20000  // 2.0
			};
			vec3_fixed outVec;
			rotateVectorM3x3(&inVec, &outVec, mtx);

			fixed16_16 mtx2[9];
			if (canFire > 1)
			{
				weapon_computeMatrix(mtx2, -s_weaponFirePitch, -s_weaponFireYaw);
			}

			// Spawn projectiles.
			fixed16_16 fire = intToFixed16(canFire);
			while (canFire)
			{
				if (*s_curPlayerWeapon->ammo == 0)
				{
					break;
				}
				fire -= ONE_16;
				canFire--;
				s32 superChargeFrame = s_superCharge ? 0 : 1;
				// This is always true if super charge is *not* active otherwise
				// it is true every other frame.
				if (superChargeFrame | (s_fireFrame & 1))
				{
					*s_curPlayerWeapon->ammo = pickup_addToValue(s_playerInfo.ammoShell, -1, 50);
				}

				ProjectileLogic* projLogic;
				fixed16_16 yPos = s_playerObject->posWS.y - s_playerObject->worldHeight + s_headwaveVerticalOffset;
				projLogic = (ProjectileLogic*)createProjectile(PROJ_MORTAR, s_playerObject->sector, s_playerObject->posWS.x, yPos, s_playerObject->posWS.z, s_playerObject);
				projLogic->flags &= ~PROJFLAG_CAMERA_PASS_SOUND;
				projLogic->prevColObj = s_playerObject;
				proj_setTransform(projLogic, s_weaponFirePitch, s_weaponFireYaw);
				
				if (canFire)
				{
					vec3_fixed worldUp = { 0 };
					if (canFire > 1)
					{
						// TODO: This doesn't seem to be hit in the base game.
						assert(0);
					}

					Tick v0 = (s_superCharge) ?  7 : 14;
					Tick v1 = (s_superCharge) ? 29 : 58;
					Tick v2 = (s_superCharge) ?  7 : 14;
					Tick v3 = (s_superCharge) ?  7 : 14;
					worldUp.z = intToFixed16(s32(float(v0 + v1 + v2 + v3) * 0.75f));
					
					vec3_fixed outVec2;
					rotateVectorM3x3(&worldUp, &outVec2, mtx2);
					// First try using the outVec value computed above.
					projLogic->delta = outVec;
					tfe_adjustWeaponCollisionSpeed(projLogic);

					ProjectileHitType hitType = proj_handleMovement(projLogic);
					if (!handleProjectileHit(projLogic, hitType))
					{
						// If that fails, then try the new value.
						projLogic->delta = outVec2;
						tfe_adjustWeaponCollisionSpeed(projLogic);

						hitType = proj_handleMovement(projLogic);
						fire = handleProjectileHit(projLogic, hitType);
					}
				}
				else
				{
					projLogic->delta = outVec;
					tfe_adjustWeaponCollisionSpeed(projLogic);

					ProjectileHitType hitType = proj_handleMovement(projLogic);
					handleProjectileHit(projLogic, hitType);
				}
				tfe_restoreWeaponCollisionSpeed(projLogic);
			}
			task_localBlockEnd;

			// Animation.
			for (taskCtx->iFrame = 0; taskCtx->iFrame < TFE_ARRAYSIZE(c_mortarAnim); taskCtx->iFrame++)
			{
				if (taskCtx->iFrame == 1)
				{
					if (s_mortarFireSndID2)
					{
						sound_stop(s_mortarFireSndID2);
					}
					s_mortarFireSndID2 = sound_play(s_mortarFireSndSrc2);
				}

				s_curPlayerWeapon->frame = c_mortarAnim[taskCtx->iFrame].waxFrame;
				s_weaponLight = c_mortarAnim[taskCtx->iFrame].weaponLight;
				taskCtx->delay = (s_superCharge) ? c_mortarAnim[taskCtx->iFrame].delaySupercharge : c_mortarAnim[taskCtx->iFrame].delayNormal;

				do
				{
					task_yield(taskCtx->delay);
					task_callTaskFunc(weapon_handleState);
				} while (msg != MSG_RUN_TASK);
			}

			s_canFireWeaponPrim = 1;
			s_canFireWeaponSec = 1;
		}
		else  // Out of Ammo
		{
			if (s_mortarOutofAmmoSndId)
			{
				sound_stop(s_mortarOutofAmmoSndId);
			}
			s_mortarOutofAmmoSndId = sound_play(s_mortarEmptySndSrc);

			taskCtx->delay = (s_superCharge) ? 3 : 7;
			s_curPlayerWeapon->frame = 0;
			do
			{
				task_yield(taskCtx->delay);
				task_callTaskFunc(weapon_handleState);
			} while (msg != MSG_RUN_TASK);

			s_canFireWeaponPrim = 0;
			s_canFireWeaponSec = 0;
			// if (s_weaponAutoMount2)
			// {
				// func_1ece78();
			// }
		}

		task_end;
	}

	void weaponFire_mine(MessageType msg)
	{
		task_begin;
		if (*s_curPlayerWeapon->ammo)
		{
			*s_curPlayerWeapon->ammo = pickup_addToValue(s_playerInfo.ammoMine, -1, 30);
			if (s_curPlayerWeapon->wakeupRange)
			{
				vec3_fixed origin = { s_playerObject->posWS.x, s_playerObject->posWS.y, s_playerObject->posWS.z };
				collision_effectObjectsInRangeXZ(s_playerObject->sector, s_curPlayerWeapon->wakeupRange, origin, hitEffectWakeupFunc, s_playerObject, ETFLAG_AI_ACTOR);
			}

			s_weaponAnimState =
			{
				1,		// frame
				0, 0,	// offset
				0, 20,	// xSpeed, ySpeed
				5, 7	// frameCount, ticksPerFrame
			};
			task_callTaskFunc(weapon_animateOnOrOffscreen);

			task_localBlockBegin;
			fixed16_16 floorHeight, ceilHeight;
			sector_getObjFloorAndCeilHeight(s_playerObject->sector, s_playerObject->posWS.y, &floorHeight, &ceilHeight);

			ProjectileType type = (s_secondaryFire) ? PROJ_LAND_MINE_PROX : PROJ_LAND_MINE;
			ProjectileLogic* mine = (ProjectileLogic*)createProjectile(type, s_playerObject->sector, s_playerObject->posWS.x, floorHeight, s_playerObject->posWS.z, s_playerObject);
			mine->vel = { 0, 0, 0 };

			if (s_mineSndId)
			{
				sound_stop(s_mineSndId);
			}
			s_mineSndId = sound_play(s_mineSndSrc);

			s32 frame = (*s_curPlayerWeapon->ammo) ? 0 : 2;
			s_weaponAnimState =
			{
				frame,	// frame
				0, 100,	// offset
				0, -20,	// xSpeed, ySpeed
				5, 7	// frameCount, ticksPerFrame
			};
			task_localBlockEnd;
			task_callTaskFunc(weapon_animateOnOrOffscreen);

			task_yield(2);
		}
		else
		{
			if (s_weaponAutoMount2)
			{
				// func_1ece78();
				// return;
			}

			s_lastWeapon = s_curWeapon;
			s_curWeapon = WPN_FIST;
			s_playerInfo.curWeapon = s_prevWeapon;
			if (s_weaponOffAnim)
			{
				task_callTaskFunc(weapon_handleOffAnimation);
			}
			weapon_setNext(s_curWeapon);
			task_callTaskFunc(weapon_handleOnAnimation);
		}
		task_end;
	}

	void weaponFire_concussion(MessageType msg)
	{
		struct LocalContext
		{
			Tick delay;
			s32  iFrame;
		};
		task_begin_ctx;

		if (*s_curPlayerWeapon->ammo)
		{
			task_localBlockBegin;
			if (s_concussionFireSndID)
			{
				sound_stop(s_concussionFireSndID);
			}
			s_concussionFireSndID = sound_play(s_concussion6SndSrc);

			if (s_curPlayerWeapon->wakeupRange)
			{
				vec3_fixed origin = { s_playerObject->posWS.x, s_playerObject->posWS.y, s_playerObject->posWS.z };
				collision_effectObjectsInRangeXZ(s_playerObject->sector, s_curPlayerWeapon->wakeupRange, origin, hitEffectWakeupFunc, s_playerObject, ETFLAG_AI_ACTOR);
			}

			fixed16_16 mtx[9];
			weapon_computeMatrix(mtx, -s_playerObject->pitch, -s_playerObject->yaw);

			vec3_fixed inVec =
			{
				0x10000, // 1.0
				0x8000,  // 0.5
				0x30000  // 3.0
			};
			vec3_fixed outVec;
			rotateVectorM3x3(&inVec, &outVec, mtx);

			fixed16_16 xPos = s_playerObject->posWS.x + outVec.x;
			fixed16_16 yPos = s_playerObject->posWS.y + outVec.y - s_playerObject->worldHeight + s_headwaveVerticalOffset;
			fixed16_16 zPos = s_playerObject->posWS.z + outVec.z;

			s32 baseVariation = s_curPlayerWeapon->variation & 0xffff;
			JBool targetFound = computeAutoaim(xPos, yPos, zPos, s_playerObject->pitch, s_playerObject->yaw, s_curPlayerWeapon->variation & 0xffff);
			if (!targetFound)
			{
				s32 variation = random(baseVariation * 2) - baseVariation;
				s_weaponFirePitch = s_playerObject->pitch + variation;

				variation = random(baseVariation * 2) - baseVariation;
				s_weaponFireYaw = s_playerObject->yaw + variation;
			}

			s32 canFire = s_canFireWeaponPrim;
			fixed16_16 mtx2[9];
			s_canFireWeaponPrim = 0;
			if (canFire > 1)
			{
				weapon_computeMatrix(mtx2, -s_weaponFirePitch, -s_weaponFireYaw);
			}

			// Spawn projectiles.
			fixed16_16 fire = intToFixed16(canFire);
			while (canFire)
			{
				if (*s_curPlayerWeapon->ammo == 0)
				{
					break;
				}
				fire -= ONE_16;
				canFire--;
				s32 superChargeFrame = s_superCharge ? 0 : 1;
				// This is always true if super charge is *not* active otherwise
				// it is true every other frame.
				if (superChargeFrame | (s_fireFrame & 1))
				{
					*s_curPlayerWeapon->ammo = pickup_addToValue(s_playerInfo.ammoPower, -4, 500);
				}

				ProjectileLogic* projLogic;
				fixed16_16 yPos = s_playerObject->posWS.y - s_playerObject->worldHeight + s_headwaveVerticalOffset;
				projLogic = (ProjectileLogic*)createProjectile(PROJ_CONCUSSION, s_playerObject->sector, s_playerObject->posWS.x, yPos, s_playerObject->posWS.z, s_playerObject);
				projLogic->flags &= ~PROJFLAG_CAMERA_PASS_SOUND;
				projLogic->prevColObj = s_playerObject;

				if (targetFound)
				{
					proj_setYawPitch(projLogic, s_wpnPitchSin, s_wpnPitchCos, s_autoAimDirX, s_autoAimDirZ);

					SecObject* projObj = projLogic->logic.obj;
					projObj->pitch = s_weaponFirePitch;
					projObj->yaw = s_weaponFireYaw;
				}
				else
				{
					proj_setTransform(projLogic, s_weaponFirePitch, s_weaponFireYaw);
				}

				if (canFire)
				{
					assert(0);
					// TODO
				}
				else
				{
					projLogic->delta = outVec;
					tfe_adjustWeaponCollisionSpeed(projLogic);

					ProjectileHitType hitType = proj_handleMovement(projLogic);
					handleProjectileHit(projLogic, hitType);

					tfe_restoreWeaponCollisionSpeed(projLogic);
				}
			}
			task_localBlockEnd;

			// Animation.
			if (s_concussionFireSndID1)
			{
				sound_stop(s_concussionFireSndID1);
			}
			s_concussionFireSndID1 = sound_play(s_concussion5SndSrc);

			for (taskCtx->iFrame = 0; taskCtx->iFrame < TFE_ARRAYSIZE(c_concussionAnim); taskCtx->iFrame++)
			{
				s_curPlayerWeapon->frame = c_concussionAnim[taskCtx->iFrame].waxFrame;
				s_weaponLight = c_concussionAnim[taskCtx->iFrame].weaponLight;
				taskCtx->delay = (s_superCharge) ? c_concussionAnim[taskCtx->iFrame].delaySupercharge : c_concussionAnim[taskCtx->iFrame].delayNormal;

				do
				{
					task_yield(taskCtx->delay);
					task_callTaskFunc(weapon_handleState);
				} while (msg != MSG_RUN_TASK);
			}

			s_canFireWeaponPrim = 1;
			s_canFireWeaponSec = 1;
		}
		else  // Out of Ammo
		{
			if (s_concussionOutOfAmmoSndID)
			{
				sound_stop(s_concussionOutOfAmmoSndID);
			}
			s_concussionOutOfAmmoSndID = sound_play(s_concussion1SndSrc);

			taskCtx->delay = (s_superCharge) ? 3 : 7;
			s_curPlayerWeapon->frame = 0;
			do
			{
				task_yield(taskCtx->delay);
				task_callTaskFunc(weapon_handleState);
			} while (msg != MSG_RUN_TASK);

			s_canFireWeaponPrim = 0;
			s_canFireWeaponSec = 0;
			// if (s_weaponAutoMount2)
			// {
				// func_1ece78();
			// }
		}

		task_end;
	}

	void weaponFire_cannon(MessageType msg)
	{
		struct LocalContext
		{
			Tick delay;
			s32  iFrame;
		};
		task_begin_ctx;

		if (s_secondaryFire)
		{
			if (*s_curPlayerWeapon->secondaryAmmo)
			{
				taskCtx->delay = (s_superCharge) ? 14 : 29;
				s_curPlayerWeapon->frame = 2;
				do
				{
					task_yield(taskCtx->delay);
					task_callTaskFunc(weapon_handleState);
				} while (msg != MSG_RUN_TASK);

				if (s_cannonFireSndID1)
				{
					sound_stop(s_cannonFireSndID1);
				}
				s_cannonFireSndID1 = sound_play(s_missile1SndSrc);

				if (s_curPlayerWeapon->wakeupRange)
				{
					vec3_fixed origin = { s_playerObject->posWS.x, s_playerObject->posWS.y, s_playerObject->posWS.z };
					collision_effectObjectsInRangeXZ(s_playerObject->sector, s_curPlayerWeapon->wakeupRange, origin, hitEffectWakeupFunc, s_playerObject, ETFLAG_AI_ACTOR);
				}

				task_localBlockBegin;
				fixed16_16 mtx[9];
				weapon_computeMatrix(mtx, -s_playerObject->pitch, -s_playerObject->yaw);

				vec3_fixed outVec;
				vec3_fixed inVec =
				{
					0x20000,	// 2.0
					0x4000,		// 0.25
					0x40000,	// 4.0
				};
				rotateVectorM3x3(&inVec, &outVec, mtx);

				fixed16_16 xPos = s_playerObject->posWS.x + outVec.x;
				fixed16_16 yPos = s_playerObject->posWS.y + outVec.y - s_playerObject->worldHeight + s_headwaveVerticalOffset;
				fixed16_16 zPos = s_playerObject->posWS.z + outVec.z;

				s32 baseVariation = s_curPlayerWeapon->variation & 0xffff;
				JBool targetFound = computeAutoaim(xPos, yPos, zPos, s_playerObject->pitch, s_playerObject->yaw, baseVariation);
				if (!targetFound)
				{
					s32 variation = random(baseVariation * 2) - baseVariation;
					s_weaponFirePitch = s_playerObject->pitch + variation;

					variation = random(baseVariation * 2) - baseVariation;
					s_weaponFireYaw = s_playerObject->yaw + variation;
				}

				s32 canFire = s_canFireWeaponSec;
				s_canFireWeaponSec = 0;

				fixed16_16 mtx2[9];
				if (canFire > 1)
				{
					weapon_computeMatrix(mtx2, -s_weaponFirePitch, -s_weaponFireYaw);
				}

				fixed16_16 fire = intToFixed16(canFire);
				while (canFire && *s_curPlayerWeapon->secondaryAmmo)
				{
					canFire--;
					fire -= ONE_16;

					s32 superChargeFrame = s_superCharge ? 0 : 1;
					if (superChargeFrame | (s_fireFrame & 1))
					{
						*s_curPlayerWeapon->secondaryAmmo = pickup_addToValue(s_playerInfo.ammoMissile, -1, 20);
					}

					fixed16_16 yPos = s_playerObject->posWS.y - s_playerObject->worldHeight + s_headwaveVerticalOffset;
					ProjectileLogic* projLogic = (ProjectileLogic*)createProjectile(PROJ_MISSILE, s_playerObject->sector, s_playerObject->posWS.x, yPos, s_playerObject->posWS.z, s_playerObject);
					projLogic->flags &= ~PROJFLAG_CAMERA_PASS_SOUND;
					projLogic->prevColObj = s_playerObject;
					if (targetFound)
					{
						proj_setYawPitch(projLogic, s_wpnPitchSin, s_wpnPitchCos, s_autoAimDirX, s_autoAimDirZ);

						SecObject* projObj = projLogic->logic.obj;
						projObj->pitch = s_weaponFirePitch;
						projObj->yaw = s_weaponFireYaw;
					}
					else
					{
						proj_setTransform(projLogic, s_weaponFirePitch, s_weaponFireYaw);
					}

					if (canFire)
					{
						// TODO
						assert(0);
					}
					else
					{
						// Initial movement and collision.
						projLogic->delta = outVec;
						tfe_adjustWeaponCollisionSpeed(projLogic);

						ProjectileHitType hit = proj_handleMovement(projLogic);
						handleProjectileHit(projLogic, hit);

						tfe_restoreWeaponCollisionSpeed(projLogic);
					}
				}
				task_localBlockEnd;

				// Animation
				s_curPlayerWeapon->frame = 3;
				s_weaponLight = 33;
				taskCtx->delay = (s_superCharge) ? 43 : 87;
				do
				{
					task_yield(taskCtx->delay);
					task_callTaskFunc(weapon_handleState);
				} while (msg != MSG_RUN_TASK);

				s_curPlayerWeapon->frame = 0;
				s_weaponLight = 0;
				taskCtx->delay = (s_superCharge) ? 29 : 58;
				do
				{
					task_yield(taskCtx->delay);
					task_callTaskFunc(weapon_handleState);
				} while (msg != MSG_RUN_TASK);

				s_canFireWeaponSec = 1;
			}
			else
			{
				s_curPlayerWeapon->frame = 0;
				taskCtx->delay = (s_superCharge) ? 3 : 7;
				do
				{
					task_yield(taskCtx->delay);
					task_callTaskFunc(weapon_handleState);
				} while (msg != MSG_RUN_TASK);

				s_canFireWeaponSec = 0;
			}
		}
		else  // Primary Fire.
		{
			if (*s_curPlayerWeapon->ammo)
			{
				if (s_cannonFireSndID)
				{
					sound_stop(s_cannonFireSndID);
				}
				s_cannonFireSndID = sound_play(s_plasma4SndSrc);

				if (s_curPlayerWeapon->wakeupRange)
				{
					vec3_fixed origin = { s_playerObject->posWS.x, s_playerObject->posWS.y, s_playerObject->posWS.z };
					collision_effectObjectsInRangeXZ(s_playerObject->sector, s_curPlayerWeapon->wakeupRange, origin, hitEffectWakeupFunc, s_playerObject, ETFLAG_AI_ACTOR);
				}

				task_localBlockBegin;
				fixed16_16 mtx[9];
				weapon_computeMatrix(mtx, -s_playerObject->pitch, -s_playerObject->yaw);

				vec3_fixed inVec =
				{
					0x10000,	// 1.0
					0x9999,		// 0.6
					0x30000		// 3.0
				};
				vec3_fixed outVec;
				rotateVectorM3x3(&inVec, &outVec, mtx);

				fixed16_16 xPos = s_playerObject->posWS.x + outVec.x;
				fixed16_16 yPos = s_playerObject->posWS.y + outVec.y - s_playerObject->worldHeight + s_headwaveVerticalOffset;
				fixed16_16 zPos = s_playerObject->posWS.z + outVec.z;

				s32 baseVariation = s_curPlayerWeapon->variation & 0xffff;
				JBool targetFound = computeAutoaim(xPos, yPos, zPos, s_playerObject->pitch, s_playerObject->yaw, baseVariation);
				if (!targetFound)
				{
					s32 variation = random(baseVariation * 2) - baseVariation;
					s_weaponFirePitch = s_playerObject->pitch + variation;

					variation = random(variation * 2) - variation;
					s_weaponFireYaw = s_playerObject->yaw + variation;
				}

				s32 canFire = s_canFireWeaponPrim;
				fixed16_16 mtx2[9];
				s_canFireWeaponPrim = 0;
				if (canFire > 1)
				{
					weapon_computeMatrix(mtx2, -s_weaponFirePitch, -s_weaponFireYaw);
				}

				fixed16_16 fire = intToFixed16(canFire);
				while (canFire && *s_curPlayerWeapon->ammo)
				{
					canFire--;
					fire -= ONE_16;

					s32 superChargeFrame = s_superCharge ? 0 : 1;
					if (superChargeFrame | (s_fireFrame & 1))
					{
						*s_curPlayerWeapon->ammo = pickup_addToValue(s_playerInfo.ammoPlasma, -1, 400);
					}

					fixed16_16 yPos = s_playerObject->posWS.y - s_playerObject->worldHeight + s_headwaveVerticalOffset;
					ProjectileLogic* projLogic = (ProjectileLogic*)createProjectile(PROJ_CANNON, s_playerObject->sector, s_playerObject->posWS.x, yPos, s_playerObject->posWS.z, s_playerObject);
					projLogic->flags &= ~PROJFLAG_CAMERA_PASS_SOUND;
					projLogic->prevColObj = s_playerObject;
					if (targetFound)
					{
						proj_setYawPitch(projLogic, s_wpnPitchSin, s_wpnPitchCos, s_autoAimDirX, s_autoAimDirZ);

						SecObject* projObj = projLogic->logic.obj;
						projObj->pitch = s_weaponFirePitch;
						projObj->yaw = s_weaponFireYaw;
					}
					else
					{
						proj_setTransform(projLogic, s_weaponFirePitch, s_weaponFireYaw);
					}

					if (canFire)
					{
						// TODO
						assert(0);
					}
					else
					{
						projLogic->delta = outVec;
						tfe_adjustWeaponCollisionSpeed(projLogic);

						// Initial movement to make sure the player isn't too close to a wall or adjoin.
						ProjectileHitType hitType = proj_handleMovement(projLogic);
						handleProjectileHit(projLogic, hitType);

						tfe_restoreWeaponCollisionSpeed(projLogic);
					}
				}
				task_localBlockEnd;

				s_curPlayerWeapon->frame = 1;
				s_weaponLight = 34;
				taskCtx->delay = ((s_superCharge) ? 14 : 29) >> 1;
				do
				{
					task_yield(taskCtx->delay);
					task_callTaskFunc(weapon_handleState);
				} while (msg != MSG_RUN_TASK);

				s_weaponLight = 0;
				if (s_isShooting)
				{
					do
					{
						task_yield(taskCtx->delay);
						task_callTaskFunc(weapon_handleState);
					} while (msg != MSG_RUN_TASK);
				}

				s_canFireWeaponPrim = 1;
			}
			else
			{
				if (s_cannonOutOfAmmoSndID)
				{
					sound_stop(s_cannonOutOfAmmoSndID);
				}
				s_cannonOutOfAmmoSndID = sound_play(s_plasmaEmptySndSrc);

				taskCtx->delay = (s_superCharge) ? 3 : 7;
				s_curPlayerWeapon->frame = 0;
				do
				{
					task_yield(taskCtx->delay);
					task_callTaskFunc(weapon_handleState);
				} while (msg != MSG_RUN_TASK);

				s_canFireWeaponPrim = 0;

				/* TODO:
				if (s_weaponAutoMount2)
				{
					func_1ece78();
				}*/
			}
		}

		task_end;
	}

	JBool computeAutoaim(fixed16_16 xPos, fixed16_16 yPos, fixed16_16 zPos, angle14_32 pitch, angle14_32 yaw, s32 variation)
	{
		if (!s_drawnSpriteCount || !TFE_Settings::getGameSettings()->df_enableAutoaim)
		{
			return JFALSE;
		}
		fixed16_16 closest = MAX_AUTOAIM_DIST;
		s32 count = s_drawnSpriteCount;
		for (s32 i = 0; i < count; i++)
		{
			SecObject* obj = s_drawnSprites[i];
			if (obj && (obj->flags & OBJ_FLAG_ENEMY))
			{
				fixed16_16 dx = obj->posWS.x - xPos;
				fixed16_16 dz = obj->posWS.z - zPos;

				fixed16_16 top = obj->posWS.y - (obj->worldHeight >> 1) - (obj->worldHeight >> 2);
				fixed16_16 dy = top - yPos;

				angle14_32 angle = vec2ToAngle(dx, dz) & ANGLE_MASK;
				angle14_32 yawDelta = (angle - yaw) & ANGLE_MASK;
				if (yawDelta > 227 && yawDelta < 16111)
				{
					continue;
				}
				fixed16_16 dist = computeDirAndLength(dx, dz, &s_autoAimDirX, &s_autoAimDirZ);
				if (dist > 0 && dist < closest)
				{
					closest = dist;
					s_weaponFireYaw = angle;
					s_weaponFirePitch = vec2ToAngle(-dy, dist) & ANGLE_MASK;
				}
			}
		}
		if (closest == MAX_AUTOAIM_DIST)
		{
			return JFALSE;
		}
		if (variation)
		{
			s32 angleVar = random(variation << 6) - (variation << 5);
			s_autoAimDirX += angleVar;
			s_autoAimDirZ += angleVar;
			s_weaponFirePitch += random(variation * 2) + (pitch >> 3) - variation;
		}
		sinCosFixed(s_weaponFirePitch, &s_wpnPitchSin, &s_wpnPitchCos);
		return JTRUE;
	}
}  // namespace TFE_DarkForces