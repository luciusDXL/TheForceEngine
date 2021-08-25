#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Player -
// Player information and object data.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Jedi/Sound/soundSystem.h>

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

	void weapon_startup();
	void weapon_enableAutomount(JBool enable);

	extern SoundSourceID s_superchargeCountdownSound;
}  // namespace TFE_DarkForces