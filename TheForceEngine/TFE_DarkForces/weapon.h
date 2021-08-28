#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Player -
// Player information and object data.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Jedi/Sound/soundSystem.h>
#include <TFE_Jedi/Task/task.h>

namespace TFE_DarkForces
{
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

	void weapon_startup();
	void weapon_enableAutomount(JBool enable);
	void weapon_setNext(s32 wpnIndex);
	void weapon_setFireRate();
	void weapon_clearFireRate();
	void weapon_createPlayerWeaponTask();

	extern SoundSourceID s_superchargeCountdownSound;
	extern Task* s_playerWeaponTask;
}  // namespace TFE_DarkForces