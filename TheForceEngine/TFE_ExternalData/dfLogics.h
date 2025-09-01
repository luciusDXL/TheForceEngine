#pragma once
//////////////////////////
// TFE Custom Logics
//////////////////////////

#include <TFE_System/types.h>

namespace TFE_ExternalData
{
	struct CustomActorLogic
	{
		const char* logicName;
		bool hasGravity = true;
		bool isFlying = false;
		u32 fov = 9557;
		u32 awareRange = 20;

		const char* alertSound = "";
		const char* painSound = "";
		const char* attack1Sound = "";
		const char* attack2Sound = "";
		const char* dieSound = "";

		// Defaults are based on what is set by default in the original code
		u32 hitPoints = 4;
		s32 dropItem = -1;
		s32 dieEffect = -1;
		bool stopOnDamage = true;

		bool hasMeleeAttack = false;
		bool hasRangedAttack = true;
		bool litWithMeleeAttack = false;
		bool litWithRangedAttack = true;
		s32 projectile = 2;
		u32 rangedAttackDelay = 291;
		u32 meleeAttackDelay = 0;
		u32 losDelay = 43695;
		u32 meleeRange = 0;
		u32 meleeDamage = 0;
		u32 meleeRate = 230;
		u32 minAttackDist = 0;
		u32 maxAttackDist = 160;
		u32 fireSpread = 30;
		vec3_float fireOffset = { 0, -1000, 0 };	// (y = -1000) will be treated as default

		u32 speed = 4;
		u32 verticalSpeed = 10;
		u32 rotationSpeed = 0x7fff;
		u32 approachVariation = 4096;
		u32 approachOffset = 3;
		u32 thinkerDelay = 2;

		f32 collisionWidth = -1;
		f32 collisionHeight = -1;
	};

	struct ExternalLogics
	{
		std::vector<CustomActorLogic> actorLogics;
	};

	ExternalLogics* getExternalLogics();
	void loadCustomLogics(); 
	void parseLogicData(char* data, const char* filename, std::vector<CustomActorLogic>& actorLogics);
}