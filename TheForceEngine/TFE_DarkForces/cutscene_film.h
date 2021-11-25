#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces "Film" format
// This handles loading and processing the cutscene "film" format,
// though it doesn't handle playback.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_DarkForces/Landru/lactor.h>
#include "cutscene.h"

namespace TFE_DarkForces
{
	enum CutsceneFilmConstants : u32
	{
		// Header
		CF_VERSION      = 4,
		// Types
		CF_TYPE_FILM         = 0x46494c4d,  // 'FILM'
		CF_TYPE_VIEW         = 0x56494557,	// 'VIEW'
		CF_TYPE_CUSTOM_ACTOR = 0x43555354,  // 'CUST'
		CF_TYPE_DELTA_ACTOR  = 0x44454c54,  // 'DELT'
		CF_TYPE_ANIM_ACTOR	 = 0x414e494d,	// 'ANIM'
		CF_TYPE_PALETTE		 = 0x504c5454,	// 'PLTT'
		CF_TYPE_GMIDI		 = 0x474d4944,	// 'GMID'
		CF_TYPE_VOC_SOUND    = 0x564f4943,  // 'BLAS'

		// Files
		CF_FILE_NULL = 0,
		CF_FILE_END,
		CF_FILE_VIEW,
		CF_FILE_ACTOR,
		CF_FILE_PALETTE,
		CF_FILE_SOUND,

		// Commands
		CF_CMD_NONE = 0,
		CF_CMD_DELETE,
		CF_CMD_END,
		CF_CMD_TIMESTAMP,
		CF_CMD_ACTOR_POS,
		CF_CMD_ACTOR_VEL,
		CF_CMD_ACTOR_Z,
		CF_CMD_ACTOR_STATE,
		CF_CMD_ACTOR_STATEV,
		CF_CMD_ACTOR_VAR1,
		CF_CMD_ACTOR_VAR2,
		CF_CMD_ACTOR_CLIP,
		CF_CMD_ACTOR_CLIPV,
		CF_CMD_ACTOR_SHOW,
		CF_CMD_ACTOR_FLIP,
		CF_CMD_PALETTE_SET,
		CF_CMD_VIEW_SETRGB,
		CF_CMD_VIEW_DEFPAL,
		CF_CMD_VIEW_FADE,
		CF_CMD_SOUND_START,
		CF_CMD_SOUND_STOP,
		CF_CMD_SOUND_VOLUME,
		CF_CMD_SOUND_FADE,
		CF_CMD_SOUND_VAR1,
		CF_CMD_SOUND_VAR2,
		CF_CMD_SOUND_CMD,
		CF_CMD_SOUND_PAN,
		CF_CMD_SOUND_PAN_FADE,
		CF_CMD_SOUND_CMD2,

		// State
		CF_STATE_VISIBLE     = FLAG_BIT(0),
		CF_STATE_ACTIVE      = FLAG_BIT(1),
		CF_STATE_DISCARD     = FLAG_BIT(2),
		CF_STATE_REFRESHABLE = FLAG_BIT(3),
		CF_STATE_REFRESH     = FLAG_BIT(4),
		CF_STATE_REPEAT      = FLAG_BIT(5),
		CF_STATE_USER_FLAG1  = FLAG_BIT(14),
		CF_STATE_USER_FLAG2  = FLAG_BIT(15),
	};

	struct RGB
	{
		u8 r, g, b;
	};

	struct Cycle
	{
		s16 dir;
		s16 rate;
		u8 active;
		u8 current;
		u8 low;
		u8 high;
	};

	enum LfdPaletteConstants : u32
	{
		PALETTE_TYPE = 0x504c5454,		// 'PLTT'
		MAX_PALETTE_CYCLES = 4,
		MAX_CYCLE_RATE = 4915,
		MAX_COLORS = 256,
		PALETTE_KEEPABLE = 0x0040,
		PALETTE_KEEP = 0x0080,
	};

	struct LfdPalette
	{
		u32 resType;
		char name[8];
		LfdPalette* next;

		u8* colors;

		Cycle cycles[MAX_PALETTE_CYCLES];
		u8 cycle_count;
		u8 cycle_active;

		s16 start;
		s16 end;
		s16 len;

		u16 flags;
		u8* varptr;
	};

	struct FilmObject
	{
		u32 resType;
		char name[16];
		u32 id;
		u32 offset;
		u8* data;
	};

	struct Film;

	typedef s16(*FilmDrawFunc)(Film*, LRect*, LRect*, s16, s16, s16);
	typedef s16(*FilmUpdateFunc)(Film*);
	typedef s16(*FilmCallback)(Film*, FilmObject*);

	struct Film
	{
		u32 resType;
		char name[8];
		Film* next;
		LfdPalette* def_palette;

		s32 start;
		s32 stop;

		LRect frameRect;
		u32 flags;

		s16 zplane;
		s16 x;
		s16 y;

		u16 cellCount;
		u16 curCell;

		s16 var1;
		s16 var2;
		u8* varPtr;
		u8* varPtr2;	// varhdl

		u8** array;
		s16 arraySize;

		FilmDrawFunc   draw;
		FilmUpdateFunc update;
		FilmCallback   callback;
	};

	Film* cutsceneFilm_load(const char* name, LRect* frameRect, s16 x, s16 y, s16 z, void* callback);
}  // TFE_DarkForces