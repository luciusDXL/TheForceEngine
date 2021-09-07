#include "player.h"
#include "automap.h"
#include "agent.h"
#include "config.h"
#include "hud.h"
#include "mission.h"
#include "pickup.h"
#include "weapon.h"
#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/InfSystem/infSystem.h>
// Internal types need to be included in this case.
#include <TFE_Jedi/InfSystem/infTypesInternal.h>
#include <TFE_Jedi/Collision/collision.h>
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

	// Lower = Less sliding/stops faster,
	// Higher = More sliding, takes longer to stop.
	enum Friction
	{
		FRICTION_CLEATS  = 0xf333, // 0.950
		FRICTION_DEFAULT = 0xf999, // 0.975
		FRICTION_FALLING = 0xfef9, // 0.996
		FRICTION_ICE     = 0xffbe, // 0.999
	};

	enum SpecialSpeeds
	{
		ICE_SPEED_MUL     = 0x11eb, // 0.070
		MOVING_SURFACE_MUL = 0xcccc, // 0.8
	};

	static const s32 c_healthDmgToFx[] =
	{
		4, 8, 12, 16, 20, 24, 28, 32,
		36, 40, 44, 48, 52, 56, 60, 63,
	};

	static const s32 c_shieldDmgToFx[] =
	{
		2, 4, 6, 8, 10, 12, 14, 16,
		18, 20, 22, 24, 26, 28, 30, 32
	};

	static const s32 c_flashAmtToFx[] =
	{
		1, 2, 3, 4, 5, 6, 7, 8, 9,
		10, 11, 12, 13, 14, 15, 16
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
	s32 s_onMovingSurface = 0;
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
	static JBool s_playerInWater = JFALSE;
	static JBool s_limitStepHeight = JTRUE;
	static JBool s_smallModeEnabled = JFALSE;
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
	static RWall* s_playerSlideWall;
		   
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
		s_onMovingSurface = 0;

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

	void cheat_enableNoheightCheck()
	{
		s_limitStepHeight = ~s_limitStepHeight;
		hud_sendTextMessage(700);
	}

	void cheat_bugMode()
	{
		s_smallModeEnabled = ~s_smallModeEnabled;
		hud_sendTextMessage(705);
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

	fixed16_16 adjustForwardSpeed(fixed16_16 spd)
	{
		spd <<= s_playerRun;
		spd >>= s_playerSlow;
		spd >>= s_onMovingSurface;	// slows down the player movement on a moving surface.
		spd <<= s_jumpScale;
		return spd;
	}

	fixed16_16 adjustStrafeSpeed(fixed16_16 spd)
	{
		spd <<= s_playerRun;
		spd >>= s_playerSlow;
		spd >>= s_onMovingSurface;	// slows down the player movement on a moving surface.
		return spd;
	}

	static fixed16_16 s_moveDirX;
	static fixed16_16 s_moveDirZ;
	static fixed16_16 s_dist;
	static fixed16_16 s_distScale;

	void computeDirectionFromAngle(angle14_32 yaw, fixed16_16* xDir, fixed16_16* zDir)
	{
		sinCosFixed(yaw, xDir, zDir);
	}

	// Rescales the vector if it is longer than maxDist.
	void limitVectorLength(fixed16_16* dx, fixed16_16* dz, fixed16_16 maxDist)
	{
		fixed16_16 halfDx = TFE_Jedi::abs(*dx) >> 1;
		fixed16_16 halfDz = TFE_Jedi::abs(*dz) >> 1;
		fixed16_16 distSq = mul16(halfDx, halfDx) + mul16(halfDz, halfDz);

		fixed16_16 dist = fixedSqrt(distSq);
		s_dist = dist * 2;

		if (s_dist > maxDist)
		{
			s_distScale = div16(maxDist, s_dist);
			*dx = mul16(*dx, s_distScale);
			*dz = mul16(*dz, s_distScale);
		}
	}

	void computeMoveFromAngleAndSpeed(fixed16_16* outMoveX, fixed16_16* outMoveZ, angle14_32 yaw, fixed16_16 speed)
	{
		computeDirectionFromAngle(yaw, &s_moveDirX, &s_moveDirZ);
		*outMoveX += mul16(s_moveDirX, speed);
		*outMoveZ += mul16(s_moveDirZ, speed);
	}

	enum Passability
	{
		PASS_ALWAYS_WALK = 0,
		PASS_DEFAULT = 0xffffffff,
	};

	static PlayerLogic* s_curPlayerLogic;
	static SecObject* s_collidedObj;
	static ColObject s_colObject;
	static ColObject s_colObj1;
	static RWall* s_colWall0;
	static RWall* s_colWallCollided;
	static RSector* s_nextSector;
	static fixed16_16 s_colSrcPosX;
	static fixed16_16 s_colSrcPosY;
	static fixed16_16 s_colSrcPosZ;
	static fixed16_16 s_colDstPosX;
	static fixed16_16 s_colDstPosY;
	static fixed16_16 s_colDstPosZ;
	static fixed16_16 s_colWidth;
	static fixed16_16 s_colHeight;
	static fixed16_16 s_colDoubleRadius;
	static fixed16_16 s_colMaxHeight;
	static fixed16_16 s_colMinHeight;
	static fixed16_16 s_colHeightBase;
	static fixed16_16 s_colMaxBaseHeight;
	static fixed16_16 s_colMinBaseHeight;
	static fixed16_16 s_colBottom;
	static fixed16_16 s_colTop;
	static fixed16_16 s_colY1;
	static fixed16_16 s_colCurLowestFloor;
	static fixed16_16 s_colCurHighestCeil;
	static fixed16_16 s_colBaseFloorHeight;
	static fixed16_16 s_colBaseCeilHeight;
	static fixed16_16 s_colFloorHeight;
	static fixed16_16 s_colCeilHeight;
	static fixed16_16 s_colCurFloor;
	static fixed16_16 s_colCurCeil;
	static fixed16_16 s_colCurTop;
	static fixed16_16 s_colCurBot;
	static angle14_32 s_colResponseAngle;
	static vec2_fixed s_colResponsePos;
	static vec2_fixed s_colResponseDir;
	static vec2_fixed s_colWallV0;
	static JBool s_colResponseStep;
	static s32 s_collisionFrameSector;
	static JBool s_objCollisionEnabled = JTRUE;

	// Returns a collision object or nullptr if no object is hit.
	SecObject* col_findObjectCollision(RSector* sector)
	{
		fixed16_16 colWidth = s_colWidth;
		if (s_objCollisionEnabled)
		{
			s32 objCount = sector->objectCount;
			SecObject** objList = sector->objectList;
			fixed16_16 relHeight = s_colDstPosY - s_colHeightBase;

			fixed16_16 dirX, dirZ;
			fixed16_16 pathDz = s_colDstPosZ - s_colSrcPosZ;
			fixed16_16 pathDx = s_colDstPosX - s_colSrcPosX;
			computeDirAndLength(pathDx, pathDz, &dirX, &dirZ);

			for (; objCount; objList++)
			{
				SecObject* obj = *objList;
				if (obj)
				{
					objCount--;
					if (!(obj->entityFlags & ETFLAG_PICKUP) && obj->worldWidth && (s_colSrcPosX != obj->posWS.x || s_colSrcPosZ != obj->posWS.z))
					{
						// Check the seperation of the object and destination position.
						// If they are seperated by more than their combined widths on the X or Z axis, then there is no collision.
						fixed16_16 sepX  = TFE_Jedi::abs(obj->posWS.x - s_colDstPosX);
						fixed16_16 sepZ  = TFE_Jedi::abs(obj->posWS.z - s_colDstPosZ);
						fixed16_16 width = obj->worldWidth + colWidth;
						if (sepX >= width || sepZ >= width)
						{
							continue;
						}

						// The top of the object is *below* the final position.
						fixed16_16 objTop = obj->posWS.y - obj->worldHeight;
						if (objTop >= s_colDstPosY || relHeight >= obj->posWS.y)
						{
							continue;
						}

						// Check XZ seperation again... (this second test can be skipped)
						sepX = TFE_Jedi::abs(s_colDstPosX - obj->posWS.x);
						sepZ = TFE_Jedi::abs(s_colDstPosZ - obj->posWS.z);
						if ((sepX >= obj->worldWidth + s_colWidth) || (sepZ >= obj->worldWidth + s_colWidth))
						{
							continue;
						}

						// Check to see if the path starts already colliding with the object.
						// And if it is, then skip collision (so they come apart and don't get stuck).
						fixed16_16 startSepX = TFE_Jedi::abs(s_colSrcPosX - obj->posWS.x);
						fixed16_16 startSepZ = TFE_Jedi::abs(s_colSrcPosZ - obj->posWS.z);
						if (startSepX < width && startSepZ < width)
						{
							continue;
						}
												
						fixed16_16 dx = s_colDstPosX - s_colSrcPosX;
						fixed16_16 dz = s_colDstPosZ - s_colSrcPosZ;
						s32 xSign = (dx < 0) ? -1 : 1;
						s32 zSign = (dz < 0) ? -1 : 1;

						// Compute the object AABB edges that need to be considered for the collision.
						// this is the same as: objEdgeX = obj->posWS.x - obj->worldWidth * xSign;
						fixed16_16 objEdgeX = (xSign >= 0) ? (obj->posWS.x - obj->worldWidth) : (obj->posWS.x + obj->worldWidth);
						fixed16_16 objEdgeZ = (zSign >= 0) ? (obj->posWS.z - obj->worldWidth) : (obj->posWS.z + obj->worldWidth);

						// Cross product between the vector from the destination to the nearest AABB corner to the start and
						// the path direction.
						// This is *zero* if the corner is exactly on the path, *negative* if the corner is between the start and destination,
						// and *positive* if the point is *past* the destination (i.e. unreachable).
						fixed16_16 cprod = mul16(objEdgeX - s_colDstPosX, dirZ) - mul16(objEdgeZ - s_colDstPosZ, dirX);
						s32 cSign = cprod < 0 ? -1 : 1;

						// Is the sign of the product different than the sign of either x or z.
						s32 signDiff = (cSign^xSign) ^ zSign;
						if (signDiff < 0)	// condition above is *true*
						{
							s_colResponseStep = JTRUE;
							if (zSign >= 0)
							{
								s_colResponseAngle = 4095;	// ~90 degrees
								s_colResponsePos.x = obj->posWS.x - obj->worldWidth;
								s_colResponsePos.z = obj->posWS.z - obj->worldWidth;
								s_colResponseDir.x = ONE_16;
								s_colResponseDir.z = 0;
								return obj;
							}
							else // zSign < 0
							{
								s_colResponseAngle = 12287;		// ~270 degrees
								s_colResponsePos.x = obj->posWS.x + obj->worldWidth;
								s_colResponsePos.z = obj->posWS.z + obj->worldWidth;
								s_colResponseDir.x = -ONE_16;
								s_colResponseDir.z = 0;

								return obj;
							}
						}
						else
						{
							s_colResponseStep = JTRUE;
							if (xSign >= 0)
							{
								s_colResponseAngle = 8191;	// ~180 degrees
								s_colResponsePos.x = obj->posWS.x - obj->worldWidth;
								s_colResponsePos.z = obj->posWS.z + obj->worldWidth;
								s_colResponseDir.x = 0;
								s_colResponseDir.z = -ONE_16;

								return obj;
							}
							else
							{
								s_colResponseAngle = 0;		// 0 degrees
								s_colResponsePos.x = obj->posWS.x + obj->worldWidth;
								s_colResponsePos.z = obj->posWS.z - obj->worldWidth;
								s_colResponseDir.x = 0;
								s_colResponseDir.z = ONE_16;

								return obj;
							}
						}
					}
				}
			}
		}
		return nullptr;
	}

	// Get the "feature" to collide against, such as an object or wall.
	// Returns JFALSE if collision else JTRUE
	u32 col_getCollisionFeature(RSector* sector, u32 passability)
	{
		s_colBaseFloorHeight = sector->floorHeight;
		s_colBaseCeilHeight  = sector->ceilingHeight;
		s_colFloorHeight     = sector->colFloorHeight;
		s_colCeilHeight      = sector->colCeilHeight;

		if (sector->secHeight < 0 && s_colDstPosY > sector->colSecHeight)
		{
			s_colCurFloor = s_colFloorHeight;
			s_colCurCeil = s_colBaseFloorHeight + sector->secHeight;
		}
		else
		{
			s_colCurFloor = sector->colSecHeight;
			s_colCurCeil  = sector->colMinHeight;	// why not sector->colCeilHeight?
		}

		if (s_colMaxBaseHeight >= s_colBaseFloorHeight)
		{
			s_colMaxBaseHeight = s_colBaseFloorHeight;
		}
		if (s_colMinBaseHeight <= s_colBaseCeilHeight)
		{
			s_colMinBaseHeight = s_colBaseCeilHeight;
		}
		if (s_colMaxHeight >= s_colFloorHeight)
		{
			s_colMaxHeight = s_colFloorHeight;
		}
		if (s_colMinHeight <= s_colCeilHeight)
		{
			s_colMinHeight = s_colCeilHeight;
		}
		if (s_colCurLowestFloor >= s_colCurFloor)
		{
			s_colCurLowestFloor = s_colCurFloor;
		}
		if (s_colCurHighestCeil <= s_colCurCeil)
		{
			s_colCurHighestCeil = s_colCurCeil;
		}

		if (s_colCurCeil > s_colCurTop)
		{
			s_colCurTop = s_colCurCeil;
			// s_2c865c = sector;
		}
		if (s_colCurLowestFloor < s_colCurBot)
		{
			s_colCurBot = s_colCurLowestFloor;
			s_nextSector = sector;
		}

		if (passability == PASS_ALWAYS_WALK || (s_colCurBot >= s_colBottom && s_colCurTop <= s_colTop && TFE_Jedi::abs(s_colCurBot - s_colCurTop) >= s_colHeight && s_colCurFloor <= s_colY1))
		{
			s_collidedObj = nullptr;// col_findObjectCollision(sector);
			if (s_collidedObj)
			{
				return JFALSE;
			}
			RWall* wall = sector->walls;
			s32 wallCount = sector->wallCount;
			sector->collisionFrame = s_collisionFrameSector;
			for (s32 i = 0; i < wallCount; i++, wall++)
			{
				s_colWallV0 = *wall->w0;
				fixed16_16 length = wall->length;

				JBool canPass = JFALSE;
				// Note WF3_ALWAYS_WALK overrides WF3_SOLID_WALL.
				if ((wall->flags3 & (WF3_ALWAYS_WALK | WF3_SOLID_WALL)) != WF3_SOLID_WALL && wall->nextSector)
				{
					canPass = JTRUE;
				}

				// Given line: (v0, v1) and position (p) compute the distance from the line:
				// dist = (p - v0).N; where N = (dz, -dx) = wall normal.
				fixed16_16 dist = mul16(s_colDstPosX - s_colWallV0.x, wall->wallDir.z) - mul16(s_colDstPosZ - s_colWallV0.z, wall->wallDir.x);
				// Convert to unsigned distance.
				if (dist < 0) { dist = -dist; }
				// Further checks can be avoided if the position is too far from the line to collide.
				if (dist < s_colWidth)
				{
					// Project s_colDstPosX/Z onto the wall, i.e.
					// u = (p-v0).D; where D = direction and v = v0 + u*D
					fixed16_16 u = mul16(s_colDstPosX - s_colWallV0.x, wall->wallDir.x) + mul16(s_colDstPosZ - s_colWallV0.z, wall->wallDir.z);
					fixed16_16 maxEdge = u + s_colWidth;
					fixed16_16 maxCol = length + s_colDoubleRadius;
					if ((u >= 0 && u <= length) || (maxEdge >= 0 && maxEdge <= maxCol))
					{
						JBool sideTest = JTRUE;
						if (canPass)
						{
							RSector* next = wall->nextSector;
							fixed16_16 srcY = s_colSrcPosY;
							fixed16_16 secHeight = next->colSecHeight;
							// If the path starts *above* the second height and then ends at or *below* it.
							if (srcY <= secHeight && s_colY1 >= secHeight && s_colTop > next->colMinHeight)
							{
								sideTest = JFALSE;
							}
						}
						// Skip the side test if dealing with a passable wall.
						if (sideTest)
						{
							// Make sure the start point is in front side of the wall.
							// side = (s_colSrcPos - s_colWallV0) x (wallDir)
							fixed16_16 side = mul16(s_colSrcPosX - s_colWallV0.x, wall->wallDir.z) - mul16(s_colSrcPosZ - s_colWallV0.z, wall->wallDir.x);
							if (side < 0)
							{
								// We hit the back side of the wall.
								continue;
							}
							fixed16_16 x1 = s_colDstPosX;
							fixed16_16 z1 = s_colDstPosZ;
							fixed16_16 z0 = s_colSrcPosZ;
							fixed16_16 x0 = s_colSrcPosX;
							fixed16_16 dz = z1 - z0;
							fixed16_16 dx = x1 - x0;
							// Check to see if the movement direction is not moving *away* from the wall.
							side = mul16(dx, wall->wallDir.z) - mul16(dz, wall->wallDir.x);
							if (side > 0)
							{
								// The collision path is moving away from the wall.
								continue;
							}
						}

						// This wall might be passable.
						if (canPass)
						{
							RSector* next = wall->nextSector;
							fixed16_16 nextFloor = (next->secHeight >= 0) ? next->colSecHeight : next->floorHeight;
							// Check to see if the object fits between the next sector floor and ceiling.
							if ((wall->flags3 & (WF3_ALWAYS_WALK | WF3_SOLID_WALL)) != WF3_ALWAYS_WALK)
							{
								if (s_colBottom > nextFloor)
								{
									s_colWallCollided = wall;
									return JFALSE;
								}
								if (s_colTop < next->colMinHeight)
								{
									s_colWallCollided = wall;
									return JFALSE;
								}
							}
							// Check to see if this sector has already been tested this "collision frame"
							if (s_collisionFrameSector != next->collisionFrame)
							{
								u32 passability = ((wall->flags3 & (WF3_ALWAYS_WALK | WF3_SOLID_WALL)) == WF3_ALWAYS_WALK) ? PASS_ALWAYS_WALK : PASS_DEFAULT;
								if (!col_getCollisionFeature(next, passability))
								{
									if (!s_colWallCollided) { s_colWallCollided = wall; }
									return JFALSE;
								}
							}
						}
						else  // !canPass
						{
							s_colWallCollided = wall;
							return JFALSE;
						}
					}
				}
			}
			return JTRUE;
		}
		s_colObj1.sector = sector;
		return JFALSE;
	}

	// Returns JFALSE if there was a collision.
	JBool col_computeCollisionResponse(RSector* sector)
	{
		fixed16_16 mx =  FIXED(9999);
		fixed16_16 mn = -FIXED(9999);
		//s_28262c = mx;
		//s_282630 = -mn;
		fixed16_16 mx2 = mx;
		fixed16_16 mn2 = mn;
		s_colMaxBaseHeight = mx;
		
		// 2053cf:
		s_colMinBaseHeight = mn;
		s_colMaxHeight = mx;
		s_colMinHeight = mn;
		s_colCurLowestFloor = mx;
		s_colCurHighestCeil = mn;

		s_collisionFrameSector++;
		s_colWallCollided = nullptr;
		s_colDoubleRadius = s_colWidth + s_colWidth;
		s_colResponseStep = JFALSE;
		// Is there a collision?
		if (!col_getCollisionFeature(sector, PASS_ALWAYS_WALK))
		{
			s_colWall0 = s_colWallCollided;
			if (s_colWall0)
			{
				s_colResponseStep = JTRUE;
				s_colResponseDir.x = s_colWall0->wallDir.x;
				s_colResponseDir.z = s_colWall0->wallDir.z;
				s_colResponseAngle = s_colWall0->angle;

				vec2_fixed* w0 = s_colWall0->w0;
				s_colResponsePos.x = w0->x;
				s_colResponsePos.z = w0->z;
			}
			return JFALSE;
		}
		return JTRUE;
	}

	void handleCollisionResponseSimple(fixed16_16 dirX, fixed16_16 dirZ, fixed16_16* moveX, fixed16_16* moveZ)
	{
		fixed16_16 proj = mul16(*moveX, dirX) + mul16(*moveZ, dirZ);
		*moveX = mul16(proj, dirZ);
		*moveZ = mul16(proj, dirZ);
	}

	void handleCollisionResponse(fixed16_16 dirX, fixed16_16 dirZ, fixed16_16 posX, fixed16_16 posZ)
	{
		SecObject* obj = s_curPlayerLogic->logic.obj;
		// Project the movement vector onto the wall direction to get how much sliding movement is left.
		fixed16_16 proj = mul16(s_curPlayerLogic->move.x, dirX) + mul16(s_curPlayerLogic->move.z, dirZ);

		// Normal = (dirZ, -dirX)
		// -P.Normal
		fixed16_16 dx = obj->posWS.x - posX;
		fixed16_16 dz = obj->posWS.z - posZ;
		fixed16_16 planarDist = mul16(dx, dirZ) - mul16(dz, dirX);

		fixed16_16 r = obj->worldWidth + 65;	// worldWidth + 0.001
		// if overlap is > 0, the player cylinder is embedded in the wall.
		fixed16_16 overlap = r - planarDist;
		if (overlap >= 0)
		{
			// push at a rate of 4 units / second.
			fixed16_16 pushSpd = FIXED(4);
			// make sure the overlap adjustment is at least the size of the "push"
			overlap = max(overlap, mul16(pushSpd, s_deltaTime));
			RSector* sector = obj->sector;
			s_colCurBot = sector->floorHeight;
		}
		// Move along 'overlap' so that the cylinder is nearly touching and move along move direction based on projection.
		// This has the effect of attracting or pushing the object in order to remove the gap or overlap.
		s_curPlayerLogic->move.x =  mul16(overlap, dirZ) + mul16(proj, dirX);
		s_curPlayerLogic->move.z = -mul16(overlap, dirX) + mul16(proj, dirZ);
	}

	// Returns JTRUE in the case of a collision, otherwise returns JFALSE.
	JBool handlePlayerCollision(PlayerLogic* playerLogic)
	{
		s_curPlayerLogic = playerLogic;
		SecObject* obj = playerLogic->logic.obj;
		s_colObject.obj = obj;

		s_colSrcPosX = obj->posWS.x;
		s_colDstPosY = obj->posWS.y;
		fixed16_16 z = obj->posWS.z;
		s_colWidth = obj->worldWidth;

		s_colHeightBase = obj->worldHeight;
		s_colSrcPosY = obj->posWS.y;

		// Handles maximum stair height.
		s_colBottom = obj->posWS.y - playerLogic->stepHeight;

		s_colSrcPosZ = obj->posWS.z;
		s_colTop = obj->posWS.y - obj->worldHeight - ONE_16;

		s_colY1 = FIXED(9999);
		s_colHeight = obj->worldHeight + ONE_16;
		RSector* sector = obj->sector;

		s_colWall0 = nullptr;
		s_colObj1.ptr = nullptr;

		s_colDstPosX = obj->posWS.x + playerLogic->move.x;
		s_colDstPosZ = obj->posWS.z + playerLogic->move.z;

		// If there was no collision, return false.
		if (col_computeCollisionResponse(sector))
		{
			return JFALSE;
		}
		if (s_colResponseStep)
		{
			handleCollisionResponse(s_colResponseDir.x, s_colResponseDir.z, s_colResponsePos.x, s_colResponsePos.z);
		}
		return JTRUE;
	}

	// Attempts to move the player. Note that collision detection has already occured accounting for step height,
	// objects, collision reponse, etc.. So this is a more basic collision detection function to validate the
	// results and fire off INF_EVENT_CROSS_LINE_XXX events as needed.
	JBool playerMove()
	{
		SecObject* player = s_playerObject;
		fixed16_16 ceilHeight;
		fixed16_16 floorHeight;
		sector_getObjFloorAndCeilHeight(player->sector, player->posWS.y, &floorHeight, &ceilHeight);

		fixed16_16 finalCeilHeight = ceilHeight;
		fixed16_16 finalFloorHeight = floorHeight;

		fixed16_16 x0 = player->posWS.x;
		fixed16_16 z0 = player->posWS.z;
		fixed16_16 x1 = x0 + s_playerLogic.move.x;
		fixed16_16 z1 = z0 + s_playerLogic.move.z;
		RSector* nextSector = player->sector;
		RWall* wall = collision_wallCollisionFromPath(player->sector, x0, z1, x1, z1);
		while (wall)
		{
			RSector* adjoinSector = wall->nextSector;
			// If the player collides with a wall, nextSector will be set to null, which means that the movement is not applied.
			nextSector = nullptr;
			if (adjoinSector && !(wall->flags3&WF3_SOLID_WALL))
			{
				if (!s_pickupFlags)
				{
					inf_triggerWallEvent(wall, s_playerObject, INF_EVENT_CROSS_LINE_FRONT);
				}
				nextSector = adjoinSector;
				sector_getObjFloorAndCeilHeight(adjoinSector, player->posWS.y, &floorHeight, &ceilHeight);

				// Current ceiling is higher than the adjoining sector, meaning there is an upper wall part.
				finalCeilHeight = max(ceilHeight, finalCeilHeight);
				// Current floor is lower than the adjoining sector, meaning there is a lower wall part.
				finalFloorHeight = min(floorHeight, finalFloorHeight);
				wall = collision_pathWallCollision(adjoinSector);
			}
			else
			{
				wall = nullptr;
			}
		}
		// nextSector will be null if the collision failed, in which case we don't move the player and return false.
		if (nextSector)
		{
			fixed16_16 stepLimit = player->posWS.y - s_playerLogic.stepHeight;
			fixed16_16 topLimit = player->posWS.y - player->worldHeight - ONE_16;
			// If the player cannot fit, then again we don't move the player and return false.
			if (finalFloorHeight >= stepLimit && finalCeilHeight <= topLimit)
			{
				// The collision was a success!
				// Move the player, change sectors if needed and adjust the map layer.
				player->posWS.x += s_playerLogic.move.x;
				player->posWS.z += s_playerLogic.move.z;

				RSector* curSector = player->sector;
				if (nextSector != curSector)
				{
					if (nextSector->layer != curSector->layer)
					{
						automap_setLayer(nextSector->layer);
					}
					sector_addObject(nextSector, player);
				}
				return JTRUE;
			}
		}
		return JFALSE;
	}

	typedef JBool(*CollisionObjFunc)(RSector*);
	typedef JBool(*CollisionProxFunc)();
	static CollisionObjFunc s_objCollisionFunc;
	static CollisionProxFunc s_objCollisionProxFunc;

	JBool handleCollisionFunc(RSector* sector)
	{
		if (!s_objCollisionFunc || s_objCollisionFunc(sector))
		{
			sector->collisionFrame = s_collisionFrameSector;

			RWall* wall = sector->walls;
			s32 wallCount = sector->wallCount;
			for (s32 i = 0; i < wallCount; i++, wall++)
			{
				vec2_fixed* w0 = wall->w0;
				s_colWallV0.x = w0->x;
				s_colWallV0.z = w0->z;

				fixed16_16 length = wall->length;
				fixed16_16 dx = s_colDstPosZ - s_colWallV0.z;
				fixed16_16 dz = s_colDstPosX - s_colWallV0.x;

				fixed16_16 pz = mul16(dz, wall->wallDir.z);
				fixed16_16 px = mul16(dx, wall->wallDir.x);

				fixed16_16 dp = pz - px;
				fixed16_16 adp = TFE_Jedi::abs(dp);
				if (adp <= s_colWidth)
				{
					fixed16_16 paramPos = mul16(dx, wall->wallDir.z) + mul16(dz, wall->wallDir.x);
					if (paramPos > length)
					{
						if (paramPos + s_colWidth > length + s_colDoubleRadius)
						{
							continue;
						}
					}
					if (s_objCollisionProxFunc && !s_objCollisionProxFunc())
					{
						return JFALSE;
					}
					RSector* nextSector = wall->nextSector;
					if (nextSector)
					{
						if ((wall->flags3 & (WF3_ALWAYS_WALK | WF3_SOLID_WALL)) != WF3_SOLID_WALL)
						{
							if (s_collisionFrameSector != nextSector->collisionFrame)
							{
								if (!handleCollisionFunc(nextSector))
								{
									return JFALSE;
								}
							}
						}
					}
				}
			}
		}
		return JTRUE;
	}

	void playerHandleCollisionFunc(RSector* sector, CollisionObjFunc func, CollisionProxFunc proxFunc)
	{
		s_objCollisionFunc = func;
		s_colDoubleRadius = s_colWidth * 2;
		s_objCollisionProxFunc = proxFunc;
		s_collisionFrameSector++;
		handleCollisionFunc(sector);
	}

	JBool collision_checkPickups(RSector* sector)
	{
		fixed16_16 floorHeight = sector->floorHeight;
		fixed16_16 ceilHeight = sector->ceilingHeight;
		if (floorHeight == ceilHeight) { return JFALSE;	}

		s32 objCount = sector->objectCount;
		SecObject** objList = sector->objectList;
		for(; objCount; objList++)
		{
			SecObject* obj = *objList;
			if (obj)
			{
				objCount--;
				if (obj->worldWidth && (obj->entityFlags & ETFLAG_PICKUP))
				{
					fixed16_16 dx = obj->posWS.x - s_colDstPosX;
					fixed16_16 dz = obj->posWS.z - s_colDstPosZ;
					fixed16_16 adx = TFE_Jedi::abs(dx);
					fixed16_16 adz = TFE_Jedi::abs(dz);
					fixed16_16 radius = obj->worldWidth + s_colWidth;
					if (adx < radius && adz < radius)
					{
						fixed16_16 objTop = obj->posWS.y - obj->worldHeight;
						fixed16_16 colliderTop = s_colDstPosY - s_colHeightBase;
						if (objTop < s_colDstPosY && colliderTop < obj->posWS.y)
						{
							s_msgEntity = s_colObject.obj;
							message_sendToObj(obj, MSG_PICKUP, nullptr);
						}
					}
				}
			}
		}
		return JTRUE;
	}

	void handlePlayerPhysics()
	{
		SecObject* player = s_playerObject;
		fixed16_16 yVel = s_playerUpVel;

		// Calculate the friction coefficient.
		fixed16_16 friction;
		if (yVel)
		{
			friction = FRICTION_FALLING;
		}
		else
		{
			JBool wearingCleats = s_wearingCleats;
			friction = FRICTION_DEFAULT;
			if ((s_playerSector->flags1 & SEC_FLAGS1_ICE_FLOOR) && !wearingCleats)	// On Ice without cleats.
			{
				s_playerRun = JFALSE;
				friction = FRICTION_ICE;
			}
			else if ((s_playerSector->flags1 & SEC_FLAGS1_SNOW_FLOOR) || wearingCleats)	// On snow or wearing cleats.
			{
				// Lower friction means the player will stop sliding sooner.
				friction = FRICTION_CLEATS;
			}
			else if (s_playerSector->secHeight - 1 >= 0)	// In water
			{
				friction = FRICTION_DEFAULT;
				s_playerRun = wearingCleats;		// Once you get cleats, you move faster through water.
				s_playerInWater = JTRUE;
			}
		}

		// Apply friction to existing velocity.
		if (s_playerVelX || s_playerVelZ)
		{
			Tick dt = s_playerTick - s_prevPlayerTick;
			// Exponential friction, this is the same as vel * friction^dt
			for (s32 i = 0; i < dt; i++)
			{
				s_playerVelX = mul16(friction, s_playerVelX);
				s_playerVelZ = mul16(friction, s_playerVelZ);
			}
			// Just stop moving if the velocity is low enough, to avoid jittering forever.
			if (distApprox(0, 0, s_playerVelX, s_playerVelZ) < HALF_16)
			{
				s_playerVelX = 0;
				s_playerVelZ = 0;
			}
		}

		// Handle moving surfaces.
		JBool onMovingSurface = JFALSE;
		fixed16_16 floorHeight    = s_playerSector->floorHeight;
		fixed16_16 secHeight      = s_playerSector->secHeight;
		fixed16_16 secFloorHeight = floorHeight + secHeight;
		s_onMovingSurface = 0;
		if (player->posWS.y == floorHeight || player->posWS.y == secFloorHeight)
		{
			Allocator* links = s_playerSector->infLink;
			InfLink* link = (InfLink*)allocator_getHead(links);
			while (link)
			{
				if (link->type == LTYPE_SECTOR)
				{
					InfElevator* elev = link->elev;
					s_playerSector->floorHeight;
					JBool effected = JFALSE;
					if ((player->posWS.y == s_playerSector->floorHeight && (elev->flags & INF_EFLAG_MOVE_FLOOR)) ||
						(player->posWS.y != s_playerSector->floorHeight && (elev->flags & INF_EFLAG_MOVE_SECHT)))
					{
						effected = JTRUE;
					}
					if (effected)
					{
						fixed16_16 speed;
						vec3_fixed vel;
						inf_getMovingElevatorVelocity(elev, &vel, &speed);

						if (!speed && s_playerSector->secHeight > 0)
						{
							vel.x = mul16(vel.x, MOVING_SURFACE_MUL);
							vel.z = mul16(vel.z, MOVING_SURFACE_MUL);
						}
						else
						{
							// TODO(Core Game Loop Release)
						}

						if (vel.x || vel.z)
						{
							if (!onMovingSurface)
							{
								s_externalVelX = vel.x;
								s_externalVelZ = vel.z;
								onMovingSurface = JTRUE;
							}
							else
							{
								s_externalVelX += vel.x;
								s_externalVelZ += vel.z;
							}
							s_onMovingSurface = 1;
						}
					}
				}

				link = (InfLink*)allocator_getNext(links);
			}
		}

		// Handle player movement.
		if (s_forwardSpd || s_strafeSpd)
		{
			RSector* sector = player->sector;
			if (!s_wearingCleats && (s_playerSector->flags1 & SEC_FLAGS1_ICE_FLOOR))
			{
				s_forwardSpd = mul16(s_forwardSpd, ICE_SPEED_MUL);
				s_strafeSpd  = mul16(s_strafeSpd,  ICE_SPEED_MUL);
			}
			fixed16_16 speed = adjustForwardSpeed(s_forwardSpd);
			computeMoveFromAngleAndSpeed(&s_playerVelX, &s_playerVelZ, player->yaw, speed);

			// Add 90 degrees to get the strafe direction.
			speed = adjustStrafeSpeed(s_strafeSpd);
			computeMoveFromAngleAndSpeed(&s_playerVelX, &s_playerVelZ, player->yaw + 4095, speed);
			limitVectorLength(&s_playerVelX, &s_playerVelZ, s_maxMoveDist);
		}

		// Then convert from player velocity to per-frame movement.
		fixed16_16 moveX = adjustForwardSpeed(s_playerVelX);
		fixed16_16 moveZ = adjustForwardSpeed(s_playerVelZ);
		s_playerLogic.move.x = mul16(moveX, s_deltaTime) + mul16(s_externalVelX, s_deltaTime);
		s_playerLogic.move.z = mul16(moveZ, s_deltaTime) + mul16(s_externalVelZ, s_deltaTime);
		s_playerLogic.stepHeight = s_limitStepHeight ? PLAYER_STEP : PLAYER_INF_STEP;
		fixed16_16 width = (s_smallModeEnabled) ? PLAYER_SIZE_SMALL : PLAYER_WIDTH;
		player->worldWidth = width;

		// Handle collision detection and response.
		JBool collided = JFALSE;
		JBool moved = JFALSE;
		JBool prevResponseStep = JFALSE;
		fixed16_16 slideDirX = 0;
		fixed16_16 slideDirZ = 0;
		s_playerSlideWall = nullptr;
		// Up to 4 iterations, to handle sliding on walls.
		for (s32 collisionIter = 4; collisionIter != 0; collisionIter--)
		{
			if (!handlePlayerCollision(&s_playerLogic))
			{
				if (s_playerLogic.move.x || s_playerLogic.move.y)
				{
					moved = JTRUE;
				}
				break;
			}

			// Determine if a collision response is required.
			collided = JTRUE;
			if (prevResponseStep != s_colResponseStep)
			{
				break;
			}
			prevResponseStep = s_colResponseStep;

			if (s_colWall0)
			{
				s_playerSlideWall = s_colWall0;
			}
			slideDirX = s_colResponseDir.x;
			slideDirZ = s_colResponseDir.z;
		}

		// If the player moved after collision, handle that here.
		if (moved)
		{
			// Adjust velocity based on collision response.
			if (collided)
			{
				// Adjust the velocity based on the collision response.
				fixed16_16 projVel = mul16(s_playerVelX, slideDirX) + mul16(s_playerVelZ, slideDirZ);
				s_playerVelX = mul16(projVel, slideDirX);
				s_playerVelZ = mul16(projVel, slideDirZ);

				// Adjust the external velocity based on the collision response.
				if (s_externalVelX || s_externalVelZ)
				{
					projVel = mul16(s_externalVelX, slideDirX) + mul16(s_externalVelZ, slideDirZ);
					s_externalVelX = mul16(projVel, slideDirX);
					s_externalVelZ = mul16(projVel, slideDirZ);
				}
			}

			// If this fails, the system will treat it as if the player didn't move (see below).
			// However, it should succeed since the original collision detection did.
			moved = playerMove();

			if (moved)
			{
				s_colWidth += PLAYER_PICKUP_ADJ;	// s_colWidth + 1.5
				playerHandleCollisionFunc(player->sector, collision_checkPickups, nullptr);
				s_colWidth -= PLAYER_PICKUP_ADJ;
			}
		}

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
		s32 healthFx, shieldFx, flashFx;
		if (s_healthDamageFx)
		{
			s32 effectIndex = min(15, s_healthDamageFx >> 16);
			healthFx = c_healthDmgToFx[effectIndex];
			s_healthDamageFx = max(0, s_healthDamageFx - ONE_16);
		}
		else
		{
			healthFx = 0;
		}
		if (s_shieldDamageFx)
		{
			s32 effectIndex = min(15, s_shieldDamageFx >> 16);
			healthFx = c_shieldDmgToFx[effectIndex];
			s_shieldDamageFx = max(0, s_shieldDamageFx - ONE_16);
		}
		else
		{
			shieldFx = 0;
		}
		if (s_flashEffect)
		{
			s32 effectIndex = min(15, s_flashEffect >> 16);
			flashFx = c_flashAmtToFx[effectIndex];
			s_flashEffect = max(0, s_flashEffect - ONE_16);
		}
		else
		{
			flashFx = 0;
		}
		setScreenFxLevels(healthFx, shieldFx, flashFx);
	}
}  // TFE_DarkForces