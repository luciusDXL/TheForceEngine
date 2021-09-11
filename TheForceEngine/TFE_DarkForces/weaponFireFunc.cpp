#include "weaponFireFunc.h"
#include "weapon.h"
#include "random.h"
#include "player.h"
#include "pickup.h"
#include "projectile.h"
#include "hitEffect.h"
#include <TFE_Jedi/Collision/collision.h>
#include <TFE_Jedi/Renderer/jediRenderer.h>

namespace TFE_DarkForces
{
	enum WeaponFireConst
	{
		MAX_AUTOAIM_DIST = FIXED(9999),
	};

	static SoundEffectID s_pistolSndId = 0;
	static fixed16_16 s_autoAimDirX;
	static fixed16_16 s_autoAimDirZ;
	static fixed16_16 s_wpnPitchSin;
	static fixed16_16 s_wpnPitchCos;
	static angle14_32 s_weaponFirePitch;
	static angle14_32 s_weaponFireYaw;

	extern SoundSourceID s_punchSwingSndSrc;
	extern SoundSourceID s_pistolSndSrc;
	extern SoundSourceID s_pistolEmptySndSrc;
	extern SoundSourceID s_rifleSndSrc;
	extern SoundSourceID s_rifleEmptySndSrc;
	extern SoundSourceID s_fusion1SndSrc;
	extern SoundSourceID s_fusion2SndSrc;
	extern SoundSourceID s_repeaterSndSrc;
	extern SoundSourceID s_repeater1SndSrc;
	extern SoundSourceID s_repeaterEmptySndSrc;
	extern SoundSourceID s_mortarFireSndSrc;
	extern SoundSourceID s_mortarFireSndSrc2;
	extern SoundSourceID s_mortarEmptySndSrc;
	extern SoundSourceID s_mineSndSrc;
	extern SoundSourceID s_concussion6SndSrc;
	extern SoundSourceID s_concussion5SndSrc;
	extern SoundSourceID s_concussion1SndSrc;
	extern SoundSourceID s_plasma4SndSrc;
	extern SoundSourceID s_plasmaEmptySndSrc;
	extern SoundSourceID s_missile1SndSrc;
	extern SoundSourceID s_weaponChangeSnd;
	extern SoundEffectID s_repeaterFireSndID;

	extern s32 s_canFireWeaponSec;
	extern s32 s_canFireWeaponPrim;
	extern u32 s_fireFrame;

	struct WeaponAnimFrame
	{
		s32 waxFrame;
		s32 weaponLight;
		Tick delaySupercharge;
		Tick delayNormal;
	};
	static const WeaponAnimFrame c_pistolAnim[3] =
	{
		{ 1, 40,  7, 14 },
		{ 2,  0,  7, 14 },
		{ 0,  0, 21, 43 },
	};

	extern void weapon_handleState(s32 id);
	void weapon_computeMatrix(fixed16_16* mtx, angle14_32 pitch, angle14_32 yaw);
	JBool computeAutoaim(fixed16_16 xPos, fixed16_16 yPos, fixed16_16 zPos, angle14_32 pitch, angle14_32 yaw, s32 variation);

	void weaponFire_fist(s32 id)
	{
		task_begin;
		// STUB
		task_end;
	}

	void weaponFire_pistol(s32 id)
	{
		struct LocalContext
		{
			Tick delay;
			s32  iFrame;
		};
		task_begin_ctx;

		{
			PlayerWeapon* weapon = s_curPlayerWeapon;
			SecObject* player = s_playerObject;
			if (*weapon->ammo == 0) { return; }

			if (s_pistolSndId)
			{
				stopSound(s_pistolSndId);
			}
			s_pistolSndId = playSound2D(s_pistolSndSrc);

			if (weapon->wakeupRange)
			{
				vec3_fixed origin = { player->posWS.x, player->posWS.y, player->posWS.z };
				collision_effectObjectsInRangeXZ(player->sector, weapon->wakeupRange, origin, hitEffectWakeupFunc, player, ETFLAG_AI_ACTOR);
			}

			fixed16_16 mtx[9];
			weapon_computeMatrix(mtx, -player->pitch, -player->yaw);

			vec3_fixed inVec =
			{
				0x6666,	// 0.4
				0x9999, // 0.6
				0x33333 // 3.2
			};
			vec3_fixed outVec;
			rotateVectorM3x3(&inVec, &outVec, mtx);

			fixed16_16 xPos = player->posWS.x + outVec.x;
			fixed16_16 yPos = outVec.y + player->posWS.y - player->worldHeight + s_headwaveVerticalOffset;
			fixed16_16 zPos = player->posWS.z + outVec.z;

			JBool targetFound = computeAutoaim(xPos, yPos, zPos, player->pitch, player->yaw, weapon->variation);
			if (!targetFound)
			{
				s32 variation = weapon->variation;
				variation = random(variation * 2) - variation;
				s_weaponFirePitch = player->pitch + variation;

				variation = random(variation * 2) - variation;
				s_weaponFireYaw = player->yaw + variation;
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
				if (*weapon->ammo == 0)
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
					*weapon->ammo = pickup_addToValue(s_playerInfo.ammoEnergy, -1, 500);
				}

				ProjectileLogic* projLogic;
				fixed16_16 yPos = player->posWS.y - player->worldHeight + s_headwaveVerticalOffset;
				projLogic = (ProjectileLogic*)createProjectile(PROJ_PISTOL_BOLT, player->sector, player->posWS.x, yPos, player->posWS.z, player);
				projLogic->flags &= ~PROJFLAG_CAMERA_PASS_SOUND;
				projLogic->prevColObj = player;

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

					ProjectileHitType hitType = proj_handleMovement(projLogic);
					if (!handleProjectileHit(projLogic, hitType))
					{
						// If that fails, then try the new value.
						projLogic->delta = outVec2;
						hitType = proj_handleMovement(projLogic);
						fire = handleProjectileHit(projLogic, hitType);
					}
				}
				else
				{
					projLogic->delta = outVec;
					ProjectileHitType hitType = proj_handleMovement(projLogic);
					handleProjectileHit(projLogic, hitType);
				}
			}
		}

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
			} while (id != 0);
		}

		task_end;
	}

	void weaponFire_rifle(s32 id)
	{
		task_begin;
		// STUB
		task_end;
	}

	void weaponFire_thermalDetonator(s32 id)
	{
		task_begin;
		// STUB
		task_end;
	}

	void weaponFire_repeater(s32 id)
	{
		task_begin;
		// STUB
		task_end;
	}

	void weaponFire_fusion(s32 id)
	{
		task_begin;
		// STUB
		task_end;
	}

	void weaponFire_mortar(s32 id)
	{
		task_begin;
		// STUB
		task_end;
	}

	void weaponFire_mine(s32 id)
	{
		task_begin;
		// STUB
		task_end;
	}

	void weaponFire_concussion(s32 id)
	{
		task_begin;
		// STUB
		task_end;
	}

	void weaponShoot_cannon(s32 id)
	{
		task_begin;
		// STUB
		task_end;
	}

	JBool computeAutoaim(fixed16_16 xPos, fixed16_16 yPos, fixed16_16 zPos, angle14_32 pitch, angle14_32 yaw, s32 variation)
	{
		if (!s_drawnSpriteCount)
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
				fixed16_16 top = obj->posWS.y - (obj->worldHeight >> 1) + (obj->worldHeight >> 2);
				fixed16_16 dy = top - yPos;
				fixed16_16 dz = obj->posWS.z - zPos;
				angle14_32 angle = vec2ToAngle(dx, dz) & 0x3fff;
				angle14_32 yawDelta = (angle - yaw) & 0x3ffff;
				if (yawDelta > 227 && yawDelta < 16111)
				{
					continue;
				}
				fixed16_16 dist = computeDirAndLength(dx, dz, &s_autoAimDirX, &s_autoAimDirZ);
				if (dist < closest)
				{
					closest = dist;
					s_weaponFireYaw = angle;
					s_weaponFirePitch = vec2ToAngle(-dy, dist) & 0x3fff;
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

	void weapon_computeMatrix(fixed16_16* mtx, angle14_32 pitch, angle14_32 yaw)
	{
		fixed16_16 cosPch, sinPch;
		sinCosFixed(pitch, &sinPch, &cosPch);

		fixed16_16 cosYaw, sinYaw;
		sinCosFixed(yaw, &sinYaw, &cosYaw);

		mtx[0] = cosYaw;
		mtx[1] = 0;
		mtx[2] = sinYaw;
		mtx[3] = mul16(sinYaw, sinPch);
		mtx[4] = cosPch;
		mtx[5] = -mul16(cosYaw, sinPch);
		mtx[6] = -mul16(sinYaw, cosPch);
		mtx[7] = sinPch;
		mtx[8] = mul16(cosPch, cosYaw);
	}
}  // namespace TFE_DarkForces