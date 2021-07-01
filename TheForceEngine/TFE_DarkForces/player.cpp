#include "player.h"

namespace TFE_DarkForces
{
	PlayerInfo s_playerInfo = { 0 };
	GoalItems s_goalItems   = { 0 };
	fixed16_16 s_energy = 2 * ONE_16;
	s32 s_lifeCount;

	JBool s_invincibility = JFALSE;
	JBool s_wearingCleats = JFALSE;
	JBool s_superCharge   = JFALSE;
	JBool s_superChargeHud= JFALSE;
	JBool s_goals[16] = { 0 };

	SecObject* s_playerObject = nullptr;
	SecObject* s_playerEye = nullptr;
	vec3_fixed s_eyePos = { 0 };	// s_camX, s_camY, s_camZ in the DOS code.
	angle14_32 s_pitch = 0, s_yaw = 0, s_roll = 0;
}  // TFE_DarkForces