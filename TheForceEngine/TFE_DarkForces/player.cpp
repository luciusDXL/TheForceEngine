#include <cstring>

#include "player.h"
#include "playerLogic.h"
#include "playerCollision.h"
#include "projectile.h"
#include "automap.h"
#include "agent.h"
#include "config.h"
#include "hud.h"
#include "mission.h"
#include "pickup.h"
#include "weapon.h"
#include <TFE_System/system.h>
#include <TFE_Settings/settings.h>
#include <TFE_Input/inputMapping.h>
#include <TFE_Game/igame.h>
#include <TFE_DarkForces/mission.h>
#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/InfSystem/infSystem.h>
#include <TFE_Jedi/Renderer/rlimits.h>
// Internal types need to be included in this case.
#include <TFE_Jedi/InfSystem/infTypesInternal.h>
#include <TFE_Jedi/Renderer/jediRenderer.h>
#include <TFE_Jedi/Renderer/RClassic_Fixed/rclassicFixed.h>
#include <TFE_Audio/audioSystem.h>

using namespace TFE_Input;

namespace TFE_DarkForces
{
	///////////////////////////////////////////
	// Constants
	///////////////////////////////////////////
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
		PLAYER_DMG_CRUSHING            = FIXED(10),   // The player suffers 10 dmg/sec. from crushing damage.
		PLAYER_FALL_SCREAM_VEL         = FIXED(60),	  // Fall speed when the player starts screaming.
		PLAYER_FALL_SCREAM_MINHEIGHT   = FIXED(55),   // The player needs to be at least 55 units from the ground before screaming.
		PLAYER_FALL_HIT_SND_HEIGHT     = FIXED(4),	  // 4.0 units, fall height for player to make a sound when hitting the ground.
		PLAYER_LAND_VEL_CHANGE		   = FIXED(60),	  // Point wear landing velocity to head change velocity changes slope.
		PLAYER_LAND_VEL_MAX            = FIXED(1000), // Maximum head change landing velocity.

		PLAYER_FORWARD_SPEED           = FIXED(256),  // Forward speed in units/sec.
		PLAYER_STRAFE_SPEED            = FIXED(192),  // Strafe speed in units/sec.
		PLAYER_CROUCH_SPEED            = 747108,      // 11.4 units/sec
		PLAYER_MOUSE_TURN_SPD          = 8,           // Mouse position delta to turn speed multiplier.
		PLAYER_KB_TURN_SPD             = 3413,        // Keyboard turn speed in angles/sec.
		PLAYER_CONTROLLER_TURN_SPD     = 3413*3,	  // Controller turn speed in angles/sec.
		PLAYER_CONTROLLER_PITCH_SPD    = 3413*2,	  // Controller turn speed in angles/sec.
		PLAYER_JUMP_IMPULSE            = -2850816,    // Jump vertical impuse: -43.5 units/sec.

		PLAYER_MAX_MOVE_DIST           = FIXED(32),
		PLAYER_STOP_ACCEL              = -540672,	  // -8.25
		PLAYER_MIN_EYE_DIST_FLOOR      = FIXED(2),
		PLAYER_GRAVITY_ACCEL           = FIXED(150),

		PITCH_LIMIT = 2047,

		HEADLAMP_ENERGY_CONSUMPTION = 0x111,    // fraction of energy consumed per second = ~0.004
		GASMASK_ENERGY_CONSUMPTION  = 0x444,    // fraction of energy consumed per second = ~0.0167
		GOGGLES_ENERGY_CONSUMPTION  = 0x444,    // fraction of energy consumed per second = ~0.0167
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
		
	///////////////////////////////////////////
	// Internal State
	///////////////////////////////////////////
	// Player Controller
	static fixed16_16 s_externalYawSpd;
	static angle14_32 s_playerPitch;
	static angle14_32 s_playerRoll;
	static fixed16_16 s_forwardSpd;
	static fixed16_16 s_strafeSpd;
	static fixed16_16 s_maxMoveDist;
	static fixed16_16 s_playerStopAccel;
	static fixed16_16 s_minEyeDistFromFloor;
	static fixed16_16 s_postLandVel;
	static fixed16_16 s_landUpVel = 0;
	static fixed16_16 s_playerVelX;
	static fixed16_16 s_playerUpVel;
	static fixed16_16 s_playerUpVel2;
	static fixed16_16 s_playerVelZ;
	static fixed16_16 s_externalVelX;
	static fixed16_16 s_externalVelZ;
	static fixed16_16 s_playerCrouchSpd;
	static fixed16_16 s_playerSpeed;
	static fixed16_16 s_prevDistFromFloor = 0;
	static fixed16_16 s_wpnSin = 0;
	static fixed16_16 s_wpnCos = 0;
	static fixed16_16 s_moveDirX;
	static fixed16_16 s_moveDirZ;
	static fixed16_16 s_dist;
	static fixed16_16 s_distScale;
	// Misc
	static s32 s_levelAtten = 0;
	static Safe* s_curSafe = nullptr;
	// Actions
	static JBool s_playerUse = JFALSE;
	static JBool s_playerActionUse = JFALSE;
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
	static fixed16_16 s_playerObjHeight;
	static angle14_32 s_playerObjPitch;
	static angle14_32 s_playerObjYaw;
	static RSector* s_playerObjSector;
	static RWall* s_playerSlideWall;
	
	///////////////////////////////////////////
	// Shared State
	///////////////////////////////////////////
	PlayerInfo s_playerInfo = { 0 };
	PlayerLogic s_playerLogic = { 0 };
	fixed16_16 s_energy = 2 * ONE_16;
	s32 s_lifeCount;
	s32 s_playerLight = 0;
	s32 s_headwaveVerticalOffset;
	u32 s_moveAvail = 0xffffffff;
	s32 s_weaponLight = 0;
	s32 s_baseAtten = 0;
	fixed16_16 s_gravityAccel;

	s32   s_invincibility = 0;
	JBool s_weaponFiring = JFALSE;
	JBool s_weaponFiringSec = JFALSE;
	JBool s_wearingCleats = JFALSE;
	JBool s_wearingGasmask = JFALSE;
	JBool s_nightvisionActive = JFALSE;
	JBool s_headlampActive = JFALSE;
	JBool s_superCharge   = JFALSE;
	JBool s_superChargeHud= JFALSE;
	JBool s_playerSecMoved = JFALSE;
	u32*  s_playerInvSaved = nullptr;

	RSector* s_playerSector = nullptr;
	SecObject* s_playerObject = nullptr;
	SecObject* s_playerEye = nullptr;
	vec3_fixed s_eyePos = { 0 };	// s_camX, s_camY, s_camZ in the DOS code.
	angle14_32 s_pitch = 0, s_yaw = 0, s_roll = 0;
	u32 s_playerEyeFlags = 4;
	Tick s_playerTick;
	Tick s_prevPlayerTick;
	Tick s_nextShieldDmgTick;
	Tick s_reviveTick = 0;
	Tick s_nextPainSndTick = 0;
	Task* s_playerTask = nullptr;

	fixed16_16 s_playerYPos;
	vec3_fixed s_camOffset = { 0 };
	angle14_32 s_camOffsetPitch = 0;
	angle14_32 s_camOffsetYaw = 0;
	angle14_32 s_camOffsetRoll = 0;
	angle14_32 s_playerYaw;

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

	fixed16_16 s_playerHeight;
	// Speed Modifiers
	s32 s_playerRun = 0;
	s32 s_jumpScale = 0;
	s32 s_playerSlow = 0;
	s32 s_onMovingSurface = 0;
			   
	///////////////////////////////////////////
	// Forward Declarations
	///////////////////////////////////////////
	void playerControlTaskFunc(MessageType msg);
	void playerControlMsgFunc(MessageType msg);
	void setPlayerLight(s32 atten);
	void setCameraOffset(fixed16_16 offsetX, fixed16_16 offsetY, fixed16_16 offsetZ);
	void setCameraAngleOffset(angle14_32 offsetPitch, angle14_32 offsetYaw, angle14_32 offsetRoll);
	void gasSectorTaskFunc(MessageType msg);

	void handlePlayerMoveControls();
	void handlePlayerPhysics();
	void handlePlayerActions();
	void handlePlayerScreenFx();
		
	///////////////////////////////////////////
	// API Implentation
	///////////////////////////////////////////
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
		s_playerInfo.itemUnused    = inv[19];
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
		s_playerInfo.selectedWeapon = -1;

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

	void player_writeInfo(u8* inv, s32* ammo)
	{
		// Write the inventory.
		inv[0]  = s_playerInfo.itemPistol;
		inv[1]  = s_playerInfo.itemRifle;
		inv[2]  = s_itemUnknown1;
		inv[3]  = s_playerInfo.itemAutogun;
		inv[4]  = s_playerInfo.itemMortar;
		inv[5]  = s_playerInfo.itemFusion;
		inv[6]  = s_itemUnknown2;
		inv[7]  = s_playerInfo.itemConcussion;
		inv[8]  = s_playerInfo.itemCannon;
		inv[9]  = s_playerInfo.itemRedKey;
		inv[10] = s_playerInfo.itemYellowKey;
		inv[11] = s_playerInfo.itemBlueKey;
		inv[12] = s_playerInfo.itemGoggles;
		inv[13] = s_playerInfo.itemCleats;
		inv[14] = s_playerInfo.itemMask;
		inv[15] = s_playerInfo.itemPlans;
		inv[16] = s_playerInfo.itemPhrik;
		inv[17] = s_playerInfo.itemNava;
		inv[18] = s_playerInfo.itemDatatape;
		inv[19] = s_playerInfo.itemUnused;
		inv[20] = s_playerInfo.itemDtWeapon;
		inv[21] = s_playerInfo.itemCode1;
		inv[22] = s_playerInfo.itemCode2;
		inv[23] = s_playerInfo.itemCode3;
		inv[24] = s_playerInfo.itemCode4;
		inv[25] = s_playerInfo.itemCode5;
		inv[26] = s_playerInfo.itemCode6;
		inv[27] = s_playerInfo.itemCode7;
		inv[28] = s_playerInfo.itemCode8;
		inv[29] = s_playerInfo.itemCode9;
		inv[30] = s_playerInfo.curWeapon;
		inv[31] = s_lifeCount;

		// Write Ammo, Shields, Health, and Energy.
		ammo[0] = s_playerInfo.ammoEnergy;
		ammo[1] = s_playerInfo.ammoPower;
		ammo[2] = s_playerInfo.ammoPlasma;
		ammo[3] = s_playerInfo.ammoDetonator;
		ammo[4] = s_playerInfo.ammoShell;
		ammo[5] = s_playerInfo.ammoMine;
		ammo[6] = s_playerInfo.ammoMissile;
		ammo[7] = s_playerInfo.shields;
		ammo[8] = s_playerInfo.health;
		ammo[9] = s_energy;
	}

	void player_setNextWeapon(s32 nextWpn)
	{
		weapon_setNext(nextWpn);
		if (nextWpn > s_playerInfo.maxWeapon)
		{
			s_playerInfo.maxWeapon = nextWpn;
		}
	}
		
	void player_createController()
	{
		s_playerTask = createTask("player control", playerControlTaskFunc, JFALSE, playerControlMsgFunc);
		task_setNextTick(s_playerTask, TASK_SLEEP);

		// Clear out inventory items that the player shouldn't start a level with, such as objectives and keys
		s_playerInfo.itemPlans    = JFALSE;
		s_playerInfo.itemPhrik    = JFALSE;
		s_playerInfo.itemNava     = JFALSE;
		s_playerInfo.itemUnused   = JFALSE;
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

		s_playerDying = 0;
		s_externalYawSpd = 0;
		s_playerPitch = 0;
		s_playerRoll = 0;
		s_forwardSpd = 0;
		s_strafeSpd = 0;

		// Set constants.
		s_maxMoveDist         = PLAYER_MAX_MOVE_DIST;
		s_playerStopAccel     = PLAYER_STOP_ACCEL;
		s_minEyeDistFromFloor = PLAYER_MIN_EYE_DIST_FLOOR;
		s_gravityAccel        = PLAYER_GRAVITY_ACCEL;

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
		TFE_System::logWrite(LOG_MSG, "Player", "Setting up level '%s'", levelName);
		if (!strcasecmp(levelName, "jabship"))
		{
			u8* src  = (u8*)&s_playerInfo;
			size_t size = (size_t)&s_playerInfo.stateUnknown - (size_t)&s_playerInfo;
			assert(size == 140);
			s_playerInvSaved = nullptr;	// This should already be null, but...
			if (!s_playerInvSaved)
			{
				u8* dst = (u8*)level_alloc(size);
				if (!dst)
				{
					TFE_System::logWrite(LOG_ERROR, "Player", "Cannot allocate player inventory space - %u bytes.", size);
					dst = (u8*)level_alloc(140);
				}

				s_playerInvSaved = (u32*)dst;
				memcpy(dst, src, size);
				memset(src, 0, size);

				TFE_System::logWrite(LOG_MSG, "Player", "Player Inventory copied, size = %u", size);

				player_setNextWeapon(0);
				disableMask();
				disableCleats();
				disableNightvision();
				hud_clearMessage();
			}
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

	void player_getVelocity(vec3_fixed* vel)
	{
		vel->x = s_playerVelX;
		vel->y = s_playerUpVel;
		vel->z = s_playerVelZ;
	}

	fixed16_16 player_getSquaredDistance(SecObject* obj)
	{
		fixed16_16 dx = obj->posWS.x - s_playerObject->posWS.x;
		fixed16_16 dy = obj->posWS.y - s_playerObject->posWS.y;
		fixed16_16 dz = obj->posWS.z - s_playerObject->posWS.z;

		// distSq overflows if dist > 181 units.
		return mul16(dx, dx) + mul16(dy, dy) + mul16(dz, dz);
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

	void player_revive()
	{
		// player_revive() is called when the player respawns, which is why it sets 100 for shields here.
		// In the case of picking up the item, the value is then set to 200 after the function call.
		s_playerInfo.shields = 100;
		s_playerInfo.health = 100;
		s_playerInfo.healthFract = 0;
		s_playerDying = 0;
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

	void giveAllWeaponsAndHealth()
	{
		s_playerInfo.health = 100;
		s_playerInfo.healthFract = 0;
		s_playerDying = 0;
		s_playerInfo.shields = 200;

		s_playerInfo.itemPistol = JTRUE;
		s_playerInfo.itemRifle = JTRUE;
		// s_282428 = JTRUE;
		s_playerInfo.itemAutogun = JTRUE;
		s_playerInfo.itemMortar = JTRUE;
		s_playerInfo.itemFusion = JTRUE;
		// s_28242c = JTRUE;
		s_playerInfo.itemConcussion = JTRUE;
		s_playerInfo.itemCannon = JTRUE;
		s_playerInfo.itemGoggles = JTRUE;
		s_playerInfo.itemCleats = JTRUE;
		s_playerInfo.itemMask = JTRUE;

		s_playerInfo.ammoEnergy = 500;
		s_playerInfo.ammoPower = 500;
		s_playerInfo.ammoDetonator = 50;
		s_playerInfo.ammoShell = 50;
		s_playerInfo.ammoPlasma = 400;
		s_playerInfo.ammoMine = 30;
		s_playerInfo.ammoMissile = 20;

		s_energy = FIXED(2);
		weapon_fixupAnim();
	}

	void giveHealthAndFullAmmo()
	{
		s_playerInfo.health = 100;
		s_playerInfo.healthFract = 0;
		s_playerDying = 0;
		s_playerInfo.shields = 200;
		s_playerInfo.ammoEnergy = 500;

		if (s_playerInfo.itemAutogun || s_playerInfo.itemFusion || s_playerInfo.itemConcussion)
		{
			s_playerInfo.ammoPower = 500;
		}
		if (s_playerInfo.itemCannon)
		{
			s_playerInfo.ammoPlasma = 400;
		}
		s_playerInfo.ammoDetonator = 50;
		if (s_playerInfo.itemMortar)
		{
			s_playerInfo.ammoShell = 50;
		}
		s_playerInfo.ammoMine = 30;
		if (s_playerInfo.itemCannon)
		{
			s_playerInfo.ammoMissile = 20;
		}
		s_energy = FIXED(2);
		weapon_fixupAnim();
	}

	void giveAllInventoryAndHealth()
	{
		s_playerInfo.health = 100;
		s_playerInfo.healthFract = 0;
		s_playerDying = 0;
		s_playerInfo.shields = 200;
		s_playerInfo.itemPistol = JTRUE;
		s_playerInfo.itemRifle = JTRUE;
		// s_282428 = JTRUE;
		s_playerInfo.itemAutogun = JTRUE;
		s_playerInfo.itemMortar = JTRUE;
		s_playerInfo.itemFusion = JTRUE;
		// s_28242c = JTRUE;
		s_playerInfo.itemConcussion = JTRUE;
		s_playerInfo.itemCannon = JTRUE;
		s_playerInfo.itemRedKey = JTRUE;
		s_playerInfo.itemYellowKey = JTRUE;
		s_playerInfo.itemBlueKey = JTRUE;
		s_playerInfo.itemGoggles = JTRUE;
		s_playerInfo.itemCleats = JTRUE;
		s_playerInfo.itemMask = JTRUE;
		s_playerInfo.itemPlans = JTRUE;
		s_playerInfo.itemPhrik = JTRUE;
		s_playerInfo.itemNava = JTRUE;
		s_playerInfo.itemDatatape = JTRUE;
		s_playerInfo.itemUnused = JTRUE;
		s_playerInfo.itemDtWeapon = JTRUE;
		s_playerInfo.itemCode1 = JTRUE;
		s_playerInfo.itemCode2 = JTRUE;
		s_playerInfo.itemCode3 = JTRUE;
		s_playerInfo.itemCode4 = JTRUE;
		s_playerInfo.itemCode5 = JTRUE;
		s_playerInfo.itemCode6 = JTRUE;
		s_playerInfo.itemCode7 = JTRUE;
		s_playerInfo.itemCode8 = JTRUE;
		s_playerInfo.itemCode9 = JTRUE;
		s_playerInfo.ammoEnergy = 500;
		s_playerInfo.ammoPower = 500;
		s_playerInfo.ammoDetonator = 50;
		s_playerInfo.ammoShell = 50;
		s_playerInfo.ammoMissile = 20;
		s_playerInfo.ammoPlasma = 400;
		s_playerInfo.ammoMine = 30;
		s_energy = FIXED(2);

		weapon_fixupAnim();
	}

	void giveKeys()
	{
		s_playerInfo.itemRedKey = JTRUE;
		s_playerInfo.itemYellowKey = JTRUE;
		s_playerInfo.itemBlueKey = JTRUE;
		s_playerInfo.itemGoggles = JTRUE;
		s_playerInfo.itemCleats = JTRUE;
		s_playerInfo.itemMask = JTRUE;
		s_playerInfo.itemPlans = JTRUE;
		s_playerInfo.itemPhrik = JTRUE;
		s_playerInfo.itemNava = JTRUE;
		s_playerInfo.itemDatatape = JTRUE;
		s_playerInfo.itemUnused = JTRUE;
		s_playerInfo.itemDtWeapon = JTRUE;
		s_playerInfo.itemCode1 = JTRUE;
		s_playerInfo.itemCode2 = JTRUE;
		s_playerInfo.itemCode3 = JTRUE;
		s_playerInfo.itemCode4 = JTRUE;
		s_playerInfo.itemCode5 = JTRUE;
		s_playerInfo.itemCode6 = JTRUE;
		s_playerInfo.itemCode7 = JTRUE;
		s_playerInfo.itemCode8 = JTRUE;
		s_playerInfo.itemCode9 = JTRUE;
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

	void cheat_postal()
	{
		giveAllWeaponsAndHealth();
		hud_sendTextMessage(703);
	}

	void cheat_fullAmmo()
	{
		giveHealthAndFullAmmo();
		hud_sendTextMessage(708);
	}

	void cheat_unlock()
	{
		giveKeys();
		hud_sendTextMessage(709);
	}

	void cheat_maxout()
	{
		giveAllInventoryAndHealth();
		hud_sendTextMessage(710);
	}

	void cheat_godMode()
	{
		if (!s_invincibility)
		{
			s_invincibility = -2;
		}
		else
		{
			s_invincibility = 0;
		}
		hud_sendTextMessage(702);
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
				renderer_computeCameraTransform(s_playerEye->sector, s_pitch, s_yaw, s_eyePos.x, s_eyePos.y, s_eyePos.z);
			}
			renderer_setWorldAmbient(s_playerLight);
		}

		// This is temporary until the full Jedi Sound system is reverse-engineered.
		vec2_fixed dir;
		sinCosFixed(s_yaw, &dir.x, &dir.z);

		Vec3f listenerPos = { fixed16ToFloat(s_eyePos.x), fixed16ToFloat(s_eyePos.y), fixed16ToFloat(s_eyePos.z) };
		Vec3f listenerDir = { fixed16ToFloat(dir.x), 0, fixed16ToFloat(dir.z) };
		TFE_Audio::update(&listenerPos, &listenerDir);
	}

	void computeDamagePushVelocity(ProjectileLogic* proj, vec3_fixed* vel)
	{
		fixed16_16 push = mul16(proj->projForce, proj->speed);
		vel->x = mul16(push, proj->dir.x);
		vel->z = mul16(push, proj->dir.z);

		if (proj->dir.y < 0)
		{
			vel->y = mul16(push, proj->dir.y) * 2;
		}
		else
		{
			vel->y = 0;
		}
	}

	void computeExplosionPushDir(vec3_fixed* pos, vec3_fixed* pushDir)
	{
		vec3_fixed delta = { pos->x - s_explodePos.x, pos->y - s_explodePos.y, pos->z - s_explodePos.z };

		fixed16_16 dirZ, dirX;
		fixed16_16 len = computeDirAndLength(delta.x, delta.z, &dirX, &dirZ);

		fixed16_16 dirY;
		computeDirAndLength(len, delta.y, &dirY, &pushDir->y);
		pushDir->x = mul16(dirY, dirX);
		pushDir->z = mul16(dirY, dirZ);
	}

	void playerControlMsgFunc(MessageType msg)
	{
		fixed16_16 shieldDmg = 0;
		JBool playHitSound = JFALSE;

		if (msg == MSG_DAMAGE)
		{
			ProjectileLogic* proj = (ProjectileLogic*)s_msgEntity;
			vec3_fixed pushVel;
			computeDamagePushVelocity(proj, &pushVel);

			s_playerVelX   += pushVel.x;
			s_playerUpVel2 += pushVel.y;
			s_playerVelZ   += pushVel.z;

			if (s_invincibility || s_config.superShield)
			{
				// TODO

				// Return because no damage is applied.
				return;
			}
			else
			{
				playHitSound = JTRUE;
				shieldDmg = proj->dmg;
			}
		}
		else if (msg == MSG_EXPLOSION)
		{
			vec3_fixed pos = { s_playerObject->posWS.x, s_playerObject->posWS.y - s_playerObject->worldHeight, s_playerObject->posWS.z };
			vec3_fixed pushDir;
			computeExplosionPushDir(&pos, &pushDir);

			fixed16_16 force = s_msgArg2;
			s_playerVelX   += mul16(force, pushDir.x);
			s_playerUpVel2 += mul16(force, pushDir.y);
			s_playerVelZ   += mul16(force, pushDir.z);

			if (s_invincibility || s_config.superShield)
			{
				// Return because no damage is applied.
				return;
			}

			playHitSound = JTRUE;
			if (s_curEffectData->type == HEFFECT_SMALL_EXP)
			{
				shieldDmg = s_msgArg1 >> 1;
			}
			else
			{
				shieldDmg = s_msgArg1;
			}
		}

		player_applyDamage(0, shieldDmg, playHitSound);
	}

	void player_reset()
	{
		s_externalYawSpd = 0;
		s_playerPitch = 0;
		s_playerRoll = 0;
		s_forwardSpd = 0;
		s_strafeSpd = 0;
		s_maxMoveDist = FIXED(2);
		// s_282344 = 34;
		s_minEyeDistFromFloor = FIXED(2);
		s_postLandVel = 0;
		s_landUpVel = 0;
		s_playerVelX = 0;
		s_playerUpVel = 0;
		s_playerUpVel2 = 0;
		s_playerVelZ = 0;
		s_externalVelX = 0;
		// s_282380 = 0;
		s_externalVelZ = 0;
		s_playerSpeed = 0;
		s_weaponLight = 0;
		s_headwaveVerticalOffset = 0;
		s_playerUse = JFALSE;
		s_playerActionUse = JFALSE;
		s_moveAvail = 0;

		s_playerStopAccel = -540672;	// -8.25
		s_playerCrouchSpd = 0;
		s_prevDistFromFloor = 0;
		s_playerObject->worldHeight = 0x5cccc;	// 5.8
	}

	void player_changeSector(RSector* newSector)
	{
		if (newSector)
		{
			RSector* prevSector = s_playerObject->sector;
			if (newSector != prevSector)
			{
				if (newSector->layer != prevSector->layer)
				{
					automap_setLayer(newSector->layer);
				}
				sector_addObject(newSector, s_playerObject);
			}
		}
	}

	void player_clearSuperCharge()
	{
		if (s_superchargeTask)
		{
			task_free(s_superchargeTask);
		}
		s_superCharge = JFALSE;
		s_superChargeHud = JFALSE;
		s_superchargeTask = nullptr;
		weapon_setFireRate();
	}

	void handlePlayerDying()
	{
		s_minEyeDistFromFloor = ONE_16;
		s_playerCrouchSpd = 0x5b332;	// 5.7
		s_playerPitch += mul16(0x6aa, s_deltaTime);
		s_playerRoll  -= mul16(0xd55, s_deltaTime);

		s_playerPitch = clamp(s_playerPitch, -1023, 1023);
		s_playerRoll  = clamp(s_playerRoll, -1592, 1592);

		s_playerSecFire = JFALSE;
		s_playerPrimaryFire = JFALSE;

		if (inputMapping_getActionState(IADF_JUMP) || inputMapping_getActionState(IADF_MENU_TOGGLE))
		{
			inputMapping_removeState(IADF_JUMP);
			inputMapping_removeState(IADF_MENU_TOGGLE);

			if (s_lifeCount != 0 && s_curSafe)
			{
				s_lifeCount -= 1;
				player_revive();
				player_reset();
				s_headlampActive = JFALSE;

				s_playerObject->yaw = s_curSafe->yaw;
				s_playerYaw = s_curSafe->yaw;
				s_playerObject->posWS.x = s_curSafe->x;
				s_playerObject->posWS.z = s_curSafe->z;

				RSector* sector = s_curSafe->sector;
				fixed16_16 floorHeight = sector->floorHeight + sector->secHeight;
				s_playerObject->posWS.y = floorHeight;
				s_playerYPos = s_playerObject->posWS.y;
				player_changeSector(sector);

				s_nextShieldDmgTick = s_curTick + 436;
				if (s_invincibilityTask)
				{
					task_free(s_invincibilityTask);
				}
				s_invincibilityTask = nullptr;
				s_invincibility = 0;
				player_clearSuperCharge();
			}
			else
			{
				player_revive();
				player_reset();
				s_levelComplete = JFALSE;
				mission_exitLevel();
			}
		}
	}

	void playerControlTaskFunc(MessageType msg)
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
		s_playerTick = s_curTick;
		s_prevPlayerTick  = s_curTick;

		while (msg != MSG_FREE_TASK)
		{
			if (msg == MSG_RUN_TASK)
			{
				if (!s_gamePaused)
				{
					handlePlayerMoveControls();
					handlePlayerPhysics();
					handlePlayerActions();
					handlePlayerScreenFx();
					if (s_playerDying)
					{
						handlePlayerDying();
					}
				}
			}
			else if (msg == MSG_DAMAGE)
			{
				// TODO(Core Game Loop Release)
			}
			else if (msg == MSG_EXPLOSION)
			{
				// TODO(Core Game Loop Release)
			}
						
			s_prevPlayerTick = s_playerTick;
			task_yield(TASK_NO_DELAY);
		}
		task_end;
	}
	
	///////////////////////////////////////////
	// Internal Implentation
	///////////////////////////////////////////
	void setPlayerLight(s32 atten)
	{
		s_playerLight = atten;
	}

	void handlePlayerMoveControls()
	{
		s_externalYawSpd = 0;
		s_forwardSpd = 0;
		s_strafeSpd = 0;
		s_playerRun = 0;
		s_jumpScale = 0;
		s_playerSlow = 0;

		if (s_playerDying)
		{
			return;
		}

		s_playerCrouchSpd = s_playerStopAccel;

		s32 mdx, mdy;
		TFE_Input::getAccumulatedMouseMove(&mdx, &mdy);
		InputConfig* inputConfig = TFE_Input::inputMapping_get();

		// Yaw change
		if (inputConfig->mouseMode == MMODE_TURN || inputConfig->mouseMode == MMODE_LOOK)
		{
			s_playerYaw += s32(f32(mdx * PLAYER_MOUSE_TURN_SPD) * inputMapping_getHorzMouseSensitivity());
			s_playerYaw &= ANGLE_MASK;
		}
		// Pitch change
		if (inputConfig->mouseMode == MMODE_LOOK)
		{
			s_playerPitch = clamp(s_playerPitch - s32(f32(mdy * PLAYER_MOUSE_TURN_SPD) * inputMapping_getVertMouseSensitivity()), -PITCH_LIMIT, PITCH_LIMIT);
		}

		// Controls
		if (inputMapping_getActionState(IADF_FORWARD))
		{
			fixed16_16 speed = mul16(PLAYER_FORWARD_SPEED, s_deltaTime);
			s_forwardSpd = max(speed, s_forwardSpd);
		}
		else if (inputMapping_getActionState(IADF_BACKWARD))
		{
			fixed16_16 speed = -mul16(PLAYER_FORWARD_SPEED, s_deltaTime);
			s_forwardSpd = min(speed, s_forwardSpd);
		}
		else if (inputMapping_getAnalogAxis(AA_MOVE))
		{
			fixed16_16 speed = mul16(mul16(PLAYER_FORWARD_SPEED, s_deltaTime), floatToFixed16(clamp(inputMapping_getAnalogAxis(AA_MOVE), -1.0f, 1.0f)));
			if (speed < 0)
			{
				s_forwardSpd = min(speed, s_forwardSpd);
			}
			else
			{
				s_forwardSpd = max(speed, s_forwardSpd);
			}
		}

		if (inputMapping_getActionState(IADF_RUN))
		{
			s_playerRun |= 1;
		}
		else if (inputMapping_getActionState(IADF_SLOW))
		{
			s_playerSlow |= 1;
		}

		s32 crouch = inputMapping_getActionState(IADF_CROUCH) ? 1 : 0;
		if (s_moveAvail & crouch)
		{
			fixed16_16 speed = PLAYER_CROUCH_SPEED;
			speed <<= s_playerRun;			// this will be '1' if running.
			speed >>= s_playerSlow;			// this will be '1' if moving slowly.
			s_playerCrouchSpd = speed;
		}

		JBool wasJumping = s_playerJumping;
		s_playerJumping = JFALSE;
		if (inputMapping_getActionState(IADF_JUMP))
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
		if (inputMapping_getActionState(IADF_TURN_LT))
		{
			fixed16_16 turnSpeed = PLAYER_KB_TURN_SPD;	// angle units per second.
			fixed16_16 dYaw = mul16(turnSpeed, s_deltaTime);
			dYaw <<= s_playerRun;		// double for "run"
			dYaw >>= s_playerSlow;		// half for "slow"

			s_playerYaw -= dYaw;
			s_playerYaw &= ANGLE_MASK;
		}
		else if (inputMapping_getActionState(IADF_TURN_RT))
		{
			fixed16_16 turnSpeed = PLAYER_KB_TURN_SPD;	// angle units per second.
			fixed16_16 dYaw = mul16(turnSpeed, s_deltaTime);
			dYaw <<= s_playerRun;		// double for "run"
			dYaw >>= s_playerSlow;		// half for "slow"

			s_playerYaw += dYaw;
			s_playerYaw &= ANGLE_MASK;
		}
		else if (inputMapping_getAnalogAxis(AA_LOOK_HORZ))
		{
			fixed16_16 turnSpeed = mul16(mul16(PLAYER_CONTROLLER_TURN_SPD, s_deltaTime), floatToFixed16(inputMapping_getAnalogAxis(AA_LOOK_HORZ)));
			s_playerYaw += turnSpeed;
			s_playerYaw &= ANGLE_MASK;
		}

		if (inputMapping_getActionState(IADF_LOOK_UP))
		{
			fixed16_16 turnSpeed = PLAYER_KB_TURN_SPD;	// angle units per second.
			fixed16_16 dPitch = mul16(turnSpeed, s_deltaTime);
			dPitch <<= s_playerRun;		// double for "run"
			dPitch >>= s_playerSlow;	// half for "slow"
			s_playerPitch = clamp(s_playerPitch + dPitch, -PITCH_LIMIT, PITCH_LIMIT);
		}
		else if (inputMapping_getActionState(IADF_LOOK_DN))
		{
			fixed16_16 turnSpeed = PLAYER_KB_TURN_SPD;	// angle units per second.
			fixed16_16 dPitch = mul16(turnSpeed, s_deltaTime);
			dPitch <<= s_playerRun;		// double for "run"
			dPitch >>= s_playerSlow;	// half for "slow"
			s_playerPitch = clamp(s_playerPitch - dPitch, -PITCH_LIMIT, PITCH_LIMIT);
		}
		else if (inputMapping_getAnalogAxis(AA_LOOK_VERT))
		{
			fixed16_16 turnSpeed = mul16(mul16(PLAYER_CONTROLLER_PITCH_SPD, s_deltaTime), floatToFixed16(inputMapping_getAnalogAxis(AA_LOOK_VERT)));
			s_playerPitch = clamp(s_playerPitch + turnSpeed, -PITCH_LIMIT, PITCH_LIMIT);
		}

		if (inputMapping_getActionState(IADF_CENTER_VIEW))
		{
			s_playerPitch = 0;
			s_playerRoll  = 0;
		}

		if (inputMapping_getActionState(IADF_STRAFE_RT))
		{
			fixed16_16 speed = mul16(PLAYER_STRAFE_SPEED, s_deltaTime);
			s_strafeSpd = max(speed, s_strafeSpd);
		}
		else if (inputMapping_getActionState(IADF_STRAFE_LT))
		{
			fixed16_16 speed = -mul16(PLAYER_STRAFE_SPEED, s_deltaTime);
			s_strafeSpd = min(speed, s_strafeSpd);
		}
		else if (inputMapping_getAnalogAxis(AA_STRAFE))
		{
			fixed16_16 speed = mul16(mul16(PLAYER_STRAFE_SPEED, s_deltaTime), floatToFixed16(clamp(inputMapping_getAnalogAxis(AA_STRAFE), -1.0f, 1.0f)));
			if (speed < 0)
			{
				s_strafeSpd = min(speed, s_strafeSpd);
			}
			else
			{
				s_strafeSpd = max(speed, s_strafeSpd);
			}
		}

		if (inputMapping_getActionState(IADF_USE))
		{
			s_playerUse = JTRUE;
		}

		if (inputMapping_getActionState(IADF_PRIMARY_FIRE))
		{
			s_playerPrimaryFire = JTRUE;
		}
		else if (inputMapping_getActionState(IADF_SECONDARY_FIRE))
		{
			s_playerSecFire = JTRUE;
		}

		// Reduce the players ability to adjust the velocity while they have vertical velocity.
		if (s_playerUpVel)
		{
			// TFE specific
			const s32 airControl = 8 - TFE_Settings::getGameSettings()->df_airControl;
			// In the original DOS code, airControl = 8.
			s_forwardSpd >>= airControl;
			s_strafeSpd  >>= airControl;
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
				
	void handlePlayerPhysics()
	{
		SecObject* player = s_playerObject;
		RSector* origSector = player->sector;
		fixed16_16 yVel = s_playerUpVel;
		s_playerInWater = JFALSE;

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
			for (Tick i = 0; i < dt; i++)
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
			if (!s_colResponseStep)
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

		if (!moved)
		{
			s_colDstPosX = player->posWS.x;
			s_colDstPosZ = player->posWS.z;
			col_computeCollisionResponse(player->sector);
			s_playerLogic.move.x = 0;
			s_playerLogic.move.z = 0;
			s_playerVelX = 0;
			s_playerVelZ = 0;
		}
		s_playerSector = s_nextSector;

		if (s_externalVelX || s_externalVelZ)
		{
			fixed16_16 friction = FRICTION_DEFAULT;
			Tick dt = s_playerTick - s_prevPlayerTick;
			for (Tick i = 0; i < dt; i++)
			{
				s_externalVelX = mul16(s_externalVelX, friction);
				s_externalVelZ = mul16(s_externalVelZ, friction);
			}
			fixed16_16 approxLen = distApprox(0, 0, s_externalVelX, s_externalVelZ);
			if (approxLen < HALF_16)
			{
				s_externalVelX = 0;
				s_externalVelZ = 0;
			}
		}

		if (s_playerSecMoved)
		{
			RSector* newSector = sector_which3D(player->posWS.x, player->posWS.y, player->posWS.z);
			RSector* curSector = player->sector;
			// Handle the case where the player changed sectors due to moving sectors.
			if (newSector && newSector != curSector)
			{
				if (newSector->layer != curSector->layer)
				{
					automap_setLayer(newSector->layer);
				}
				sector_addObject(newSector, player);
			}
			s_playerSecMoved = JFALSE;
		}

		if (origSector != player->sector)
		{
			RSector* newSector = player->sector;
			if (newSector->flags1 & SEC_FLAGS1_SAFESECTOR)	
			{
				Safe* safe = level_getSafeFromSector(newSector);
				if (safe)
				{
					s_curSafe = safe;
				}
			}

			if (newSector->flags1 & SEC_FLAGS1_SECRET)
			{
				// Remove the flag so the secret isn't counted twice.
				newSector->flags1 &= ~SEC_FLAGS1_SECRET;
				s_secretsFound++;
				if (s_secretCount)
				{
					// 100.0 * found / count
					fixed16_16 percentage = mul16(FIXED(100), div16(intToFixed16(s_secretsFound), intToFixed16(s_secretCount)));
					s_secretsPercent = floor16(percentage);
				}
				else
				{
					s_secretsPercent = 100;
				}
				s_secretsPercent = max(0, min(100, s_secretsPercent));
			}
		}

		// Adjust angles.
		player->yaw = s_playerYaw & ANGLE_MASK;
		player->pitch = s_playerPitch;
		if (s_externalYawSpd)
		{
			s_externalYawSpd = mul16(s_externalYawSpd, s_deltaTime);
			fixed16_16 speed = s_externalYawSpd << s_playerRun;
			speed >>= s_playerSlow;
			player->yaw += speed;
		}
		sinCosFixed(player->yaw, &s_playerLogic.dir.x, &s_playerLogic.dir.z);

		// Handle falling and jumping.
		fixed16_16 ceilHeight;
		sector_getObjFloorAndCeilHeight(s_playerSector, player->posWS.y, &floorHeight, &ceilHeight);
		fixed16_16 floorRelHeight = floorHeight - player->posWS.y;
		if (floorRelHeight > PLAYER_FALL_SCREAM_MINHEIGHT && s_playerUpVel2 > PLAYER_FALL_SCREAM_VEL && !s_kyleScreamSoundId && !s_playerDying)
		{
			s_kyleScreamSoundId = playSound2D(s_kyleScreamSoundSource);
		}
		sector_getObjFloorAndCeilHeight(player->sector, player->posWS.y, &floorHeight, &ceilHeight);

		// Warning: even though the code calculates 'gravityAccelDt' - it doesn't actually apply to the velocity.
		// I'm not sure where this happens yet - so for now it is added here until I can figure out what I'm missing.
		// Fortunately it seems to work correctly.
		fixed16_16 gravityAccelDt = mul16(s_gravityAccel, s_deltaTime);
		s_playerUpVel2 += gravityAccelDt;
		s_playerUpVel = s_playerUpVel2;
		s_playerYPos += mul16(s_playerUpVel, s_deltaTime);

		s_playerLogic.move.y = s_playerYPos - player->posWS.y;
				
		player->posWS.y = s_playerYPos;
		if (s_playerYPos >= s_colCurLowestFloor)
		{
			if (s_kyleScreamSoundId)
			{
				stopSound(s_kyleScreamSoundId);
				s_kyleScreamSoundId = NULL_SOUND;
			}
			// Handle player land event - this both plays a sound effect and sends an INF message.
			if (s_prevDistFromFloor)
			{
				if (s_nextSector->secHeight - 1 >= 0)
				{
					// Second height is below ground, so this is liquid.
					playSound2D(s_landSplashSound);
				}
				else if (s_prevDistFromFloor > PLAYER_FALL_HIT_SND_HEIGHT)
				{
					// Second height is at or above ground.
					playSound2D(s_landSolidSound);
				}
				message_sendToSector(s_nextSector, player, INF_EVENT_LAND, MSG_TRIGGER);

				// 's_playerUpVel' determines how much the view collapses to the ground based on hit velocity.
				// vel: s_playerUpVel
				// postLandVel = vel < 60 ? vel / 4 : vel / 8 + 7.5
				// note that 60/4 = 60/8 + 7.5
				s_postLandVel = s_playerUpVel >> 2;
				if (s_playerUpVel >= PLAYER_LAND_VEL_CHANGE)	// 60 units per second
				{
					s_postLandVel -= ((s_playerUpVel - PLAYER_LAND_VEL_CHANGE) >> 3);
				}
				s_postLandVel = min(PLAYER_LAND_VEL_MAX, s_postLandVel);	// Limit the maximum landing velocity.
				s_landUpVel = s_playerUpVel;
			}
			else
			{
				fixed16_16 yPos = player->posWS.y;
				fixed16_16 yMove = s_playerLogic.move.y;
				fixed16_16 distFromFloor = player->posWS.y - s_colCurLowestFloor;
				// This adjusts the world height to compensate for the step height.
				// Over time the world height will be restored.
				if (distFromFloor > yMove)
				{
					player->worldHeight += (s_colCurBot - yPos + yMove);
				}
			}

			s_playerYPos = s_colCurLowestFloor;
			player->posWS.y = s_colCurLowestFloor;
			fixed16_16 newUpVel = min(0, s_playerUpVel2);
			s_playerUpVel = newUpVel;
			s_playerUpVel2 = newUpVel;
		}
		else
		{
			fixed16_16 playerTop = s_playerYPos - player->worldHeight - ONE_16;
			if (playerTop < s_colCurHighestCeil)
			{
				fixed16_16 yVel = max(0, s_playerUpVel2);
				s_playerUpVel = yVel;
				s_playerUpVel2 = yVel;

				fixed16_16 newPlayerBot = s_colCurHighestCeil + player->worldHeight + ONE_16;
				if (newPlayerBot > s_colCurLowestFloor)
				{
					s_playerYPos = s_colCurLowestFloor;
				}
				else // Otherwise place the player so that their height is maintained.
				{
					s_playerYPos = newPlayerBot;
				}
				player->posWS.y = s_playerYPos;
			}
		}

		// Crouch
		player->worldHeight -= mul16(s_playerCrouchSpd, s_deltaTime);

		// Land animation.
		if (s_postLandVel)
		{
			// Modify the player height by post land velocity.
			player->worldHeight -= mul16(s_postLandVel, s_deltaTime);
			// Reduce the post land velocity by 20 units / second.
			s_postLandVel -= mul16(FIXED(20), s_deltaTime);
			if (s_postLandVel < 0)
			{
				s_postLandVel = 0;
			}
		}
		else
		{
			s_landUpVel = 0;
		}

		fixed16_16 eyeToCeil = max(0, player->posWS.y - s_colCurHighestCeil);
		fixed16_16 eyeHeight = eyeToCeil;
		eyeToCeil = min(ONE_16, eyeToCeil);	// the player eye should be clamped to 1 unit below the ceiling if possible.
		eyeHeight -= eyeToCeil;
		eyeHeight = min(PLAYER_HEIGHT, eyeHeight);

		RSector* sector = player->sector;
		fixed16_16 minEyeDistFromFloor = (s_smallModeEnabled) ? PLAYER_SIZE_SMALL : s_minEyeDistFromFloor;
		secHeight = sector->secHeight;
		// The base min distance from the floor is (0) for solid floors and (depth + 0.25) for liquids.
		fixed16_16 minDistToFloor = (sector->secHeight >= 0) ? secHeight + 0x4000 : 0;
		// Then the base is adjusted so it is never smaller than s_minEyeDistFromFloor.
		minDistToFloor = max(minEyeDistFromFloor, minDistToFloor);

		// Adjust world height over time to stand straight up and be the correct distance from the floor.
		if (player->worldHeight < minDistToFloor)
		{
			if (s_postLandVel)
			{
				// Fall Damage if moving at faster than 107 units / second.
				if (s_landUpVel > FIXED(107))
				{
					fixed16_16 dmg = 2 * (s_landUpVel - FIXED(107));
					player_applyDamage(dmg, 0, JFALSE);
					s_landUpVel = 0;
				}
			}
			player->worldHeight = minDistToFloor;
		}
		// Make sure eye height is clamped.
		player->worldHeight = min(eyeHeight, player->worldHeight);

		// Crushing Damage.
		if (player->worldHeight < minEyeDistFromFloor)
		{
			if (!s_crushSoundId)
			{
				s_crushSoundId = playSound2D(s_crushSoundSource);
			}
			// Crushing damage is 10 damage/second
			fixed16_16 crushingDmg = mul16(PLAYER_DMG_CRUSHING, s_deltaTime);
			player_applyDamage(0, crushingDmg, JFALSE);
			player->worldHeight = minDistToFloor;
			playerHandleCollisionFunc(player->sector, collision_handleCrushing, nullptr);
		}
		else if (s_crushSoundId)
		{
			stopSound(s_crushSoundId);
			s_crushSoundId = NULL_SOUND;
		}

		if (!s_smallModeEnabled)
		{
			fixed16_16 dH = PLAYER_HEIGHT - player->worldHeight;
			fixed16_16 maxMove = (s_playerInWater) ? FIXED(25) : FIXED(32);
			s_maxMoveDist = maxMove - dH * 4;
			//s32 dhIntX16 = floor16(dH << 4);
			//s32 dH = s32(float(dhIntX16) * 54.6f);
			//s_282344 = 34 - dH;
		}

		// Headwave
		s32 xWpnWaveOffset = 0;
		s_headwaveVerticalOffset = 0;
		if (s_config.headwave && (player->flags & 2))
		{
			fixed16_16 playerSpeed = distApprox(0, 0, s_playerVelX, s_playerVelZ);
			if (!moved)
			{
				playerSpeed = 0;
			}
			if ((s_playerSector->flags1 & SEC_FLAGS1_ICE_FLOOR) && !s_wearingCleats)
			{
				playerSpeed = 0;
			}

			if (playerSpeed != s_playerSpeed)
			{
				fixed16_16 speedDelta = playerSpeed - s_playerSpeed;
				fixed16_16 maxFrameChange = mul16(FIXED(32), s_deltaTime);

				if (speedDelta > maxFrameChange)
				{
					speedDelta = maxFrameChange;
				}
				else if (speedDelta < -maxFrameChange)
				{
					speedDelta = -maxFrameChange;
				}
				s_playerSpeed += speedDelta;
			}
			sinCosFixed((s_curTick & 0xffff) << 7, &s_wpnSin, &s_wpnCos);

			fixed16_16 playerSpeedFract = div16(s_playerSpeed, FIXED(32));
			s_headwaveVerticalOffset = mul16(mul16(s_wpnCos, PLAYER_HEADWAVE_VERT_SPD), playerSpeedFract);

			sinCosFixed((s_curTick & 0xffff) << 6, &s_wpnSin, &s_wpnCos);
			xWpnWaveOffset = mul16(playerSpeedFract, s_wpnCos) >> 12;

			// Water...
			if (s_playerSector->secHeight - 1 >= 0)
			{
				if (s_externalVelX || s_externalVelZ)
				{
					fixed16_16 externSpd = distApprox(0, 0, s_externalVelX, s_externalVelZ);

					// Replace the fractional part with the current time fractional part.
					// I think this is meant to add some "randomness" to the headwave while in water.
					fixed16_16 speed = externSpd & (~0xffff);
					speed |= (s_curTick & 0xffff);
					// Then multiply by 16 and take the cosine - this is meant to be a small modification to the weapon motion
					// (note that the base multiplier is 128).
					sinCosFixed(speed << 4, &s_wpnSin, &s_wpnCos);
					// Modify the headwave motion.
					s_headwaveVerticalOffset += mul16(s_wpnCos, PLAYER_HEADWAVE_VERT_WATER_SPD);
				}
			}
		}
		setCameraOffset(0, s_headwaveVerticalOffset, 0);

		// Apply the weapon motion.
		PlayerWeapon* weapon = s_curPlayerWeapon;
		weapon->xWaveOffset = xWpnWaveOffset;					// the x offset has 4 bits of sub-texel precision.
		weapon->yWaveOffset = s_headwaveVerticalOffset >> 13;	// the y offset is probably the same 4 bits of precision & multiplied by half.
			   
		// The moves the player can make are restricted based on whether they are on the floor or not.
		if (s_colCurLowestFloor == player->posWS.y)
		{
			s_moveAvail = 0xffffffff;
			s_prevDistFromFloor = 0;
		}
		else
		{
			// Player is *not* on the floor, so things like crouching and jumping are not available.
			s_moveAvail = 0;
		}

		// Apply sector or floor damage.
		u32 lowAndHighFlag = SEC_FLAGS1_LOW_DAMAGE | SEC_FLAGS1_HIGH_DAMAGE;
		u32 dmgFlags = s_playerSector->flags1 & lowAndHighFlag;
		// Handle damage floors.
		if (dmgFlags == SEC_FLAGS1_LOW_DAMAGE && s_moveAvail)
		{
			fixed16_16 dmg = mul16(PLAYER_DMG_FLOOR_LOW, s_deltaTime);
			player_applyDamage(dmg, 0, JTRUE);
		}
		else if (dmgFlags == SEC_FLAGS1_HIGH_DAMAGE && s_moveAvail)
		{
			fixed16_16 dmg = mul16(PLAYER_DMG_FLOOR_HIGH, s_deltaTime);
			player_applyDamage(dmg, 0, JTRUE);
		}
		else if (dmgFlags == lowAndHighFlag && !s_wearingGasmask && !s_playerDying)
		{
			fixed16_16 dmg = mul16(PLAYER_DMG_FLOOR_LOW, s_deltaTime);
			player_applyDamage(dmg, 0, JFALSE);

			if (!s_gasSectorTask)
			{
				s_gasSectorTask = createSubTask("gas sector", gasSectorTaskFunc);
			}
		}
		// Free the gas damage task if the gas mask is worn or if the damage flags no longer match up.
		if (s_gasSectorTask && (s_wearingGasmask || dmgFlags != lowAndHighFlag || s_playerDying))
		{
			task_free(s_gasSectorTask);
			s_gasSectorTask = nullptr;
		}

		// Handle damage walls.
		if (s_playerSlideWall && (s_playerSlideWall->flags1 & WF1_DAMAGE_WALL))
		{
			fixed16_16 dmg = mul16(PLAYER_DMG_WALL, s_deltaTime);
			player_applyDamage(dmg, 0, JTRUE);
		}

		// Double check the player Y position against the floor and ceiling.
		if (player->posWS.y > s_colCurLowestFloor)
		{
			s_playerYPos = s_colCurLowestFloor;
			player->posWS.y = s_colCurLowestFloor;
		}
		if (player->posWS.y < s_colCurHighestCeil)
		{
			s_playerYPos = s_colCurHighestCeil;
			player->posWS.y = s_colCurHighestCeil;
		}

		if (player->worldHeight - s_camOffset.y > player->posWS.y - s_colMinHeight)
		{
			player->worldHeight = player->posWS.y - s_colMinHeight + s_camOffset.y;
		}
		if (player->worldHeight - s_camOffset.y < player->posWS.y - s_colMaxHeight + 0x4000)
		{
			player->worldHeight = player->posWS.y - s_colMaxHeight + s_camOffset.y + 0x4000;
		}

		weapon->rollOffset = -div16(s_playerRoll, 13);
		weapon->pchOffset  = (s_playerPitch + (s_playerPitch < 0 ? (1<<6) : 0)) >> 6;

		// Handle camera lighting and night vision.
		if (player->flags & 2)
		{
			// This code is incorrect but it doesn't actually matter...
			// roll is always 0.
			setCameraAngleOffset(0, s_playerRoll, 0);

			s32 headlamp = 0;
			if (s_headlampActive)
			{
				fixed16_16 energy = min(ONE_16, s_energy);
				headlamp = floor16(mul16(energy, FIXED(64)));
				headlamp = 2*MAX_LIGHT_LEVEL - min(MAX_LIGHT_LEVEL, headlamp);
			}
			s32 atten = max(headlamp, s_weaponLight + s_levelAtten);
			s_baseAtten = atten;
			if (s_nightvisionActive)
			{
				atten = 0;
			}
			setPlayerLight(atten);
		}

		fixed16_16 distFromFloor = s_colCurLowestFloor - player->posWS.y;
		if (distFromFloor <= s_prevDistFromFloor)
		{
			distFromFloor = s_prevDistFromFloor;
		}
		s_prevDistFromFloor = max(distFromFloor, s_prevDistFromFloor);
	}

	void player_applyDamage(fixed16_16 healthDmg, fixed16_16 shieldDmg, JBool playHitSound)
	{
		fixed16_16 shields = intToFixed16(s_playerInfo.shields);
		fixed16_16 health  = intToFixed16(s_playerInfo.health);
		health += s_playerInfo.healthFract;

		s32 applyDmg = s_invincibility ? 0 : 1;
		if (applyDmg && health && shieldDmg >= 0)
		{
			if (shieldDmg && s_curTick > s_nextShieldDmgTick && !s_config.superShield)
			{
				// The amount of shield energy left after taking *half* damage.
				fixed16_16 halfShieldDmg = TFE_Jedi::abs(shieldDmg) >> 1;
				shields = max(0, shields - halfShieldDmg);
				if (shields < FIXED(50))
				{
					// healthDmg += shieldDmg * (1 - shields/50.0)
					fixed16_16 fracDmgToHealth = ONE_16 - div16(shields, FIXED(50));
					healthDmg += mul16(fracDmgToHealth, shieldDmg);
				}
				// Now take the other half away.
				shields = max(0, shields - halfShieldDmg);
				s_playerInfo.shields = pickup_addToValue(floor16(shields), 0, 200);
				if (playHitSound)
				{
					playSound2D(s_playerShieldHitSoundSource);
				}

				s_shieldDamageFx += (shieldDmg << 2);
				if (s_shieldDamageFx > FIXED(17))
				{
					s_shieldDamageFx = FIXED(17);
				}
			}
		}
		applyDmg = (s_invincibility == -2) ? 0 : 1;
		if (applyDmg && healthDmg)
		{
			health -= healthDmg;
			if (health < ONE_16)
			{
				s_playerInfo.healthFract = 0;
				// We could just set the health to 0 here...
				s_playerInfo.health = pickup_addToValue(0, 0, 100);
				if (playHitSound)
				{
					playSound2D(s_playerDeathSoundSource);
				}
				if (s_gasSectorTask)
				{
					task_free(s_gasSectorTask);
				}
				health = 0;
				s_gasSectorTask = nullptr;
				s_playerDying = 0xffffffff;
				s_reviveTick = s_curTick + 436;
			}
			else
			{
				if (playHitSound && s_curTick > s_nextPainSndTick)
				{
					playSound2D(s_playerHealthHitSoundSource);
					s_nextPainSndTick = s_curTick + 0x48;
				}
				health = max(0, health);
				s32 healthInt = floor16(health);
				s32 healthFrac = fract16(health);
				s_playerInfo.health = healthInt;
				s_playerInfo.healthFract = healthFrac;
			}
			s_healthDamageFx += TFE_Jedi::abs(healthDmg) >> 1;
			s_healthDamageFx = max(ONE_16, min(FIXED(17), s_healthDamageFx));
		}
	}

	static s32 s_prevCollisionFrameWall;

	void handlePlayerUseAction()
	{
		SecObject* player = s_playerObject;
		RSector* sector = player->sector;

		// Trigger the sector the player is standing in.
		message_sendToSector(sector, s_playerObject, INF_EVENT_NUDGE_FRONT, MSG_TRIGGER);

		// Then cast a ray and see if the wall in front of the player is close enough to trigger.
		fixed16_16 x0 = player->posWS.x;
		fixed16_16 z0 = player->posWS.z;
		fixed16_16 x1 = x0 + mul16(FIXED(4), s_playerLogic.dir.x);
		fixed16_16 z1 = z0 + mul16(FIXED(4), s_playerLogic.dir.z);
		RWall* wall = collision_wallCollisionFromPath(sector, x0, z0, x1, z1);
		if (wall)
		{
			RSector* nextSector = wall->nextSector;
			fixed16_16 eyeHeight = player->posWS.y - player->worldHeight;
			if (nextSector)
			{
				s_prevCollisionFrameWall = s_collisionFrameWall;
				fixed16_16 x, z;
				collision_getHitPoint(&x, &z);

				// Nudge the wall.
				vec2_fixed* w0 = wall->w0;
				fixed16_16 distFromW0 = vec2Length(w0->x - x, w0->z - z);
				inf_wallAndMirrorSendMessageAtPos(wall, s_playerObject, INF_EVENT_NUDGE_FRONT, distFromW0, eyeHeight);
				// Nudge the next sector from the outside.
				message_sendToSector(nextSector, s_playerObject, INF_EVENT_NUDGE_BACK, MSG_TRIGGER);
				// Set the sector collision frame to avoid infinite loops.
				nextSector->collisionFrame = s_collisionFrameWall;

				// The eye ray can go through the opening.
				if (eyeHeight > nextSector->ceilingHeight && eyeHeight < nextSector->floorHeight)
				{
					// Loop as the ray goes from sector to sector.
					do
					{
						wall = collision_pathWallCollision(nextSector);
						if (wall)
						{
							// Trigger the next wall.
							collision_getHitPoint(&x, &z);
							distFromW0 = vec2Length(w0->x - x, w0->z - z);
							inf_wallAndMirrorSendMessageAtPos(wall, s_playerObject, INF_EVENT_NUDGE_FRONT, distFromW0, eyeHeight);

							nextSector = wall->nextSector;
							if (nextSector)
							{
								if (s_collisionFrameWall != nextSector->collisionFrame)
								{
									// Nudge the next sector from the outside.
									message_sendToSector(nextSector, s_playerObject, INF_EVENT_NUDGE_BACK, MSG_TRIGGER);
									nextSector->collisionFrame = s_collisionFrameWall;
								}
								// Make sure the eye ray fits in the opening.
								if (eyeHeight < nextSector->ceilingHeight || eyeHeight > nextSector->floorHeight)
								{
									break;
								}
							}
						}
					} while (wall && nextSector);
				}
				if (s_collisionFrameWall != s_prevCollisionFrameWall)
				{
					TFE_System::logWrite(LOG_ERROR, "Player Use", "MARK HAS UNEXPECTEDLY CHANGED.");
				}
			}
			else
			{
				fixed16_16 x, z;
				collision_getHitPoint(&x, &z);

				vec2_fixed* w0 = wall->w0;
				s32 distFromW0 = vec2Length(w0->x - x, w0->z - z);
				inf_wallAndMirrorSendMessageAtPos(wall, s_playerObject, INF_EVENT_NUDGE_FRONT, distFromW0, eyeHeight);
			}
		}
	}
				
	void handlePlayerActions()
	{
		if (s_energy)
		{
			if (s_headlampActive)
			{
				fixed16_16 energyDelta = mul16(HEADLAMP_ENERGY_CONSUMPTION, s_deltaTime);
				s_energy -= energyDelta;
			}
			if (s_wearingGasmask)
			{
				fixed16_16 energyDelta = mul16(GASMASK_ENERGY_CONSUMPTION, s_deltaTime);
				s_energy -= energyDelta;
			}
			if (s_nightvisionActive)
			{
				fixed16_16 energyDelta = mul16(GOGGLES_ENERGY_CONSUMPTION, s_deltaTime);
				s_energy -= energyDelta;
			}
			if (s_energy <= 0)
			{
				if (s_nightvisionActive)
				{
					s_nightvisionActive = JFALSE;
					disableNightvisionInternal();
					hud_sendTextMessage(9);
				}
				if (s_headlampActive)
				{
					hud_sendTextMessage(12);
					s_headlampActive = JFALSE;
				}
				if (s_wearingGasmask)
				{
					hud_sendTextMessage(19);
					s_wearingGasmask = JFALSE;
					if (s_gasmaskTask)
					{
						task_free(s_gasmaskTask);
						s_gasmaskTask = nullptr;
					}
				}
				s_energy = 0;
			}
		}
		if (s_playerUse)
		{
			s_playerUse = JFALSE;
			if (!s_playerActionUse)
			{
				handlePlayerUseAction();
				s_playerActionUse = JTRUE;
			}
		}
		else
		{
			s_playerActionUse = JFALSE;
		}

		if (s_playerPrimaryFire)
		{
			s_playerPrimaryFire = JFALSE;
			if (!s_weaponFiring && s_playerWeaponTask)
			{
				s_weaponFiring = JTRUE;
				// This causes the weapon to fire.
				s_msgArg1 = WFIRETYPE_PRIMARY;
				task_runAndReturn(s_playerWeaponTask, MSG_START_FIRING);
			}
		}
		// This happens when the player *stops* firing.
		else if (s_weaponFiring && s_playerWeaponTask)
		{
			s_weaponFiring = JFALSE;
			if (!s_weaponFiringSec)
			{
				// Spin down.
				task_runAndReturn(s_playerWeaponTask, MSG_STOP_FIRING);
			}
			else
			{
				// Start secondary fire.
				s_msgArg1 = WFIRETYPE_SECONDARY;
				task_runAndReturn(s_playerWeaponTask, MSG_START_FIRING);
			}
		}

		if (s_playerSecFire)
		{
			s_playerSecFire = JFALSE;
			if (!s_weaponFiringSec && s_playerWeaponTask)
			{
				s_weaponFiringSec = JTRUE;
				s_msgArg1 = WFIRETYPE_SECONDARY;
				task_runAndReturn(s_playerWeaponTask, MSG_START_FIRING);
			}
		}
		// This happens when the player *stops* firing.
		else if (s_weaponFiringSec && s_playerWeaponTask) 
		{
			s_weaponFiringSec = JFALSE;
			if (!s_weaponFiring)
			{
				// Spin down
				task_runAndReturn(s_playerWeaponTask, MSG_STOP_FIRING);
			}
			else
			{
				// Start primary fire.
				s_msgArg1 = WFIRETYPE_PRIMARY;
				task_runAndReturn(s_playerWeaponTask, MSG_START_FIRING);
			}
		}

		if (!s_playerDying)
		{
			// Weapon select.
			if (inputMapping_getActionState(IADF_WEAPON_1) == STATE_PRESSED)
			{
				s_playerInfo.selectedWeapon = WPN_FIST;
			}
			if (inputMapping_getActionState(IADF_WEAPON_2) == STATE_PRESSED && s_playerInfo.itemPistol)
			{
				s_playerInfo.selectedWeapon = WPN_PISTOL;
			}
			if (inputMapping_getActionState(IADF_WEAPON_3) == STATE_PRESSED && s_playerInfo.itemRifle)
			{
				s_playerInfo.selectedWeapon = WPN_RIFLE;
			}
			if (inputMapping_getActionState(IADF_WEAPON_4) == STATE_PRESSED)
			{
				s_playerInfo.selectedWeapon = WPN_THERMAL_DET;
			}
			if (inputMapping_getActionState(IADF_WEAPON_5) == STATE_PRESSED && s_playerInfo.itemAutogun)
			{
				s_playerInfo.selectedWeapon = WPN_REPEATER;
			}
			if (inputMapping_getActionState(IADF_WEAPON_6) == STATE_PRESSED && s_playerInfo.itemFusion)
			{
				s_playerInfo.selectedWeapon = WPN_FUSION;
			}
			if (inputMapping_getActionState(IADF_WEAPON_7) == STATE_PRESSED)
			{
				s_playerInfo.selectedWeapon = WPN_MINE;
			}
			if (inputMapping_getActionState(IADF_WEAPON_8) == STATE_PRESSED && s_playerInfo.itemMortar)
			{
				s_playerInfo.selectedWeapon = WPN_MORTAR;
			}
			if (inputMapping_getActionState(IADF_WEAPON_9) == STATE_PRESSED && s_playerInfo.itemConcussion)
			{
				s_playerInfo.selectedWeapon = WPN_CONCUSSION;
			}
			if (inputMapping_getActionState(IADF_WEAPON_10) == STATE_PRESSED && s_playerInfo.itemCannon)
			{
				s_playerInfo.selectedWeapon = WPN_CANNON;
			}

			s32 selectedWpn = s_playerInfo.selectedWeapon;
			if (selectedWpn != -1)
			{
				s32 curWeapon = s_playerInfo.curWeapon;
				if (selectedWpn != curWeapon)
				{
					if (s_playerWeaponTask)
					{
						// Change weapon
						s_msgArg1 = selectedWpn;
						task_runAndReturn(s_playerWeaponTask, MSG_SWITCH_WPN);

						if (s_playerInfo.curWeapon > s_playerInfo.maxWeapon)
						{
							s_playerInfo.maxWeapon = s_playerInfo.curWeapon;
						}
					}
					else
					{
						s_playerInfo.index2 = selectedWpn;
					}
				}
				s_playerInfo.selectedWeapon = -1;
			}
		}
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
			shieldFx = c_shieldDmgToFx[effectIndex];
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

	void gasSectorTaskFunc(MessageType msg)
	{
		task_begin;
		while (1)
		{
			playSound2D(s_gasDamageSoundSource);
			task_yield(291);	// wait for 2 seconds.
		}
		task_end;
	}
		
	JBool player_hasWeapon(s32 weaponIndex)
	{
		JBool retval = JFALSE;
		switch (weaponIndex)
		{
		case 2:
			retval = s_playerInfo.itemPistol;
			break;
		case 3:
			retval = s_playerInfo.itemRifle;
			break;
		case 4:
			retval = s_playerInfo.ammoDetonator > 0 ? JTRUE : JFALSE;
			break;
		case 5:
			retval = s_playerInfo.itemAutogun;
			break;
		case 6:
			retval = s_playerInfo.itemFusion;
			break;
		case 7:
			retval = s_playerInfo.ammoMine > 0 ? JTRUE : JFALSE;
			break;
		case 8:
			retval = s_playerInfo.itemMortar;
			break;
		case 9:
			retval = s_playerInfo.itemConcussion;
			break;
		case 10:
			retval = s_playerInfo.itemCannon;
			break;
		}
		return retval;
	}

	JBool player_hasItem(s32 itemIndex)
	{
		JBool retval = JFALSE;
		switch (itemIndex)
		{
		case 0:
			retval = s_playerInfo.itemRedKey;
			break;
		case 1:
			retval = s_playerInfo.itemYellowKey;
			break;
		case 2:
			retval = s_playerInfo.itemBlueKey;
			break;
		case 3:
			retval = s_playerInfo.itemGoggles;
			break;
		case 4:
			retval = s_playerInfo.itemCleats;
			break;
		case 5:
			retval = s_playerInfo.itemMask;
			break;
		case 6:
			retval = s_playerInfo.itemPlans;
			break;
		case 7:
			retval = s_playerInfo.itemPhrik;
			break;
		case 8:
			retval = s_playerInfo.itemNava;
			break;
		case 9:
			retval = s_playerInfo.itemDatatape;
			break;
		case 10:
			retval = s_playerInfo.itemUnused;
			break;
		case 11:
			retval = s_playerInfo.itemDtWeapon;
			break;
		case 12:
			retval = s_playerInfo.itemCode1;
			break;
		case 13:
			retval = s_playerInfo.itemCode2;
			break;
		case 14:
			retval = s_playerInfo.itemCode3;
			break;
		case 15:
			retval = s_playerInfo.itemCode4;
			break;
		case 16:
			retval = s_playerInfo.itemCode5;
			break;
		case 17:
			retval = s_playerInfo.itemCode6;
			break;
		case 18:
			retval = s_playerInfo.itemCode7;
			break;
		case 19:
			retval = s_playerInfo.itemCode8;
			break;
		case 20:
			retval = s_playerInfo.itemCode9;
			break;
		}
		return retval;
	}
}  // TFE_DarkForces