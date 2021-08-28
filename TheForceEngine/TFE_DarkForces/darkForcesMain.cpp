#include "darkForcesMain.h"
#include "actor.h"
#include "agent.h"
#include "gameMessage.h"
#include "hud.h"
#include "item.h"
#include "mission.h"
#include "player.h"
#include "projectile.h"
#include "time.h"
#include "weapon.h"
#include "GameUI/agentMenu.h"
#include <TFE_System/system.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_Archive/archive.h>
#include <TFE_Jedi/Level/rfont.h>
#include <TFE_Jedi/InfSystem/infSystem.h>
#include <TFE_Jedi/Task/task.h>
#include <TFE_Jedi/Renderer/jediRenderer.h>
#include <TFE_Jedi/Task/task.h>
#include <assert.h>

namespace TFE_DarkForces
{
	/////////////////////////////////////////////
	// Constants
	/////////////////////////////////////////////
	static const char* c_gobFileNames[] =
	{
		"DARK.GOB",
		"SOUNDS.GOB",
		"TEXTURES.GOB",
		"SPRITES.GOB",
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
		GMODE_ERROR = -1,
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
		
	// For this release, cutscenes and mission briefings are not supported - so just hack together the cutscene list.
	// This is normally read from disk and controls the flow between missions.
	static CutsceneData s_cutsceneData[] =
	{
		{1,  GMODE_MISSION, 0},
		{2,  GMODE_MISSION, 0},
		{3,  GMODE_MISSION, 0},
		{4,  GMODE_MISSION, 0},
		{5,  GMODE_MISSION, 0},
		{6,  GMODE_MISSION, 0},
		{7,  GMODE_MISSION, 0},
		{8,  GMODE_MISSION, 0},
		{9,  GMODE_MISSION, 0},
		{10, GMODE_MISSION, 0},
		{11, GMODE_MISSION, 0},
		{12, GMODE_MISSION, 0},
		{13, GMODE_MISSION, 0},
		{14, GMODE_MISSION, 0}
	};
	
	/////////////////////////////////////////////
	// Internal State
	/////////////////////////////////////////////
	static JBool s_cutscenesEnabled = JTRUE;
	static JBool s_localMsgLoaded = JFALSE;
	static JBool s_useJediPath = JFALSE;
	static JBool s_hudModeStd = JTRUE;
	static const char* s_launchLevelName = nullptr;
	static GameMessages s_localMessages;
	static GameMessages s_hotKeyMessages;
	static TextureData* s_diskErrorImg = nullptr;
	static Font* s_swFont1 = nullptr;
	static Font* s_mapNumFont = nullptr;
	static SoundSourceID s_screenShotSndSrc = NULL_SOUND;
	static void* s_briefingList;	// STUBBED - to be replaced by the real structure.

	static Task* s_loadMissionTask = nullptr;

	static GameState s_state = GSTATE_STARTUP_CUTSCENES;
	static s32 s_levelIndex;
	static s32 s_cutsceneIndex;
	static JBool s_abortLevel;
		
	/////////////////////////////////////////////
	// Forward Declarations
	/////////////////////////////////////////////
	void printGameInfo();
	void processCommandLineArgs(s32 argCount, const char* argv[]);
	void enableCutscenes(JBool enable);
	void loadCustomGob(const char* gobName);
	s32  loadLocalMessages();
	void buildSearchPaths();
	void openGobFiles();
	void gameStartup();
	void loadAgentAndLevelData();
	void cutscenes_startup(s32 id);
	void startNextMode();

	/////////////////////////////////////////////
	// API
	/////////////////////////////////////////////
		
	// This is the equivalent of the initial part of main() in Dark Forces DOS.
	// This part loads and sets up the game.
	bool DarkForces::runGame(s32 argCount, const char* argv[])
	{
		printGameInfo();
		buildSearchPaths();
		processCommandLineArgs(argCount, argv);
		loadLocalMessages();
		openGobFiles();
		TFE_Jedi::task_setDefaults();
		TFE_Jedi::setupInitCameraAndLights();
		gameStartup();
		loadAgentAndLevelData();
		
		return true;
	}

	void DarkForces::exitGame()
	{
		const s32 count = s_localMessages.count;
		GameMessage* msg = s_localMessages.msgList;
		for (s32 i = 0; i < count; i++, msg++)
		{
			free(msg->text);
			msg->text = nullptr;
		}
		gameMessage_freeBuffer();

		// Clear paths and archives.
		TFE_Paths::clearSearchPaths();
		TFE_Paths::clearLocalArchives();
		task_shutdown();
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

		switch (s_state)
		{
			case GSTATE_STARTUP_CUTSCENES:
			{
				cutscenes_startup(10);
				s_invalidLevelIndex = JFALSE;
			} break;
			case GSTATE_AGENT_MENU:
			{
				if (!agentMenu_update(&s_levelIndex))
				{
					agent_updateAgentSavedData();
				
					s_invalidLevelIndex = JTRUE;
					for (s32 i = 0; i < TFE_ARRAYSIZE(s_cutsceneData); i++)
					{
						if (s_cutsceneData[i].levelIndex >= 0 && s_cutsceneData[i].levelIndex == s_levelIndex)
						{
							s_cutsceneIndex = i;
							s_invalidLevelIndex = JFALSE;
							break;
						}
					}

					s_abortLevel = JFALSE;
					agent_setNextLevelByIndex(s_levelIndex);
					startNextMode();
				}
			} break;
			case GSTATE_CUTSCENE:
			{
				// STUB
				startNextMode();
			} break;
			case GSTATE_BRIEFING:
			{
				// STUB
				startNextMode();
			} break;
			case GSTATE_MISSION:
			{
				// At this point the mission has already been launched.
				// The task system will take over. Basically every frame we just check to see if there are any tasks running.
				if (!task_getCount())
				{
					// Temp:
					s_abortLevel = JTRUE;

					// We have returned from the mission tasks.
					// disableLevelMusic();

					//if (!s_levelComplete)
					//{
					//	s_abortLevel = JTRUE;
					//}
					//else
					//{
					//	s_cutsceneIndex++;
					//	s32 completedLevelIndex = level_getIndex();
					//	u8 diff = level_getDifficulty();

						// Save the level completion, inventory and other stats into the agent data and then save to disk.
					//	agent_saveLevelCompletion(diff, completedLevelIndex);
					//	agent_updateSavedData();
					//	agent_saveInventory(s_agentId, completedLevelIndex + 1);
					//	agent_setNextLevelByIndex(completedLevelIndex + 1);
					//}

					startNextMode();
				}
			} break;
		}
	}

	void startNextMode()
	{
		if (s_invalidLevelIndex || s_abortLevel)
		{
			s_state = GSTATE_AGENT_MENU;
			return;
		}

		GameMode mode = s_cutsceneData[s_cutsceneIndex].nextGameMode;
		switch (mode)
		{
			case GMODE_ERROR:
			{
				// STUB
				// Error Handling.
			} break;
			case GMODE_CUTSCENE:
			{
				// STUB
				// cutscenes_startup(s_cutsceneData[s_cutsceneIndex].cutscene);
				// This will also change s_state -> GSTATE_CUTSCENE
			} break;
			case GMODE_BRIEFING:
			{
				// STUB
				// This will also change s_state -> GSTATE_BRIEFING
			}  break;
			case GMODE_MISSION:
			{
				s_loadMissionTask = pushTask(mission_startTaskFunc, JTRUE);

				// disableLevelMusic();
				s32 levelIndex = agent_getLevelIndex();
				// startLevelMusic(levelIndex);

				agent_setLevelComplete(JFALSE);
				agent_readSavedDataForLevel(s_agentId, levelIndex);

				// The load mission task should begin immediately once the Task System updates,
				// so launchCurrentTask() is not required here.
				// In the original, the task system would simply loop here.
				s_state = GSTATE_MISSION;
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
	void processCommandLineArgs(s32 argCount, const char* argv[])
	{
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
					s_launchLevelName = arg + 2;
				}
				else if (c == 'u' || c == 'U')
				{
					loadCustomGob(arg + 2);
				}
			}
			
		}
	}

	void enableCutscenes(JBool enable)
	{
		s_cutscenesEnabled = enable;
	}

	void loadCustomGob(const char* gobName)
	{
		FilePath archivePath;
		if (TFE_Paths::getFilePath(gobName, &archivePath))
		{
			Archive* archive = Archive::getArchive(ARCHIVE_GOB, gobName, archivePath.path);
			if (archive)
			{
				TFE_Paths::addLocalArchive(archive);
			}
		}
	}

	s32 loadLocalMessages()
	{
		if (s_localMsgLoaded)
		{
			return 1;
		}

		FilePath path;
		TFE_Paths::getFilePath("local.msg", &path);
		return parseMessageFile(&s_localMessages, &path, 0);
	}

	void buildSearchPaths()
	{
		TFE_Paths::addLocalSearchPath("");
		TFE_Paths::addLocalSearchPath("LFD/");
		// Dark Forces also adds C:/ and C:/LFD but TFE won't be doing that for obvious reasons...
		
		// Add some extra directories, if they exist.
		// Obviously these were not in the original code.
		TFE_Paths::addLocalSearchPath("Mods/");

		// Add Mods/ paths to the program data directory and local executable directory.
		// Note only directories that exist are actually added.
		const char* programData = TFE_Paths::getPath(PATH_PROGRAM_DATA);
		const char* programDir = TFE_Paths::getPath(PATH_PROGRAM);
		char path[TFE_MAX_PATH];
		
		sprintf(path, "%sMods/", programData);
		TFE_Paths::addAbsoluteSearchPath(path);

		sprintf(path, "%sMods/", programDir);
		TFE_Paths::addAbsoluteSearchPath(path);
	}

	void openGobFiles()
	{
		for (s32 i = 0; i < TFE_ARRAYSIZE(c_gobFileNames); i++)
		{
			FilePath archivePath;
			if (TFE_Paths::getFilePath(c_gobFileNames[i], &archivePath))
			{
				assert(archivePath.path[0] != 0);
				Archive* archive = Archive::getArchive(ARCHIVE_GOB, c_gobFileNames[i], archivePath.path);
				if (archive)
				{
					TFE_Paths::addLocalArchive(archive);
				}
			}
			else
			{
				TFE_System::logWrite(LOG_ERROR, "Dark Forces Main", "Cannot find required game data - '%s'.", c_gobFileNames[i]);
			}
		}
	}
		
	void loadMapNumFont()
	{
		FilePath filePath;
		if (TFE_Paths::getFilePath("map-nums.fnt", &filePath))
		{
			s_mapNumFont = font_load(&filePath);
		}
	}

	void loadCutsceneList()
	{
		// STUBBED
		// This will be handled when cutscenes are integrated.
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
		inf_loadDefaultSwitchSound();
		actor_allocatePhysicsActorList();
		loadCutsceneList();
		projectile_startup();
		hitEffect_startup();
		weapon_startup();

		FilePath filePath;
		TFE_Paths::getFilePath("swfont1.fnt", &filePath);
		s_swFont1 = font_load(&filePath);

		renderer_setVisionEffect(0);
		renderer_setupCameraLight(JFALSE, JFALSE);

		if (TFE_Paths::getFilePath("wait.bm", &filePath))
		{
			s_loadScreen = TFE_Jedi::bitmap_load(&filePath, 0);
		}
		if (TFE_Paths::getFilePath("wait.pal", &filePath))
		{
			FileStream::readContents(&filePath, s_loadingScreenPal, 768);
		}

		weapon_enableAutomount(JFALSE);	// TODO: This should come from the config file, with things like key bindings, etc.
		s_screenShotSndSrc = sound_Load("scrshot.voc");
		setSoundSourceVolume(s_screenShotSndSrc, 127);
	}

	void* loadBriefingList(const char* fileName)
	{
		// STUB
		// TODO in the following releases.
		return nullptr;
	}

	void cutscenes_startup(s32 id)
	{
		// STUB
		// TODO in the following releases.

		s_state = GSTATE_AGENT_MENU;
	}
		
	void loadAgentAndLevelData()
	{
		agent_loadData();
		if (!agent_loadLevelList("jedi.lvl"))
		{
			TFE_System::logWrite(LOG_ERROR, "DarkForcesMain", "Failed to load level list.");
		}
		s_briefingList = loadBriefingList("briefing.lst");

		FilePath filePath;
		if (TFE_Paths::getFilePath("hotkeys.msg", &filePath))
		{
			parseMessageFile(&s_hotKeyMessages, &filePath, 1);
		}
		if (TFE_Paths::getFilePath("diskerr.bm", &filePath))
		{
			s_diskErrorImg = bitmap_load(&filePath, 0);
		}
		if (!s_diskErrorImg)
		{
			TFE_System::logWrite(LOG_ERROR, "DarkForcesMain", "Failed to load diskerr image.");
		}
	}
}