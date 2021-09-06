#include "player.h"
#include "automap.h"
#include "agent.h"
#include "config.h"
#include "hud.h"
#include "pickup.h"
#include "weapon.h"
#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/Renderer/jediRenderer.h>
#include <TFE_Jedi/Renderer/RClassic_Fixed/rclassicFixed.h>

namespace TFE_DarkForces
{
	enum PlayerConstants
	{
		PLAYER_HEIGHT                  = 0x5cccc,	  // 5.8 units
		PLAYER_WIDTH                   = 0x1cccc,	  // 1.8 units
		PLAYER_PICKUP_ADJ              = 0x18000,	  // 1.5 units
		PLAYER_SIZE_SMALL              = 0x4ccc,	  // 0.3 units
		PLAYER_STEP                    = 0x38000,	  // 3.5 units
		PLAYER_INF_STEP                = FIXED(9999),
		PLAYER_HEADWAVE_VERT_SPD       = 0xc000,	  // 0.75   units/sec.
		PLAYER_HEADWAVE_VERT_WATER_SPD = 0x3000,	  // 0.1875 units/sec.
		PLAYER_DMG_FLOOR_LOW           = FIXED(5),	  // Low  damage floors apply  5 dmg/sec.
		PLAYER_DMG_FLOOR_HIGH          = FIXED(10),	  // High damage floors apply 10 dmg/sec.
		PLAYER_DMG_WALL				   = FIXED(20),	  // Damage walls apply 20 dmg/sec.
		PLAYER_FALL_SCREAM_VEL         = FIXED(60),	  // Fall speed when the player starts screaming.
		PLAYER_FALL_SCREAM_MINHEIGHT   = FIXED(55),   // The player needs to be at least 55 units from the ground before screaming.
		PLAYER_FALL_HIT_SND_HEIGHT     = 0x4000,	  // 0.25 units, fall height for player to make a sound when hitting the ground.
		PLAYER_LAND_VEL_CHANGE		   = FIXED(60),	  // Point wear landing velocity to head change velocity changes slope.
		PLAYER_LAND_VEL_MAX            = FIXED(1000), // Maximum head change landing velocity.

		PLAYER_FORWARD_SPEED           = FIXED(256),  // Forward speed in units/sec.
		PLAYER_STRAFE_SPEED            = FIXED(192),  // Strafe speed in units/sec.
		PLAYER_CROUCH_SPEED            = 747108,      // 11.4 units/sec
		PLAYER_MOUSE_TURN_SPD          = 8,           // Mouse position delta to turn speed multiplier.
		PLAYER_KB_TURN_SPD             = 3413,        // Keyboard turn speed in angles/sec.
		PLAYER_JUMP_IMPULSE            = -2850816,    // Jump vertical impuse: -43.5 units/sec.

		PITCH_LIMIT = 2047,
	};

	struct PlayerLogic
	{
		Logic logic;

		vec2_fixed dir;
		vec3_fixed move;
		fixed16_16 stepHeight;
	};

	PlayerInfo s_playerInfo = { 0 };
	PlayerLogic s_playerLogic = { 0 };
	GoalItems s_goalItems   = { 0 };
	fixed16_16 s_energy = 2 * ONE_16;
	s32 s_lifeCount;
	s32 s_playerLight = 0;
	u32 s_moveAvail = 0xffffffff;
	fixed16_16 s_gravityAccel;

	JBool s_invincibility = JFALSE;
	JBool s_wearingCleats = JFALSE;
	JBool s_wearingGasmask = JFALSE;
	JBool s_nightvisionActive = JFALSE;
	JBool s_headlampActive = JFALSE;
	JBool s_superCharge   = JFALSE;
	JBool s_superChargeHud= JFALSE;
	JBool s_playerSecMoved = JFALSE;
	u32*  s_playerInvSaved = nullptr;
	JBool s_goals[16] = { 0 };

	RSector* s_playerSector = nullptr;
	SecObject* s_playerObject = nullptr;
	SecObject* s_playerEye = nullptr;
	vec3_fixed s_eyePos = { 0 };	// s_camX, s_camY, s_camZ in the DOS code.
	angle14_32 s_pitch = 0, s_yaw = 0, s_roll = 0;
	u32 s_playerEyeFlags = 4;
	Tick s_playerTick;
	Tick s_prevPlayerTick;
	Tick s_nextShieldDmgTick;
	Task* s_playerTask = nullptr;

	vec3_fixed s_camOffset = { 0 };
	angle14_32 s_camOffsetPitch = 0;
	angle14_32 s_camOffsetYaw = 0;
	angle14_32 s_camOffsetRoll = 0;

	JBool s_itemUnknown1;	// 0x282428
	JBool s_itemUnknown2;	// 0x28242c

	SoundSourceID s_landSplashSound;
	SoundSourceID s_landSolidSound;
	SoundSourceID s_gasDamageSoundSource;
	SoundSourceID s_maskSoundSource1;
	SoundSourceID s_maskSoundSource2;
	SoundSourceID s_invCountdownSound;
	SoundSourceID s_jumpSoundSource;
	SoundSourceID s_nightVisionActiveSoundSource;
	SoundSourceID s_nightVisionDeactiveSoundSource;
	SoundSourceID s_playerDeathSoundSource;
	SoundSourceID s_crushSoundSource;
	SoundSourceID s_playerHealthHitSoundSource;
	SoundSourceID s_kyleScreamSoundSource;
	SoundSourceID s_playerShieldHitSoundSource;

	// Player Controller
	static fixed16_16 s_externalYawSpd;
	static angle14_32 s_playerYaw;
	static angle14_32 s_playerPitch;
	static angle14_32 s_playerRoll;
	static fixed16_16 s_forwardSpd;
	static fixed16_16 s_strafeSpd;
	static fixed16_16 s_maxMoveDist;
	static fixed16_16 s_playerStopAccel;
	static fixed16_16 s_minEyeDistFromFloor;
	static fixed16_16 s_postLandVel;
	static fixed16_16 s_playerVelX;
	static fixed16_16 s_playerUpVel;
	static fixed16_16 s_playerUpVel2;
	static fixed16_16 s_playerVelZ;
	static fixed16_16 s_externalVelX;
	static fixed16_16 s_externalVelZ;
	static fixed16_16 s_playerCrouchSpd;
	static fixed16_16 s_playerSpeed;
	fixed16_16 s_playerHeight;
	// Speed Modifiers
	s32 s_playerRun = 0;
	s32 s_jumpScale = 0;
	s32 s_playerSlow = 0;
	s32 s_waterSpeed = 0;
	// Misc
	static s32 s_weaponLight = 0;
	static s32 s_levelAtten = 0;
	static s32 s_headwaveVerticalOffset;
	static Safe* s_curSafe = nullptr;
	// Actions
	static JBool s_playerUse = JFALSE;
	static JBool s_playerActionUse = JFALSE;
	static JBool s_weaponFiring = JFALSE;
	static JBool s_weaponFiringSec = JFALSE;
	static JBool s_playerPrimaryFire = JFALSE;
	static JBool s_playerSecFire = JFALSE;
	static JBool s_playerJumping = JFALSE;
	// Currently playing sound effects.
	static SoundEffectID s_crushSoundId = 0;
	static SoundEffectID s_kyleScreamSoundId = 0;
	// Position and orientation.
	static vec3_fixed s_playerPos;
	static fixed16_16 s_playerYPos;
	static fixed16_16 s_playerObjHeight;
	static angle14_32 s_playerObjPitch;
	static angle14_32 s_playerObjYaw;
	static RSector* s_playerObjSector;
		   
	void playerControlTaskFunc(s32 id);
	void setPlayerLight(s32 atten);
	void setCameraOffset(fixed16_16 offsetX, fixed16_16 offsetY, fixed16_16 offsetZ);
	void setCameraAngleOffset(angle14_32 offsetPitch, angle14_32 offsetYaw, angle14_32 offsetRoll);

	void handlePlayerMoveControls();
	void handlePlayerPhysics();
	void handlePlayerActions();
	void handlePlayerScreenFx();

	void player_init()
	{
		s_landSplashSound                = sound_Load("swim-in.voc");
		s_landSolidSound                 = sound_Load("land-1.voc");
		s_gasDamageSoundSource           = sound_Load("choke.voc");
		s_maskSoundSource1               = sound_Load("mask1.voc");
		s_maskSoundSource2               = sound_Load("mask2.voc");
		s_invCountdownSound              = sound_Load("quarter.voc");
		s_jumpSoundSource                = sound_Load("jump-1.voc");
		s_nightVisionActiveSoundSource   = sound_Load("goggles1.voc");
		s_nightVisionDeactiveSoundSource = sound_Load("goggles2.voc");
		s_playerDeathSoundSource         = sound_Load("kyledie1.voc");
		s_crushSoundSource               = sound_Load("crush.voc");
		s_playerHealthHitSoundSource     = sound_Load("health1.voc");
		s_kyleScreamSoundSource          = sound_Load("fall.voc");
		s_playerShieldHitSoundSource     = sound_Load("shield1.voc");

		s_playerInfo.ammoEnergy  = pickup_addToValue(0, 100, 999);
		s_playerInfo.ammoPower   = pickup_addToValue(0, 100, 999);
		s_playerInfo.ammoPlasma  = pickup_addToValue(0, 100, 999);
		s_playerInfo.shields     = pickup_addToValue(0, 100, 200);
		s_playerInfo.health      = pickup_addToValue(0, 100, 100);
		s_playerInfo.healthFract = 0;
		s_energy = FIXED(2);
	}
		
	void player_readInfo(u8* inv, s32* ammo)
	{
		// Read the inventory.
		s_playerInfo.itemPistol    = inv[0];
		s_playerInfo.itemRifle     = inv[1];
		s_itemUnknown1             = inv[2];
		s_playerInfo.itemAutogun   = inv[3];
		s_playerInfo.itemMortar    = inv[4];
		s_playerInfo.itemFusion    = inv[5];
		s_itemUnknown2             = inv[6];
		s_playerInfo.itemConcussion= inv[7];
		s_playerInfo.itemCannon    = inv[8];
		s_playerInfo.itemRedKey    = inv[9];
		s_playerInfo.itemYellowKey = inv[10];
		s_playerInfo.itemBlueKey   = inv[11];
		s_playerInfo.itemGoggles   = inv[12];
		s_playerInfo.itemCleats    = inv[13];
		s_playerInfo.itemMask      = inv[14];
		s_playerInfo.itemPlans     = inv[15];
		s_playerInfo.itemPhrik     = inv[16];
		s_playerInfo.itemNava      = inv[17];
		s_playerInfo.itemDatatape  = inv[18];
		s_playerInfo.itemUnkown    = inv[19];
		s_playerInfo.itemDtWeapon  = inv[20];
		s_playerInfo.itemCode1 = inv[21];
		s_playerInfo.itemCode2 = inv[22];
		s_playerInfo.itemCode3 = inv[23];
		s_playerInfo.itemCode4 = inv[24];
		s_playerInfo.itemCode5 = inv[25];
		s_playerInfo.itemCode6 = inv[26];
		s_playerInfo.itemCode7 = inv[27];
		s_playerInfo.itemCode8 = inv[28];
		s_playerInfo.itemCode9 = inv[29];

		// Handle current weapon.
		s32 curWeapon = inv[30];
		weapon_setNext(curWeapon);
		s_playerInfo.maxWeapon = max(curWeapon, s_playerInfo.maxWeapon);

		// Life Count.
		s_lifeCount = inv[31];

		// Read Ammo, shields, health and energy.
		s_playerInfo.ammoEnergy    = ammo[0];
		s_playerInfo.ammoPower     = ammo[1];
		s_playerInfo.ammoPlasma    = ammo[2];
		s_playerInfo.ammoDetonator = ammo[3];
		s_playerInfo.ammoShell     = ammo[4];
		s_playerInfo.ammoMine      = ammo[5];
		s_playerInfo.ammoMissile   = ammo[6];
		s_playerInfo.shields = ammo[7];
		s_playerInfo.health  = ammo[8];
		s_energy = ammo[9];
	}
		
	void player_createController()
	{
		s_playerTask = pushTask("player control", playerControlTaskFunc);
		task_setNextTick(s_playerTask, TASK_SLEEP);

		// Clear out inventory items that the player shouldn't start a level with, such as objectives and keys
		s_playerInfo.itemPlans    = JFALSE;
		s_playerInfo.itemPhrik    = JFALSE;
		s_playerInfo.itemNava     = JFALSE;
		s_playerInfo.itemUnkown   = JFALSE;
		s_playerInfo.itemDtWeapon = JFALSE;
		s_playerInfo.itemCode1    = JFALSE;
		s_playerInfo.itemCode2    = JFALSE;
		s_playerInfo.itemCode3    = JFALSE;
		s_playerInfo.itemCode4    = JFALSE;
		s_playerInfo.itemCode5    = JFALSE;
		s_playerInfo.itemCode6    = JFALSE;
		s_playerInfo.itemCode7    = JFALSE;
		s_playerInfo.itemCode8    = JFALSE;
		s_playerInfo.itemCode9    = JFALSE;
		s_playerInfo.itemRedKey   = JFALSE;
		s_playerInfo.itemYellowKey= JFALSE;
		s_playerInfo.itemBlueKey  = JFALSE;

		// Clear out any lingering damage or flash effects.
		s_healthDamageFx = 0;
		s_shieldDamageFx = 0;
		s_flashEffect = 0;

		s_pickupFlags = 0;
		s_externalYawSpd = 0;
		s_playerPitch = 0;
		s_playerRoll = 0;
		s_forwardSpd = 0;
		s_strafeSpd = 0;

		// Set constants.
		s_maxMoveDist         = FIXED(32);	// 32.00
		s_playerStopAccel     = -540672;	// -8.25
		s_minEyeDistFromFloor = FIXED(2);	//  2.00
		s_gravityAccel        = FIXED(150);	// 150.0

		// Initialize values.
		s_postLandVel = 0;
		s_playerRun   = 0;
		s_jumpScale   = 0;
		s_playerSlow  = 0;
		s_waterSpeed  = 0;

		s_playerVelX   = 0;
		s_playerUpVel  = 0;
		s_playerUpVel2 = 0;
		s_playerVelZ   = 0;
		s_externalVelX = 0;
		s_externalVelZ = 0;
		s_playerCrouchSpd = 0;
		s_playerSpeed  = 0;

		s_weaponLight    = 0;
		s_levelAtten     = 0;
		s_prevPlayerTick = 0;
		s_headwaveVerticalOffset = 0;

		s_playerUse         = JFALSE;
		s_playerActionUse   = JFALSE;
		s_weaponFiring      = JFALSE;
		s_weaponFiringSec   = JFALSE;
		s_playerPrimaryFire = JFALSE;
		s_playerSecFire     = JFALSE;
		s_playerJumping     = JFALSE;

		s_crushSoundId = 0;
		s_kyleScreamSoundId = 0;

		s_curSafe = nullptr;
		// The player will always start a level with at least 3 lives.
		s_lifeCount = max(3, s_lifeCount);

		s_playerInvSaved    = nullptr;
		s_invincibilityTask = nullptr;
		s_gasmaskTask       = nullptr;
		s_gasSectorTask     = nullptr;
		s_nextShieldDmgTick = 0;
		s_invincibility = 0;

		// The player will always start a level with at least 100 shields, though if they have more it carries over.
		s_playerInfo.shields = max(100, s_playerInfo.shields);
		// The player starts a new level with full health and energy.
		s_playerInfo.health = 100;
		s_playerInfo.healthFract = 0;
		s_energy = FIXED(2);

		s_wearingGasmask    = JFALSE;
		s_wearingCleats     = JFALSE;
		s_nightvisionActive = JFALSE;
		s_headlampActive    = JFALSE;

		// Handle level-specific hacks.
		const char* levelName = agent_getLevelName();
		if (!strcasecmp(levelName, "japship"))
		{
			// TODO(Core Game Loop Release)
		}
		else if (!strcasecmp(levelName, "gromas"))
		{
			// Adjust the lighting on gromas in order to accenuate the fog effect.
			s_levelAtten = 30;
		}
	}

	void player_clearEyeObject()
	{
		s_playerEye = nullptr;
	}

	void playerLogicCleanupFunc(Logic* logic)
	{
		// TODO(Core Game Loop Release)
	}

	void player_setupObject(SecObject* obj)
	{
		s_playerObject = obj;
		obj_addLogic(obj, (Logic*)&s_playerLogic, s_playerTask, playerLogicCleanupFunc);

		s_playerObject->entityFlags|= ETFLAG_PLAYER;
		s_playerObject->flags      |= OBJ_FLAG_HAS_COLLISION;
		s_playerObject->worldHeight = PLAYER_HEIGHT;
		s_playerObject->worldWidth  = PLAYER_WIDTH;

		s_playerYaw   = s_playerObject->yaw;
		s_playerPitch = 0;
		s_playerRoll  = 0;

		s_playerYPos = s_playerObject->posWS.y;
		s_playerLogic.stepHeight = PLAYER_STEP;
		task_makeActive(s_playerTask);

		weapon_setNext(s_playerInfo.curWeapon);
		s_playerInfo.maxWeapon = max(s_playerInfo.curWeapon, s_playerInfo.maxWeapon);

		s_playerSecMoved = JFALSE;
		s_playerSector = obj->sector;
	}

	// TODO(Core Game Loop Release): Finish camera setup.
	void player_setupEyeObject(SecObject* obj)
	{
		if (s_playerEye)
		{
			s_playerEye->flags &= ~2;
			s_playerEye->flags |= s_playerEyeFlags;
		}
		s_playerEye = obj;
		player_setupCamera();

		s_playerEye->flags |= 2;
		s_playerEyeFlags = s_playerEye->flags & 4;
		s_playerEye->flags &= ~4;

		s_eyePos.x = s_playerEye->posWS.x;
		s_eyePos.y = s_playerEye->posWS.y;
		s_eyePos.z = s_playerEye->posWS.z;

		s_pitch = s_playerEye->pitch;
		s_yaw   = s_playerEye->yaw;
		s_roll  = s_playerEye->roll;

		setCameraOffset(0, 0, 0);
		setCameraAngleOffset(0, 0, 0);
	}

	s32 player_getLifeCount()
	{
		return s_lifeCount;
	}
					
	void setCameraOffset(fixed16_16 offsetX, fixed16_16 offsetY, fixed16_16 offsetZ)
	{
		s_camOffset = { offsetX, offsetY, offsetZ };
	}

	void setCameraAngleOffset(angle14_32 offsetPitch, angle14_32 offsetYaw, angle14_32 offsetRoll)
	{
		s_camOffsetPitch = offsetPitch;
		s_camOffsetYaw = offsetYaw;
		s_camOffsetRoll = offsetRoll;
	}

	void cheat_teleport()
	{
		RSector* sector = sector_which3D_Map(s_mapX0, s_mapZ0, s_mapLayer);
		if (sector)
		{
			s_playerEye->posWS.x = s_mapX0;
			s_playerEye->posWS.z = s_mapZ0;
			s_playerEye->posWS.y = sector->floorHeight;
			sector_addObject(sector, s_playerEye);
			s_playerSector = sector;

			player_setupEyeObject(s_playerEye);
			player_setupCamera();
		}
	}

	void player_setupCamera()
	{
		if (s_playerEye)
		{
			s_eyePos.x = s_playerEye->posWS.x + s_camOffset.x;
			s_eyePos.y = s_playerEye->posWS.y - (s_playerEye->worldHeight + s_camOffset.y);
			s_eyePos.z = s_playerEye->posWS.z + s_camOffset.z;

			s_pitch = s_playerEye->pitch + s_camOffsetPitch;
			s_yaw   = s_playerEye->yaw   + s_camOffsetYaw;
			s_roll  = s_playerEye->roll  + s_camOffsetRoll;

			if (s_playerEye->sector)
			{
				RClassic_Fixed::computeCameraTransform(s_playerEye->sector, s_pitch, s_yaw, s_eyePos.x, s_eyePos.y, s_eyePos.z);
			}
			renderer_setWorldAmbient(s_playerLight);
		}
	}
		
	void playerControlTaskFunc(s32 id)
	{
		task_begin;
		s_curSafe = level_getSafeFromSector(s_playerObject->sector);
		s_playerPos.x = s_playerObject->posWS.x;
		s_playerPos.y = s_playerObject->posWS.y;
		s_playerPos.z = s_playerObject->posWS.z;
		s_playerObjHeight = s_playerObject->worldHeight;
		s_playerObjPitch  = s_playerObject->pitch;
		s_playerObjYaw    = s_playerObject->yaw;
		s_playerObjSector = s_playerObject->sector;
		s_prevPlayerTick  = s_curTick;

		while (id != -1)
		{
			if (id == 0)
			{
				handlePlayerMoveControls();
				handlePlayerPhysics();
				handlePlayerActions();
				handlePlayerScreenFx();
			}
			else if (id == 22)
			{
				// TODO(Core Game Loop Release)
			}
			else if (id == 23)
			{
				// TODO(Core Game Loop Release)
			}
						
			s_prevPlayerTick = s_playerTick;
			task_yield(TASK_NO_DELAY);
		}
		task_end;
	}
	
	void setPlayerLight(s32 atten)
	{
		s_playerLight = atten;
	}

	void handlePlayerMoveControls()
	{
		s_externalYawSpd = 0;
		s_forwardSpd = 0;
		s_strafeSpd  = 0;
		s_playerRun  = 0;
		s_jumpScale  = 0;
		s_playerSlow = 0;
		
		if (s_pickupFlags)
		{
			return;
		}

		s_playerCrouchSpd = s_playerStopAccel;
				
		s32 mdx, mdy;
		TFE_Input::getMouseMove(&mdx, &mdy);
		// Yaw change
		if (s_config.mouseTurnEnabled || s_config.mouseLookEnabled)
		{
			s_playerYaw += mdx * PLAYER_MOUSE_TURN_SPD;
			s_playerYaw &= 0x3fff;
		}
		// Pitch change
		if (s_config.mouseLookEnabled)
		{
			s_playerPitch = clamp(s_playerPitch - mdy*PLAYER_MOUSE_TURN_SPD, -PITCH_LIMIT, PITCH_LIMIT);
		}
				
		// Controls
		if (getActionState(IA_FORWARD))
		{
			fixed16_16 speed = mul16(PLAYER_FORWARD_SPEED, s_deltaTime);
			s_forwardSpd = max(speed, s_forwardSpd);
		}
		else if (getActionState(IA_BACKWARD))
		{
			fixed16_16 speed = -mul16(PLAYER_FORWARD_SPEED, s_deltaTime);
			s_forwardSpd = min(speed, s_forwardSpd);
		}

		if (getActionState(IA_RUN))
		{
			s_playerRun |= 1;
		}
		else if (getActionState(IA_SLOW))
		{
			s_playerSlow |= 1;
		}

		s32 crouch = getActionState(IA_CROUCH) ? 1 : 0;
		if (s_moveAvail & crouch)
		{
			fixed16_16 speed = PLAYER_CROUCH_SPEED;
			speed <<= s_playerRun;			// this will be '1' if running.
			speed >>= s_playerSlow;			// this will be '1' if moving slowly.
			s_playerCrouchSpd = speed;
		}

		JBool wasJumping = s_playerJumping;
		s_playerJumping = JFALSE;
		if (getActionState(IA_JUMP))
		{
			if (!s_moveAvail || wasJumping)
			{
				s_playerJumping = wasJumping;
			}
			else
			{
				playSound2D(s_jumpSoundSource);

				fixed16_16 speed = PLAYER_JUMP_IMPULSE << s_jumpScale;
				s_playerJumping = JTRUE;
				s_playerUpVel  = speed;
				s_playerUpVel2 = speed;
			}
		}

		//////////////////////////////////////////
		// Pitch and Roll controls.
		//////////////////////////////////////////
		if (getActionState(IA_TURN_LT))
		{
			fixed16_16 turnSpeed = PLAYER_KB_TURN_SPD;	// angle units per second.
			fixed16_16 dYaw = mul16(turnSpeed, s_deltaTime);
			dYaw <<= s_playerRun;		// double for "run"
			dYaw >>= s_playerSlow;		// half for "slow"

			s_playerYaw -= dYaw;
			s_playerYaw &= 0x3fff;
		}
		else if (getActionState(IA_TURN_RT))
		{
			fixed16_16 turnSpeed = PLAYER_KB_TURN_SPD;	// angle units per second.
			fixed16_16 dYaw = mul16(turnSpeed, s_deltaTime);
			dYaw <<= s_playerRun;		// double for "run"
			dYaw >>= s_playerSlow;		// half for "slow"

			s_playerYaw += dYaw;
			s_playerYaw &= 0x3fff;
		}

		if (getActionState(IA_LOOK_UP))
		{
			fixed16_16 turnSpeed = PLAYER_KB_TURN_SPD;	// angle units per second.
			fixed16_16 dPitch = mul16(turnSpeed, s_deltaTime);
			dPitch <<= s_playerRun;		// double for "run"
			dPitch >>= s_playerSlow;	// half for "slow"
			s_playerPitch = clamp(s_playerPitch + dPitch, -PITCH_LIMIT, PITCH_LIMIT);
		}
		else if (getActionState(IA_LOOK_DN))
		{
			fixed16_16 turnSpeed = PLAYER_KB_TURN_SPD;	// angle units per second.
			fixed16_16 dPitch = mul16(turnSpeed, s_deltaTime);
			dPitch <<= s_playerRun;		// double for "run"
			dPitch >>= s_playerSlow;	// half for "slow"
			s_playerPitch = clamp(s_playerPitch - dPitch, -PITCH_LIMIT, PITCH_LIMIT);
		}

		if (getActionState(IA_CENTER_VIEW))
		{
			s_playerPitch = 0;
			s_playerRoll  = 0;
		}

		if (getActionState(IA_STRAFE_RT))
		{
			fixed16_16 speed = mul16(PLAYER_STRAFE_SPEED, s_deltaTime);
			s_strafeSpd = max(speed, s_strafeSpd);
		}
		else if (getActionState(IA_STRAFE_LT))
		{
			fixed16_16 speed = -mul16(PLAYER_STRAFE_SPEED, s_deltaTime);
			s_strafeSpd = max(speed, s_strafeSpd);
		}

		if (getActionState(IA_USE))
		{
			s_playerUse = JTRUE;
		}

		if (getActionState(IA_PRIMARY_FIRE))
		{
			s_playerPrimaryFire = JTRUE;
		}
		else if (getActionState(IA_SECONDARY_FIRE))
		{
			s_playerSecFire = JTRUE;
		}
	}

	void handlePlayerPhysics()
	{
		// TODO(Core Game Loop Release)
		SecObject* player = s_playerObject;
		player->yaw = s_playerYaw & 0x3fff;
		player->pitch = s_playerPitch;
		if (s_externalYawSpd)
		{
			s_externalYawSpd = mul16(s_externalYawSpd, s_deltaTime);
			fixed16_16 speed = s_externalYawSpd << s_playerRun;
			speed >>= s_playerSlow;
			player->yaw += speed;
		}
		sinCosFixed(player->yaw, &s_playerLogic.dir.x, &s_playerLogic.dir.z);

		// TODO(Core Game Loop Release): Insert into the correct location within this function.
		{
			SecObject* player = s_playerObject;
			if (player->flags & 2)
			{
				// This code is incorrect but it doesn't actually matter...
				// roll is always 0.
				setCameraAngleOffset(0, s_playerRoll, 0);

				s32 headlamp = 0;
				if (s_headlampActive)
				{
					s32 energy = min(ONE_16, s_energy);
					headlamp = floor16(mul16(energy, FIXED(64)));
					headlamp = min(31, headlamp);
				}
				s32 atten = max(headlamp, s_weaponLight + s_levelAtten);
				//s_baseAtten = atten;
				if (s_nightvisionActive)
				{
					atten = 0;
				}
				setPlayerLight(atten);
			}
		}
		///////////////////////////////////////////
	}

	void handlePlayerActions()
	{
		// TODO(Core Game Loop Release)
	}

	void handlePlayerScreenFx()
	{
		// TODO(Core Game Loop Release)
	}
}  // TFE_DarkForces