#pragma once
#include <TFE_System/types.h>
#include <TFE_Jedi/Math/fixedPoint.h>

///////////////////////////////////////////
// TFE Externalised Pickup data
///////////////////////////////////////////

namespace TFE_ExternalData
{
	struct MaxAmounts
	{
		s32 ammoEnergyMax = 500;
		s32 ammoPowerMax = 500;
		s32 ammoShellMax = 50;
		s32 ammoPlasmaMax = 400;
		s32 ammoDetonatorMax = 50;
		s32 ammoMineMax = 30;
		s32 ammoMissileMax = 20;
		s32 shieldsMax = 200;
		s32 batteryPowerMax = 2 * ONE_16;
		s32 healthMax = 100;
	};
	
	struct ExternalPickup
	{
		const char* name = "";
		s32 type;
		s32 weaponIndex = -1;
		JBool* playerItem = nullptr;
		s32* playerAmmo = nullptr;
		s32 amount = 0;
		s32 message1 = -1;
		s32 message2 = -1;
		bool fullBright = false;
		bool noRemove = false;
		const char* asset = "";
	};

	
	MaxAmounts* getMaxAmounts();
	ExternalPickup* getExternalPickups();
	void clearExternalPickups();
	void loadExternalPickups();
	void parseExternalPickups(char* data, bool fromMod);
	bool validateExternalPickups();
}