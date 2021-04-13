#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Player -
// Player information and object data.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Asset/dfKeywords.h>
#include <TFE_Level/rsector.h>
#include <TFE_Level/robject.h>

namespace TFE_DarkForces
{
	struct PlayerInfo
	{
		// Items
		//   Weapons
		s32 itemRifle;
		s32 itemAutogun;
		s32 itemMortar;
		s32 itemFusion;
		s32 itemConcussion;
		s32 itemCannon;
		//   Keys
		s32 itemRedKey;
		s32 itemYellowKey;
		s32 itemBlueKey;
		s32 itemGoggles;
		s32 itemCleats;
		s32 itemMask;
		//   Inventory
		s32 itemPlans;
		s32 itemPhrik;
		s32 itemDatatape;
		s32 itemUnkown;
		s32 itemNava;
		s32 itemDtWeapon;
		s32 itemCode1;
		s32 itemCode2;
		s32 itemCode3;
		s32 itemCode4;
		s32 itemCode5;
		s32 itemCode6;
		s32 itemCode7;
		s32 itemCode8;
		s32 itemCode9;
		//   Ammo
		s32 ammoEnergy;
		s32 ammoPower;
		s32 ammoPlasma;
		s32 ammoDetonator;
		s32 ammoShell;
		s32 ammoMine;
		s32 ammoMissile;
		//   Health & Shields
		s32 stateUnknown;
		s32 shields;
		s32 health;
	};

	extern PlayerInfo s_playerInfo;
	extern fixed16_16 s_energy;
}  // namespace TFE_DarkForces