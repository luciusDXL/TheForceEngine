#pragma once
//////////////////////////
// TFE Custom Logics
//////////////////////////

#include <TFE_System/types.h>

struct CustomActorLogic
{
	const char* logicName;
	bool hasGravity = true;
	bool isFlying = false;
	u32 fov = 9557;

	const char* alertSound = "";
	const char* painSound = "";
	const char* attack1Sound = "";
	const char* attack2Sound = "";
	const char* dieSound = "";

	// Defaults are based on what is set by default in the original code
	u32 hitPoints = 4;
	s32 dropItem = -1;
	s32 dieEffect = -1;

	bool hasMeleeAttack = false;
	bool hasRangedAttack = true;
	bool litWithMeleeAttack = false;
	bool litWithRangedAttack = true;
	s32 projectile = 2;
	u32 rangedAttackDelay = 291;
	u32 meleeAttackDelay = 0;
	u32 meleeRange = 0;
	u32 meleeDamage = 0;
	u32 meleeRate = 230;
	u32 minAttackDist = 0;
	u32 maxAttackDist = 160;
	u32 fireSpread = 30;

	u32 speed = 4;
	u32 verticalSpeed = 10;
	u32 rotationSpeed = 0x7fff;
	u32 delay = 72;
	u32 startDelay = 2;
};

struct ExternalLogics
{
	std::vector<CustomActorLogic> actorLogics;
};

namespace TFE_Settings
{
	void parseLogicData(char* data, std::vector<CustomActorLogic> &actorLogics);
}