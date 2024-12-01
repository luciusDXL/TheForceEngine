#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Item IDs.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Asset/spriteAsset_Jedi.h>
#include "sound.h"

namespace TFE_DarkForces
{
	enum ItemId
	{
		ITEM_NONE = -1,			// -1
		ITEM_PLANS = 0,			// 0x00
		ITEM_PHRIK = 1,			// 0x01
		ITEM_NAVA = 2,			// 0x02
		ITEM_DT_WEAPON = 3,		// 0x03
		ITEM_DATATAPE = 4,		// 0x04
		ITEM_RIFLE = 5,			// 0x05
		ITEM_AUTOGUN = 6,		// 0x06
		ITEM_MORTAR = 7,		// 0x07
		ITEM_FUSION = 8,		// 0x08
		ITEM_CONCUSSION = 9,	// 0x09
		ITEM_CANNON = 10,		// 0x0a
		ITEM_ENERGY = 11,		// 0x0b
		ITEM_POWER = 12,		// 0x0c
		ITEM_PLASMA = 13,		// 0x0d
		ITEM_DETONATOR = 14,	// 0x0e
		ITEM_DETONATORS = 15,	// 0x0f
		ITEM_SHELL = 16,		// 0x10
		ITEM_SHELLS = 17,		// 0x11
		ITEM_MINE = 18,			// 0x12
		ITEM_MINES = 19,		// 0x13
		ITEM_MISSILE = 20,		// 0x14
		ITEM_MISSILES = 21,		// 0x15
		ITEM_SHIELD = 22,		// 0x16
		ITEM_RED_KEY = 23,		// 0x17
		ITEM_YELLOW_KEY = 24,	// 0x18
		ITEM_BLUE_KEY = 25,		// 0x19
		ITEM_GOGGLES = 26,		// 0x1a
		ITEM_CLEATS = 27,		// 0x1b
		ITEM_MASK = 28,			// 0x1c
		ITEM_BATTERY = 29,		// 0x1d
		ITEM_CODE1 = 30,		// 0x1e
		ITEM_CODE2 = 31,		// 0x1f
		ITEM_CODE3 = 32,		// 0x20
		ITEM_CODE4 = 33,		// 0x21
		ITEM_CODE5 = 34,		// 0x22
		ITEM_CODE6 = 35,		// 0x23
		ITEM_CODE7 = 36,		// 0x24
		ITEM_CODE8 = 37,		// 0x25
		ITEM_CODE9 = 38,		// 0x26
		ITEM_INVINCIBLE = 39,	// 0x27
		ITEM_SUPERCHARGE = 40,	// 0x28
		ITEM_REVIVE = 41,		// 0x29
		ITEM_LIFE = 42,			// 0x2a
		ITEM_MEDKIT = 43,		// 0x2b
		ITEM_PILE = 44,			// 0x2c
		ITEM_UNUSED = 45,		// 0x2d
		ITEM_COUNT = 46,		// 0x2e
	};

	enum ItemType
	{
		ITYPE_NONE     = 0,		
		ITYPE_OBJECTIVE= 1,  // Mission objective item.
		ITYPE_WEAPON   = 2,  // Weapons
		ITYPE_AMMO     = 4,  // Pickups that refill ammo, health, shields, etc.
		ITYPE_USABLE   = 8,  // Keys and usable inventory items.
		ITYPE_CODEKEY  = 16, // Code keys (Non-usable Inventory Items)
		ITYPE_POWERUP  = 32, // Powerups & Extra lives.
		ITYPE_SPECIAL  = 64, // Special - only a single item fits in this type (ITEM_PILE).
	};

	struct ItemData
	{
		union
		{
			Wax* wax;
			WaxFrame* frame;
		};
		JBool isWax;
	};

	extern SoundSourceId s_powerupPickupSnd;
	extern SoundSourceId s_objectivePickupSnd;
	extern SoundSourceId s_itemPickupSnd;
	extern ItemData      s_itemData[ITEM_COUNT];

	void item_loadData();
	SecObject* item_create(ItemId itemId);
}  // TFE_DarkForces