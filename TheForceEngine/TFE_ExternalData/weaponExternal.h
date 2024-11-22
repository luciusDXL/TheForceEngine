#pragma once
#include <TFE_System/types.h>

///////////////////////////////////////////
// TFE Externalised Weapon data
///////////////////////////////////////////

namespace TFE_ExternalData
{
	struct ExternalProjectile
	{
		const char* type = nullptr;
		
		// Projectile object
		const char* assetType = "spirit";
		const char* asset = "";
		bool fullBright = false;
		bool zeroWidth = false;
		bool autoAim = false;
		bool movable = false;
		
		// Projectile logic
		const char* updateFunc = "";
		u32 damage;
		u32 falloffAmount;
		u32 nextFalloffTick;
		u32 damageFalloffDelta;
		u32 minDamage;
		u32 force;
		u32 speed;
		u32 horzBounciness;
		u32 vertBounciness;
		s32 bounceCount;
		u32 reflectVariation;
		u32 duration;
		s32 homingAngularSpeed;
		const char* flightSound = "";
		const char* reflectSound = "";
		const char* cameraPassSound = "";
		s32 reflectEffectId = -1;
		s32 hitEffectId = -1;
		bool explodeOnTimeout = false;
	};

	struct ExternalEffect
	{
		const char* type = nullptr;
		const char* wax = "";
		s32 force;
		s32 damage;
		s32 explosiveRange;
		s32 wakeupRange;
		const char* soundEffect = "";
		s32 soundPriority;
	};

	ExternalProjectile* getExternalProjectiles();
	ExternalEffect* getExternalEffects();
	void clearExternalProjectiles();
	void clearExternalEffects();
	void loadExternalProjectiles();
	void parseExternalProjectiles(char* data, bool fromMod);
	bool validateExternalProjectiles();
	void loadExternalEffects();
	void parseExternalEffects(char* data, bool fromMod);
	bool validateExternalEffects();
}