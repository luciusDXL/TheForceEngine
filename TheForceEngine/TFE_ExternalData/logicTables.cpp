#include "logicTables.h"

namespace TFE_ExternalData
{
	const char* df_projectileTable[] =
	{
		"punch",			// 0
		"pistol_bolt",		// 1
		"rifle_bolt",		// 2
		"thermal_det",		// 3
		"repeater",			// 4
		"plasma",			// 5
		"mortar",			// 6
		"land_mine",		// 7
		"land_mine_prox",	// 8
		"land_mine_placed",	// 9
		"concussion",		// 10
		"cannon",			// 11
		"missile",			// 12
		"turret_bolt",		// 13
		"remote_bolt",		// 14
		"exp_barrel",		// 15
		"homing_missile",	// 16
		"probe_proj",		// 17
		"bobafett_ball",	// 18
	};

	const char* df_dropItemTable[] =
	{
		"PLANS",			// 0
		"PHRIK",			// 1
		"NAVA",				// 2
		"DT_WEAPON",		// 3
		"DATATAPE",			// 4
		"RIFLE",			// 5
		"AUTOGUN",			// 6
		"MORTAR",			// 7
		"FUSION",			// 8
		"CONCUSSION",		// 9
		"CANNON",			// 10
		"ENERGY",			// 11
		"POWER",			// 12
		"PLASMA",			// 13
		"DETONATOR",		// 14
		"DETONATORS",		// 15
		"SHELL",			// 16
		"SHELLS",			// 17
		"MINE",				// 18
		"MINES",			// 19
		"MISSILE",			// 20
		"MISSILES",			// 21
		"SHIELD",			// 22
		"RED_KEY",			// 23
		"YELLOW_KEY",		// 24
		"BLUE_KEY",			// 25
		"GOGGLES",			// 26
		"CLEATS",			// 27
		"MASK",				// 28
		"BATTERY",			// 29
		"CODE1",			// 30
		"CODE2",			// 31
		"CODE3",			// 32
		"CODE4",			// 33
		"CODE5",			// 34
		"CODE6",			// 35
		"CODE7",			// 36
		"CODE8",			// 37
		"CODE9",			// 38
		"INVINCIBLE",		// 39
		"SUPERCHARGE",		// 40
		"REVIVE",			// 41
		"LIFE",				// 42
		"MEDKIT",			// 43
		"PILE",				// 44
		"ITEM10",			// 45 - added for s_playerInfo.itemUnused
	};

	const char* df_effectTable[] =
	{
		"SMALL_EXP",		// 0	// small "puff" - blaster weapons.
		"THERMDET_EXP",		// 1	// thermal detonator explosion.
		"PLASMA_EXP",		// 2	// plasma "puff".
		"MORTAR_EXP",		// 3	// mortar explosion
		"CONCUSSION",		// 4	// concussion - first stage.
		"CONCUSSION2",		// 5	// concussion - second stage.
		"MISSILE_EXP",		// 6	// missile explosion.
		"MISSILE_WEAK",		// 7	// weaker version of the missle explosion.
		"PUNCH",			// 8	// punch
		"CANNON_EXP",		// 9	// cannon "puff".
		"REPEATER_EXP",		// 10	// repeater "puff".
		"LARGE_EXP",		// 11	// large explosion such as land mine.
		"EXP_BARREL",		// 12	// exploding barrel.
		"EXP_INVIS",		// 13	// an explosion that makes no sound and has no visual effect.
		"SPLASH",			// 14	// water splash
		"EXP_35",			// 15	// medium explosion, 35 damage.
		"EXP_NO_DMG",		// 16	// medium explosion, no damage.
		"EXP_25",			// 17	// medium explosion, 25 damage.
	};

	const char* df_weaponTable[] =
	{
		"FIST",				// 0
		"PISTOL",			// 1
		"RIFLE",			// 2
		"THERMAL_DET",		// 3
		"REPEATER",			// 4
		"FUSION",			// 5
		"MORTAR",			// 6
		"MINE",				// 7
		"CONCUSSION",		// 8
		"CANNON",			// 9
	};
}