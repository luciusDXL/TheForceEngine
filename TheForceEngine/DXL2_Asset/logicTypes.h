#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Logic Types
//////////////////////////////////////////////////////////////////////
#include <DXL2_System/types.h>

enum LogicType
{
	// Player
	LOGIC_PLAYER,		//  Player controlled object
	// General
	LOGIC_SHIELD,		//	20 shield units	114
	LOGIC_BATTERY,		//	battery unit	211
	LOGIC_CLEATS,		//	ice cleats	304
	LOGIC_GOGGLES,		//	infra red goggles	303
	LOGIC_MASK,			//	gas mask	305
	LOGIC_MEDKIT,		//	med kit	311
	// Weapons -
	LOGIC_RIFLE,		//	Blaster rifle / 15 energy units	100 / 101
	LOGIC_AUTOGUN,		//	Repeater Rifle / 30 power units	103 / 104
	LOGIC_FUSION,		//	Jeron fusion cutter / 50 power units	107 / 108
	LOGIC_MORTAR,		//	Mortar Gun / 3 mortar shells	105 / 106
	LOGIC_CONCUSSION,	//	Concussion Rifle / 100 power units	110 / 111
	LOGIC_CANNON,		//	Assault cannon / 30 plasma units	112 / 113
	// Ammo -
	LOGIC_ENERGY,		//	15 energy units	200
	LOGIC_DETONATOR,	//	1 thermal detonator	203
	LOGIC_DETONATORS,	//	5 thermal detonators	204
	LOGIC_POWER,		//	10 power units	201
	LOGIC_MINE,			//	1 mine	207
	LOGIC_MINES,		//5 mines	208
	LOGIC_SHELL,		//	1 mortar shell	205
	LOGIC_SHELLS,		//	5 mortar shells	206
	LOGIC_PLASMA,		//	20 Plasma units	202
	LOGIC_MISSILE,		//	1 missile	209
	LOGIC_MISSILES,		//	5 missiles	210
	// Bonuses -
	LOGIC_SUPERCHARGE,	//	weapon supercharge	307
	LOGIC_INVINCIBLE,	//	shield supercharge	306
	LOGIC_LIFE,			//	extra life	310
	LOGIC_REVIVE,		//	revive	308
	// Keys -
	LOGIC_BLUE,			//	blue key	302
	LOGIC_RED,			//	red key	300
	LOGIC_YELLOW,		//	yellow key	301
	LOGIC_CODE1,		//	code key 1	501
	LOGIC_CODE2,		//	code key 2	502
	LOGIC_CODE3,		//	code key 3	503
	LOGIC_CODE4,		//	code key 4	504
	LOGIC_CODE5,		//  code key 5	505
	LOGIC_CODE6,		//	code key 6	506
	LOGIC_CODE7,		//	code key 7	507
	LOGIC_CODE8,		//	code key 8	508
	LOGIC_CODE9,		//	code key 9	509
	// Goal items -
	LOGIC_DATATAPE,		//	data tapes	406
	LOGIC_PLANS,		//	Death Star plans	400
	LOGIC_DT_WEAPON,	//	broken DT weapon	405
	LOGIC_NAVA,			//	Nava Card	402
	LOGIC_PHRIK,		//	Phrik metal	401
	LOGIC_PILE,			//	Your Gear	312
	// Enemy logics
	LOGIC_GENERATOR,	// Generate enemies.
	// Imperials -
	LOGIC_I_OFFICER,	//	Imperial officer
	LOGIC_I_OFFICERR,	//	Officer with red key
	LOGIC_I_OFFICERB,	//	Officer with blue key
	LOGIC_I_OFFICERY,	//	Officer with yellow key
	LOGIC_I_OFFICER1,	//	Officer with code key 1
	LOGIC_I_OFFICER2,	//	Officer with code key 2
	LOGIC_I_OFFICER3,	//	Officer with code key 3
	LOGIC_I_OFFICER4,	//	Officer with code key 4
	LOGIC_I_OFFICER5,	//	Officer with code key 5
	LOGIC_I_OFFICER6,	//	Officer with code key 6
	LOGIC_I_OFFICER7,	//	Officer with code key 7
	LOGIC_I_OFFICER8,	//	Officer with code key 8
	LOGIC_I_OFFICER9,	//	Officer with code key 9
	LOGIC_TROOP,		//	Stormtrooper
	LOGIC_STORM1,		//	Stormtrooper
	LOGIC_COMMANDO,		//	Imperial Commando
	// Aliens -
	LOGIC_BOSSK,		//	Bossk
	LOGIC_G_GUARD,		//	Gammorean Guard
	LOGIC_REE_YEES,		//	ReeYees with thermal detonators
	LOGIC_REE_YEES2,	//	ReeYees w / o thermal detonators
	LOGIC_SEWER1,		//	Sewer creature
	// Robots -
	LOGIC_INT_DROID,	//	Interrogator droid
	LOGIC_PROBE_DROID,	//	Probe droid
	LOGIC_REMOTE,		//	Remote
	// Bosses -
	LOGIC_BOBA_FETT,	//	Boba Fett
	LOGIC_KELL,			//	Kell Dragon
	LOGIC_D_TROOP1,		//	Phase 1 Dark Trooper
	LOGIC_D_TROOP2,		//	Phase 2 Dark Trooper
	LOGIC_D_TROOP3,		//	Phase 3 Dark Trooper(Mohc)
	// Special sprite logics
	LOGIC_SCENERY,		//	Displays first cell of wax 0, then all of wax 1 when attacked
	LOGIC_ANIM,			//	Displays wax 0 over and over
	LOGIC_BARREL,		//	Power Generating unit
	LOGIC_LAND_MINE,	//	Land mine
	// 3D object logics
	LOGIC_TURRET,		//	gun turret
	LOGIC_MOUSEBOT,		//	mousebot
	LOGIC_WELDER,		//	welding arm
	// 3D object motion logics
	LOGIC_UPDATE,		// to perpetually rotate a 3D, and
	LOGIC_KEY,			// to give a VUE motion to the 3D
	// Final Count
	LOGIC_COUNT,
	LOGIC_INVALID = LOGIC_COUNT
};

static const u32 c_logicMsg[LOGIC_COUNT]=
{
	// Player
	0,	// LOGIC_PLAYER	
	// General
	114, // LOGIC_SHIELD	
	211, // LOGIC_BATTERY	
	304, // LOGIC_CLEATS	
	303, // LOGIC_GOGGLES
	305, // LOGIC_MASK
	311, // LOGIC_MEDKIT	
	// Weapons -
	100, // LOGIC_RIFLE
	103, // LOGIC_AUTOGUN
	107, // LOGIC_FUSION	
	105, // LOGIC_MORTAR
	110, // LOGIC_CONCUSSION
	112, // LOGIC_CANNON
	// Ammo -
	200, // LOGIC_ENERGY
	203, // LOGIC_DETONATOR
	204, // LOGIC_DETONATORS
	201, // LOGIC_POWER
	207, // LOGIC_MINE
	208, // LOGIC_MINES
	205, // LOGIC_SHELL
	206, // LOGIC_SHELLS
	202, // LOGIC_PLASMA
	209, // LOGIC_MISSILE
	210, // LOGIC_MISSILES
	// Bonuses -
	307, // LOGIC_SUPERCHARGE
	306, // LOGIC_INVINCIBLE
	310, // LOGIC_LIFE
	308, // LOGIC_REVIVE
	// Keys -
	302, // LOGIC_BLUE
	300, // LOGIC_RED
	301, // LOGIC_YELLOW
	501, // LOGIC_CODE1	
	502, // LOGIC_CODE2	
	503, // LOGIC_CODE3	
	504, // LOGIC_CODE4	
	505, // LOGIC_CODE5	
	506, // LOGIC_CODE6	
	507, // LOGIC_CODE7	
	508, // LOGIC_CODE8	
	509, // LOGIC_CODE9	
	// Goal items -
	406, // LOGIC_DATATAPE
	400, // LOGIC_PLANS
	405, // LOGIC_DT_WEAPON
	402, // LOGIC_NAVA
	401, // LOGIC_PHRIK
	312, // LOGIC_PILE
	// Enemy logics
	0,   // LOGIC_GENERATOR
	// Imperials -
	0,	// LOGIC_I_OFFICER
	0,	// LOGIC_I_OFFICERR
	0,	// LOGIC_I_OFFICERB
	0,	// LOGIC_I_OFFICERY
	0,	// LOGIC_I_OFFICER1
	0,	// LOGIC_I_OFFICER2
	0,	// LOGIC_I_OFFICER3
	0,	// LOGIC_I_OFFICER4
	0,	// LOGIC_I_OFFICER5
	0,	// LOGIC_I_OFFICER6
	0,	// LOGIC_I_OFFICER7
	0,	// LOGIC_I_OFFICER8
	0,	// LOGIC_I_OFFICER9
	0,	// LOGIC_TROOP
	0,	// LOGIC_STORM1
	0,	// LOGIC_COMMANDO
	// Aliens -
	0,	// LOGIC_BOSSK
	0,	// LOGIC_G_GUARD
	0,	// LOGIC_REE_YEES
	0,	// LOGIC_REE_YEES2
	0,	// LOGIC_SEWER1
	// Robots -
	0,	// LOGIC_INT_DROID
	0,	// LOGIC_PROBE_DROID
	0,	// LOGIC_REMOTE
	// Bosses -
	0,	// LOGIC_BOBA_FETT
	0,	// LOGIC_KELL
	0,	// LOGIC_D_TROOP1
	0,	// LOGIC_D_TROOP2
	0,	// LOGIC_D_TROOP3
	// Special sprite logics
	0,	// LOGIC_SCENERY
	0,	// LOGIC_ANIM
	0,	// LOGIC_BARREL
	0,	// LOGIC_LAND_MINE
	// 3D object logics
	0,	// LOGIC_TURRET
	0,	// LOGIC_MOUSEBOT
	0,	// LOGIC_WELDER
	// 3D object motion logics
	0,	// LOGIC_UPDATE
	0,	// LOGIC_KEY
};

static const char* c_logicName[LOGIC_COUNT] =
{
	// Player
	"PLAYER",		// LOGIC_PLAYER
	// General
	"SHIELD",		// LOGIC_SHIELD
	"BATTERY",		// LOGIC_BATTERY
	"CLEATS",		// LOGIC_CLEATS
	"GOGGLES",		// LOGIC_GOGGLES
	"MASK",			// LOGIC_MASK
	"MEDKIT",		// LOGIC_MEDKIT
	// Weapons -
	"RIFLE",		// LOGIC_RIFLE
	"AUTOGUN",		// LOGIC_AUTOGUN
	"FUSION",		// LOGIC_FUSION
	"MORTAR",		// LOGIC_MORTAR
	"CONCUSSION",	// LOGIC_CONCUSSION
	"CANNON",		// LOGIC_CANNON
	// Ammo -
	"ENERGY",		// LOGIC_ENERGY
	"DETONATOR",	// LOGIC_DETONATOR
	"DETONATORS",	// LOGIC_DETONATORS
	"POWER",		// LOGIC_POWER
	"MINE",			// LOGIC_MINE
	"MINES",		// LOGIC_MINES
	"SHELL",		// LOGIC_SHELL
	"SHELLS",		// LOGIC_SHELLS
	"PLASMA",		// LOGIC_PLASMA
	"MISSILE",		// LOGIC_MISSILE
	"MISSILES",		// LOGIC_MISSILES
	// Bonuses -
	"SUPERCHARGE",	// LOGIC_SUPERCHARGE
	"INVINCIBLE",	// LOGIC_INVINCIBLE
	"LIFE",			// LOGIC_LIFE
	"REVIVE",		// LOGIC_REVIVE
	// Keys -
	"BLUE",			// LOGIC_BLUE
	"RED",			// LOGIC_RED
	"YELLOW",		// LOGIC_YELLOW
	"CODE1",		// LOGIC_CODE1
	"CODE2",		// LOGIC_CODE2
	"CODE3",		// LOGIC_CODE3
	"CODE4",		// LOGIC_CODE4
	"CODE5",		// LOGIC_CODE5
	"CODE6",		// LOGIC_CODE6
	"CODE7",		// LOGIC_CODE7
	"CODE8",		// LOGIC_CODE8
	"CODE9",		// LOGIC_CODE9
	// Goal items -
	"DATATAPE",		// LOGIC_DATATAPE
	"PLANS",		// LOGIC_PLANS
	"DT_WEAPON",	// LOGIC_DT_WEAPON
	"NAVA",			// LOGIC_NAVA
	"PHRIK",		// LOGIC_PHRIK
	"PILE",			// LOGIC_PILE
	// Enemy logics
	"GENERATOR",	// LOGIC_GENERATOR
	// Imperials -
	"I_OFFICER",	// LOGIC_I_OFFICER
	"I_OFFICERR",	// LOGIC_I_OFFICERR
	"I_OFFICERB",	// LOGIC_I_OFFICERB
	"I_OFFICERY",	// LOGIC_I_OFFICERY
	"I_OFFICER1",	// LOGIC_I_OFFICER1
	"I_OFFICER2",	// LOGIC_I_OFFICER2
	"I_OFFICER3",	// LOGIC_I_OFFICER3
	"I_OFFICER4",	// LOGIC_I_OFFICER4
	"I_OFFICER5",	// LOGIC_I_OFFICER5
	"I_OFFICER6",	// LOGIC_I_OFFICER6
	"I_OFFICER7",	// LOGIC_I_OFFICER7
	"I_OFFICER8",	// LOGIC_I_OFFICER8
	"I_OFFICER9",	// LOGIC_I_OFFICER9
	"TROOP",		// LOGIC_TROOP
	"STORM1",		// LOGIC_STORM1
	"COMMANDO",		// LOGIC_COMMANDO
	// Aliens -
	"BOSSK",		// LOGIC_BOSSK
	"G_GUARD",		// LOGIC_G_GUARD
	"REE_YEES",		// LOGIC_REE_YEES
	"REE_YEES2",	// LOGIC_REE_YEES2
	"SEWER1",		// LOGIC_SEWER1
	// Robots -
	"INT_DROID",	// LOGIC_INT_DROID
	"PROBE_DROID",	// LOGIC_PROBE_DROID
	"REMOTE",		// LOGIC_REMOTE
	// Bosses -
	"BOBA_FETT",	// LOGIC_BOBA_FETT
	"KELL",			// LOGIC_KELL
	"D_TROOP1",		// LOGIC_D_TROOP1
	"D_TROOP2",		// LOGIC_D_TROOP2
	"D_TROOP3",		// LOGIC_D_TROOP3
	// Special sprite logics
	"SCENERY",		// LOGIC_SCENERY
	"ANIM",			// LOGIC_ANIM
	"BARREL",		// LOGIC_BARREL
	"LAND_MINE",	// LOGIC_LAND_MINE
	// 3D object logics
	"TURRET",		// LOGIC_TURRET
	"MOUSEBOT",		// LOGIC_MOUSEBOT
	"WELDER",		// LOGIC_WELDER
	// 3D object motion logics
	"UPDATE",		// LOGIC_UPDATE
	"KEY",			// LOGIC_KEY
};


enum ObjectClass
{
	CLASS_SPIRIT,
	CLASS_SAFE,
	CLASS_SPRITE,
	CLASS_FRAME,
	CLASS_3D,
	CLASS_SOUND,
	CLASS_COUNT,
	CLASS_INAVLID = CLASS_COUNT
};

static const char* c_objectClassName[CLASS_COUNT] =
{
	"SPIRIT",	// CLASS_SPIRIT
	"SAFE",		// CLASS_SAFE
	"SPRITE",	// CLASS_SPRITE
	"FRAME",	// CLASS_FRAME
	"3D",		// CLASS_3D
	"SOUND",	// CLASS_SOUND
};
