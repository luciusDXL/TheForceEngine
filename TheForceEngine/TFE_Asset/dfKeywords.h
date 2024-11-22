#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Keyword Table.
// This table is used when parsing Dark Forces data files. The list of
// strings was extracted from the DF executable.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <string>
#include <vector>

// Keywords used by Dark Forces.
// Note: there are some repeats, so there are a few elements called KW_xxx2; for example KW_KEY2
enum KEYWORD
{
	KW_UNKNOWN = -1,
	KW_VISIBLE = 0,			// not implemented in DF
	KW_SHADED,				// not implemented in DF
	KW_LIGHT,				// not implemented in DF
	KW_PARENT,				// not implemented in DF
	KW_D_X,					// not implemented in DF
	KW_D_Y,					// not implemented in DF
	KW_D_Z,					// not implemented in DF
	KW_D_PITCH,				// logics
	KW_D_YAW,				// logics
	KW_D_ROLL,				// logics
	KW_D_VIEW_PITCH,		// not implemented in DF
	KW_D_VIEW_YAW,			// not implemented in DF
	KW_D_VIEW_ROLL,			// not implemented in DF
	KW_VIEW_PITCH,			// not implemented in DF
	KW_VIEW_YAW,			// not implemented in DF
	KW_VIEW_ROLL,			// not implemented in DF
	KW_TYPE,				
	KW_LOGIC,				// "TYPE" and "LOGIC" are synonymous in DF
	KW_VUE,					// logics
	KW_VUE_APPEND,			// logics
	KW_EYE,					// logics
	KW_BOSS,				// logics
	KW_UPDATE,				// logics
	KW_EYE_D_XYZ,			// not implemented in DF
	KW_EYE_D_PYR,			// not implemented in DF
	KW_SYNC,				// not implemented in DF
	KW_FRAME_RATE,			// logics
	KW_START,				// INF
	KW_STOP_Y,				// not implemented in DF
	KW_STOP,				// INF
	KW_SPEED,				// INF
	KW_MASTER,				// INF
	KW_ANGLE,				// INF
	KW_PAUSE,				// logics
	KW_ADJOIN,				// INF
	KW_TEXTURE,				// INF
	KW_SLAVE,				// INF
	KW_TRIGGER_ACTION,		// not implemented in DF
	KW_CONDITION,			// not implemented in DF
	KW_CLIENT,				// INF
	KW_MESSAGE,				// INF
	KW_TEXT,				// INF
	KW_EVENT,				// INF
	KW_EVENT_MASK,			// INF
	KW_ENTITY_MASK,			// INF
	KW_OBJECT_MASK,			// INF
	KW_CENTER,				// INF
	KW_KEY,					// logics
	KW_ADDON,				// INF
	KW_FLAGS,				// INF
	KW_SOUND_COLON,			// INF
	KW_PAGE,				// INF
	KW_SOUND,
	KW_SYSTEM,				// INF
	KW_SAFE,
	KW_LEVEL,				// INF
	KW_AMB_SOUND,			// INF
	KW_ELEVATOR,			// INF
	KW_BASIC,				// INF
	KW_BASIC_AUTO,			// INF
	KW_ENCLOSED,			// INF (unimplemented)
	KW_INV,					// INF
	KW_MID,					// INF (unimplemented)
	KW_DOOR,				// INF
	KW_DOOR_INV,			// INF
	KW_DOOR_MID,			// INF
	KW_MORPH_SPIN1,			// INF
	KW_MORPH_SPIN2,			// INF
	KW_MORPH_MOVE1,			// INF
	KW_MORPH_MOVE2,			// INF
	KW_MOVE_CEILING,		// INF
	KW_MOVE_FLOOR,			// INF
	KW_MOVE_FC,				// INF
	KW_MOVE_OFFSET,			// INF
	KW_MOVE_WALL,			// INF
	KW_ROTATE_WALL,			// INF
	KW_SCROLL_WALL,			// INF
	KW_SCROLL_FLOOR,		// INF
	KW_SCROLL_CEILING,		// INF
	KW_CHANGE_LIGHT,		// INF
	KW_CHANGE_WALL_LIGHT,	// INF
	KW_TRIGGER,			// This is the trigger ID that will be found.
	KW_SWITCH1,				// INF
	KW_TOGGLE,				// INF
	KW_SINGLE,				// INF
	KW_STANDARD,			// INF
	KW_TELEPORTER,			// INF
	KW_ENTITY_ENTER,		// not implemented in DF
	KW_SECTOR,				// INF
	KW_LINE,				// INF
	KW_TARGET,				// INF
	KW_MOVE,				// INF for Teleporter basic, which is implemented but unused in game
	KW_CHUTE,				// INF
	KW_TRIGGER2,		// This is a repeat and should never be found directly.
	KW_NEXT_STOP,			// INF
	KW_PREV_STOP,			// INF
	KW_GOTO_STOP,			// INF
	KW_MASTER_ON,			// INF
	KW_MASTER_OFF,			// INF
	KW_DONE,				// INF
	KW_SET_BITS,			// INF
	KW_CLEAR_BITS,			// INF
	KW_COMPLETE,			// INF
	KW_LIGHTS,				// INF
	KW_WAKEUP,				// INF
	KW_NONE,
	KW_STORM,				// not implemented in DF
	KW_ENTITY,				// not implemented in DF
	KW_PLAYER,				// logics
	KW_TROOP,				// logics (shared implementation with STORM1)
	KW_STORM1,				// logics
	KW_INT_DROID,			// logics
	KW_PROBE_DROID,			// logics
	KW_D_TROOP1,			// logics
	KW_D_TROOP2,			// logics
	KW_D_TROOP3,			// logics
	KW_BOBA_FETT,			// logics
	KW_COMMANDO,			// logics
	KW_I_OFFICER,			// logics
	KW_I_OFFICER1,			// logics
	KW_I_OFFICER2,			// logics
	KW_I_OFFICER3,			// logics
	KW_I_OFFICER4,			// logics
	KW_I_OFFICER5,			// logics
	KW_I_OFFICER6,			// logics
	KW_I_OFFICER7,			// logics
	KW_I_OFFICER8,			// logics
	KW_I_OFFICER9,			// logics
	KW_I_OFFICERR,			// logics
	KW_I_OFFICERY,			// logics
	KW_I_OFFICERB,			// logics
	KW_G_GUARD,				// logics
	KW_REE_YEES,			// logics
	KW_REE_YEES2,			// logics
	KW_BOSSK,				// logics
	KW_BARREL,				// logics
	KW_LAND_MINE,			// logics
	KW_KELL,				// logics
	KW_SEWER1,				// logics
	KW_REMOTE,				// logics
	KW_TURRET,				// logics
	KW_MOUSEBOT,			// logics
	KW_WELDER,				// logics
	KW_SCENERY,				// logics
	KW_ANIM,				// logics
	KW_KEY_COLON,			// INF
	KW_GENERATOR,			// logics
	KW_TRUE,
	KW_FALSE,
	KW_3D,
	KW_SPRITE,
	KW_FRAME,
	KW_SPIRIT,
	KW_ITEM,				// logics
	KW_DISPATCH,			// logics (dispatch)
	KW_RADIUS,				// logics
	KW_HEIGHT,				// logics
	KW_BATTERY,				// logics
	KW_BLUE,				// logics
	KW_CANNON,				// logics
	KW_CLEATS,				// logics
	KW_CODE1,				// logics
	KW_CODE2,				// logics
	KW_CODE3,				// logics
	KW_CODE4,				// logics
	KW_CODE5,				// logics
	KW_CODE6,				// logics
	KW_CODE7,				// logics
	KW_CODE8,				// logics
	KW_CODE9,				// logics
	KW_CONCUSSION,			// logics
	KW_DETONATOR,			// logics
	KW_DETONATORS,			// logics
	KW_DT_WEAPON,			// logics
	KW_DATATAPE,			// logics
	KW_ENERGY,				// logics
	KW_FUSION,				// logics
	KW_GOGGLES,				// logics
	KW_MASK,				// logics
	KW_MINE,				// logics
	KW_MINES,				// logics
	KW_MISSILE,				// logics
	KW_MISSILES,			// logics
	KW_MORTAR,				// logics
	KW_NAVA,				// logics
	KW_PHRIK,				// logics
	KW_PLANS,				// logics
	KW_PLASMA,				// logics
	KW_POWER,				// logics
	KW_RED,					// logics
	KW_RIFLE,				// logics
	KW_SHELL,				// logics
	KW_SHELLS,				// logics
	KW_SHIELD,				// logics
	KW_INVINCIBLE,			// logics
	KW_REVIVE,				// logics
	KW_SUPERCHARGE,			// logics
	KW_LIFE,				// logics
	KW_MEDKIT,				// logics
	KW_PILE,				// logics
	KW_YELLOW,				// logics
	KW_AUTOGUN,				// logics
	KW_DELAY,				// logics (generators)
	KW_INTERVAL,			// logics (generators)
	KW_MAX_ALIVE,			// logics (generators)
	KW_MIN_DIST,			// logics (generators)
	KW_MAX_DIST,			// logics (generators)
	KW_NUM_TERMINATE,		// logics (generators)
	KW_WANDER_TIME,			// logics (generators)
	KW_PLUGIN,				// logics (dispatch)
	KW_THINKER,				// logics (dispatch)
	KW_FOLLOW,				// logics (dispatch)
	KW_FOLLOW_Y,			// logics (dispatch)
	KW_RANDOM_YAW,			// logics (dispatch)
	KW_MOVER,				// logics (dispatch)
	KW_SHAKER,				// logics (dispatch)
	KW_PERSONALITY,			// not implemented in DF
	KW_SEQEND,
	KW_M_TRIGGER,			// INF
	// TFE ADDED
	KW_SCRIPTCALL,
	KW_COUNT
};

extern KEYWORD getKeywordIndex(const char* keywordString);