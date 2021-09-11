#include "weaponFireFunc.h"
#include "weapon.h"
#include "random.h"
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

	extern void weapon_handleState(s32 id);
	JBool computeAutoaim(fixed16_16 xPos, fixed16_16 yPos, fixed16_16 zPos, angle14_32 pitch, angle14_32 yaw, s32 variation);

	void weaponFire_fist()
	{
		// STUB
	}

	void weaponFire_pistol()
	{
		// STUB
	}

	void weaponFire_rifle()
	{
		// STUB
	}

	void weaponFire_thermalDetonator()
	{
		// STUB
	}

	void weaponFire_repeater()
	{
		// STUB
	}

	void weaponFire_fusion()
	{
		// STUB
	}

	void weaponFire_mortar()
	{
		// STUB
	}

	void weaponFire_mine()
	{
		// STUB
	}

	void weaponFire_concussion()
	{
		// STUB
	}

	void weaponShoot_cannon()
	{
		// STUB
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
}  // namespace TFE_DarkForces