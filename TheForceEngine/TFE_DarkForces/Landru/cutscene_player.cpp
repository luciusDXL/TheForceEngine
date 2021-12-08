#include "cutscene_player.h"
#include "cutscene_film.h"
#include "lcanvas.h"
#include "lsound.h"
#include "lmusic.h"
#include "lsystem.h"
#include "time.h"
#include "textCrawl.h"
#include <TFE_DarkForces/Landru/ltimer.h>
#include <TFE_Game/igame.h>
#include <TFE_System/system.h>
#include <TFE_Input/input.h>
#include <TFE_Archive/lfdArchive.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Renderer/virtualFramebuffer.h>
#include <TFE_Jedi/Sound/soundSystem.h>
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
	static LTick s_frameDelay;
	static LActor* s_textCrawl = nullptr;
	static Film* s_film = nullptr;

	extern CutsceneState* s_playSeq;
	extern s32 s_soundVolume;
	extern s32 s_musicVolume;
	extern s32 s_enabled;

	static JBool s_skipSceneInput = JFALSE;
	static JBool s_nextSceneInput = JFALSE;
	static JBool s_cutscenePause = JFALSE;
	static JBool s_cutsceneStepFrame = JFALSE;

	void cutscene_customSoundCallback(LActor* actor, s32 time);
	s32  lcutscenePlayer_endView(s32 time);
				
	void cutscenePlayer_setFramerate(s32 fps)
	{
		fps = clamp(fps, MIN_FPS, MAX_FPS);
		s_frameDelay = c_frameRateDelay[fps - MIN_FPS];
		ltime_setFrameRate(s_frameDelay);
	}
	
	JBool cutscene_loadCallback(Film* film, FilmObject* obj)
	{
		if (obj->id == CF_FILE_ACTOR)
		{
			LActor* actor = (LActor*)obj->data;
			if (actor->resType == CF_TYPE_CUSTOM_ACTOR)
			{
				// custom actors send midi cue points.
				lactor_setCallback(actor, cutscene_customSoundCallback);
			}
		}
		else if (obj->id == CF_FILE_SOUND)
		{
			LSound* sound = (LSound*)obj->data;
			if (sound->var2 > 0)
			{
				lsound_setKeep(sound);
			}
		}
		return JFALSE;
	}
		
	void cutscenePlayer_start(s32 sceneId)
	{
		s_scene = sceneId;
		s_textCrawl = nullptr;
		
		// Find current scene.
		s_playId = 0;
		while (sceneId != s_playSeq[s_playId].id && s_playSeq[s_playId].id != SCENE_EXIT)
		{
			s_playId++;
		}

		// Start the next sequence of MIDI music.
		if (s_playSeq[s_playId].music > 0)
		{
			lmusic_startCutscene(s_playSeq[s_playId].music);
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
			lcanvas_getBounds(&rect);

			lsystem_setAllocator(LALLOC_CUTSCENE);
			s_film = cutsceneFilm_load(name, &rect, 0, 0, 0, cutscene_loadCallback);
			if (!s_film)
			{
				lsystem_clearAllocator(LALLOC_CUTSCENE);
				lsystem_setAllocator(LALLOC_PERSISTENT);

				// Close the archive.
				TFE_Paths::removeLastArchive();
				delete lfd;

				TFE_System::logWrite(LOG_ERROR, "CutscenePlayer", "Unable to load all items in cutscene '%s'.", name);
				s_scene = SCENE_EXIT;
				return;
			}
			lview_setUpdateFunc(lcutscenePlayer_endView);

			// Close the archive.
			TFE_Paths::removeLastArchive();
			delete lfd;
					   			
			// Text Crawl handling
			if (sceneId == TEXTCRAWL_SCENE)
			{
				s_textCrawl = lactor_find(CF_TYPE_DELTA_ACTOR, "textcraw");
				if (s_textCrawl)
				{
					openTextCrawl(s_textCrawl, s_film);
				}
			}

			// In the original code, the playback loop starts here, and then the cleanup afterward.
			// For TFE, the function will return and then cutscenePlayer_update() will handle each frame.
			lview_startLoop();
		}
		else
		{
			s_scene = SCENE_EXIT;
		}
	}

	void cutscenePlayer_stop()
	{
		if (s_textCrawl)
		{
			closeTextCrawl(s_textCrawl);
			s_textCrawl = nullptr;
		}
		lview_clearUpdateFunc();
	}
				
	JBool cutscenePlayer_update()
	{
		if (s_scene == SCENE_EXIT) { return JFALSE; }

		// TFE: Added since inputs can be skipped at slow framerates.
		if (TFE_Input::keyPressed(KEY_ESCAPE) || TFE_Input::keyPressed(KEY_RETURN))
		{
			s_skipSceneInput = JTRUE;
		}
		else if (TFE_Input::keyPressed(KEY_SPACE))
		{
			s_nextSceneInput = JTRUE;
		}

		/////////////////////////////////
		// DEBUG: Single step cutscenes.
		if (TFE_Input::keyPressed(KEY_PAUSE))
		{
			s_cutscenePause = ~s_cutscenePause;
		}

		s_cutsceneStepFrame = JFALSE;
		if (s_cutscenePause && TFE_Input::keyPressed(KEY_BACKSPACE))
		{
			s_cutsceneStepFrame = JTRUE;
		}
		/////////////////////////////////

		JBool stepView = JFALSE;
		if (s_cutscenePause)
		{
			stepView = s_cutsceneStepFrame;
		}
		else
		{
			tfe_updateLTime();
			stepView = ltime_isFrameReady();
		}

		if (stepView)
		{
			s32 exitValue = lview_loop();
			if (exitValue != VIEW_LOOP_RUNNING)
			{
				lview_endLoop();
				cutscenePlayer_stop();
				cutsceneFilm_remove(s_film);
				cutsceneFilm_free(s_film);
				s_film = nullptr;
				
				if (exitValue != SCENE_EXIT)
				{
					cutscenePlayer_start(exitValue);
				}
				else
				{
					s_scene = SCENE_EXIT;
				}
			}
		}

		if (s_scene == SCENE_EXIT)
		{
			lmusic_stop();
			lsystem_clearAllocator(LALLOC_CUTSCENE);
			lsystem_setAllocator(LALLOC_PERSISTENT);
		}

		return s_scene != SCENE_EXIT ? JTRUE : JFALSE;
	}

	s32 lcutscenePlayer_endView(s32 time)
	{
		s16 nextScene = s_playSeq[s_playId].nextId;
		s16 skipScene = s_playSeq[s_playId].skip;

		if (s_skipSceneInput)
		{
			s_skipSceneInput = JFALSE;
			sound_stopAll();
			lmusic_stop();
			vfb_forceToBlack();
			lcanvas_clear();
			return skipScene;
		}
		else if (s_nextSceneInput)
		{
			s_nextSceneInput = JFALSE;
			sound_stopAll();
			vfb_forceToBlack();
			lcanvas_clear();
			return nextScene;
		}
		else if (s_film->curCell >= s_film->cellCount)
		{
			return nextScene;
		}

		return VIEW_LOOP_RUNNING;
	}

	void cutscene_customSoundCallback(LActor* actor, s32 time)
	{
		s32 var1 = actor->var1;
		if (var1)
		{
			if (var1 < 0) { lmusic_setCuePoint(0); }
			         else { lmusic_setCuePoint(var1); }
		}
	}
}  // TFE_DarkForces