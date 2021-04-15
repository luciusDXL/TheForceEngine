#include "pickup.h"
#include "player.h"

namespace TFE_DarkForces
{
	Task* s_pickupTask;

	ItemId getPickupItemId(const char* keyword)
	{
		const KEYWORD kw = getKeywordIndex(keyword);
		ItemId id;
		switch (kw)
		{
			case KW_BATTERY:    id = ITEM_BATTERY;    break;
			case KW_BLUE:       id = ITEM_BLUE_KEY;   break;
			case KW_CANNON:     id = ITEM_CANNON;     break;
			case KW_CLEATS:     id = ITEM_CLEATS;     break;
			case KW_CODE1:      id = ITEM_CODE1;      break;
			case KW_CODE2:      id = ITEM_CODE2;      break;
			case KW_CODE3:      id = ITEM_CODE3;      break;
			case KW_CODE4:      id = ITEM_CODE4;      break;
			case KW_CODE5:      id = ITEM_CODE5;      break;
			case KW_CODE6:      id = ITEM_CODE6;      break;
			case KW_CODE7:      id = ITEM_CODE7;      break;
			case KW_CODE8:      id = ITEM_CODE8;      break;
			case KW_CODE9:      id = ITEM_CODE9;      break;
			case KW_CONCUSSION: id = ITEM_CONCUSSION; break;
			case KW_DETONATOR:  id = ITEM_DETONATOR;  break;
			case KW_DETONATORS: id = ITEM_DETONATORS; break;
			case KW_DT_WEAPON:  id = ITEM_DT_WEAPON;  break;
			case KW_DATATAPE:   id = ITEM_DATATAPE;   break;
			case KW_ENERGY:     id = ITEM_ENERGY;     break;
			case KW_FUSION:     id = ITEM_FUSION;     break;
			case KW_GOGGLES:    id = ITEM_GOGGLES;    break;
			case KW_MASK:       id = ITEM_MASK;       break;
			case KW_MINE:       id = ITEM_MINE;       break;
			case KW_MINES:      id = ITEM_MINES;      break;
			case KW_MISSLE:     id = ITEM_MISSILE;    break;
			case KW_MISSLES:    id = ITEM_MISSILES;   break;
			case KW_MORTAR:     id = ITEM_MORTAR;     break;
			case KW_NAVA:       id = ITEM_NAVA;       break;
			case KW_PHRIK:      id = ITEM_PHRIK;      break;
			case KW_PLANS:      id = ITEM_PLANS;      break;
			case KW_PLASMA:     id = ITEM_PLASMA;     break;
			case KW_POWER:      id = ITEM_POWER;      break;
			case KW_RED:        id = ITEM_RED_KEY;    break;
			case KW_RIFLE:      id = ITEM_RIFLE;      break;
			case KW_SHELL:      id = ITEM_SHELL;      break;
			case KW_SHELLS:     id = ITEM_SHELLS;     break;
			case KW_SHIELD:     id = ITEM_SHIELD;     break;
			case KW_INVINCIBLE: id = ITEM_INVINCIBLE; break;
			case KW_REVIVE:     id = ITEM_REVIVE;     break;
			case KW_SUPERCHARGE:id = ITEM_SUPERCHARGE;break;
			case KW_LIFE:       id = ITEM_LIFE;       break;
			case KW_MEDKIT:     id = ITEM_MEDKIT;     break;
			case KW_PILE:       id = ITEM_PILE;       break;
			case KW_YELLOW:     id = ITEM_YELLOW_KEY; break;
			case KW_AUTOGUN:    id = ITEM_AUTOGUN;    break;
			default:            id = ITEM_PLANS;
		}
		return id;
	}

	void logic_pickupFunc()
	{
		// TODO
	}

	Logic* obj_createPickup(SecObject* obj, ItemId id)
	{
		Pickup* pickup = (Pickup*)malloc(sizeof(Pickup));
		obj_addLogic(obj, (Logic*)pickup, s_pickupTask, logic_pickupFunc);

		obj->entityFlags |= ETFLAG_PICKUP;

		// Setup the pickup based on the ItemId.
		pickup->id = id;
		pickup->index = -1;
		pickup->item = nullptr;
		pickup->value = nullptr;
		pickup->amount = 0;
		pickup->msgId[0] = -1;
		pickup->msgId[1] = -1;

		switch (id)
		{
			// MISSION ITEMS
			case ITEM_PLANS:
			{
				pickup->type = ITYPE_INV_ITEM;
				pickup->item = &s_playerInfo.itemPlans;
				pickup->msgId[0] = 400;

				obj->flags |= (OBJ_FLAG_FULLBRIGHT | OBJ_FLAG_BIT_6);
			} break;
			case ITEM_PHRIK:
			{
				pickup->type = ITYPE_INV_ITEM;
				pickup->item = &s_playerInfo.itemPhrik;
				pickup->msgId[0] = 401;

				obj->flags |= (OBJ_FLAG_FULLBRIGHT | OBJ_FLAG_BIT_6);
			} break;
			case ITEM_NAVA:
			{
				pickup->type = ITYPE_INV_ITEM;
				pickup->item = &s_playerInfo.itemNava;
				pickup->msgId[0] = 402;

				obj->flags |= (OBJ_FLAG_FULLBRIGHT | OBJ_FLAG_BIT_6);
			} break;
			case ITEM_DT_WEAPON:
			{
				pickup->type = ITYPE_INV_ITEM;
				pickup->item = &s_playerInfo.itemDtWeapon;
				pickup->msgId[0] = 405;

				obj->flags |= OBJ_FLAG_BIT_6;
			} break;
			case ITEM_DATATAPE:
			{
				pickup->type = ITYPE_INV_ITEM;
				pickup->item = &s_playerInfo.itemDatatape;
				pickup->msgId[0] = 406;

				obj->flags |= OBJ_FLAG_BIT_6;
			} break;
			// WEAPONS
			case ITEM_RIFLE:
			{
				pickup->index = 2;
				pickup->type = ITYPE_WEAPON;
				pickup->item = &s_playerInfo.itemRifle;
				pickup->value = &s_playerInfo.ammoEnergy;
				pickup->amount = 15;
				pickup->msgId[0] = 100;
				pickup->msgId[1] = 101;
				pickup->maxAmount = 500;
			} break;
			case ITEM_AUTOGUN:
			{
				pickup->index = 4;
				pickup->type = ITYPE_WEAPON;
				pickup->item = &s_playerInfo.itemAutogun;
				pickup->value = &s_playerInfo.ammoPower;
				pickup->amount = 30;
				pickup->msgId[0] = 103;
				pickup->msgId[1] = 104;
				pickup->maxAmount = 500;
			} break;
			case ITEM_MORTAR:
			{
				pickup->index = 6;
				pickup->type = ITYPE_WEAPON;
				pickup->item = &s_playerInfo.itemMortar;
				pickup->value = &s_playerInfo.ammoShell;
				pickup->amount = 3;
				pickup->msgId[0] = 105;
				pickup->msgId[1] = 106;
				pickup->maxAmount = 50;
			} break;
			case ITEM_FUSION:
			{
				pickup->index = 5;
				pickup->type = ITYPE_WEAPON;
				pickup->item = &s_playerInfo.itemFusion;
				pickup->value = &s_playerInfo.ammoPower;
				pickup->amount = 50;
				pickup->msgId[0] = 107;
				pickup->msgId[1] = 108;
				pickup->maxAmount = 500;
			} break;
			case ITEM_CONCUSSION:
			{
				pickup->index = 8;
				pickup->type = ITYPE_WEAPON;
				pickup->item = &s_playerInfo.itemConcussion;
				pickup->value = &s_playerInfo.ammoPower;
				pickup->amount = 100;
				pickup->msgId[0] = 110;
				pickup->msgId[1] = 111;
				pickup->maxAmount = 500;
			} break;
			case ITEM_CANNON:
			{
				pickup->index = 9;
				pickup->type = ITYPE_WEAPON;
				pickup->item = &s_playerInfo.itemCannon;
				pickup->value = &s_playerInfo.ammoPlasma;
				pickup->amount = 30;
				pickup->msgId[0] = 112;
				pickup->msgId[1] = 113;
				pickup->maxAmount = 400;
			} break;
			// AMMO
			case ITEM_ENERGY:
			{
				pickup->type = ITYPE_AMMO;
				pickup->value = &s_playerInfo.ammoEnergy;
				pickup->amount = 15;
				pickup->msgId[0] = 200;
				pickup->maxAmount = 500;
			} break;
			case ITEM_POWER:
			{
				pickup->type = ITYPE_AMMO;
				pickup->value = &s_playerInfo.ammoPower;
				pickup->amount = 10;
				pickup->msgId[0] = 201;
				pickup->maxAmount = 500;
			} break;
			case ITEM_PLASMA:
			{
				pickup->type = ITYPE_AMMO;
				pickup->value = &s_playerInfo.ammoPlasma;
				pickup->amount = 20;
				pickup->msgId[0] = 202;
				pickup->maxAmount = 400;
			} break;
			case ITEM_DETONATOR:
			{
				pickup->type = ITYPE_AMMO;
				pickup->value = &s_playerInfo.ammoDetonator;
				pickup->amount = 1;
				pickup->msgId[0] = 203;
				pickup->maxAmount = 50;
			} break;
			case ITEM_DETONATORS:
			{
				pickup->type = ITYPE_AMMO;
				pickup->value = &s_playerInfo.ammoDetonator;
				pickup->amount = 5;
				pickup->msgId[0] = 204;
				pickup->maxAmount = 50;
			} break;
			case ITEM_SHELL:
			{
				pickup->type = ITYPE_AMMO;
				pickup->value = &s_playerInfo.ammoShell;
				pickup->amount = 1;
				pickup->msgId[0] = 205;
				pickup->maxAmount = 50;
			} break;
			case ITEM_SHELLS:
			{
				pickup->type = ITYPE_AMMO;
				pickup->value = &s_playerInfo.ammoShell;
				pickup->amount = 5;
				pickup->msgId[0] = 206;
				pickup->maxAmount = 50;
			} break;
			case ITEM_MINE:
			{
				pickup->type = ITYPE_AMMO;
				pickup->value = &s_playerInfo.ammoMine;
				pickup->amount = 1;
				pickup->msgId[0] = 207;
				pickup->maxAmount = 30;
			} break;
			case ITEM_MINES:
			{
				pickup->type = ITYPE_AMMO;
				pickup->value = &s_playerInfo.ammoMine;
				pickup->amount = 5;
				pickup->msgId[0] = 208;
				pickup->maxAmount = 30;
			} break;
			case ITEM_MISSILE:
			{
				pickup->type = ITYPE_AMMO;
				pickup->value = &s_playerInfo.ammoMissile;
				pickup->amount = 1;
				pickup->msgId[0] = 209;
				pickup->maxAmount = 20;
			} break;
			case ITEM_MISSILES:
			{
				pickup->type = ITYPE_AMMO;
				pickup->value = &s_playerInfo.ammoMissile;
				pickup->amount = 5;
				pickup->msgId[0] = 210;
				pickup->maxAmount = 20;
			} break;
			// PICKUPS & KEYS
			case ITEM_SHIELD:
			{
				pickup->type = ITYPE_AMMO;
				pickup->value = &s_playerInfo.shields;
				pickup->amount = 20;
				pickup->msgId[0] = 114;
				pickup->maxAmount = 200;
			} break;
			case ITEM_RED_KEY:
			{
				pickup->type = ITYPE_KEY_ITEM;
				pickup->item = &s_playerInfo.itemRedKey;
				pickup->msgId[0] = 300;
			} break;
			case ITEM_YELLOW_KEY:
			{
				pickup->type = ITYPE_KEY_ITEM;
				pickup->item = &s_playerInfo.itemYellowKey;
				pickup->msgId[0] = 301;
			} break;
			case ITEM_BLUE_KEY:
			{
				pickup->type = ITYPE_KEY_ITEM;
				pickup->item = &s_playerInfo.itemBlueKey;
				pickup->msgId[0] = 302;
			} break;
			case ITEM_GOGGLES:
			{
				pickup->type = ITYPE_KEY_ITEM;
				pickup->item = &s_playerInfo.itemGoggles;
				pickup->value = &s_energy;
				pickup->msgId[0] = 303;
				pickup->amount = ONE_16;
				pickup->maxAmount = 2 * ONE_16;
			} break;
			case ITEM_CLEATS:
			{
				pickup->type = ITYPE_KEY_ITEM;
				pickup->item = &s_playerInfo.itemCleats;
				pickup->msgId[0] = 304;
			} break;
			case ITEM_MASK:
			{
				pickup->type = ITYPE_KEY_ITEM;
				pickup->item = &s_playerInfo.itemMask;
				pickup->value = &s_energy;
				pickup->msgId[0] = 305;
				pickup->amount = ONE_16;
				pickup->maxAmount = 2 * ONE_16;
			} break;
			case ITEM_BATTERY:
			{
				pickup->type = ITYPE_AMMO;
				pickup->value = &s_energy;
				pickup->msgId[0] = 211;
				pickup->amount = ONE_16;
				pickup->maxAmount = 2 * ONE_16;
			} break;
			case ITEM_CODE1:
			{
				pickup->type = ITYPE_INV_ITEM;
				pickup->item = &s_playerInfo.itemCode1;
				pickup->msgId[0] = 501;

				obj->flags |= (OBJ_FLAG_FULLBRIGHT | OBJ_FLAG_BIT_6);
			} break;
			case ITEM_CODE2:
			{
				pickup->type = ITYPE_INV_ITEM;
				pickup->item = &s_playerInfo.itemCode2;
				pickup->msgId[0] = 502;

				obj->flags |= (OBJ_FLAG_FULLBRIGHT | OBJ_FLAG_BIT_6);
			} break;
			case ITEM_CODE3:
			{
				pickup->type = ITYPE_INV_ITEM;
				pickup->item = &s_playerInfo.itemCode3;
				pickup->msgId[0] = 503;

				obj->flags |= (OBJ_FLAG_FULLBRIGHT | OBJ_FLAG_BIT_6);
			} break;
			case ITEM_CODE4:
			{
				pickup->type = ITYPE_INV_ITEM;
				pickup->item = &s_playerInfo.itemCode4;
				pickup->msgId[0] = 504;

				obj->flags |= (OBJ_FLAG_FULLBRIGHT | OBJ_FLAG_BIT_6);
			} break;
			case ITEM_CODE5:
			{
				pickup->type = ITYPE_INV_ITEM;
				pickup->item = &s_playerInfo.itemCode5;
				pickup->msgId[0] = 505;

				obj->flags |= (OBJ_FLAG_FULLBRIGHT | OBJ_FLAG_BIT_6);
			} break;
			case ITEM_CODE6:
			{
				pickup->type = ITYPE_INV_ITEM;
				pickup->item = &s_playerInfo.itemCode6;
				pickup->msgId[0] = 506;

				obj->flags |= (OBJ_FLAG_FULLBRIGHT | OBJ_FLAG_BIT_6);
			} break;
			case ITEM_CODE7:
			{
				pickup->type = ITYPE_INV_ITEM;
				pickup->item = &s_playerInfo.itemCode7;
				pickup->msgId[0] = 507;

				obj->flags |= (OBJ_FLAG_FULLBRIGHT | OBJ_FLAG_BIT_6);
			} break;
			case ITEM_CODE8:
			{
				pickup->type = ITYPE_INV_ITEM;
				pickup->item = &s_playerInfo.itemCode8;
				pickup->msgId[0] = 508;

				obj->flags |= (OBJ_FLAG_FULLBRIGHT | OBJ_FLAG_BIT_6);
			} break;
			case ITEM_CODE9:
			{
				pickup->type = ITYPE_INV_ITEM;
				pickup->item = &s_playerInfo.itemCode9;
				pickup->msgId[0] = 509;

				obj->flags |= (OBJ_FLAG_FULLBRIGHT | OBJ_FLAG_BIT_6);
			} break;
			case ITEM_INVINCIBLE:
			{
				pickup->type = ITYPE_POWERUP;
				pickup->item = nullptr;
				pickup->msgId[0] = 306;
			} break;
			case ITEM_SUPERCHARGE:
			{
				pickup->type = ITYPE_POWERUP;
				pickup->item = nullptr;
				pickup->msgId[0] = 307;
			} break;
			case ITEM_REVIVE:
			{
				pickup->type = ITYPE_POWERUP;
				pickup->item = nullptr;
				pickup->msgId[0] = 308;
			} break;
			case ITEM_LIFE:
			{
				pickup->type = ITYPE_POWERUP;
				pickup->item = nullptr;
				pickup->msgId[0] = 310;
			} break;
			case ITEM_MEDKIT:
			{
				pickup->type = ITYPE_AMMO;
				pickup->value = &s_playerInfo.health;
				pickup->amount = 20;
				pickup->msgId[0] = 311;
				pickup->maxAmount = 100;
			} break;
			case ITEM_PILE:
			{
				pickup->type = ITYPE_SPECIAL;
			} break;
		}
		return (Logic*)pickup;
	}

}  // TFE_DarkForces