#include "player.h"

namespace TFE_DarkForces
{
	PlayerInfo s_playerInfo = { 0 };
	fixed16_16 s_energy = 2 * ONE_16;
	s32 s_lifeCount;

	JBool s_invincibility = JFALSE;
	JBool s_wearingCleats = JFALSE;
	JBool s_hasPlans      = JFALSE;
	JBool s_hasPhrik      = JFALSE;
	JBool s_hasNava       = JFALSE;
	JBool s_hasDatatape   = JFALSE;
	JBool s_hasDtWeapon   = JFALSE;
	JBool s_hasStolenInventory = JFALSE;

	SecObject* s_playerObject = nullptr;
	SecObject* s_playerEye = nullptr;
	vec3_fixed s_eyePos = { 0 };	// s_camX, s_camY, s_camZ in the DOS code.
	angle14_32 s_pitch = 0, s_yaw = 0, s_roll = 0;
}  // TFE_DarkForces