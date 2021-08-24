#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Player -
// Player information and object data.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Asset/dfKeywords.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_Jedi/Level/robject.h>

namespace TFE_DarkForces
{
	struct PlayerInfo
	{
		// Items
		//   Weapons
		JBool itemRifle;
		JBool itemAutogun;
		JBool itemMortar;
		JBool itemFusion;
		JBool itemConcussion;
		JBool itemCannon;
		//   Keys
		JBool itemRedKey;
		JBool itemYellowKey;
		JBool itemBlueKey;
		JBool itemGoggles;
		JBool itemCleats;
		JBool itemMask;
		//   Inventory
		JBool itemPlans;
		JBool itemPhrik;
		JBool itemDatatape;
		JBool itemUnkown;
		JBool itemNava;
		JBool itemDtWeapon;
		JBool itemCode1;
		JBool itemCode2;
		JBool itemCode3;
		JBool itemCode4;
		JBool itemCode5;
		JBool itemCode6;
		JBool itemCode7;
		JBool itemCode8;
		JBool itemCode9;
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
		s32 showHit;
		//   Weapon
		s32 currentWeapon;
		//   Other
		s32 index0;
		s32 index1;
		s32 index2;
	};

	struct GoalItems
	{
		JBool plans;	// Death Star plans
		JBool phrik;	// Phrik metal sample
		JBool nava;		// Nava card in Nar Shadaa
		JBool nava2;	// Nava card in Jaba's ship.
		JBool datatape;	// Datatapes from Imperial City.
		JBool dtWeapon;	// Dark Trooper Weapon
		JBool stolenInv;// Stolen inventory
	};

	extern PlayerInfo s_playerInfo;
	extern GoalItems s_goalItems;
	extern fixed16_16 s_energy;
	extern s32 s_lifeCount;
	extern JBool s_invincibility;
	extern JBool s_wearingCleats;
	extern JBool s_superCharge;
	extern JBool s_superChargeHud;
	extern JBool s_goals[];
	// Eye parameters
	extern vec3_fixed s_eyePos;	// s_camX, s_camY, s_camZ in the DOS code.
	extern angle14_32 s_pitch, s_yaw, s_roll;

	extern SecObject* s_playerObject;
	extern SecObject* s_playerEye;
}  // namespace TFE_DarkForces