#include "pickup.h"
#include "player.h"
#include "hud.h"
#include <TFE_Jedi/InfSystem/message.h>
#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/Sound/soundSystem.h>
#include <TFE_Jedi/Task/task.h>

using namespace TFE_Message;
using namespace TFE_JediSound;
using namespace TFE_Level;
using namespace TFE_TaskSystem;

namespace TFE_DarkForces
{
	//////////////////////////////////////////////////////////////
	// Structures and Constants
	//////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////
	// Internal State
	//////////////////////////////////////////////////////////////
	SoundSourceID s_invCountdownSound = 0;
	SoundSourceID s_superchargeCountdownSound = 0;
	
	u32 s_pickupFlags = 0;
	// Pointer to memory where player inventory is saved.
	u32* s_playerInvSaved = nullptr;
	Task* s_pickupTask = nullptr;
	Task* s_uTask = nullptr;	// unkown task, was s_282acc
	Task* s_superchargeTask = nullptr;
	Task* s_invTask = nullptr;

	//////////////////////////////////////////////////////////////
	// Forward Declarations
	//////////////////////////////////////////////////////////////
	s32 pickup_addToValue(s32 curValue, s32 amountToAdd, s32 maxAmount);
	void pickupInvincibility();
	void pickupSupercharge();
	void pickupInventory();

	//////////////////////////////////////////////////////////////
	// API Implementation
	//////////////////////////////////////////////////////////////
	// TODO: Move keyword to pickup mapping to a datafile to avoid hardcoding.
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

	void pickup_cleanupFunc(Logic* logic)
	{
		deleteLogicAndObject(logic);
		free(logic);
	}
			
	// The current pickup being processed is stored in Message::s_msgTarget
	// The object "picking up" the item is stored in Message::s_msgEntity
	void pickupLogicFunc(s32 id)
	{
		struct LocalContext
		{
			SecObject* pickupObj;
			SecObject* entity;
			Pickup* pickup;
			JBool pickedUpItem;
		};
		task_begin_ctx;
		while (1)
		{
			// Sleep until called.
			task_yield(TASK_SLEEP);

			if (id == PICKUP_DELETE)
			{
				pickup_cleanupFunc((Logic*)s_msgTarget);
			}
			else if (id == PICKUP_ACQUIRE)
			{
				taskCtx->pickup = (Pickup*)s_msgTarget;
				taskCtx->entity = (SecObject*)s_msgEntity;
				// Only the player can pickup an item.
				if (!(taskCtx->entity->entityFlags & ETFLAG_PLAYER))
				{
					continue;
				}

				taskCtx->pickedUpItem = s_pickupFlags ? JFALSE : JTRUE;
				// Handle pickup based on type.
				if (taskCtx->pickup->type == ITYPE_WEAPON)
				{
					s32 maxAmount = taskCtx->pickup->maxAmount;
					if (!(*taskCtx->pickup->item) || *taskCtx->pickup->value < maxAmount)
					{
						*taskCtx->pickup->value = pickup_addToValue(*taskCtx->pickup->value, taskCtx->pickup->amount, maxAmount);
						if (*taskCtx->pickup->item)
						{
							hud_sendTextMessage(taskCtx->pickup->msgId[1]);
						}
						else
						{
							*taskCtx->pickup->item = JTRUE;
							hud_sendTextMessage(taskCtx->pickup->msgId[0]);
							s_playerInfo.currentWeapon = taskCtx->pickup->index;
						}
					}
					else
					{
						taskCtx->pickedUpItem = JFALSE;
					}
				}
				else if (taskCtx->pickup->type == ITYPE_AMMO)
				{
					// Shield powerups cannot be picked up while invincibility is enabled even if shields are at less than maximum.
					if (taskCtx->pickup->id == ITEM_SHIELD && s_invincibility)
					{
						taskCtx->pickedUpItem = JFALSE;
					}
					s32 curValue = *taskCtx->pickup->value;
					if (taskCtx->pickedUpItem && curValue < taskCtx->pickup->maxAmount)
					{
						*taskCtx->pickup->value = pickup_addToValue(*taskCtx->pickup->value, taskCtx->pickup->amount, taskCtx->pickup->maxAmount);
						hud_sendTextMessage(taskCtx->pickup->msgId[0]);
					}
				}
				else if (taskCtx->pickup->type == ITYPE_KEY_ITEM)
				{
					*taskCtx->pickup->item = JTRUE;
					hud_sendTextMessage(taskCtx->pickup->msgId[0]);
					if (taskCtx->pickup->value)
					{
						*taskCtx->pickup->value = pickup_addToValue(*taskCtx->pickup->value, taskCtx->pickup->amount, taskCtx->pickup->maxAmount);
					}
					if (taskCtx->pickup->id == ITEM_CLEATS && s_wearingCleats == 0)
					{
						s_wearingCleats = JTRUE;
					}
				}
				else if (taskCtx->pickup->type == ITYPE_OBJECTIVE)
				{
					*taskCtx->pickup->item = JTRUE;
					hud_sendTextMessage(taskCtx->pickup->msgId[0]);
					if (s_completeSector)
					{
						message_sendToSector(s_completeSector, nullptr, 0, MSG_TRIGGER);
					}

					switch (taskCtx->pickup->id)
					{
						case ITEM_PLANS:
						{
							s_goalItems.plans = JTRUE;
						} break;
						case ITEM_PHRIK:
						{
							s_goalItems.phrik = JTRUE;
						} break;
						case ITEM_NAVA:
						{
							s_goalItems.nava = JTRUE;
						} break;
						case ITEM_DT_WEAPON:
						{
							s_goalItems.dtWeapon = JTRUE;
						} break;
						case ITEM_DATATAPE:
						{
							s_goalItems.datatape = JTRUE;
						} break;
					}
				}
				else if (taskCtx->pickup->type == ITYPE_INV_ITEM)
				{
					*taskCtx->pickup->item = JTRUE;
					hud_sendTextMessage(taskCtx->pickup->msgId[0]);
				}
				else if (taskCtx->pickup->type == ITYPE_POWERUP)
				{
					switch (taskCtx->pickup->id)
					{
						case ITEM_INVINCIBLE:
						{
							pickupInvincibility();
						} break;
						case ITEM_SUPERCHARGE:
						{
							pickupSupercharge();
						} break;
						case ITEM_REVIVE:
						{
							if (s_playerInfo.health < 100 || s_playerInfo.shields < 200)
							{
								playerRevive();
								// The function sets shields to 100, so set it to the proper value here.
								s_playerInfo.shields = 200;
							}
							else
							{
								taskCtx->pickedUpItem = JFALSE;
							}
						} break;
						case ITEM_LIFE:
						{
							if (s_lifeCount < 9)
							{
								s_lifeCount++;
							}
							else
							{
								taskCtx->pickedUpItem = 0;
							}
						} break;
					}
					if (taskCtx->pickedUpItem)
					{
						hud_sendTextMessage(taskCtx->pickup->msgId[0]);
					}
				}
				else if (taskCtx->pickup->type == ITYPE_SPECIAL)
				{
					hud_sendTextMessage(312);
					pickupInventory();
					s_goalItems.stolenInv = JTRUE;

					if (s_completeSector)
					{
						message_sendToSector(s_completeSector, nullptr, 0, MSG_TRIGGER);
					}
				}

				// Finish
				if (!taskCtx->pickedUpItem)
				{
					continue;
				}
				// Set the world width to 0 so the object cannot be picked up even if it isn't fully deleted.
				taskCtx->pickupObj = taskCtx->pickup->logic.obj;
				taskCtx->pickupObj->worldWidth = 0;

				// Play pickup sound.
				if (taskCtx->pickup->type == ITYPE_KEY_ITEM || taskCtx->pickup->type == ITYPE_POWERUP)
				{
					playSound2D(s_powerupPickupSnd);
				}
				else if (taskCtx->pickup->type == ITYPE_OBJECTIVE || taskCtx->pickup->type == ITYPE_SPECIAL)
				{
					playSound2D(s_invItemPickupSnd);
				}
				else
				{
					playSound2D(s_wpnPickupSnd);
				}

				// Initialize effect
				s_flashEffect = FIXED(15);
				task_makeActive(s_pickupTask);

				// Delete the pickup next frame...
				{
					task_yield(0);
					// Remove the item.
					pickup_cleanupFunc((Logic*)taskCtx->pickup);
				}
			}
		}  // while (1)
		task_end;
	}

	// TODO: Move pickup data to an external data file to avoid hardcoding.
	Logic* obj_createPickup(SecObject* obj, ItemId id)
	{
		Pickup* pickup = (Pickup*)malloc(sizeof(Pickup));
		obj_addLogic(obj, (Logic*)pickup, s_pickupTask, pickup_cleanupFunc);

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
				pickup->type = ITYPE_OBJECTIVE;
				pickup->item = &s_playerInfo.itemPlans;
				pickup->msgId[0] = 400;

				obj->flags |= (OBJ_FLAG_FULLBRIGHT | OBJ_FLAG_BIT_6);
			} break;
			case ITEM_PHRIK:
			{
				pickup->type = ITYPE_OBJECTIVE;
				pickup->item = &s_playerInfo.itemPhrik;
				pickup->msgId[0] = 401;

				obj->flags |= (OBJ_FLAG_FULLBRIGHT | OBJ_FLAG_BIT_6);
			} break;
			case ITEM_NAVA:
			{
				pickup->type = ITYPE_OBJECTIVE;
				pickup->item = &s_playerInfo.itemNava;
				pickup->msgId[0] = 402;

				obj->flags |= (OBJ_FLAG_FULLBRIGHT | OBJ_FLAG_BIT_6);
			} break;
			case ITEM_DT_WEAPON:
			{
				pickup->type = ITYPE_OBJECTIVE;
				pickup->item = &s_playerInfo.itemDtWeapon;
				pickup->msgId[0] = 405;

				obj->flags |= OBJ_FLAG_BIT_6;
			} break;
			case ITEM_DATATAPE:
			{
				pickup->type = ITYPE_OBJECTIVE;
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

	void playerRevive()
	{
		// playerRevive() is called when the player respawns, which is why it sets 100 for shields here.
		// In the case of picking up the item, the value is then set to 200 after the function call.
		s_playerInfo.shields = 100;
		s_playerInfo.health = 100;
		s_playerInfo.showHit = 0;
		s_pickupFlags = 0;
	}

	//////////////////////////////////////////////////////////////
	// Internal Implementation
	//////////////////////////////////////////////////////////////
	s32 pickup_addToValue(s32 curValue, s32 amountToAdd, s32 maxAmount)
	{
		s32 newValue = curValue + amountToAdd;
		if (newValue < 0)
		{
			newValue = 0;
		}
		else if (newValue > maxAmount)
		{
			newValue = maxAmount;
		}
		return newValue;
	}

	void invincibilityTaskFunc(s32 id)
	{
		struct LocalContext
		{
			s32 i;
		};
		task_begin_ctx;
		task_waitWhileIdNotZero(6554);	// ~45 seconds.

		for (taskCtx->i = 4; taskCtx->i >= 0; taskCtx->i--)
		{
			playSound2D(s_invCountdownSound);
			s_playerInfo.shields = 200;
			task_waitWhileIdNotZero(87);	// 0.6 seconds.

			s_playerInfo.shields = 0xffffffff;
			task_waitWhileIdNotZero(87);	// 0.6 seconds.
		}
		s_playerInfo.shields = 200;
		s_invincibility = 0;
		s_invTask = nullptr;

		task_end;
	}

	void superchargeTaskFunc(s32 id)
	{
		struct LocalContext
		{
			s32 i;
		};
		task_begin_ctx;

		s_superChargeHud = JTRUE;
		s_superCharge = JTRUE;

		// This causes the task to be suspended for 45 seconds - which is the time the supercharge is active
		// without the running out warning.
		task_waitWhileIdNotZero(6554);	// ~45 seconds.

		for (taskCtx->i = 4; taskCtx->i >= 0; taskCtx->i--)
		{
			playSound2D(s_superchargeCountdownSound);
			s_superChargeHud = JFALSE;
			task_waitWhileIdNotZero(87);	// 0.6 seconds.

			s_superChargeHud = JTRUE;
			task_waitWhileIdNotZero(87);	// 0.6 seconds.
		}
		s_superCharge = JFALSE;
		s_superChargeHud = JFALSE;
		s_superchargeTask = nullptr;

		task_end;
	}
		
	void pickupInvincibility()
	{
		s_invincibility = JTRUE;
		s_playerInfo.shields = JTRUE;	// This seems like a bug...
		// Free old invincibility task and create a new invincibility task.
		if (s_invTask)
		{
			freeTask(s_invTask);
		}
		s_invTask = createTask(invincibilityTaskFunc);
	}

	void pickupSupercharge()
	{
		// Free old supercharge task and create a new supercharge task.
		if (s_superchargeTask)
		{
			freeTask(s_superchargeTask);
		}
		s_superchargeTask = createTask(superchargeTaskFunc);
	}

	void pickupInventory()
	{
		// Get the size of the PlayerInfo structure up to but not including s_playerInfo.stateUnknown.
		size_t size = (size_t)&s_playerInfo.stateUnknown - (size_t)&s_playerInfo;
		if (!s_playerInvSaved)
		{
			return;
		}
		// Copy the saved player info and add it to the current player info.
		u32* dst = (u32*)&s_playerInfo;
		u32* src = s_playerInvSaved;
		for (s32 sizeCopied = 0; sizeCopied < size; sizeCopied += 4, src++, dst++)
		{
			(*dst) += (*src);
		}

		// Clear out the nava card item since it is one of the current objectives.
		s_playerInfo.itemNava = JFALSE;
		if (s_playerInfo.index1 != s_playerInfo.index0)
		{
			if (s_uTask)
			{
				s_msgArg1 = s_playerInfo.index1;
				runTask(s_uTask, 2);
				s_playerInfo.index1 = max(s_playerInfo.index0, s_playerInfo.index1);
			}
			else
			{
				s_playerInfo.index2 = s_playerInfo.index1;
			}
		}

		// Free saved inventory.
		free(s_playerInvSaved);
		s_playerInvSaved = nullptr;
		// Clamp ammo values.
		s_playerInfo.ammoEnergy    = pickup_addToValue(s_playerInfo.ammoEnergy,    0, 500);
		s_playerInfo.ammoPower     = pickup_addToValue(s_playerInfo.ammoPower,     0, 500);
		s_playerInfo.ammoPlasma    = pickup_addToValue(s_playerInfo.ammoPlasma,    0, 400);
		s_playerInfo.ammoDetonator = pickup_addToValue(s_playerInfo.ammoDetonator, 0,  50);
		s_playerInfo.ammoShell     = pickup_addToValue(s_playerInfo.ammoShell,     0,  50);
		s_playerInfo.ammoMine      = pickup_addToValue(s_playerInfo.ammoMine,      0,  30);
		s_playerInfo.ammoMissile   = pickup_addToValue(s_playerInfo.ammoMissile,   0,  20);
	}

}  // TFE_DarkForces