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

	enum WeaponTaskId
	{
		WTID_FREE_TASK = -1,
		WTID_SWITCH_WEAPON = 2,
		WTID_START_FIRING = 4,
		WTID_STOP_FIRING = 5,
		WTID_HOLSTER = 6,
		WTID_COUNT
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
	void player_cycleWeapons(s32 change);

	extern PlayerWeapon* s_curPlayerWeapon;
	extern SoundSourceID s_superchargeCountdownSound;
	extern Task* s_playerWeaponTask;
}  // namespace TFE_DarkForces