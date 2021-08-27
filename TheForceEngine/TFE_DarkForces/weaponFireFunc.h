#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Player -
// Player information and object data.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>

namespace TFE_DarkForces
{
	typedef void(*WeaponFireFunc)();

	void weaponFire_fist();
	void weaponFire_pistol();
	void weaponFire_rifle();
	void weaponFire_thermalDetonator();
	void weaponFire_repeater();
	void weaponFire_fusion();
	void weaponFire_mortar();
	void weaponFire_mine();
	void weaponFire_concussion();
	void weaponShoot_cannon();
}  // namespace TFE_DarkForces