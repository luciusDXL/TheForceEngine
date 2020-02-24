#pragma once
//////////////////////////////////////////////////////////////////////
// DarkXL 2 Script System
//////////////////////////////////////////////////////////////////////
#include <DXL2_System/types.h>
#include "weapons.h"

class DXL2_Renderer;
class Player;

namespace DXL2_WeaponSystem
{
	bool init(DXL2_Renderer* renderer);
	void shutdown();

	void switchToWeapon(Weapon weapon);
	// Motion is roughly a 0 - 1 value that maps from player motion.
	void update(f32 motion, Player* player);
	void draw(Player* player, Vec3f* cameraPos, u8 ambient);

	void shoot(Player* player, const Vec2f* dir);
}
