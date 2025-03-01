#include <cstring>
#include <vector>
#include <TFE_System/cJSON.h>
#include <TFE_System/system.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_DarkForces/item.h>
#include <TFE_DarkForces/player.h>
#include "pickupExternal.h"
#include "logicTables.h"

namespace TFE_ExternalData
{
	static MaxAmounts s_externalMaxAmounts;
	static ExternalPickup s_externalPickups[TFE_DarkForces::ITEM_COUNT];
	static bool s_externalPickupsFromMod = false;

	////////////////////////////////
	// Forward Declarations
	////////////////////////////////
	int getPickupIndex(char* name);
	bool tryAssignMaxAmount(cJSON* data);
	bool tryAssignPickupProperty(cJSON* data, ExternalPickup& pickup);


	MaxAmounts* getMaxAmounts()
	{
		return &s_externalMaxAmounts;
	}

	ExternalPickup* getExternalPickups()
	{
		return s_externalPickups;
	}

	void clearExternalPickups()
	{
		s_externalPickupsFromMod = false;
		for (int i = 0; i < TFE_DarkForces::ITEM_COUNT; i++)
		{
			s_externalPickups[i].name = nullptr;
		}

		// Restore defaults
		s_externalMaxAmounts.ammoEnergyMax = 500;
		s_externalMaxAmounts.ammoPowerMax = 500;
		s_externalMaxAmounts.ammoShellMax = 50;
		s_externalMaxAmounts.ammoPlasmaMax = 400;
		s_externalMaxAmounts.ammoDetonatorMax = 50;
		s_externalMaxAmounts.ammoMineMax = 30;
		s_externalMaxAmounts.ammoMissileMax = 20;
		s_externalMaxAmounts.shieldsMax = 200;
		s_externalMaxAmounts.batteryPowerMax = 2 * ONE_16;
		s_externalMaxAmounts.healthMax = 100;
	}

	void loadExternalPickups()
	{
		char extDataFile[TFE_MAX_PATH];
		strcpy(extDataFile, "ExternalData/DarkForces/pickups.json");
		if (!TFE_Paths::mapSystemPath(extDataFile))
		{
			const char* programDir = TFE_Paths::getPath(PATH_PROGRAM);
			sprintf(extDataFile, "%sExternalData/DarkForces/pickups.json", programDir);
		}

		TFE_System::logWrite(LOG_MSG, "EXTERNAL_DATA", "Loading pickup data");
		FileStream file;
		if (!file.open(extDataFile, FileStream::MODE_READ)) { return; }

		const size_t size = file.getSize();
		char* data = (char*)malloc(size + 1);
		if (!data || size == 0)
		{
			TFE_System::logWrite(LOG_ERROR, "EXTERNAL_DATA", "Pickups.json is %u bytes in size and cannot be read.", size);
			return;
		}
		file.readBuffer(data, (u32)size);
		data[size] = 0;
		file.close();

		parseExternalPickups(data, false);
		free(data);
	}

	void parseExternalPickups(char* data, bool fromMod)
	{
		// If pickups have already been loaded from a mod, don't replace them
		if (s_externalPickupsFromMod)
		{
			return;
		}

		cJSON* root = cJSON_Parse(data);
		if (root)
		{
			cJSON* section = root->child;
			while (section)
			{
				if (cJSON_IsObject(section) && strcasecmp(section->string, "maxAmounts") == 0)
				{
					// parse the Max Amounts
					cJSON* maxAmt = section->child;
					while (maxAmt)
					{
						if (cJSON_IsNumber(maxAmt))
						{
							tryAssignMaxAmount(maxAmt);
						}
						
						maxAmt = maxAmt->next;
					}
				}
				else if (cJSON_IsArray(section) && strcasecmp(section->string, "pickups") == 0)
				{
					cJSON* pickup = section->child;
					while (pickup)
					{
						cJSON* pickupName = pickup->child;

						// get the pickup name
						if (pickupName && cJSON_IsString(pickupName) && strcasecmp(pickupName->string, "name") == 0)
						{
							// For now stick with the hardcoded DF list
							int index = getPickupIndex(pickupName->valuestring);

							if (index >= 0)
							{
								ExternalPickup extPickup = {};
								extPickup.name = pickupName->valuestring;

								cJSON* pickupData = pickupName->next;
								if (pickupData && cJSON_IsObject(pickupData))
								{
									cJSON* dataItem = pickupData->child;

									// iterate through the data and assign properties
									while (dataItem)
									{
										tryAssignPickupProperty(dataItem, extPickup);
										dataItem = dataItem->next;
									}
								}

								s_externalPickups[index] = extPickup;
							}
						}

						pickup = pickup->next;
					}
				}

				section = section->next;
			}

			s_externalPickupsFromMod = fromMod && validateExternalPickups();
		}
		else
		{
			const char* error = cJSON_GetErrorPtr();
			if (error)
			{
				TFE_System::logWrite(LOG_ERROR, "EXTERNAL_DATA", "Failed to parse json before\n%s", error);
			}
			else
			{
				TFE_System::logWrite(LOG_ERROR, "EXTERNAL_DATA", "Failed to parse json");
			}
		}
	}

	bool validateExternalPickups()
	{
		// If the name field is null, we can assume the pickup's data hasn't loaded properly
		for (int i = 0; i < TFE_DarkForces::ITEM_COUNT; i++)
		{
			if (!s_externalPickups[i].name)
			{
				return false;
			}
		}

		return true;
	}

	int getPickupIndex(char* name)
	{
		for (int i = 0; i < TFE_DarkForces::ITEM_COUNT; i++)
		{
			if (strcasecmp(name, TFE_ExternalData::df_dropItemTable[i]) == 0)
			{
				return i;
			}
		}

		return -1;
	}

	bool tryAssignMaxAmount(cJSON* data)
	{
		if (strcasecmp(data->string, "ammoEnergy") == 0)
		{
			s_externalMaxAmounts.ammoEnergyMax = data->valueint;
			return true;
		}

		if (strcasecmp(data->string, "ammoPower") == 0)
		{
			s_externalMaxAmounts.ammoPowerMax = data->valueint;
			return true;
		}

		if (strcasecmp(data->string, "ammoShell") == 0)
		{
			s_externalMaxAmounts.ammoShellMax = data->valueint;
			return true;
		}

		if (strcasecmp(data->string, "ammoPlasma") == 0)
		{
			s_externalMaxAmounts.ammoPlasmaMax = data->valueint;
			return true;
		}

		if (strcasecmp(data->string, "ammoDetonator") == 0)
		{
			s_externalMaxAmounts.ammoDetonatorMax = data->valueint;
			return true;
		}

		if (strcasecmp(data->string, "ammoMine") == 0)
		{
			s_externalMaxAmounts.ammoMineMax = data->valueint;
			return true;
		}

		if (strcasecmp(data->string, "ammoMissile") == 0)
		{
			s_externalMaxAmounts.ammoMissileMax = data->valueint;
			return true;
		}

		if (strcasecmp(data->string, "shields") == 0)
		{
			s_externalMaxAmounts.shieldsMax = data->valueint;
			return true;
		}

		if (strcasecmp(data->string, "batteryPower") == 0)
		{
			// battery power is exposed as a percentage
			f32 fraction = data->valueint / 100.0f;
			s_externalMaxAmounts.batteryPowerMax = s32(fraction * 2 * ONE_16);
			return true;
		}

		if (strcasecmp(data->string, "health") == 0)
		{
			s_externalMaxAmounts.healthMax = data->valueint;
			return true;
		}

		return false;
	}

	bool tryAssignPickupProperty(cJSON* data, ExternalPickup& pickup)
	{
		if (cJSON_IsString(data) && strcasecmp(data->string, "type") == 0)
		{
			if (strcasecmp(data->valuestring, "objective") == 0)
			{
				pickup.type = TFE_DarkForces::ITYPE_OBJECTIVE;
				return true;
			}
			if (strcasecmp(data->valuestring, "weapon") == 0)
			{
				pickup.type = TFE_DarkForces::ITYPE_WEAPON;
				return true;
			}

			if (strcasecmp(data->valuestring, "ammo") == 0)
			{
				pickup.type = TFE_DarkForces::ITYPE_AMMO;
				return true;
			}
			if (strcasecmp(data->valuestring, "usable") == 0)
			{
				pickup.type = TFE_DarkForces::ITYPE_USABLE;
				return true;
			}
			if (strcasecmp(data->valuestring, "codekey") == 0)
			{
				pickup.type = TFE_DarkForces::ITYPE_CODEKEY;
				return true;
			}
			if (strcasecmp(data->valuestring, "powerup") == 0)
			{
				pickup.type = TFE_DarkForces::ITYPE_POWERUP;
				return true;
			}
			if (strcasecmp(data->valuestring, "special") == 0)
			{
				pickup.type = TFE_DarkForces::ITYPE_SPECIAL;
				return true;
			}

			return false;
		}

		if (cJSON_IsNumber(data) && strcasecmp(data->string, "weaponIndex") == 0)
		{
			pickup.weaponIndex = data->valueint;
			return true;
		}
		
		if (cJSON_IsString(data) && strcasecmp(data->string, "playerItem") == 0)
		{
			if (strcasecmp(data->valuestring, "itemPlans") == 0)
			{
				pickup.playerItem = &TFE_DarkForces::s_playerInfo.itemPlans;
				return true;
			}

			if (strcasecmp(data->valuestring, "itemNava") == 0)
			{
				pickup.playerItem = &TFE_DarkForces::s_playerInfo.itemNava;
				return true;
			}

			if (strcasecmp(data->valuestring, "itemPhrik") == 0)
			{
				pickup.playerItem = &TFE_DarkForces::s_playerInfo.itemPhrik;
				return true;
			}

			if (strcasecmp(data->valuestring, "itemDtWeapon") == 0)
			{
				pickup.playerItem = &TFE_DarkForces::s_playerInfo.itemDtWeapon;
				return true;
			}

			if (strcasecmp(data->valuestring, "itemDatatape") == 0)
			{
				pickup.playerItem = &TFE_DarkForces::s_playerInfo.itemDatatape;
				return true;
			}

			if (strcasecmp(data->valuestring, "itemUnused") == 0)
			{
				pickup.playerItem = &TFE_DarkForces::s_playerInfo.itemUnused;
				return true;
			}

			if (strcasecmp(data->valuestring, "itemRifle") == 0)
			{
				pickup.playerItem = &TFE_DarkForces::s_playerInfo.itemRifle;
				return true;
			}

			if (strcasecmp(data->valuestring, "itemAutogun") == 0)
			{
				pickup.playerItem = &TFE_DarkForces::s_playerInfo.itemAutogun;
				return true;
			}

			if (strcasecmp(data->valuestring, "itemMortar") == 0)
			{
				pickup.playerItem = &TFE_DarkForces::s_playerInfo.itemMortar;
				return true;
			}

			if (strcasecmp(data->valuestring, "itemFusion") == 0)
			{
				pickup.playerItem = &TFE_DarkForces::s_playerInfo.itemFusion;
				return true;
			}

			if (strcasecmp(data->valuestring, "itemConcussion") == 0)
			{
				pickup.playerItem = &TFE_DarkForces::s_playerInfo.itemConcussion;
				return true;
			}

			if (strcasecmp(data->valuestring, "itemCannon") == 0)
			{
				pickup.playerItem = &TFE_DarkForces::s_playerInfo.itemCannon;
				return true;
			}

			if (strcasecmp(data->valuestring, "itemRedKey") == 0)
			{
				pickup.playerItem = &TFE_DarkForces::s_playerInfo.itemRedKey;
				return true;
			}

			if (strcasecmp(data->valuestring, "itemYellowKey") == 0)
			{
				pickup.playerItem = &TFE_DarkForces::s_playerInfo.itemYellowKey;
				return true;
			}

			if (strcasecmp(data->valuestring, "itemBlueKey") == 0)
			{
				pickup.playerItem = &TFE_DarkForces::s_playerInfo.itemBlueKey;
				return true;
			}

			if (strcasecmp(data->valuestring, "itemGoggles") == 0)
			{
				pickup.playerItem = &TFE_DarkForces::s_playerInfo.itemGoggles;
				return true;
			}

			if (strcasecmp(data->valuestring, "itemCleats") == 0)
			{
				pickup.playerItem = &TFE_DarkForces::s_playerInfo.itemCleats;
				return true;
			}

			if (strcasecmp(data->valuestring, "itemMask") == 0)
			{
				pickup.playerItem = &TFE_DarkForces::s_playerInfo.itemMask;
				return true;
			}

			if (strcasecmp(data->valuestring, "itemCode1") == 0)
			{
				pickup.playerItem = &TFE_DarkForces::s_playerInfo.itemCode1;
				return true;
			}

			if (strcasecmp(data->valuestring, "itemCode2") == 0)
			{
				pickup.playerItem = &TFE_DarkForces::s_playerInfo.itemCode2;
				return true;
			}

			if (strcasecmp(data->valuestring, "itemCode3") == 0)
			{
				pickup.playerItem = &TFE_DarkForces::s_playerInfo.itemCode3;
				return true;
			}

			if (strcasecmp(data->valuestring, "itemCode4") == 0)
			{
				pickup.playerItem = &TFE_DarkForces::s_playerInfo.itemCode4;
				return true;
			}

			if (strcasecmp(data->valuestring, "itemCode5") == 0)
			{
				pickup.playerItem = &TFE_DarkForces::s_playerInfo.itemCode5;
				return true;
			}

			if (strcasecmp(data->valuestring, "itemCode6") == 0)
			{
				pickup.playerItem = &TFE_DarkForces::s_playerInfo.itemCode6;
				return true;
			}

			if (strcasecmp(data->valuestring, "itemCode7") == 0)
			{
				pickup.playerItem = &TFE_DarkForces::s_playerInfo.itemCode7;
				return true;
			}

			if (strcasecmp(data->valuestring, "itemCode8") == 0)
			{
				pickup.playerItem = &TFE_DarkForces::s_playerInfo.itemCode8;
				return true;
			}

			if (strcasecmp(data->valuestring, "itemCode9") == 0)
			{
				pickup.playerItem = &TFE_DarkForces::s_playerInfo.itemCode9;
				return true;
			}

			return false;
		}

		if (cJSON_IsString(data) && strcasecmp(data->string, "playerAmmo") == 0)
		{
			if (strcasecmp(data->valuestring, "ammoEnergy") == 0)
			{
				pickup.playerAmmo = &TFE_DarkForces::s_playerInfo.ammoEnergy;
				return true;
			}

			if (strcasecmp(data->valuestring, "ammoPower") == 0)
			{
				pickup.playerAmmo = &TFE_DarkForces::s_playerInfo.ammoPower;
				return true;
			}

			if (strcasecmp(data->valuestring, "ammoPlasma") == 0)
			{
				pickup.playerAmmo = &TFE_DarkForces::s_playerInfo.ammoPlasma;
				return true;
			}

			if (strcasecmp(data->valuestring, "ammoDetonator") == 0)
			{
				pickup.playerAmmo = &TFE_DarkForces::s_playerInfo.ammoDetonator;
				return true;
			}

			if (strcasecmp(data->valuestring, "ammoShell") == 0)
			{
				pickup.playerAmmo = &TFE_DarkForces::s_playerInfo.ammoShell;
				return true;
			}

			if (strcasecmp(data->valuestring, "ammoMine") == 0)
			{
				pickup.playerAmmo = &TFE_DarkForces::s_playerInfo.ammoMine;
				return true;
			}

			if (strcasecmp(data->valuestring, "ammoMissile") == 0)
			{
				pickup.playerAmmo = &TFE_DarkForces::s_playerInfo.ammoMissile;
				return true;
			}

			if (strcasecmp(data->valuestring, "shields") == 0)
			{
				pickup.playerAmmo = &TFE_DarkForces::s_playerInfo.shields;
				return true;
			}

			if (strcasecmp(data->valuestring, "batteryPower") == 0)
			{
				pickup.playerAmmo = &TFE_DarkForces::s_batteryPower;
				return true;
			}

			if (strcasecmp(data->valuestring, "health") == 0)
			{
				pickup.playerAmmo = &TFE_DarkForces::s_playerInfo.health;
				return true;
			}

			return false;
		}

		if (cJSON_IsNumber(data) && strcasecmp(data->string, "amount") == 0)
		{
			pickup.amount = data->valueint;
			return true;
		}

		if (cJSON_IsNumber(data) && strcasecmp(data->string, "message1") == 0)
		{
			pickup.message1 = data->valueint;
			return true;
		}

		if (cJSON_IsNumber(data) && strcasecmp(data->string, "message2") == 0)
		{
			pickup.message2 = data->valueint;
			return true;
		}

		if (cJSON_IsBool(data) && strcasecmp(data->string, "fullBright") == 0)
		{
			pickup.fullBright = cJSON_IsTrue(data);
			return true;
		}

		if (cJSON_IsBool(data) && strcasecmp(data->string, "noRemove") == 0)
		{
			pickup.noRemove = cJSON_IsTrue(data);
			return true;
		}

		if (cJSON_IsString(data) && strcasecmp(data->string, "asset") == 0)
		{
			pickup.asset = data->valuestring;
			return true;
		}

		return false;
	}
}