#include "cutscene_player.h"
#include "cutscene_film.h"
#include "util.h"
#include "time.h"
#include <TFE_DarkForces/Landru/ltimer.h>
#include <TFE_Game/igame.h>
#include <TFE_System/system.h>
#include <TFE_Archive/lfdArchive.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Renderer/virtualFramebuffer.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_System/parser.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	enum CutsceneInternalConstants
	{
		TEXTCRAWL_SCENE = 30,
		MIN_FPS = 4,
		MAX_FPS = 20,
		CUT_TICKS_PER_SECOND = 240,
	};

	// Note that the cutscene player seems to operate at a rate of 240 ticks / second.
	// Also note that some values don't match, for example 5 fps is 48 ticks delay and htere it is marked as 49.
	// 42 ticks delay is obviously wrong for 4 fps (it should be 60). It looks like this table was adjusted for the desired
	// look instead of the source data.
	static const s32 c_frameRateDelay[] =
	{
		42,
		49,
		40,
		35,
		31,
		28,
		25,
		23,
		20,
		19,
		17,
		16,
		15,
		14,
		13,
		12,
		12,
	};

	static s32 s_scene  = SCENE_EXIT;
	static s32 s_playId = 0;
	static Tick s_frameDelay;

	extern CutsceneState* s_playSeq;
	extern s32 s_soundVolume;
	extern s32 s_musicVolume;
	extern s32 s_enabled;

	s32 cutscenePlayer_playScene(s32 scene);
		
	void cutscenePlayer_setFramerate(s32 fps)
	{
		fps = clamp(fps, MIN_FPS, MAX_FPS);
		s_frameDelay = c_frameRateDelay[fps - MIN_FPS];
		ltime_setFrameRate(s_frameDelay);
	}
	
	s16 cutscene_loadCallback(Film* film, FilmObject* obj)
	{
		if (obj->id == CF_FILE_ACTOR)
		{
			// TODO
		}
		else if (obj->id == CF_FILE_SOUND)
		{
			// TODO
		}
		return 0;
	}

	void cutscenePlayer_start(s32 scene)
	{
		s_scene = scene;

		// Find current scene.
		s_playId = 0;
		while (scene != s_playSeq[s_playId].id && s_playSeq[s_playId].id != SCENE_EXIT)
		{
			s_playId++;
		}

		// Start the next sequence of MIDI music.
		if (s_playSeq[s_playId].music > 0)
		{
			// TODO
			// Sound_StartCutscene(s_playSeq[s_playId].music);
		}

		Archive* lfd = nullptr;
		if (s_playSeq[s_playId].id != SCENE_EXIT)
		{
			FilePath path;
			if (!TFE_Paths::getFilePath(s_playSeq[s_playId].archive, &path))
			{
				s_scene = SCENE_EXIT;
				return;
			}
			lfd = new LfdArchive();
			if (!lfd->open(path.path))
			{
				delete lfd;
				s_scene = SCENE_EXIT;
				return;
			}
			TFE_Paths::addLocalArchive(lfd);

			char name[16];
			CutsceneState* scene = &s_playSeq[s_playId];
			strcpy(name, scene->scene);
			cutscenePlayer_setFramerate(scene->speed);

			// Set the sound and music volume.
			s32 baseMusicVol = cutscene_getMusicVolume();
			if (baseMusicVol > 0)  // Set music volume as percentage of the base volume.
			{
				s32 v = clamp(baseMusicVol * scene->volume / 100, 0, 127);
				// ImSetMusicVol(v);
			}
			// ImSetSfxVol(cutscene_getSoundVolume());

			// Load & Setup
			LRect rect;
			Film* film = cutsceneFilm_load(name, &rect, 0, 0, 0, (void*)cutscene_loadCallback);
			if (!film)
			{
				TFE_System::logWrite(LOG_ERROR, "CutscenePlayer", "Unable to load all items in cutscene '%s'.", name);
				s_scene = SCENE_EXIT;
				return;
			}
			// TODO

			// Text Crawl handling

			// Time to startup.
			
			TFE_Paths::removeLastArchive();
			delete lfd;
		}
	}

	JBool cutscenePlayer_update()
	{
		if (s_scene == SCENE_EXIT) { return JFALSE; }

		// update...

		s_scene = cutscenePlayer_playScene(s_scene);
		return s_scene != SCENE_EXIT ? JTRUE : JFALSE;
	}

	s32 cutscenePlayer_playScene(s32 scene)
	{
		return scene;
	}
}  // TFE_DarkForces