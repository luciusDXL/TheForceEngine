#include <cstring>
#include <vector>
#include <TFE_Settings/logics.h>
#include <TFE_System/cJSON.h>
#include <TFE_System/system.h>

namespace TFE_Settings
{
	const char* s_projectileTable[] =
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
	
	const char* s_dropItemTable[] =
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
	};

	const char* s_dieEffectTable[] =
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


	///////////////////////////
	/// Forward declarations 
	///////////////////////////
	bool tryAssignProperty(cJSON* data, CustomActorLogic& customLogic);

	
	void parseLogicData(char* data, std::vector<CustomActorLogic> &actorLogics)
	{
		cJSON* root = cJSON_Parse(data);
		if (root)
		{
			cJSON* section = root->child;
			if (section && cJSON_IsArray(section) && strcasecmp(section->string, "logics") == 0)
			{
				cJSON* logic = section->child;
				while (logic)
				{
					cJSON* logicName = logic->child;

					// get the logic name
					if (logicName && cJSON_IsString(logicName) && strcasecmp(logicName->string, "logicname") == 0)
					{
						CustomActorLogic customLogic;
						customLogic.logicName = logicName->valuestring;

						cJSON* logicData = logicName->next;
						if (logicData && cJSON_IsObject(logicData))
						{
							cJSON* dataItem = logicData->child;

							// iterate through the data and assign properties
							while (dataItem)
							{
								tryAssignProperty(dataItem, customLogic);
								dataItem = dataItem->next;
							}
						}

						actorLogics.push_back(customLogic);
					}

					logic = logic->next;
				}
			}
		}
		else
		{
			const char* error = cJSON_GetErrorPtr();
			if (error)
			{
				TFE_System::logWrite(LOG_ERROR, "LOGICS", "Failed to parse json before\n%s", error);
			}
			else
			{
				TFE_System::logWrite(LOG_ERROR, "LOGICS", "Failed to parse json");
			}
		}
	}

	bool tryAssignProperty(cJSON* data, CustomActorLogic &customLogic)
	{
		if (!data)
		{
			return false;
		}
		
		if (cJSON_IsBool(data) && strcasecmp(data->string, "hasGravity") == 0)
		{
			customLogic.hasGravity = cJSON_IsTrue(data);
			return true;
		}

		if (cJSON_IsBool(data) && strcasecmp(data->string, "isFlying") == 0)
		{
			customLogic.isFlying = cJSON_IsTrue(data);
			return true;
		}

		if (cJSON_IsNumber(data) && strcasecmp(data->string, "fieldOfView") == 0)
		{
			customLogic.fov = data->valueint * 45;
			return true;
		}

		if (cJSON_IsString(data) && strcasecmp(data->string, "alertSound") == 0)
		{
			customLogic.alertSound = data->valuestring;
			return true;
		}
		
		if (cJSON_IsString(data) && strcasecmp(data->string, "painSound") == 0)
		{
			customLogic.painSound = data->valuestring;
			return true;
		}

		if (cJSON_IsString(data) && strcasecmp(data->string, "dieSound") == 0)
		{
			customLogic.dieSound = data->valuestring;
			return true;
		}

		if (cJSON_IsString(data) && strcasecmp(data->string, "attack1Sound") == 0)
		{
			customLogic.attack1Sound = data->valuestring;
			return true;
		}

		if (cJSON_IsString(data) && strcasecmp(data->string, "attack2Sound") == 0)
		{
			customLogic.attack2Sound = data->valuestring;
			return true;
		}

		if (cJSON_IsNumber(data) && strcasecmp(data->string, "hitPoints") == 0)
		{
			customLogic.hitPoints = data->valueint;
			return true;
		}

		// Drop item as number
		if (cJSON_IsNumber(data) && strcasecmp(data->string, "dropItem") == 0)
		{
			customLogic.dropItem = data->valueint;
			return true;
		}

		// Drop item as string
		if (cJSON_IsString(data) && strcasecmp(data->string, "dropItem") == 0)
		{
			for (int i = 0; i <= 44; i++)
			{
				if (strcasecmp(data->valuestring, s_dropItemTable[i]) == 0)
				{
					customLogic.dropItem = i;
					return true;
				}
			}

			return false;
		}

		// Die effect as a number
		if (cJSON_IsNumber(data) && strcasecmp(data->string, "dieEffect") == 0)
		{
			customLogic.dieEffect = data->valueint;
			return true;
		}

		// Die effect as a string
		if (cJSON_IsString(data) && strcasecmp(data->string, "dieEffect") == 0)
		{
			for (int i = 0; i <= 17; i++)
			{
				if (strcasecmp(data->valuestring, s_dieEffectTable[i]) == 0)
				{
					customLogic.dieEffect = i;
					return true;
				}
			}

			return false;
		}

		if (cJSON_IsBool(data) && strcasecmp(data->string, "hasMeleeAttack") == 0)
		{
			customLogic.hasMeleeAttack = cJSON_IsTrue(data);
			return true;
		}

		if (cJSON_IsBool(data) && strcasecmp(data->string, "hasRangedAttack") == 0)
		{
			customLogic.hasRangedAttack = cJSON_IsTrue(data);
			return true;
		}

		if (cJSON_IsBool(data) && strcasecmp(data->string, "litWithMeleeAttack") == 0)
		{
			customLogic.litWithMeleeAttack = cJSON_IsTrue(data);
			return true;
		}
		
		if (cJSON_IsBool(data) && strcasecmp(data->string, "litWithRangedAttack") == 0)
		{
			customLogic.litWithRangedAttack = cJSON_IsTrue(data);
			return true;
		}
		
		// Projectile as number
		if (cJSON_IsNumber(data) && strcasecmp(data->string, "projectile") == 0)
		{
			customLogic.projectile = data->valueint;
			return true;
		}

		// Projectile as string
		if (cJSON_IsString(data) && strcasecmp(data->string, "projectile") == 0)
		{
			for (int i = 0; i <= 18; i++)
			{
				if (strcasecmp(data->valuestring, s_projectileTable[i]) == 0)
				{
					customLogic.projectile = i;
					return true;
				}
			}

			return false;
		}

		if (cJSON_IsNumber(data) && strcasecmp(data->string, "rangedAttackDelay") == 0)
		{
			customLogic.rangedAttackDelay = data->valueint;
			return true;
		}

		if (cJSON_IsNumber(data) && strcasecmp(data->string, "meleeAttackDelay") == 0)
		{
			customLogic.meleeAttackDelay = data->valueint;
			return true;
		}

		if (cJSON_IsNumber(data) && strcasecmp(data->string, "meleeRange") == 0)
		{
			customLogic.meleeRange = data->valueint;
			return true;
		}

		if (cJSON_IsNumber(data) && strcasecmp(data->string, "meleeDamage") == 0)
		{
			customLogic.meleeDamage = data->valueint;
			return true;
		}

		if (cJSON_IsNumber(data) && strcasecmp(data->string, "meleeRate") == 0)
		{
			customLogic.meleeRate = data->valueint;
			return true;
		}

		if (cJSON_IsNumber(data) && strcasecmp(data->string, "minAttackDist") == 0)
		{
			customLogic.minAttackDist = data->valueint;
			return true;
		}

		if (cJSON_IsNumber(data) && strcasecmp(data->string, "maxAttackDist") == 0)
		{
			customLogic.maxAttackDist = data->valueint;
			return true;
		}

		if (cJSON_IsNumber(data) && strcasecmp(data->string, "fireSpread") == 0)
		{
			customLogic.fireSpread = data->valueint;
			return true;
		}

		if (cJSON_IsNumber(data) && strcasecmp(data->string, "speed") == 0)
		{
			customLogic.speed = data->valueint;
			return true;
		}

		if (cJSON_IsNumber(data) && strcasecmp(data->string, "verticalSpeed") == 0)
		{
			customLogic.verticalSpeed = data->valueint;
			return true;
		}

		return false;
	}
}