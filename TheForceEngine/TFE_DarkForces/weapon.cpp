#include "weapon.h"
#include "player.h"
#include "pickup.h"
#include <TFE_System/system.h>
#include <TFE_Jedi/Level/rtexture.h>

namespace TFE_DarkForces
{
	///////////////////////////////////////////
	// Structure Definitions
	///////////////////////////////////////////
	struct PlayerWeapon
	{
		s32 frameCount;
		TextureData* frames[8];
		s32 frame;
		s32 xPos[8];
		s32 yPos[8];
		u32 flags;
		s32 rollOffset;
		s32 pchOffset;
		s32 xWaveOffset;
		s32 yWaveOffset;
		s32 xOffset;
		s32 yOffset;
		s32* ammo;
		s32* secondaryAmmo;
		s32 u8c;
		s32 u90;
		fixed16_16 wakeupRange;
		s32 variation;
	};

	///////////////////////////////////////////
	// Internal State
	///////////////////////////////////////////
	static JBool s_weaponTexturesLoaded = JFALSE;
	static JBool s_isShooting = JFALSE;
	static JBool s_secondaryFire = JFALSE;
	static JBool s_weaponAutoMount2 = JFALSE;

	static TextureData* s_rhand1 = nullptr;
	static TextureData* s_gasmaskTexture = nullptr;
	static PlayerWeapon s_playerWeaponList[WPN_COUNT];
	static PlayerWeapon* s_curPlayerWeapon = nullptr;

	static s32 s_prevWeapon;
	static s32 s_curWeapon;
	static s32 s_nextWeapon;
	static s32 s_lastWeapon;

	static SoundSourceID s_punchSwingSndSrc;
	static SoundSourceID s_pistolSndSrc;
	static SoundSourceID s_pistolEmptySndSrc;
	static SoundSourceID s_rifleSndSrc;
	static SoundSourceID s_rifleEmptySndSrc;
	static SoundSourceID s_fusion1SndSrc;
	static SoundSourceID s_fusion2SndSrc;
	static SoundSourceID s_repeaterSndSrc;
	static SoundSourceID s_repeater1SndSrc;
	static SoundSourceID s_repeaterEmptySndSrc;
	static SoundSourceID s_mortarFireSndSrc;
	static SoundSourceID s_mortarFireSndSrc2;
	static SoundSourceID s_mortarEmptySndSrc;
	static SoundSourceID s_mineSndSrc;
	static SoundSourceID s_concussion6SndSrc;
	static SoundSourceID s_concussion5SndSrc;
	static SoundSourceID s_concussion1SndSrc;
	static SoundSourceID s_plasma4SndSrc;
	static SoundSourceID s_plasmaEmptySndSrc;
	static SoundSourceID s_missile1SndSrc;
	static SoundSourceID s_weaponChangeSnd;

	///////////////////////////////////////////
	// Shared State
	///////////////////////////////////////////
	SoundSourceID s_superchargeCountdownSound;

	///////////////////////////////////////////
	// Forward Declarations
	///////////////////////////////////////////
	TextureData* loadWeaponTexture(const char* texName);
	void weapon_loadTextures();

	///////////////////////////////////////////
	// API Implementation
	///////////////////////////////////////////
	void weapon_startup()
	{
		// TODO: Move this into data instead of hard coding it like vanilla Dark Forces.
		s_playerWeaponList[WPN_FIST] =
		{
			4,						// frameCount
			{nullptr},				// frames[]
			0,						// frame
			{172, 55, 59, 56},		// xPos[]
			{141, 167, 114, 141},	// yPos[]
			7,						// flags
			0,						// rollOffset
			0,						// pchOffset
			0,						// xWaveOffset
			0,						// yWaveOffset
			0,						// xOffset
			0,						// yOffset
			nullptr,				// ammo
			nullptr,				// secondaryAmmo
			0,						// u8c
			0,						// u90
			0,						// wakeupRange
			0,						// variation
		};
		s_playerWeaponList[WPN_PISTOL] =
		{
			3,						// frameCount
			{nullptr},				// frames[]
			0,						// frame
			{0xa5, 0xa9, 0xa9},		// xPos[]
			{0x8e, 0x88, 0x88},	    // yPos[]
			7,						// flags
			0,						// rollOffset
			0,						// pchOffset
			0,						// xWaveOffset
			0,						// yWaveOffset
			0,						// xOffset
			0,						// yOffset
			&s_playerInfo.ammoEnergy,// ammo
			nullptr,				// secondaryAmmo
			0,						// u8c
			0,						// u90
			FIXED(45),				// wakeupRange
			22,						// variation
		};
		s_playerWeaponList[WPN_RIFLE] =
		{
			2,						// frameCount
			{nullptr},				// frames[]
			0,						// frame
			{0x71, 0x70},			// xPos[]
			{0x7f, 0x72},			// yPos[]
			7,						// flags
			0,						// rollOffset
			0,						// pchOffset
			0,						// xWaveOffset
			0,						// yWaveOffset
			0,						// xOffset
			0,						// yOffset
			&s_playerInfo.ammoEnergy,// ammo
			nullptr,				// secondaryAmmo
			0,						// u8c
			0,						// u90
			FIXED(50),				// wakeupRange
			0x56,					// variation
		};
		s_playerWeaponList[WPN_THERMAL_DET] =
		{
			4,						// frameCount
			{nullptr},				// frames[]
			0,						// frame
			{0xa1, 0xb3, 0xb4, 0x84},// xPos[]
			{0x83, 0xa5, 0x66, 0x96},// yPos[]
			7,						// flags
			0,						// rollOffset
			0,						// pchOffset
			0,						// xWaveOffset
			0,						// yWaveOffset
			0,						// xOffset
			0,						// yOffset
			&s_playerInfo.ammoDetonator,// ammo
			nullptr,				// secondaryAmmo
			0,						// u8c
			0,						// u90
			0,						// wakeupRange
			0x2d002d,				// variation
		};
		s_playerWeaponList[WPN_REPEATER] =
		{
			3,						// frameCount
			{nullptr},				// frames[]
			0,						// frame
			{0x9c, 0xa3, 0xa3},		// xPos[]
			{0x8a, 0x8c, 0x8c},		// yPos[]
			7,						// flags
			0,						// rollOffset
			0,						// pchOffset
			0,						// xWaveOffset
			0,						// yWaveOffset
			0,						// xOffset
			0,						// yOffset
			&s_playerInfo.ammoPower,// ammo
			nullptr,				// secondaryAmmo
			0,						// u8c
			0,						// u90
			FIXED(60),				// wakeupRange
			0x2d002d,				// variation
		};
		s_playerWeaponList[WPN_FUSION] =
		{
			6,						// frameCount
			{nullptr},				// frames[]
			0,						// frame
			{0x13, 0x17, 0x17, 0x17, 0x17, 0x17},// xPos[]
			{0x98, 0x9b, 0x9b, 0x9b, 0x9b, 0x9b},// yPos[]
			7,						// flags
			0,						// rollOffset
			0,						// pchOffset
			0,						// xWaveOffset
			0,						// yWaveOffset
			0,						// xOffset
			0,						// yOffset
			&s_playerInfo.ammoPower,// ammo
			nullptr,				// secondaryAmmo
			0,						// u8c
			0,						// u90
			FIXED(55),				// wakeupRange
			0x2d,					// variation
		};
		s_playerWeaponList[WPN_MORTAR] =
		{
			4,						// frameCount
			{nullptr},				// frames[]
			0,						// frame
			{0x7b, 0x7e, 0x7f, 0x7b},// xPos[]
			{0x77, 0x75, 0x77, 0x74},// yPos[]
			7,						// flags
			0,						// rollOffset
			0,						// pchOffset
			0,						// xWaveOffset
			0,						// yWaveOffset
			0,						// xOffset
			0,						// yOffset
			&s_playerInfo.ammoShell,// ammo
			nullptr,				// secondaryAmmo
			0,						// u8c
			0,						// u90
			FIXED(60),				// wakeupRange
			0x2d,					// variation
		};
		s_playerWeaponList[WPN_MINE] =
		{
			3,						// frameCount
			{nullptr},				// frames[]
			0,						// frame
			{0x69, 0x69, 0x84},		// xPos[]
			{0x99, 0x99, 0x96},		// yPos[]
			7,						// flags
			0,						// rollOffset
			0,						// pchOffset
			0,						// xWaveOffset
			0,						// yWaveOffset
			0,						// xOffset
			0,						// yOffset
			&s_playerInfo.ammoMine, // ammo
			nullptr,				// secondaryAmmo
			0,						// u8c
			0,						// u90
			0,						// wakeupRange
			0,						// variation
		};
		s_playerWeaponList[WPN_CONCUSSION] =
		{
			3,						// frameCount
			{nullptr},				// frames[]
			0,						// frame
			{0x82, 0xbf, 0xbc},		// xPos[]
			{0x83, 0x81, 0x84},		// yPos[]
			7,						// flags
			0,						// rollOffset
			0,						// pchOffset
			0,						// xWaveOffset
			0,						// yWaveOffset
			0,						// xOffset
			0,						// yOffset
			&s_playerInfo.ammoPower,// ammo
			nullptr,				// secondaryAmmo
			0,						// u8c
			0,						// u90
			FIXED(70),				// wakeupRange
			0,						// variation
		};
		s_playerWeaponList[WPN_CANNON] =
		{
			4,						// frameCount
			{nullptr},				// frames[]
			0,						// frame
			{206, 208, 224, 230},	// xPos[]
			{-74, -60, 81, 86},     // yPos[]
			7,						// flags
			0,						// rollOffset
			0,						// pchOffset
			0,						// xWaveOffset
			0,						// yWaveOffset
			0,						// xOffset
			0,						// yOffset
			&s_playerInfo.ammoPlasma,// ammo
			&s_playerInfo.ammoMissile,// secondaryAmmo
			0,						// u8c
			0,						// u90
			FIXED(55),				// wakeupRange
			0x2d002d,				// variation
		};

		// Load Textures.
		weapon_loadTextures();

		// Load Sounds.
		s_punchSwingSndSrc    = sound_Load("swing.voc");
		s_pistolSndSrc        = sound_Load("pistol-1.voc");
		s_pistolEmptySndSrc   = sound_Load("pistout1.voc");
		s_rifleSndSrc         = sound_Load("rifle-1.voc");
		s_rifleEmptySndSrc    = sound_Load("riflout.voc");
		s_fusion1SndSrc       = sound_Load("fusion1.voc");
		s_fusion2SndSrc       = sound_Load("fusion2.voc");
		s_repeaterSndSrc      = sound_Load("repeater.voc");
		s_repeater1SndSrc     = sound_Load("repeat-1.voc");
		s_repeaterEmptySndSrc = sound_Load("rep-emp.voc");
		s_mortarFireSndSrc    = sound_Load("mortar4.voc");
		s_mortarFireSndSrc2   = sound_Load("mortar2.voc");
		s_mortarEmptySndSrc   = sound_Load("mortar9.voc");
		s_mineSndSrc          = sound_Load("claymor1.voc");
		s_concussion6SndSrc   = sound_Load("concuss6.voc");
		s_concussion5SndSrc   = sound_Load("concuss5.voc");
		s_concussion1SndSrc   = sound_Load("concuss1.voc");
		s_plasma4SndSrc       = sound_Load("plasma4.voc");
		s_plasmaEmptySndSrc   = sound_Load("plas-emp.voc");
		s_missile1SndSrc      = sound_Load("missile1.voc");
		s_weaponChangeSnd     = sound_Load("weapon1.voc");
		s_superchargeCountdownSound = sound_Load("quarter.voc");

		s_isShooting       = JFALSE;
		s_secondaryFire    = JFALSE;
		s_superCharge      = JFALSE;
		s_superChargeHud   = JFALSE;
		s_superchargeTask  = nullptr;
		s_weaponAutoMount2 = JFALSE;
		s_prevWeapon = WPN_PISTOL;
		s_curWeapon  = WPN_PISTOL;
		s_lastWeapon = WPN_PISTOL;
		s_curPlayerWeapon = &s_playerWeaponList[WPN_PISTOL];
	}

	///////////////////////////////////////////
	// Internal Implementation
	///////////////////////////////////////////
	void weapon_loadTextures()
	{
		if (!s_weaponTexturesLoaded)
		{
			s_rhand1 = loadWeaponTexture("rhand1.bm");
			s_gasmaskTexture = loadWeaponTexture("gmask.bm");

			s_playerWeaponList[WPN_FIST].frames[0] = s_rhand1;
			s_playerWeaponList[WPN_FIST].frames[1] = loadWeaponTexture("punch1.bm");
			s_playerWeaponList[WPN_FIST].frames[2] = loadWeaponTexture("punch2.bm");
			s_playerWeaponList[WPN_FIST].frames[3] = loadWeaponTexture("punch3.bm");

			s_playerWeaponList[WPN_PISTOL].frames[0] = loadWeaponTexture("pistol1.bm");
			s_playerWeaponList[WPN_PISTOL].frames[1] = loadWeaponTexture("pistol2.bm");
			s_playerWeaponList[WPN_PISTOL].frames[2] = loadWeaponTexture("pistol3.bm");

			s_playerWeaponList[WPN_RIFLE].frames[0] = loadWeaponTexture("rifle1.bm");
			s_playerWeaponList[WPN_RIFLE].frames[1] = loadWeaponTexture("rifle2.bm");

			s_playerWeaponList[WPN_THERMAL_DET].frames[0] = loadWeaponTexture("therm1.bm");
			s_playerWeaponList[WPN_THERMAL_DET].frames[1] = loadWeaponTexture("therm2.bm");
			s_playerWeaponList[WPN_THERMAL_DET].frames[2] = loadWeaponTexture("therm3.bm");
			s_playerWeaponList[WPN_THERMAL_DET].frames[3] = s_rhand1;

			s_playerWeaponList[WPN_REPEATER].frames[0] = loadWeaponTexture("autogun1.bm");
			s_playerWeaponList[WPN_REPEATER].frames[1] = loadWeaponTexture("autogun2.bm");
			s_playerWeaponList[WPN_REPEATER].frames[2] = loadWeaponTexture("autogun3.bm");

			s_playerWeaponList[WPN_FUSION].frames[0] = loadWeaponTexture("fusion1.bm");
			s_playerWeaponList[WPN_FUSION].frames[1] = loadWeaponTexture("fusion2.bm");
			s_playerWeaponList[WPN_FUSION].frames[2] = loadWeaponTexture("fusion3.bm");
			s_playerWeaponList[WPN_FUSION].frames[3] = loadWeaponTexture("fusion4.bm");
			s_playerWeaponList[WPN_FUSION].frames[4] = loadWeaponTexture("fusion5.bm");
			s_playerWeaponList[WPN_FUSION].frames[5] = loadWeaponTexture("fusion6.bm");

			s_playerWeaponList[WPN_MORTAR].frames[0] = loadWeaponTexture("mortar1.bm");
			s_playerWeaponList[WPN_MORTAR].frames[1] = loadWeaponTexture("mortar2.bm");
			s_playerWeaponList[WPN_MORTAR].frames[2] = loadWeaponTexture("mortar3.bm");
			s_playerWeaponList[WPN_MORTAR].frames[3] = loadWeaponTexture("mortar4.bm");

			s_playerWeaponList[WPN_MINE].frames[0] = loadWeaponTexture("clay1.bm");
			s_playerWeaponList[WPN_MINE].frames[1] = loadWeaponTexture("clay2.bm");
			s_playerWeaponList[WPN_MINE].frames[2] = s_rhand1;

			s_playerWeaponList[WPN_CONCUSSION].frames[0] = loadWeaponTexture("concuss1.bm");
			s_playerWeaponList[WPN_CONCUSSION].frames[1] = loadWeaponTexture("concuss2.bm");
			s_playerWeaponList[WPN_CONCUSSION].frames[2] = loadWeaponTexture("concuss3.bm");

			s_playerWeaponList[WPN_CANNON].frames[0] = loadWeaponTexture("assault1.bm");
			s_playerWeaponList[WPN_CANNON].frames[1] = loadWeaponTexture("assault2.bm");
			s_playerWeaponList[WPN_CANNON].frames[2] = loadWeaponTexture("assault3.bm");
			s_playerWeaponList[WPN_CANNON].frames[3] = loadWeaponTexture("assault4.bm");

			s_weaponTexturesLoaded = JTRUE;
		}
	}

	TextureData* loadWeaponTexture(const char* texName)
	{
		FilePath filePath;
		if (TFE_Paths::getFilePath(texName, &filePath))
		{
			return bitmap_load(&filePath, 0);
		}
		else
		{
			TFE_System::logWrite(LOG_ERROR, "Weapon", "Weapon_Startup: %s NOT FOUND.", texName);
		}
		return nullptr;
	}
}  // namespace TFE_DarkForces