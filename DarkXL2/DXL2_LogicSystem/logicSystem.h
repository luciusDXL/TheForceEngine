#pragma once
//////////////////////////////////////////////////////////////////////
// DarkXL 2 Script System
//////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>
#include <vector>
struct GameObject;
struct Logic;
struct EnemyGenerator;
class Player;

namespace DXL2_LogicSystem
{
	bool init(Player* player);
	void shutdown();

	void update();

	void clearObjectLogics();
	bool registerObjectLogics(GameObject* gameObject, const std::vector<Logic>& logics, const std::vector<EnemyGenerator>& generators);

	void damageObject(const GameObject* gameObject, s32 damage);
	void sendPlayerCollisionTrigger(const GameObject* gameObject);
}
