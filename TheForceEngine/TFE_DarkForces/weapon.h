#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Player -
// Player information and object data.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Sound/soundSystem.h>
#include <TFE_Jedi/Task/task.h>

namespace TFE_Jedi
{
	struct DrawRect;
}

namespace TFE_DarkForces
{
	struct PlayerWeapon
	{
		s32 frameCount;
		TextureData* frames[8];
		s32 frame;
		s32 xPos[8];
		s32 yPos[8];
		u32 flags;
		s32 rollOffset;
		s32 pchOffset;
		s32 xWaveOffset;
		s32 yWaveOffset;
		s32 xOffset;
		s32 yOffset;
		s32* ammo;
		s32* secondaryAmmo;
		s32 u8c;
		s32 u90;
		fixed16_16 wakeupRange;
		s32 variation;
	};

	struct WeaponAnimState
	{
		s32 frame;
		s32 startOffsetX;
		s32 startOffsetY;
		s32 xSpeed;
		s32 ySpeed;
		s32 frameCount;
		u32 ticksPerFrame;
	};

	///////////////////////////////////////////
	// Constants
	///////////////////////////////////////////
	enum WeaponID
	{
		WPN_FIST = 0,
		WPN_PISTOL,
		WPN_RIFLE,
		WPN_THERMAL_DET,
		WPN_REPEATER,
		WPN_FUSION,
		WPN_MORTAR,
		WPN_MINE,
		WPN_CONCUSSION,
		WPN_CANNON,
		WPN_COUNT
	};

	enum WeaponFireMode
	{
		WFIRE_PRIMARY = 0,
		WFIRE_SECONDARY,
	};

	enum WeaponFireType
	{
		WFIRETYPE_PRIMARY = JFALSE,		// Primary fire.
		WFIRETYPE_SECONDARY = JTRUE,	// Secondary fire.
	};

	void weapon_startup();
	void weapon_enableAutomount(JBool enable);
	void weapon_setNext(s32 wpnIndex);
	void weapon_setFireRate();
	void weapon_clearFireRate();
	void weapon_createPlayerWeaponTask();
	void weapon_holster();
	void weapon_draw(u8* display, DrawRect* rect);
	void weapon_fixupAnim();
	void weapon_stopFiring();
	void player_cycleWeapons(s32 change);
	void weapon_computeMatrix(fixed16_16* mtx, angle14_32 pitch, angle14_32 yaw);

	extern PlayerWeapon* s_curPlayerWeapon;
	extern SoundSourceID s_superchargeCountdownSound;
	extern Task* s_playerWeaponTask;

	// Weapon sounds (external since AI uses it).
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
}  // namespace TFE_DarkForces