#include <cstring>

#include "darkForcesMain.h"
#include "agent.h"
#include "automap.h"
#include "config.h"
#include "briefingList.h"
#include "gameMessage.h"
#include "gameMusic.h"
#include "hud.h"
#include "item.h"
#include "mission.h"
#include "player.h"
#include "pickup.h"
#include "projectile.h"
#include "random.h"
#include "time.h"
#include "weapon.h"
#include "vueLogic.h"
#include "GameUI/menu.h"
#include "GameUI/agentMenu.h"
#include "GameUI/escapeMenu.h"
#include "GameUI/missionBriefing.h"
#include "GameUI/pda.h"
#include "Landru/lsystem.h"
#include "Landru/lmusic.h"
#include "Landru/cutscene_film.h"
#include <TFE_DarkForces/Landru/cutscene.h>
#include <TFE_DarkForces/Landru/cutsceneList.h>
#include <TFE_DarkForces/Actor/actor.h>
#include <TFE_Game/reticle.h>
#include <TFE_Input/inputMapping.h>
#include <TFE_Memory/memoryRegion.h>
#include <TFE_Settings/settings.h>
#include <TFE_System/system.h>
#include <TFE_System/tfeMessage.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/physfswrapper.h>
#include <TFE_A11y/accessibility.h>
#include <TFE_Audio/midiPlayer.h>
#include <TFE_Audio/audioSystem.h>
#include <TFE_Asset/modelAsset_jedi.h>
#include <TFE_Asset/spriteAsset_Jedi.h>
#include <TFE_Jedi/Level/rfont.h>
#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/InfSystem/infSystem.h>
#include <TFE_Jedi/Task/task.h>
#include <TFE_Jedi/Renderer/jediRenderer.h>
#include <TFE_Jedi/Task/task.h>
#include <TFE_Jedi/IMuse/imuse.h>
#include <TFE_Jedi/Serialization/serialization.h>
#include <assert.h>
#include <physfs.h>

// Add texture callbacks.
#include <TFE_Jedi/Level/levelTextures.h>

using namespace TFE_Memory;
using namespace TFE_Input;

namespace TFE_DarkForces
{
	/////////////////////////////////////////////
	// Constants
	/////////////////////////////////////////////
	static const char* c_gobFileNames[] =
	{
		"SOUNDS.GOB",
		"TEXTURES.GOB",
		"SPRITES.GOB",
	};

	static const char* c_optionalGobFileNames[] =
	{
		"enhanced.gob",
		"extras.gob",
	};

	// main game levels (not the ones from a GOB)
	static const char* c_mainGameGobFileNames[] =
	{
		"DARK.GOB",
	};

	enum GameState
	{
		GSTATE_STARTUP_CUTSCENES = 0,
		GSTATE_AGENT_MENU,
		GSTATE_CUTSCENE,
		GSTATE_BRIEFING,
		GSTATE_MISSION,
		GSTATE_COUNT
	};

	enum GameMode
	{
		GMODE_END = -1,
		GMODE_CUTSCENE = 0,
		GMODE_BRIEFING = 1,
		GMODE_MISSION = 2,
	};

	struct CutsceneData
	{
		s32 levelIndex;
		GameMode nextGameMode;
		s32 cutscene;
	};
		
	static CutsceneData s_cutsceneData[] =
	{
		{ 1,  GMODE_CUTSCENE, 100 },
		{ 1,  GMODE_BRIEFING,   0 },
		{ 1,  GMODE_MISSION,	0 },
		{ 1,  GMODE_CUTSCENE, 150 },

		{ 2,  GMODE_CUTSCENE, 200 },
		{ 2,  GMODE_BRIEFING,   0 },
		{ 2,  GMODE_MISSION,    0 },
		{ 2,  GMODE_CUTSCENE, 250 },

		{ 3,  GMODE_CUTSCENE, 300 },
		{ 3,  GMODE_BRIEFING,   0 },
		{ 3,  GMODE_MISSION,	0 },
		{ 3,  GMODE_CUTSCENE, 350 },

		{ 4,  GMODE_CUTSCENE, 400 },
		{ 4,  GMODE_BRIEFING,   0 },
		{ 4,  GMODE_MISSION,    0 },
		{ 4,  GMODE_CUTSCENE, 450 },

		{ 5,  GMODE_CUTSCENE, 500 },
		{ 5,  GMODE_BRIEFING,	0 },
		{ 5,  GMODE_MISSION,	0 },
		{ 5,  GMODE_CUTSCENE, 550 },

		{ 6,  GMODE_CUTSCENE, 600 },
		{ 6,  GMODE_BRIEFING,	0 },
		{ 6,  GMODE_MISSION,	0 },
		{ 6,  GMODE_CUTSCENE, 650 },

		{ 7,  GMODE_CUTSCENE, 700 },
		{ 7,  GMODE_BRIEFING,	0 },
		{ 7,  GMODE_MISSION,	0 },
		{ 7,  GMODE_CUTSCENE, 750 },

		{ 8,  GMODE_CUTSCENE, 800 },
		{ 8,  GMODE_BRIEFING,	0 },
		{ 8,  GMODE_MISSION, 	0 },
		{ 8,  GMODE_CUTSCENE, 850 },

		{ 9,  GMODE_CUTSCENE, 900 },
		{ 9,  GMODE_BRIEFING,	0 },
		{ 9,  GMODE_MISSION,	0 },
		{ 9,  GMODE_CUTSCENE, 950 },

		{ 10, GMODE_CUTSCENE,1000 },
		{ 10, GMODE_BRIEFING,   0 },
		{ 10, GMODE_MISSION,	0 },
		{ 10, GMODE_CUTSCENE,1050 },

		{ 11, GMODE_CUTSCENE,1100 },
		{ 11, GMODE_BRIEFING,	0 },
		{ 11, GMODE_MISSION,	0 },
		{ 11, GMODE_CUTSCENE,1150 },

		{ 12, GMODE_CUTSCENE,1200 },
		{ 12, GMODE_BRIEFING,	0 },
		{ 12, GMODE_MISSION,	0 },
		{ 12, GMODE_CUTSCENE,1250 },

		{ 13, GMODE_CUTSCENE,1300 },
		{ 13, GMODE_BRIEFING,	0 },
		{ 13, GMODE_MISSION,	0 },
		{ 13, GMODE_CUTSCENE,1350 },

		{ 14, GMODE_CUTSCENE,1400 },
		{ 14, GMODE_BRIEFING,   0 },
		{ 14, GMODE_MISSION,	0 },
		{ 14, GMODE_CUTSCENE,1450 },
		{ 14, GMODE_CUTSCENE,1500 },	//	game ending.
		// Game flow end (restart).
		{ -1, GMODE_END, -1 }
	};

	/////////////////////////////////////////////
	// Internal State
	/////////////////////////////////////////////
	struct RunGameState
	{
		s32 argCount = 0;
		char* args[64] = { 0 };

		JBool cutscenesEnabled = JTRUE;
		JBool localMsgLoaded   = JFALSE;
		s32   startLevel       = 0;
		GameState state        = GSTATE_STARTUP_CUTSCENES;
		s32   levelIndex       = 0;
		s32   cutsceneIndex    = 0;
		JBool abortLevel       = JFALSE;
	};
	struct SharedGameState
	{
		GameMessages localMessages = {};
		GameMessages hotKeyMessages = {};
		TextureData* diskErrorImg = nullptr;
		Font* swFont1 = nullptr;
		Font* mapNumFont = nullptr;
		SoundSourceId screenShotSndSrc = NULL_SOUND;
		BriefingList  briefingList = { 0 };
		JBool gameStarted = JFALSE;

		Task* loadMissionTask = nullptr;
		CutsceneState* cutsceneList = nullptr;
		char customGobName[256] = "";
		LangHotkeys langKeys;
	};
	static RunGameState   s_runGameState = {};
	static SharedGameState s_sharedState = {};

	/////////////////////////////////////////////
	// Forward Declarations
	/////////////////////////////////////////////
	void printGameInfo();
	void processCommandLineArgs(s32 argCount, const char* argv[], char* startLevel);
	void enableCutscenes(JBool enable);
	void loadCustomGob(const char* gobName);
	void setInitialLevel(const char* levelName);
	s32  loadLocalMessages();
	void gameStartup();
	void loadAgentAndLevelData();
	void startNextMode();
	void freeAllMidi();
	void pauseLevelSound();
	void resumeLevelSound();

	// find out if a gob contains the level list
	static bool gobHasJedilvl(const char *modgob)
	{
		// mount over the mod examination area, maybe the mod ZIP already
		// provides the jedi.lvl and we catch that here.
		TFEMount m = vpMountVirt(VPATH_TMP, modgob, VPATH_TMP, true, false);
		if (!m)
			return false;
		bool found = vpFileExists(VPATH_TMP, "jedi.lvl", false);
		vpUnmount(m);
		return found;
	}

	/*
	 * Mount the virtual gamedata tree:
	 * - First add the DF source (folder or Archive) from real filesystem
	 * - Now add the contents of all the base resource GOBs which are now accessible from within this tree
	 * - Add contents of optional extra GOBs from DF Remaster
	 * - Add the contents of the cutscene LFDs (LFD/ folder)
	 * - If we load a MOD:
	 *    - enumerate all the GOB files in the mod
	 *    - find out whether mod or any of its GOBs provide a "jedi.lvl" file
	 *    - mount the mod folder/archive at the gametree root, first in the search order.
	 *    - add the contents of all the MOD GOBs to the gametree root, first in the search order.
	 *      This way their contents override any existing content from earlier steps.
	 * - NO JEDI.LVL already found:
	 *    - add the contents of the standard DARK.GOB level container.
	 * - Add the contents of all the LFD files at the root level of the gamedata tree,
	 *    front in the search order.
	 *    This way any LFD files which are usually overridden by mods (i.e. dfbrief.lfd)
	 *    have now preference over of the ones from the first step.
	 */
	static bool mountGame(const char* srcpath, const char* modpath)
	{
		TFEExtList telfd = { "lfd" };
		TFEExtList tegob = { "gob" };
		TFEMount m[1024];
		int j, skipped;
		bool loaddefgob;
	
		j = 0;
		skipped = 0;
		loaddefgob = true;		// assume DARK.GOB needs to be loaded

		vpUnmountTree(VPATH_GAME);
		m[j] = vpMountReal(srcpath, VPATH_GAME);
		if (!m[j])
		{
			TFE_System::logWrite(LOG_ERROR, "DarkForcesMain", "cannot mount game source");
			return false;
		} else {
			TFE_System::logWrite(LOG_MSG, "DarkForcesMain", "mounted %s", srcpath);
		}
		++j;

		/*
		 * MAIN Resource GOBs
		 */
		for (auto i : c_gobFileNames)
		{
			m[j] = vpMountVirt(VPATH_GAME, i, VPATH_GAME, true, false);
			if (!m[j])
			{
				TFE_System::logWrite(LOG_ERROR, "DarkForcesMain", "cannot mount: %s", i);
				vpUnmountTree(VPATH_GAME);
				return false;
			}
			else
			{
				TFE_System::logWrite(LOG_MSG, "DarkForcesMain", "mounted %s", i);
			}
			++j;
		}

		/*
		 * the DF Remaster optional GOBs
		 */
		for (auto i : c_optionalGobFileNames)
		{
			m[j] = vpMountVirt(VPATH_GAME, i, VPATH_GAME, true, false);
			if (m[j])
				++j;
			else
				++skipped;

			TFE_System::logWrite(LOG_MSG, "DarkForcesMain", m[j] ? "mounted %s" : "skipped %s", i);
		}

		/*
		 * Add the contents of all the cutscene LFDs in the DF LFD/ subdir
		 */
		TFEFileList tl;
		if (vpGetFileList(VPATH_GAME, "lfd/", tl, telfd, false))
		{
			char buf[32];
			for (auto i : tl)
			{
				sprintf(buf, "LFD/%s", i.c_str());
				m[j] = vpMountVirt(VPATH_GAME, buf, VPATH_GAME, false, false);
				TFE_System::logWrite(LOG_MSG, "DarkForcesMain", m[j] ? "mounted %s" : "failed %s", buf);
				if (m[j])
					j++;
			}
		}
		else
		{
			TFE_System::logWrite(LOG_ERROR, "DarkForcesMain", "unable to enumerate LFD/ contents, aborting");
			return false;
		}

		/*
		 * external MOD
		 */
		if (modpath)
		{
			TFEFileList goblist, customlist;

			// get a list of GOBs in the archive. LFDs are handled universally later.
			TFEMount tm = vpMountReal(modpath, VPATH_TMP);
			if (!tm)
			{
				TFE_System::logWrite(LOG_ERROR, "DarkForcesMain", "could not find MOD %s, aborting", modpath);
				return false;
			}
			bool ok1 = vpGetFileList(VPATH_TMP, goblist, tegob);

			if (!ok1)
			{
				TFE_System::logWrite(LOG_ERROR, "DarkForcesMain", "MOD %s, unable to enumerate files. aborting", modpath);
				vpUnmount(tm);
				return false;
			}

			// look for a JEDI.LVL file in the mod gobs.
			bool hasjedilvl = false;
			for (auto i : goblist)
			{
				if (gobHasJedilvl(i.c_str()))
				{
					hasjedilvl = true;
					break;
				}
			}
			// in case no gobs..
			if (!hasjedilvl)
				hasjedilvl |= vpFileExists(VPATH_TMP, "jedi.lvl", false);

			vpUnmount(tm);
			if (hasjedilvl)
				loaddefgob = false;

			// now mount the mod at the base dir
			m[j] = vpMountReal(modpath, VPATH_GAME, true);
			if (!m[j])
			{
				TFE_System::logWrite(LOG_ERROR, "DarkForcesMain", "MOD %s, unable to mount, aborting", modpath);
				return false;
			}
			j++;

			customlist.clear();
			customlist.emplace_back(modpath);
			for (auto i : goblist)
			{
				m[j] = vpMountVirt(VPATH_GAME, i.c_str(), VPATH_GAME, true, true);
				if (!m[j])
				{
					TFE_System::logWrite(LOG_ERROR, "DarkForcesMain", "MOD %s, unable to mount GOB %s", modpath, i.c_str());
					return false;
				} else {
					TFE_System::logWrite(LOG_MSG, "DarkForcesMain", "MOD %s, mounted %s", modpath, i.c_str());
				}
				j++;

				// record the GOB as a custom one (i.e. not from base game
				// and therefore not eligible for DF Remaster asset replacement.
				char *gobpath = vpToVpath(VPATH_GAME, i.c_str());
				customlist.emplace_back(gobpath);
				free(gobpath);
			}
			bitmap_setCustomArchives(customlist);
		}

		/*
		 * DARK.GOB (main levels)
		 */
		if (loaddefgob)
		{
			/*
			 * Main Game Level GOB
			 */
			for (auto i : c_mainGameGobFileNames)
			{
				m[j] = vpMountVirt(VPATH_GAME, i, VPATH_GAME, true, false);
				if (m[j]) {
					TFE_System::logWrite(LOG_MSG, "DarkForcesMain", "mounted %s", i);
				} else {
					TFE_System::logWrite(LOG_ERROR, "DarkForcesMain", "could not find %s, aborting", i);
					return false;
				}
				j++;
			}
		}
		
		/*
		 * Add all the LFDs in the root folder now,
		 * since the MODs now override the base game ones (i.e. dfbrief.lfd)
		 */
		tl.clear();
		if (vpGetFileList(VPATH_GAME, tl, telfd))
		{
			char buf[32];
			for (auto i : tl)
			{
				m[j] = vpMountVirt(VPATH_GAME, i.c_str(), VPATH_GAME, false, true);
				TFE_System::logWrite(LOG_MSG, "DarkForcesMain", m[j] ? "mounted %s" : "skipped %s", i.c_str());
				if (m[j])
					j++;
			}
		} else {
			TFE_System::logWrite(LOG_ERROR, "DarkForcesMain", "unable to enumerate root LFD files, aborting");
			return false;
		}

		/*
		 * Adjustable HUD
		 */
		m[j] = vpMountVirt(VPATH_TFE, "Mods/TFE/AdjustableHud/", VPATH_GAME, true, false);
		TFE_System::logWrite(LOG_MSG, "DarkForcesMain", m[j] ? "mounted %s" : "skipped %s", "Mods/TFE/AdjustableHud/");
		j++;

#if 0
		{	// dump the file listing
			TFEFileList tl;
			if (vpGetFileList(VPATH_GAME, tl))
				for (auto i : tl)
					printf("=> %s\n", i.c_str());
		}
#endif
		TFE_System::logWrite(LOG_MSG, "DarkForcesMain", "mounted %d, skipped %d optional archives", j, skipped);
		return true;
	}


	/////////////////////////////////////////////
	// API
	/////////////////////////////////////////////
		
	// This is the equivalent of the initial part of main() in Dark Forces DOS.
	// This part loads and sets up the game.
	bool DarkForces::runGame(s32 argCount, const char* argv[], vpFile* stream)
	{
		// TFE: Initially disable the reticle.
		reticle_enable(false);

		bool ok = mountGame(TFE_Paths::getPath(PATH_SOURCE_DATA), nullptr);
		if (!ok)
		{
			vpUnmountTree(VPATH_GAME);
			return false;
		}

		if (!stream)
		{
			// Normal start.
			s_runGameState.argCount = min(64, argCount);
			for (s32 i = 0; i < s_runGameState.argCount; i++)
			{
				if (i == 0)
				{
					// No need to store the executable path, just put in a dummy value so everything else works as-is.
					s_runGameState.args[i] = (char*)game_alloc(strlen("ExeName") + 1);
					strcpy(s_runGameState.args[i], "ExeName");
				}
				else
				{
					s_runGameState.args[i] = (char*)game_alloc(strlen(argv[i]) + 1);
					strcpy(s_runGameState.args[i], argv[i]);
				}
			}
		}
		else
		{
			// Start from save game.
			SERIALIZE(SaveVersionInit, s_runGameState.argCount, 0);
			for (s32 i = 0; i < s_runGameState.argCount; i++)
			{
				u32 length;
				SERIALIZE(SaveVersionInit, length, 0);

				s_runGameState.args[i] = (char*)game_alloc(length + 1);
				SERIALIZE_BUF(SaveVersionInit, s_runGameState.args[i], length);
				s_runGameState.args[i][length] = 0;
			}

			argCount = s_runGameState.argCount;
			argv = (const char**)s_runGameState.args;
		}

		char startLevel[TFE_MAX_PATH] = "";
		bitmap_setAllocator(s_gameRegion);

		printGameInfo();
		processCommandLineArgs(argCount, argv, startLevel);
		loadLocalMessages();

		// Sound is initialized before the task system.
		sound_open(s_gameRegion);

		TFE_Jedi::task_setDefaults();
		TFE_Jedi::task_setMinStepInterval(1.0f / f32(TICKS_PER_SECOND));
		TFE_Jedi::setupInitCameraAndLights();
		config_startup();
		gameStartup();
		loadAgentAndLevelData();
		lsystem_init();

		renderer_init();

		// Handle start level
		setInitialLevel(startLevel);
		
		// TFE Specific
		agentMenu_load(&s_sharedState.langKeys);
		escapeMenu_load(&s_sharedState.langKeys);
		// Add texture callbacks.
		renderer_addHudTextureCallback(TFE_Jedi::level_getLevelTextures);
		renderer_addHudTextureCallback(TFE_Jedi::level_getObjectTextures);

		// Deserialize.
		if (stream)
		{
			SERIALIZE(SaveVersionInit, s_runGameState.cutscenesEnabled, JTRUE);
			SERIALIZE(SaveVersionInit, s_runGameState.localMsgLoaded, JFALSE);
			SERIALIZE(SaveVersionInit, s_runGameState.startLevel, 0);
			SERIALIZE(SaveVersionInit, s_runGameState.state, GSTATE_STARTUP_CUTSCENES);
			SERIALIZE(SaveVersionInit, s_runGameState.levelIndex, 0);
			SERIALIZE(SaveVersionInit, s_runGameState.cutsceneIndex, 0);
			SERIALIZE(SaveVersionInit, s_runGameState.abortLevel, 0);
		}

		s_sharedState.gameStarted = JTRUE;
		sound_setLevelStart();
		return true;
	}

	void DarkForces::exitGame()
	{
		if (s_sharedState.gameStarted)
		{
			saveLevelStatus();
		}
		freeAllMidi();

		gameMessage_freeBuffer();
		briefingList_freeBuffer();
		cutsceneList_freeBuffer();
		cutsceneFilm_reset();
		lsystem_destroy();
		bitmap_clearAll();
		
		// Clear paths and archives.
		task_shutdown();

		// Sound is destroyed after the task system.
		sound_close();
		config_shutdown();

		// TFE Specific
		// Reset state
		actor_exitState();
		weapon_resetState();
		renderer_resetState();
		agentMenu_resetState();
		menu_resetState();
		pda_resetState();
		escapeMenu_resetState();
		vue_resetState();
		lsystem_destroy();
		hud_reset();
		
		// TFE
		TFE_Sprite_Jedi::freeAll();
		TFE_Model_Jedi::freeAll();
		reticle_enable(false);
		texturepacker_reset();

		TFE_MidiPlayer::resume();
		TFE_Audio::resume();

		// Reset state.
		s_sharedState = {};
		s_runGameState = {};

		vpUnmountTree(VPATH_GAME);
	}

	void DarkForces::pauseGame(bool pause)
	{
		mission_pause(pause ? JTRUE : JFALSE);
	}

	bool DarkForces::isPaused()
	{
		return s_gamePaused;
	}

	void DarkForces::pauseSound(bool pause)
	{
		if (pause) { pauseLevelSound(); }
		else { resumeLevelSound(); }
	}

	void DarkForces::restartMusic()
	{
		gameMusic_setState(MUS_STATE_NULLSTATE);
		ImReintializeMidi();
		gameMusic_setState(MUS_STATE_STALK);
	}
		
	void handleLevelComplete()
	{
		s32 completedLevelIndex = agent_getLevelIndex();
		u8 diff = s_agentData[s_agentId].difficulty;

		// Save the level completion, inventory and other stats into the agent data and then save to disk.
		agent_saveLevelCompletion(diff, completedLevelIndex);
		agent_updateAgentSavedData();
		agent_saveInventory(s_agentId, completedLevelIndex + 1);
		agent_setNextLevelByIndex(completedLevelIndex + 1);
	}

	void saveLevelStatus()
	{
		if (s_levelComplete)
		{
			handleLevelComplete();
		}
		else
		{
			agent_updateAgentSavedData();
		}
	}

	bool DarkForces::canSave()
	{
		return s_runGameState.state == GSTATE_MISSION;
	}

	void DarkForces::getLevelName(char* name)
	{
		const char* levelName = agent_getLevelDisplayName();
		if (levelName)
		{
			strcpy(name, levelName);
		}
		else
		{
			name[0] = 0;
		}
	}

	void skipToLevelNextScene(s32 index)
	{
		if (!index) { return; }

		s_invalidLevelIndex = JTRUE;
		for (s32 i = 0; i < TFE_ARRAYSIZE(s_cutsceneData); i++)
		{
			if (s_cutsceneData[i].levelIndex >= 0 && s_cutsceneData[i].levelIndex == index && s_cutsceneData[i].nextGameMode == GMODE_BRIEFING)
			{
				s_runGameState.cutsceneIndex = i - 1;
				s_invalidLevelIndex = JFALSE;
				break;
			}
		}
	}

	void DarkForces::getModList(char* modList)
	{
		strcpy(modList, s_sharedState.customGobName);
	}

	/**********The basic structure of the Dark Forces main loop is as follows:***************
	while (1)  // <- This will be replaced by the function call from the main TFE loop.
	{
		// TFE: This becomes a game state in TFE - where runAgentMenu() gets an update function - it can't loop forever.
		s32 levelIndex = runAgentMenu();  <- this loops internally until complete and returns the level to load.
		updateAgentSavedData();	// handle any agent changes.

		// Invalid level index just maps back to the runAgentMenu().
		if (levelIndex <= 0) { continue; }  // <- This becomes a return in TFE, back off and try again.

		// Then we go through the list of cutscenes, looking for the first instance that matches our desired level.
		s32 levelDataIndex;	// <- this is the index into the cutscene list.
		for (s32 i = 0; ; i++)
		{
			if (cutsceneData[i].levelIndex >= 0 && s_agentLevelData[n].levelIndex == levelIndex)
			{
				levelDataIndex = i;
				break;
			}
		}
		// Then do some init setup for the next level ahead of time; the actual loading will happen after the cutscenes and mission briefing.
		setLevelByIndex(levelIndex);
		
		// The inner most loop - this cycles through the cutscene entries, each of which lists the game mode.
		// TFE: Again this becomes a game state, where each iteration through this loop is from a single function call into loopGame().
		while (!s_invalidLevelIndex && !s_abortLevel)
		{
			GameMode mode = s_agentLevelData[levelDataIndex].nextGameMode;
			switch (mode)
			{
				case GMODE_ERROR:
					// Error out.
				break;
				case GMODE_CUTSCENE:
					// TFE: Cutscene playback becomes a state.
					// Play the cutscene
					levelDataIndex++;	// <- next loop, this goes to the next cutscene or instruction.
				break;
				case GMODE_BRIEFING:
					// TFE: Briefing becomes a state.
					// Handle the mission briefing.
					levelDataIndex++;	// <- next loop, this goes to the next cutscene or instruction.
				break;
				case GMODE_MISSION:
					// TFE: In Mission becomes a state, but here the Task System will take up the slack (which will act very similarly to the original game).
					// Create the loadMission task, which will then create the main task, etc.
					// Start the level music.
					// Read saved game data for this agent and this level.
					// Launch the level load task.
					// After returning from the level load task, stop the music.
					// If not complete set abortLevel to true (which breaks out of this inner loop) otherwise

					// Save the game state and level completion info to disk.
					levelDataIndex++;	// <- next loop, this goes to the next cutscene or instruction.
					levelIndex++;
					setNextLevelByIndex(levelIndex);	// <- prepares the next level for when we get to it after the cutscenes and briefing.
				break;
			}
		}  // Inner Loop
	}  // Outer Loop
	****************************************************/
	void DarkForces::loopGame()
	{
		updateTime();
				
		switch (s_runGameState.state)
		{
			case GSTATE_STARTUP_CUTSCENES:
			{
				s_runGameState.state = GSTATE_CUTSCENE;
				s_invalidLevelIndex = JTRUE;
				if (s_runGameState.cutscenesEnabled && !s_runGameState.startLevel)
				{
					cutscene_play(10);
				}
				else
				{
					startNextMode();
				}
			} break;
			case GSTATE_AGENT_MENU:
			{
				bool levelSelected = false;
				if (s_runGameState.startLevel)
				{
					s_runGameState.abortLevel = JFALSE;
					s_runGameState.levelIndex = s_runGameState.startLevel;
					s_runGameState.startLevel = 0;
					levelSelected = true;
				}
				else if (!agentMenu_update(&s_runGameState.levelIndex))
				{
					agent_updateAgentSavedData();
					levelSelected = true;
				}

				if (levelSelected)
				{
					s_invalidLevelIndex = JTRUE;
					for (s32 i = 0; i < TFE_ARRAYSIZE(s_cutsceneData); i++)
					{
						if (s_cutsceneData[i].levelIndex >= 0 && s_cutsceneData[i].levelIndex == s_runGameState.levelIndex)
						{
							s_runGameState.cutsceneIndex = i;
							s_invalidLevelIndex = JFALSE;
							break;
						}
					}

					lmusic_reset();
					s_runGameState.abortLevel = JFALSE;
					agent_setNextLevelByIndex(s_runGameState.levelIndex);
					startNextMode();
				}
			} break;
			case GSTATE_CUTSCENE:
			{
				if (cutscene_update())
				{
					if (TFE_A11Y::cutsceneCaptionsEnabled()) { TFE_A11Y::drawCaptions(); }
				}
				else
				{
					s_runGameState.cutsceneIndex++;
					if (s_cutsceneData[s_runGameState.cutsceneIndex].nextGameMode == GMODE_END)
					{
						s_runGameState.state = GSTATE_AGENT_MENU;
						s_invalidLevelIndex = JTRUE;
					}
					else
					{
						startNextMode();
					}
					TFE_A11Y::clearActiveCaptions();
				}
			} break;
			case GSTATE_BRIEFING:
			{
				s32 skill;
				JBool abort;
				lmusic_reset();	// Fix a Dark Forces bug where music won't play when entering a cutscene again without restarting.
				if (!missionBriefing_update(&skill, &abort))
				{
					missionBriefing_cleanup();
					TFE_Input::clearAccumulatedMouseMove();

					if (abort)
					{
						s_invalidLevelIndex = JTRUE;
						s_runGameState.cutsceneIndex--;
					}
					else
					{
						s_agentData[s_agentId].difficulty = skill;
						s_runGameState.cutsceneIndex++;
					}
					startNextMode();
				}
			} break;
			case GSTATE_MISSION:
			{
				// At this point the mission has already been launched.
				// The task system will take over. Basically every frame we just check to see if there are any tasks running.
				if (task_getCount())
				{
					if (!s_gamePaused && TFE_A11Y::gameplayCaptionsEnabled()) { TFE_A11Y::drawCaptions(); }
				}
				else
				{
					// We have returned from the mission tasks.
					renderer_reset();
					gameMusic_stop();
					sound_levelStop();
					agent_levelEndTask();
					lmusic_reset();	// Fix a Dark Forces bug where music won't play when entering a cutscene again without restarting.
					pda_cleanup();

					// Reset
					TFE_Jedi::renderer_setType(RENDERER_SOFTWARE);
					TFE_Jedi::render_setResolution();
					TFE_Jedi::renderer_setLimits();

					// TFE
					reticle_enable(false);

					if (!s_levelComplete)
					{
						s_runGameState.abortLevel = JTRUE;
						s_runGameState.cutsceneIndex--;
					}
					else
					{
						s_runGameState.cutsceneIndex++;
						handleLevelComplete();
					}
					
					startNextMode();

					region_clear(s_levelRegion);
					bitmap_clearLevelData();
					bitmap_setAllocator(s_gameRegion);
					level_freeAllAssets();
					TFE_A11Y::clearActiveCaptions();
				}
			} break;
		}
	}

	void loadCutsceneList()
	{
		s_sharedState.cutsceneList = cutsceneList_load("cutscene.lst");
		cutscene_init(s_sharedState.cutsceneList);
	}
	
	void freeAllMidi()
	{
		gameMusic_stop();
	}

	void pauseLevelSound()
	{
		TFE_MidiPlayer::pause();
		TFE_Audio::pause();
	}

	void resumeLevelSound()
	{
		TFE_MidiPlayer::resume();
		TFE_Audio::resume();
	}

	void clearBufferedSound()
	{
		TFE_Audio::bufferedAudioClear();
	}

	void startNextMode()
	{
		if (s_invalidLevelIndex || s_runGameState.abortLevel)
		{
			s_runGameState.state = GSTATE_AGENT_MENU;
			return;
		}

		GameMode mode = s_cutsceneData[s_runGameState.cutsceneIndex].nextGameMode;
		switch (mode)
		{
			case GMODE_END:
			{
				s_runGameState.cutsceneIndex = 0;
				s_invalidLevelIndex = JTRUE;
				startNextMode();
			} break;
			case GMODE_CUTSCENE:
			{
				if (s_runGameState.cutscenesEnabled && cutscene_play(s_cutsceneData[s_runGameState.cutsceneIndex].cutscene))
				{
					s_runGameState.state = GSTATE_CUTSCENE;
				}
				else
				{
					s_runGameState.cutsceneIndex++;
					startNextMode();
				}
			} break;
			case GMODE_BRIEFING:
			{
				BriefingInfo* brief = nullptr;
				if (s_runGameState.cutscenesEnabled)
				{
					const char* levelName = agent_getLevelName();
					s32 briefingIndex = 0;
					for (s32 i = 0; i < s_sharedState.briefingList.count; i++)
					{
						if (strcasecmp(levelName, s_sharedState.briefingList.briefing[i].mission) == 0)
						{
							briefingIndex = i;
							break;
						}
					}

					s32 skill = (s32)s_agentData[s_agentId].difficulty;
					brief = &s_sharedState.briefingList.briefing[briefingIndex];
					if (brief)
					{
						missionBriefing_start(brief->archive, brief->bgAnim, levelName, brief->palette, skill, &s_sharedState.langKeys);
						s_runGameState.state = GSTATE_BRIEFING;
					}
				}

				if (!brief)
				{
					s_runGameState.cutsceneIndex++;
					startNextMode();
				}
			}  break;
			case GMODE_MISSION:
			{
				sound_levelStart();

				bitmap_setAllocator(s_levelRegion);
				actor_clearState();

				task_reset();
				inf_clearState();
				s_sharedState.loadMissionTask = createTask("start mission", mission_startTaskFunc, JTRUE);
				mission_setLoadMissionTask(s_sharedState.loadMissionTask);

				s32 levelIndex = agent_getLevelIndex();
				gameMusic_start(levelIndex);

				agent_setLevelComplete(JFALSE);
				agent_readSavedDataForLevel(s_agentId, levelIndex);

				// The load mission task should begin immediately once the Task System updates,
				// so launchCurrentTask() is not required here.
				// In the original, the task system would simply loop here.
				s_runGameState.state = GSTATE_MISSION;
			}
		}
	}

	/////////////////////////////////////////////
	// Internal Implementation
	/////////////////////////////////////////////
	void printGameInfo()
	{
		TFE_System::logWrite(LOG_MSG, "Game", "Dark Forces Version: %d.%d (Build %d)", 1, 0, 1);
	}

	// Note: not all command line arguments have been brought over from the DOS version.
	// Many no longer make sense and in some cases will always be available (such as screenshots).
	void processCommandLineArgs(s32 argCount, const char* argv[], char* startLevel)
	{
		s_sharedState.customGobName[0] = 0;
		TFE_Settings::clearModSettings();

		for (s32 i = 0; i < argCount; i++)
		{
			const char* arg = argv[i];
			char c = arg[0];

			if (c == '-' || c == '/' || c == '+')
			{
				c = arg[1];
				if (c == 'c' || c == 'C')
				{
					enableCutscenes(arg[2] == '1' ? JTRUE : JFALSE);
				}
				else if ((c == 'l' || c == 'L') && arg[2])
				{
					strcpy(startLevel, arg + 2);
				}
				else if (c == 'u' || c == 'U')
				{
					loadCustomGob(arg + 2);
				}
			}
		}

		// TFE: Support drag and drop.
		if (argCount == 2)
		{
			const char* arg = argv[1];
			if (arg && arg[0] && arg[0] != '-')
			{
				const size_t len = strlen(arg);
				const char* ext = len > 3 ? &arg[len - 3] : nullptr;
				if (ext && strcasecmp(ext, "zip") == 0)
				{
					// Next check to see if the path is already in the local paths.
					char path[TFE_MAX_PATH];
					char fileName[TFE_MAX_PATH];
					size_t len = strlen(arg);
					size_t lastSlash = 0;
					for (size_t i = 0; i < len; i++)
					{
						if (arg[i] == '\\' || arg[i] == '/')
						{
							lastSlash = i;
						}
					}
					memcpy(path, arg, lastSlash + 1);
					memcpy(fileName, &arg[lastSlash + 1], len - lastSlash - 1);

					TFE_Paths::fixupPathAsDirectory(path);
					TFE_Paths::addAbsoluteSearchPath(path);
					TFE_System::logWrite(LOG_MSG, "DarkForces", "Drag and Drop Mod File: '%s'; Path: '%s'", fileName, path);
					
					loadCustomGob(fileName);
				}
			}
		}
	}

	void enableCutscenes(JBool enable)
	{
		s_runGameState.cutscenesEnabled = enable;
	}

	void setInitialLevel(const char* levelName)
	{
		s_runGameState.startLevel = 0;

		if (!levelName || levelName[0] == 0) { return; }
		s_runGameState.startLevel = agent_getLevelIndexFromName(levelName);
	}

	void loadCustomGob(const char* gobName)
	{
		TFE_Settings::loadCustomModSettings();
	}

	s32 loadLocalMessages()
	{
		if (s_runGameState.localMsgLoaded)
		{
			return 1;
		}

		return parseMessageFile(&s_sharedState.localMessages, "local.msg", 0);
	}

	void loadMapNumFont()
	{
		s_sharedState.mapNumFont = font_load("map-nums.fnt");
	}

	Font* getMapNumFont()
	{
		return s_sharedState.mapNumFont;
	}

	static void parseKey(GameMessages* msgs, u32 keyId, KeyboardCode* dest, KeyboardCode dflt)
	{
		GameMessage* m = getGameMessage(msgs, keyId);
		unsigned char c;

		*dest = dflt;
		if (m)
		{
			c = toupper(m->text[0]);
			if ((c >= 'A') && (c <= 'Z'))
			{
				*dest = (KeyboardCode)((u32)(KEY_A) + (c - 'A'));
			}
		}
	}

	static void loadLangHotkeys(void)
	{
		GameMessages msgs;

		parseMessageFile(&msgs, "HOTKEYS.MSG", 1);
		parseKey(&msgs, 160, &s_sharedState.langKeys.k_yes,   KEY_Y);
		parseKey(&msgs, 350, &s_sharedState.langKeys.k_quit,  KEY_Q);
		parseKey(&msgs, 330, &s_sharedState.langKeys.k_cont,  KEY_R);
		parseKey(&msgs, 340, &s_sharedState.langKeys.k_conf,  KEY_C);
		parseKey(&msgs, 110, &s_sharedState.langKeys.k_agdel, KEY_R);
		parseKey(&msgs, 130, &s_sharedState.langKeys.k_begin, KEY_B);
		parseKey(&msgs, 240, &s_sharedState.langKeys.k_easy,  KEY_E);
		parseKey(&msgs, 250, &s_sharedState.langKeys.k_med,   KEY_M);
		parseKey(&msgs, 260, &s_sharedState.langKeys.k_hard,  KEY_H);
		parseKey(&msgs, 230, &s_sharedState.langKeys.k_canc,  KEY_C);
	}
				
	void gameStartup()
	{
		hud_loadGraphics();
		hud_loadGameMessages();
		loadMapNumFont();
		inf_loadSounds();
		actor_loadSounds();
		item_loadData();
		player_init();
		actor_allocatePhysicsActorList();
		loadCutsceneList();
		projectile_startup();
		hitEffect_startup();
		weapon_startup();
		loadLangHotkeys();

		s_sharedState.swFont1 = font_load("SWFONT1.FNT");

		renderer_setVisionEffect(0);
		renderer_setupCameraLight(JFALSE, JFALSE);

		s_loadScreen = bitmap_load("WAIT.BM", 1, POOL_GAME);
		vpFile pal(VPATH_GAME, "WAIT.PAL", false);
		if (pal)
		{
			pal.read(s_loadingScreenPal, 768);
		}
		pal.close();

		weapon_enableAutomount(s_config.wpnAutoMount);
		s_sharedState.screenShotSndSrc = sound_load("SCRSHOT.VOC", SOUND_PRIORITY_HIGH0);
		sound_setBaseVolume(s_sharedState.screenShotSndSrc, 127);
	}

	void loadAgentAndLevelData()
	{
		agent_loadData();
		if (!agent_loadLevelList("JEDI.LVL"))
		{
			TFE_System::logWrite(LOG_ERROR, "DarkForcesMain", "Failed to load level list.");
		}
		if (!parseBriefingList(&s_sharedState.briefingList, "BRIEFING.LST"))
		{
			TFE_System::logWrite(LOG_ERROR, "DarkForcesMain", "Failed to load briefing list.");
		}

		parseMessageFile(&s_sharedState.hotKeyMessages, "HOTKEYS.MSG", 1);

		s_sharedState.diskErrorImg = bitmap_load("diskerr.bm", 0, POOL_GAME);
		if (!s_sharedState.diskErrorImg)
		{
			TFE_System::logWrite(LOG_ERROR, "DarkForcesMain", "Failed to load diskerr image.");
		}
	}
		
	void startMissionFromSave(s32 levelIndex)
	{
		// We have returned from the mission tasks.
		renderer_reset();
		gameMusic_stop();
		sound_levelStop();
		agent_levelEndTask();
		lmusic_reset();	// Fix a Dark Forces bug where music won't play when entering a cutscene again without restarting.
		pda_cleanup();
		reticle_enable(true);

		region_clear(s_levelRegion);
		bitmap_clearLevelData();
		level_freeAllAssets();

		// Next
		sound_levelStart();
		bitmap_setAllocator(s_levelRegion);
		actor_clearState();

		task_reset();
		inf_clearState();
		mission_setLoadingFromSave();	// This tells the mission system that this is loading from a save.
		s_sharedState.loadMissionTask = createTask("start mission", mission_startTaskFunc, JTRUE);
		mission_setLoadMissionTask(s_sharedState.loadMissionTask);
		gameMusic_start(levelIndex);

		s_runGameState.state = GSTATE_MISSION;
		mission_setupTasks();
	}

	static void serializeLoopState(vpFile* stream, DarkForces* game)
	{
		if (s_sharedState.gameStarted)
		{
			SERIALIZE(SaveVersionInit, s_runGameState.argCount, 0);
			for (s32 i = 0; i < s_runGameState.argCount; i++)
			{
				u32 length = 0;
				if (serialization_getMode() == SMODE_WRITE)
				{
					length = (u32)strlen(s_runGameState.args[i]);
				}
				SERIALIZE(SaveVersionInit, length, 0);
				if (serialization_getMode() == SMODE_READ)
				{
					s_runGameState.args[i] = (char*)game_alloc(length + 1);
				}
				SERIALIZE_BUF(SaveVersionInit, s_runGameState.args[i], length);
				s_runGameState.args[i][length] = 0;
			}

			SERIALIZE(SaveVersionInit, s_runGameState.cutscenesEnabled, JTRUE);
			SERIALIZE(SaveVersionInit, s_runGameState.localMsgLoaded, JFALSE);
			SERIALIZE(SaveVersionInit, s_runGameState.startLevel, 0);
			SERIALIZE(SaveVersionInit, s_runGameState.state, GSTATE_STARTUP_CUTSCENES);
			SERIALIZE(SaveVersionInit, s_runGameState.levelIndex, 0);
			SERIALIZE(SaveVersionInit, s_runGameState.cutsceneIndex, 0);
			SERIALIZE(SaveVersionInit, s_runGameState.abortLevel, 0);
		}
		else if (serialization_getMode() == SMODE_READ)  // We need to start the game.
		{
			game->runGame(0, nullptr, stream);
		}
	}

	static void serializeVersion(vpFile* stream)
	{
		SERIALIZE_VERSION(SaveVersionInit);
	}

	bool DarkForces::serializeGameState(vpFile* stream, const char* filename, bool writeState)
	{
		if (!stream) { return false; }
		if (writeState && filename)
		{
			// Write the save message.
			const char* msg = TFE_System::getMessage(TFE_MSG_SAVE);
			if (msg)
			{
				char fullMsg[TFE_MAX_PATH];
				sprintf(fullMsg, "%s [%s]", msg, filename);
				hud_sendTextMessage(fullMsg, 0);
			}
		}

		time_pause(JTRUE);
		if (writeState)
		{
			serialization_setMode(SMODE_WRITE);
		}
		else
		{
			serialization_setMode(SMODE_READ);
		}

		serializeVersion(stream);
		serializeLoopState(stream, this);
		agent_serialize(stream);
		time_serialize(stream);
		if (!writeState)
		{
			startMissionFromSave(agent_getLevelIndex());
		}
		sound_serializeLevelSounds(stream);
		random_serialize(stream);
		automap_serialize(stream);
		hitEffect_serializeTasks(stream);
		weapon_serialize(stream);
		mission_serializeColorMap(stream);
		level_serialize(stream);
		inf_serialize(stream);
		pickupLogic_serializeTasks(stream);
		mission_serialize(stream);

		if (!writeState)
		{
			agent_restartEndLevelTask();
		}

		time_pause(JFALSE);
		if (!writeState)
		{
			task_updateTime();
			mission_pause(JFALSE);
		}
		return true;
	}
	
	bool validateSourceData(const char *path)
	{
		fprintf(stderr, ">DF(%s)\n", path);
		bool ok = false;
		static const char * const testfiles[] = {
			"DARK.GOB", "SOUNDS.GOB", "SPRITES.GOB", "TEXTURES.GOB"
		};

		TFEMount m = vpMountReal(path, VPATH_TMP);
		if (!m) {
			ok = false;
			goto out;
		}

		ok = true;
		for (auto i : testfiles)
		{
			if (!vpFileExists(VPATH_TMP, i, false))
			{
				ok = false;
				break;
			}
		}
		vpUnmount(m);
out:
		fprintf(stderr, "<DF(%s) %d\n", path, ok);
		return ok;
	}

}