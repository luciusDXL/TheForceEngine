#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Player -
// Player information and object data.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>

namespace TFE_DarkForces
{
	typedef void(*WeaponFireFunc)(s32 id);

	void weaponFire_fist(s32 id);
	void weaponFire_pistol(s32 id);
	void weaponFire_rifle(s32 id);
	void weaponFire_thermalDetonator(s32 id);
	void weaponFire_repeater(s32 id);
	void weaponFire_fusion(s32 id);
	void weaponFire_mortar(s32 id);
	void weaponFire_mine(s32 id);
	void weaponFire_concussion(s32 id);
	void weaponShoot_cannon(s32 id);
}  // namespace TFE_DarkForces