#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces "Film" format
// This handles loading and processing the cutscene "film" format,
// though it doesn't handle playback.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_DarkForces/Landru/lactor.h>
#include "cutscene.h"
#include "lpalette.h"

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
		
	struct FilmObject
	{
		u32 resType;
		char name[16];
		u32 id;
		u32 offset;
		u8* data;
	};

	struct Film;

	typedef JBool(*FilmDrawFunc)(Film*, LRect*, LRect*, s16, s16, JBool);
	typedef void(*FilmUpdateFunc)(Film*);
	typedef JBool(*FilmCallback)(Film*, s32);
	typedef JBool(*FilmLoadCallback)(Film*, FilmObject*);

	struct Film
	{
		u32 resType;
		char name[16];
		Film* next;
		LPalette* def_palette;

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

		FilmDrawFunc   drawFunc;
		FilmUpdateFunc updateFunc;
		FilmCallback   callback;
	};

	Film* cutsceneFilm_allocate();
	void cutsceneFilm_free(Film* film);

	Film* cutsceneFilm_load(const char* name, LRect* frameRect, s16 x, s16 y, s16 z, FilmLoadCallback callback);
	void cutsceneFilm_discardData(Film* film);
	void cutsceneFilm_keepData(Film* film);
	void cutsceneFilm_rewindActor(Film* film, FilmObject* filmObj, u8* data);
	void cutsceneFilm_stepActor(Film* film, FilmObject* filmObj, u8* data);

	void cutsceneFilm_add(Film* film);
	void cutsceneFilm_remove(Film* film);

	void cutsceneFilm_updateFilms(s32 time);
	void cutsceneFilm_updateCallbacks(s32 time);
	void cutsceneFilm_drawFilms(JBool refresh);
}  // TFE_DarkForces