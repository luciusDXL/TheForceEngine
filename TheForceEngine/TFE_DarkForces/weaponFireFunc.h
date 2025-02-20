#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Weapon Fire Functions
// Logic for player weapons (not enemy weapons)
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Jedi/InfSystem/message.h>

namespace TFE_DarkForces
{
	typedef void(*WeaponFireFunc)(MessageType msg);

	void weaponFire_fist(MessageType msg);
	void weaponFire_pistol(MessageType msg);
	void weaponFire_rifle(MessageType msg);
	void weaponFire_thermalDetonator(MessageType msg);
	void weaponFire_repeater(MessageType msg);
	void weaponFire_fusion(MessageType msg);
	void weaponFire_mortar(MessageType msg);
	void weaponFire_mine(MessageType msg);
	void weaponFire_concussion(MessageType msg);
	void weaponFire_cannon(MessageType msg);
	void resetWeaponFunc();
}  // namespace TFE_DarkForces