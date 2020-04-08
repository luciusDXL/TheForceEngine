#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Script System
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "weapons.h"

class TFE_Renderer;
class Player;

namespace TFE_WeaponSystem
{
	bool init(TFE_Renderer* renderer);
	void shutdown();

	void switchToWeapon(Weapon weapon);
	// Motion is roughly a 0 - 1 value that maps from player motion.
	void update(f32 motion, Player* player);
	void draw(Player* player, Vec3f* cameraPos, u8 ambient);

	void shoot(Player* player, const Vec2f* dir);
	void release();
}
