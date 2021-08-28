#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Player -
// Player information and object data.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Asset/dfKeywords.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Jedi/Sound/soundSystem.h>
#include "time.h"

namespace TFE_DarkForces
{
	struct PlayerInfo
	{
		// Items
		//   Weapons
		JBool itemPistol;
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
		s32 healthFract;
		//   Weapon
		s32 selectedWeapon;
		s32 curWeapon;
		s32 maxWeapon;
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
	extern JBool s_wearingGasmask;
	extern JBool s_nightvisionActive;
	extern JBool s_headlampActive;
	extern JBool s_superCharge;
	extern JBool s_superChargeHud;
	extern JBool s_goals[];
	extern u32* s_playerInvSaved;
	// Eye parameters
	extern vec3_fixed s_eyePos;	// s_camX, s_camY, s_camZ in the DOS code.
	extern angle14_32 s_pitch, s_yaw, s_roll;
	extern Tick s_playerTick;

	extern SecObject* s_playerObject;
	extern SecObject* s_playerEye;

	// Sounds
	extern SoundSourceID s_landSplashSound;
	extern SoundSourceID s_landSolidSound;
	extern SoundSourceID s_gasDamageSoundSource;
	extern SoundSourceID s_maskSoundSource1;
	extern SoundSourceID s_maskSoundSource2;
	extern SoundSourceID s_invCountdownSound;
	extern SoundSourceID s_jumpSoundSource;
	extern SoundSourceID s_nightVisionActiveSoundSource;
	extern SoundSourceID s_nightVisionDeactiveSoundSource;
	extern SoundSourceID s_playerDeathSoundSource;
	extern SoundSourceID s_crushSoundSource;
	extern SoundSourceID s_playerHealthHitSoundSource;
	extern SoundSourceID s_kyleScreamSoundSource;
	extern SoundSourceID s_playerShieldHitSoundSource;

	void player_init();
	void player_readInfo(u8* inv, s32* ammo);
	void player_createController();
}  // namespace TFE_DarkForces