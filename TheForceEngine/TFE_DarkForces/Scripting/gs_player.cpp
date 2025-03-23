#include "gs_player.h"
#include "assert.h"
#include <TFE_DarkForces/player.h>
#include <TFE_DarkForces/mission.h>
#include <TFE_DarkForces/pickup.h>
#include <TFE_DarkForces/weapon.h>
#include <TFE_System/system.h>
#include <angelscript.h>

namespace TFE_DarkForces
{
	void killPlayer()
	{
		sound_play(s_playerDeathSoundSource);
		if (s_gasSectorTask)
		{
			task_free(s_gasSectorTask);
		}
		s_playerInfo.health = 0;
		s_gasSectorTask = nullptr;
		s_playerDying = JTRUE;
		s_reviveTick = s_curTick + 436;
	}

	s32 getBatteryPercent()
	{
		return (s32)(100.0 * s_batteryPower / FIXED(2) );
	}

	void setHealth(s32 value)
	{
		s_playerInfo.health = value;

		if (s_playerInfo.health <= 0)
		{
			killPlayer();
		}
	}

	void setShields(s32 value)
	{
		s_playerInfo.shields = value;
	}

	void setBattery(s32 value)
	{
		s_batteryPower = (fixed16_16)((value / 100.0) * FIXED(2));
	}

	void setAmmoEnergy(s32 value)
	{
		s_playerInfo.ammoEnergy = value;
	}

	void setAmmoPower(s32 value)
	{
		s_playerInfo.ammoPower = value;
	}

	void setAmmoDetonator(s32 value)
	{
		s_playerInfo.ammoDetonator = value;
	}

	void setAmmoShell(s32 value)
	{
		s_playerInfo.ammoShell = value;
	}

	void setAmmoMine(s32 value)
	{
		s_playerInfo.ammoMine = value;
	}

	void setAmmoMissile(s32 value)
	{
		s_playerInfo.ammoMissile = value;
	}

	void setAmmoPlasma(s32 value)
	{
		s_playerInfo.ammoPlasma = value;
	}

	void addToHealth(s32 value)
	{
		s_playerInfo.health += value;

		if (s_playerInfo.health <= 0)
		{
			killPlayer();
		}
	}

	void addToShields(s32 value)
	{
		s_playerInfo.shields += value;
		if (s_playerInfo.shields < 0)
		{
			s_playerInfo.shields = 0;
		}
	}

	bool hasRedKey()
	{
		return s_playerInfo.itemRedKey == JTRUE;
	}

	bool hasBlueKey()
	{
		return s_playerInfo.itemBlueKey == JTRUE;
	}

	bool hasYellowKey()
	{
		return s_playerInfo.itemYellowKey == JTRUE;
	}

	bool hasGoggles()
	{
		return s_playerInfo.itemGoggles == JTRUE;
	}

	bool hasCleats()
	{
		return s_playerInfo.itemCleats == JTRUE;
	}

	bool hasMask()
	{
		return s_playerInfo.itemMask == JTRUE;
	}

	void giveRedKey()
	{
		s_playerInfo.itemRedKey = JTRUE;
	}

	void giveBlueKey()
	{
		s_playerInfo.itemBlueKey = JTRUE;
	}

	void giveYellowKey()
	{
		s_playerInfo.itemYellowKey = JTRUE;
	}

	void giveGoggles()
	{
		s_playerInfo.itemGoggles = JTRUE;
	}

	void giveCleats()
	{
		s_playerInfo.itemCleats = JTRUE;
	}

	void giveMask()
	{
		s_playerInfo.itemMask = JTRUE;
	}

	void removeRedKey()
	{
		s_playerInfo.itemRedKey = JFALSE;
	}

	void removeBlueKey()
	{
		s_playerInfo.itemBlueKey = JFALSE;
	}

	void removeYellowKey()
	{
		s_playerInfo.itemYellowKey = JFALSE;
	}

	void removeGoggles()
	{
		if (s_nightVisionActive)
		{
			disableNightVision();
		}
		
		s_playerInfo.itemGoggles = JFALSE;
	}

	void removeCleats()
	{
		if (s_wearingCleats)
		{
			disableCleats();
		}
		
		s_playerInfo.itemCleats = JFALSE;
	}

	void removeMask()
	{
		if (s_wearingGasmask)
		{
			disableMask();
		}
		
		s_playerInfo.itemMask = JFALSE;
	}

	bool hasPistol()
	{
		return s_playerInfo.itemPistol == JTRUE;
	}

	void givePistol()
	{
		s_playerInfo.itemPistol = JTRUE;
	}

	bool hasRifle() 
	{
		return s_playerInfo.itemRifle == JTRUE;
	}

	void giveRifle()
	{
		s_playerInfo.itemRifle = JTRUE;
	}

	bool hasAutogun()
	{
		return s_playerInfo.itemAutogun == JTRUE;
	}

	void giveAutogun()
	{
		s_playerInfo.itemAutogun = JTRUE;
	}

	bool hasFusion()
	{
		return s_playerInfo.itemFusion == JTRUE;
	}

	void giveFusion()
	{
		s_playerInfo.itemFusion = JTRUE;
	}

	bool hasMortar()
	{
		return s_playerInfo.itemMortar == JTRUE;
	}

	void giveMortar()
	{
		s_playerInfo.itemMortar = JTRUE;
	}

	bool hasConcussion()
	{
		return s_playerInfo.itemConcussion == JTRUE;
	}

	void giveConcussion()
	{
		s_playerInfo.itemConcussion = JTRUE;
	}

	bool hasCannon()
	{
		return s_playerInfo.itemCannon == JTRUE;
	}

	void giveCannon()
	{
		s_playerInfo.itemCannon = JTRUE;
	}

	void removePistol()
	{
		if (s_playerInfo.curWeapon == WPN_PISTOL)
		{
			s_playerInfo.newWeapon = s_playerInfo.itemRifle ? WPN_RIFLE : WPN_FIST;
		}
		s_playerInfo.itemPistol = JFALSE;
	}

	void removeRifle()
	{
		if (s_playerInfo.curWeapon == WPN_RIFLE)
		{
			s_playerInfo.newWeapon = s_playerInfo.itemPistol ? WPN_PISTOL : WPN_FIST;
		}
		s_playerInfo.itemRifle = JFALSE;
	}

	void removeAutogun()
	{
		if (s_playerInfo.curWeapon == WPN_REPEATER)
		{
			s_playerInfo.newWeapon = s_playerInfo.itemPistol ? WPN_PISTOL : WPN_FIST;
		}
		s_playerInfo.itemAutogun = JFALSE;
	}

	void removeFusion()
	{
		if (s_playerInfo.curWeapon == WPN_FUSION)
		{
			s_playerInfo.newWeapon = s_playerInfo.itemPistol ? WPN_PISTOL : WPN_FIST;
		}
		s_playerInfo.itemFusion = JFALSE;
	}

	void removeMortar()
	{
		if (s_playerInfo.curWeapon == WPN_MORTAR)
		{
			s_playerInfo.newWeapon = s_playerInfo.itemPistol ? WPN_PISTOL : WPN_FIST;
		}
		s_playerInfo.itemMortar = JFALSE;
	}

	void removeConcussion()
	{
		if (s_playerInfo.curWeapon == WPN_CONCUSSION)
		{
			s_playerInfo.newWeapon = s_playerInfo.itemPistol ? WPN_PISTOL : WPN_FIST;
		}
		s_playerInfo.itemConcussion = JFALSE;
	}

	void removeCannon()
	{
		if (s_playerInfo.curWeapon == WPN_CANNON)
		{
			s_playerInfo.newWeapon = s_playerInfo.itemPistol ? WPN_PISTOL : WPN_FIST;
		}
		s_playerInfo.itemCannon = JFALSE;
	}

	bool GS_Player::scriptRegister(ScriptAPI api)
	{
		ScriptClassBegin("Player", "player", api);
		{
			ScriptObjFunc("void kill()", killPlayer);
			
			// Health / ammo getters
			ScriptLambdaPropertyGet("int get_health()", s32, { return s_playerInfo.health; });
			ScriptLambdaPropertyGet("int get_shields()", s32, { return s_playerInfo.shields; });
			ScriptLambdaPropertyGet("int get_ammoEnergy()", s32, { return s_playerInfo.ammoEnergy; });
			ScriptLambdaPropertyGet("int get_ammoPower()", s32, { return s_playerInfo.ammoPower; });
			ScriptLambdaPropertyGet("int get_ammoDetonator()", s32, { return s_playerInfo.ammoDetonator; });
			ScriptLambdaPropertyGet("int get_ammoShell()", s32, { return s_playerInfo.ammoShell; });
			ScriptLambdaPropertyGet("int get_ammoMine()", s32, { return s_playerInfo.ammoMine; });
			ScriptLambdaPropertyGet("int get_ammoPlasma()", s32, { return s_playerInfo.ammoPlasma; });
			ScriptLambdaPropertyGet("int get_ammoMissile()", s32, { return s_playerInfo.ammoMissile; });
			ScriptPropertyGetFunc("int get_battery()", getBatteryPercent);

			// Health / ammo setters
			ScriptPropertySetFunc("void set_health(int)", setHealth);
			ScriptPropertySetFunc("void set_shields(int)", setShields);
			ScriptPropertySetFunc("void set_battery(int)", setBattery);
			ScriptPropertySetFunc("void set_ammoEnergy(int)", setAmmoEnergy);
			ScriptPropertySetFunc("void set_ammoPower(int)", setAmmoPower);
			ScriptPropertySetFunc("void set_ammoDetonator(int)", setAmmoDetonator);
			ScriptPropertySetFunc("void set_ammoShell(int)", setAmmoShell);
			ScriptPropertySetFunc("void set_ammoMine(int)", setAmmoMine);
			ScriptPropertySetFunc("void set_ammoMissile(int)", setAmmoMissile);
			ScriptPropertySetFunc("void set_ammoPlasma(int)", setAmmoPlasma);

			ScriptObjFunc("void addToHealth(int)", addToHealth);
			ScriptObjFunc("void addToShields(int)", addToShields);

			// Items
			ScriptObjFunc("bool hasRedKey()", hasRedKey);
			ScriptObjFunc("bool hasBlueKey()", hasBlueKey);
			ScriptObjFunc("bool hasYellowKey()", hasYellowKey);
			ScriptObjFunc("bool hasGoggles()", hasGoggles);
			ScriptObjFunc("bool hasCleats()", hasCleats);
			ScriptObjFunc("bool hasMask()", hasMask);

			ScriptObjFunc("void giveRedKey()", giveRedKey);
			ScriptObjFunc("void giveBlueKey()", giveBlueKey);
			ScriptObjFunc("void giveYellowKey()", giveYellowKey);
			ScriptObjFunc("void giveGoggles()", giveGoggles);
			ScriptObjFunc("void giveCleats()", giveCleats);
			ScriptObjFunc("void giveMask()", giveMask);

			ScriptObjFunc("void removeRedKey()", removeRedKey);
			ScriptObjFunc("void removeBlueKey()", removeBlueKey);
			ScriptObjFunc("void removeYellowKey()", removeYellowKey);
			ScriptObjFunc("void removeGoggles()", removeGoggles);
			ScriptObjFunc("void removeCleats()", removeCleats);
			ScriptObjFunc("void removeMask()", removeMask);

			// Weapons
			ScriptObjFunc("bool hasPistol()", hasPistol);
			ScriptObjFunc("bool hasRifle()", hasRifle);
			ScriptObjFunc("bool hasAutogun()", hasAutogun);
			ScriptObjFunc("bool hasFusion()", hasFusion);
			ScriptObjFunc("bool hasMortar()", hasMortar);
			ScriptObjFunc("bool hasConcussion()", hasConcussion);
			ScriptObjFunc("bool hasCannon()", hasCannon);

			ScriptObjFunc("void givePistol()", givePistol);
			ScriptObjFunc("void giveRifle()", giveRifle);
			ScriptObjFunc("void giveAutogun()", giveAutogun);
			ScriptObjFunc("void giveFusion()", giveFusion);
			ScriptObjFunc("void giveMortar()", giveMortar);
			ScriptObjFunc("void giveConcussion()", giveConcussion);
			ScriptObjFunc("void giveCannon()", giveCannon);

			ScriptObjFunc("void removePistol()", removePistol);
			ScriptObjFunc("void removeRifle()", removeRifle);
			ScriptObjFunc("void removeAutogun()", removeAutogun);
			ScriptObjFunc("void removeFusion()", removeFusion);
			ScriptObjFunc("void removeMortar()", removeMortar);
			ScriptObjFunc("void removeConcussion()", removeConcussion);
			ScriptObjFunc("void removeCannon()", removeCannon);
		}
		ScriptClassEnd();
	}
}