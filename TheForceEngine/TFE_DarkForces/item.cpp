#include <cstring>

#include "item.h"
#include "pickup.h"
#include "animLogic.h"
#include <TFE_FileSystem/paths.h>
#include <TFE_ExternalData/pickupExternal.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	///////////////////////////////////////////
	// Constants
	///////////////////////////////////////////
	
	/* TFE: Moved to external data 
	static const char* c_itemResoure[ITEM_COUNT] =
	{
		"IDPLANS.WAX",  // ITEM_PLANS
		"IPHRIK.WAX",   // ITEM_PHRIK
		"INAVA.WAX",    // ITEM_NAVA
		"IDTGUN.WAX",   // ITEM_DT_WEAPON
		"IDATA.FME",    // ITEM_DATATAPE
		"IST-GUNI.FME", // ITEM_RIFLE
		"IAUTOGUN.FME", // ITEM_AUTOGUN
		"IMORTAR.FME",  // ITEM_MORTAR
		"IFUSION.FME",  // ITEM_FUSION
		"ICONCUS.FME",  // ITEM_CONCUSSION 
		"ICANNON.FME",  // ITEM_CANNON
		"IENERGY.FME",  // ITEM_ENERGY
		"IPOWER.FME",   // ITEM_POWER
		"IPLAZMA.FME",  // ITEM_PLASMA
		"IDET.FME",     // ITEM_DETONATOR
		"IDETS.FME",    // ITEM_DETONATORS 
		"ISHELL.FME",   // ITEM_SHELL
		"ISHELLS.FME",  // ITEM_SHELLS
		"IMINE.FME",    // ITEM_MINE
		"IMINES.FME",   // ITEM_MINES
		"IMSL.FME",     // ITEM_MISSILE
		"IMSLS.FME",    // ITEM_MISSILES
		"IARMOR.WAX",   // ITEM_SHIELD
		"IKEYR.FME",    // ITEM_RED_KEY
		"IKEYY.FME",    // ITEM_YELLOW_KEY 
		"IKEYB.FME",    // ITEM_BLUE_KEY
		"IGOGGLES.FME", // ITEM_GOGGLES
		"ICLEATS.FME",  // ITEM_CLEATS
		"IMASK.FME",    // ITEM_MASK
		"IBATTERY.FME", // ITEM_BATTERY
		"DET_CODE.FME", // ITEM_CODE1
		"DET_CODE.FME", // ITEM_CODE2
		"DET_CODE.FME", // ITEM_CODE3
		"DET_CODE.FME", // ITEM_CODE4
		"DET_CODE.FME", // ITEM_CODE5
		"DET_CODE.FME", // ITEM_CODE6
		"DET_CODE.FME", // ITEM_CODE7
		"DET_CODE.FME", // ITEM_CODE8
		"DET_CODE.FME", // ITEM_CODE9
		"IINVINC.WAX",  // ITEM_INVINCIBLE 
		"ICHARGE.FME",  // ITEM_SUPERCHARGE
		"IREVIVE.WAX",  // ITEM_REVIVE
		"ILIFE.WAX",    // ITEM_LIFE
		"IMEDKIT.FME",  // ITEM_MEDKIT
		"IPILE.FME",    // ITEM_PILE
		"ITEM10.WAX",	// ITEM_UNUSED
	}; */

	///////////////////////////////////////////
	// Shared State
	///////////////////////////////////////////
	SoundSourceId s_powerupPickupSnd;
	SoundSourceId s_objectivePickupSnd;
	SoundSourceId s_itemPickupSnd;
	ItemData      s_itemData[ITEM_COUNT];
	
	///////////////////////////////////////////
	// API Implementation
	///////////////////////////////////////////
	void item_loadData()
	{
		s_powerupPickupSnd = sound_load("bonus.voc", SOUND_PRIORITY_HIGH4);
		s_objectivePickupSnd = sound_load("complete.voc", SOUND_PRIORITY_HIGH5);
		s_itemPickupSnd     = sound_load("key.voc", SOUND_PRIORITY_MED5);

		TFE_ExternalData::ExternalPickup* externalPickups = TFE_ExternalData::getExternalPickups();
		char ext[16];
		for (s32 i = 0; i < ITEM_COUNT; i++)
		{
			const char* item = externalPickups[i].asset;
			FileUtil::getFileExtension(item, ext);
			if (strcasecmp(ext, "WAX") == 0)
			{
				s_itemData[i].wax = TFE_Sprite_Jedi::getWax(item, POOL_GAME);
				s_itemData[i].isWax = JTRUE;
			}
			else if (strcasecmp(ext, "FME") == 0)
			{
				s_itemData[i].frame = TFE_Sprite_Jedi::getFrame(item, POOL_GAME);
				s_itemData[i].isWax = JFALSE;
			}
			// TODO: should we also add support for 3DO dropitems??
		}
	}

	SecObject* item_create(ItemId itemId)
	{
		SecObject* newObj = allocateObject();

		if (s_itemData[itemId].isWax)
		{
			sprite_setData(newObj, s_itemData[itemId].wax);
		}
		else
		{
			frame_setData(newObj, s_itemData[itemId].frame);
		}
		// TODO: should we also add support for 3DO dropitems??

		obj_createPickup(newObj, itemId);
		if (s_itemData[itemId].isWax)
		{
			obj_setSpriteAnim(newObj);
		}

		return newObj;
	}
}  // TFE_DarkForces