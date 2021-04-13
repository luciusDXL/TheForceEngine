#include "player.h"

namespace TFE_DarkForces
{
	PlayerInfo s_playerInfo = { 0 };
	fixed16_16 s_energy = 2 * ONE_16;
	SecObject* s_playerObject = nullptr;
}  // TFE_DarkForces