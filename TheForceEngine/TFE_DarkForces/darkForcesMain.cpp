#include <cstring>

#include "darkForcesMain.h"
#include "agent.h"
#include "config.h"
#include "briefingList.h"
#include "gameMessage.h"
#include "hud.h"
#include "item.h"
#include "mission.h"
#include "player.h"
#include "projectile.h"
#include "time.h"
#include "weapon.h"
#include "vueLogic.h"
#include "GameUI/menu.h"
#include "GameUI/agentMenu.h"
#include "GameUI/escapeMenu.h"
#include "GameUI/missionBriefing.h"
#include "GameUI/pda.h"
#include "Landru/lsystem.h"
#include <TFE_DarkForces/Landru/cutscene.h>
#include <TFE_DarkForces/Landru/cutsceneList.h>
#include <TFE_DarkForces/Actor/actor.h>
#include <TFE_Memory/memoryRegion.h>
#include <TFE_System/system.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_Audio/midiPlayer.h>
#include <TFE_Audio/audioSystem.h>
#include <TFE_Asset/gmidAsset.h>
#include <TFE_Archive/archive.h>
#include <TFE_Archive/zipArchive.h>
#include <TFE_Archive/gobMemoryArchive.h>
#include <TFE_Jedi/Level/rfont.h>
#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/InfSystem/infSystem.h>
#include <TFE_Jedi/Task/task.h>
#include <TFE_Jedi/Renderer/jediRenderer.h>
#include <TFE_Jedi/Task/task.h>
#include <assert.h>

// Debugging
#include <TFE_DarkForces/Actor/actorDebug.h>

using namespace TFE_Memory;

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

	enum GameConstants
	{
		MAX_MOD_LFD = 16,
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
	static BriefingList s_briefingList = { 0 };

	static const GMidiAsset* s_levelStalk;
	static const GMidiAsset* s_levelFight;
	static const GMidiAsset* s_levelBoss;

	static Task* s_loadMissionTask = nullptr;
	static CutsceneState* s_cutsceneList = nullptr;

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
	void startNextMode();
	void disableLevelMusic();
	void freeAllMidi();
	void pauseLevelSound();
	void resumeLevelSound();
	void startLevelMusic(s32 levelIndex);

	/////////////////////////////////////////////
	// API
	/////////////////////////////////////////////
		
	// This is the equivalent of the initial part of main() in Dark Forces DOS.
	// This part loads and sets up the game.
	bool DarkForces::runGame(s32 argCount, const char* argv[])
	{
		bitmap_setAllocator(s_gameRegion);

		printGameInfo();
		buildSearchPaths();
		processCommandLineArgs(argCount, argv);
		loadLocalMessages();
		openGobFiles();
		TFE_Jedi::task_setDefaults();
		TFE_Jedi::task_setMinStepInterval(1.0f / f32(TICKS_PER_SECOND));
		TFE_Jedi::setupInitCameraAndLights();
		config_startup();
		gameStartup();
		loadAgentAndLevelData();
		lsystem_init();

		renderer_init();
		
		// TFE Specific
		actorDebug_init();

		return true;
	}
		
	void DarkForces::exitGame()
	{
		freeAllMidi();

		s_localMessages.count = 0;
		s_localMessages.msgList = nullptr;
		s_hotKeyMessages.count = 0;
		s_hotKeyMessages.msgList = nullptr;
		gameMessage_freeBuffer();
		briefingList_freeBuffer();
		cutsceneList_freeBuffer();
		lsystem_destroy();

		// Clear paths and archives.
		TFE_Paths::clearSearchPaths();
		TFE_Paths::clearLocalArchives();
		task_shutdown();

		config_shutdown();

		// TFE Specific
		// Reset state
		s_state = GSTATE_STARTUP_CUTSCENES;
		s_localMsgLoaded = JFALSE;
		s_hudModeStd = JTRUE;
		s_screenShotSndSrc = NULL_SOUND;
		s_loadMissionTask = nullptr;
		weapon_resetState();
		renderer_resetState();
		sound_freeAll();
		level_freeAllAssets();
		agentMenu_resetState();
		menu_resetState();
		pda_resetState();
		escapeMenu_resetState();
		vue_resetState();
		lsystem_destroy();
		// Free debug data
		actorDebug_free();
	}

	void DarkForces::pauseGame(bool pause)
	{
		mission_pause(pause ? JTRUE : JFALSE);
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
				cutscene_play(10);
				s_state = GSTATE_CUTSCENE;
				s_invalidLevelIndex = JTRUE;
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
				if (!cutscene_update())
				{
					s_cutsceneIndex++;
					if (s_cutsceneData[s_cutsceneIndex].nextGameMode == GMODE_END)
					{
						s_state = GSTATE_AGENT_MENU;
						s_invalidLevelIndex = JTRUE;
					}
					else
					{
						startNextMode();
					}
				}
			} break;
			case GSTATE_BRIEFING:
			{
				s32 skill;
				JBool abort;
				if (!missionBriefing_update(&skill, &abort))
				{
					missionBriefing_cleanup();
					TFE_Input::clearAccumulatedMouseMove();

					if (abort)
					{
						s_invalidLevelIndex = JTRUE;
						s_cutsceneIndex--;
					}
					else
					{
						s_agentData[s_agentId].difficulty = skill;
						s_cutsceneIndex++;
					}
					startNextMode();
				}
			} break;
			case GSTATE_MISSION:
			{
				// At this point the mission has already been launched.
				// The task system will take over. Basically every frame we just check to see if there are any tasks running.
				if (!task_getCount())
				{
					// We have returned from the mission tasks.
					renderer_reset();
					disableLevelMusic();
					sound_stopAll();
					agent_levelEndTask();
					pda_cleanup();

					if (!s_levelComplete)
					{
						s_abortLevel = JTRUE;
						s_cutsceneIndex--;
					}
					else
					{
						s_cutsceneIndex++;
						s32 completedLevelIndex = agent_getLevelIndex();
						u8 diff = s_agentData[s_agentId].difficulty;

						// Save the level completion, inventory and other stats into the agent data and then save to disk.
						agent_saveLevelCompletion(diff, completedLevelIndex);
						agent_updateAgentSavedData();
						agent_saveInventory(s_agentId, completedLevelIndex + 1);
						agent_setNextLevelByIndex(completedLevelIndex + 1);
					}
					
					startNextMode();

					region_clear(s_levelRegion);
					region_clear(s_resRegion);
					bitmap_setAllocator(s_gameRegion);
				}
			} break;
		}
	}

	void loadCutsceneList()
	{
		s_cutsceneList = cutsceneList_load("cutscene.lst");
		cutscene_init(s_cutsceneList);
	}
	
	void disableLevelMusic()
	{
		TFE_MidiPlayer::stop();
	}

	void freeAllMidi()
	{
		disableLevelMusic();
		TFE_GmidAsset::freeAll();
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

	void startLevelMusic(s32 levelIndex)
	{
		char stalkTrackName[64];
		char fightTrackName[64];
		char bossTrackName[64];
		sprintf(stalkTrackName, "STALK-%02d.GMD", levelIndex);
		sprintf(fightTrackName, "FIGHT-%02d.GMD", levelIndex);
		sprintf(bossTrackName, "BOSS-%02d.GMD", levelIndex);

		s_levelStalk = TFE_GmidAsset::get(stalkTrackName);
		s_levelFight = TFE_GmidAsset::get(fightTrackName);
		s_levelBoss = TFE_GmidAsset::get(bossTrackName);
		TFE_MidiPlayer::playSong(s_levelStalk, true);
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
			case GMODE_END:
			{
				s_cutsceneIndex = 0;
				s_invalidLevelIndex = JTRUE;
				startNextMode();
			} break;
			case GMODE_CUTSCENE:
			{
				if (cutscene_play(s_cutsceneData[s_cutsceneIndex].cutscene))
				{
					s_state = GSTATE_CUTSCENE;
				}
				else
				{
					s_cutsceneIndex++;
					startNextMode();
				}
			} break;
			case GMODE_BRIEFING:
			{
				// TODO: Check to see if cutscenes are disabled, if so we also skip the mission briefing.
				const char* levelName = agent_getLevelName();
				s32 briefingIndex = 0;
				for (s32 i = 0; i < s_briefingList.count; i++)
				{
					if (strcasecmp(levelName, s_briefingList.briefing[i].mission) == 0)
					{
						briefingIndex = i;
						break;
					}
				}

				s32 skill = (s32)s_agentData[s_agentId].difficulty;
				BriefingInfo* brief = &s_briefingList.briefing[briefingIndex];
				missionBriefing_start(brief->archive, brief->bgAnim, levelName, brief->palette, skill);

				s_state = GSTATE_BRIEFING;
			}  break;
			case GMODE_MISSION:
			{
				bitmap_setAllocator(s_resRegion);
				actor_clearState();
				actorDebug_clear();

				task_reset();
				inf_clearState();
				s_loadMissionTask = createTask("start mission", mission_startTaskFunc, JTRUE);
				mission_setLoadMissionTask(s_loadMissionTask);

				disableLevelMusic();
				s32 levelIndex = agent_getLevelIndex();
				startLevelMusic(levelIndex);

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
		s_cutscenesEnabled = enable;
	}

	void loadCustomGob(const char* gobName)
	{
		FilePath archivePath;
		s32 lfdIndex[MAX_MOD_LFD];
		char lfdName[MAX_MOD_LFD][TFE_MAX_PATH];
		char briefingName[TFE_MAX_PATH];
		s32 lfdCount = 0;
		s32 briefingIndex = -1;

		if (TFE_Paths::getFilePath(gobName, &archivePath))
		{
			// Is this really a gob?
			const size_t len = strlen(gobName);
			const char* ext = &gobName[len - 3];
			if (strcasecmp(ext, "zip") == 0 || strcasecmp(ext, "pk3") == 0)
			{
				// In the case of a zip file, we want to extract the GOB into an in-memory format and use that directly.
				ZipArchive zipArchive;
				if (zipArchive.open(archivePath.path))
				{
					s32 gobIndex = -1;
					const u32 count = zipArchive.getFileCount();
					for (u32 i = 0; i < count; i++)
					{
						const char* name = zipArchive.getFileName(i);
						const size_t nameLen = strlen(name);
						const char* zext = &name[nameLen - 3];
						if (strcasecmp(zext, "gob") == 0)
						{
							gobIndex = i;
						}
						else if (strcasecmp(zext, "lfd") == 0 && lfdCount < MAX_MOD_LFD)
						{
							if (strstr(name, "brief") || strstr(name, "BRIEF"))
							{
								briefingIndex = i;
							}
							else
							{
								lfdIndex[lfdCount++] = i;
							}
						}
					}

					// If there is only 1 LFD, assume it is mission briefings.
					if (lfdCount == 1 && briefingIndex < 0)
					{
						briefingIndex = lfdIndex[0];
						lfdCount = 0;
					}

					if (gobIndex >= 0)
					{
						u32 bufferLen = (u32)zipArchive.getFileLength(gobIndex);
						u8* buffer = (u8*)malloc(bufferLen);
						zipArchive.openFile(gobIndex);
						zipArchive.readFile(buffer, bufferLen);
						zipArchive.closeFile();

						GobMemoryArchive* gobArchive = new GobMemoryArchive();
						gobArchive->open(buffer, bufferLen);
						TFE_Paths::addLocalArchive(gobArchive);
					}

					char tempPath[TFE_MAX_PATH];
					sprintf(tempPath, "%sTemp/", TFE_Paths::getPath(PATH_PROGRAM_DATA));
					// Extract and copy the briefing.
					if (briefingIndex >= 0)
					{
						u32 bufferLen = (u32)zipArchive.getFileLength(briefingIndex);
						u8* buffer = (u8*)malloc(bufferLen);
						zipArchive.openFile(briefingIndex);
						zipArchive.readFile(buffer, bufferLen);
						zipArchive.closeFile();

						char lfdPath[TFE_MAX_PATH];
						sprintf(lfdPath, "%sdfbrief.lfd", tempPath);
						FileStream file;
						if (file.open(lfdPath, FileStream::MODE_WRITE))
						{
							file.writeBuffer(buffer, bufferLen);
							file.close();
						}
						free(buffer);

						TFE_Paths::addSingleFilePath("dfbrief.lfd", lfdPath);
					}
					// Extract and copy the LFD.
					for (s32 i = 0; i < lfdCount; i++)
					{
						u32 bufferLen = (u32)zipArchive.getFileLength(lfdIndex[i]);
						u8* buffer = (u8*)malloc(bufferLen);
						zipArchive.openFile(lfdIndex[i]);
						zipArchive.readFile(buffer, bufferLen);
						zipArchive.closeFile();

						char lfdPath[TFE_MAX_PATH];
						sprintf(lfdPath, "%scutscenes%d.lfd", tempPath, i);
						FileStream file;
						if (file.open(lfdPath, FileStream::MODE_WRITE))
						{
							file.writeBuffer(buffer, bufferLen);
							file.close();
						}
						free(buffer);

						TFE_Paths::addSingleFilePath(zipArchive.getFileName(lfdIndex[i]), lfdPath);
					}

					zipArchive.close();
				}
			}
			else
			{
				Archive* archive = Archive::getArchive(ARCHIVE_GOB, gobName, archivePath.path);
				if (archive)
				{
					TFE_Paths::addLocalArchive(archive);

					// Handle LFD files.
					char modPath[TFE_MAX_PATH];
					FileUtil::getFilePath(archivePath.path, modPath);

					// Look for LFD files.
					lfdCount = 0;
					briefingIndex = -1;

					FileList fileList;
					FileUtil::readDirectory(modPath, "lfd", fileList);
					const size_t count = fileList.size();
					const std::string* file = fileList.data();
															
					for (size_t i = 0; i < count; i++, file++)
					{
						const size_t len = file->length();
						const char* name = file->c_str();

						if (lfdCount < 16)
						{
							if (strstr(name, "brief") || strstr(name, "BRIEF"))
							{
								briefingIndex = s32(i);
								strcpy(briefingName, name);
							}
							else if (lfdCount < MAX_MOD_LFD)
							{
								strcpy(lfdName[lfdCount], name);
								lfdCount++;
							}
						}
					}

					// If there is only 1 LFD, assume it is mission briefings.
					if (lfdCount == 1 && briefingIndex < 0)
					{
						strcpy(briefingName, lfdName[0]);
						briefingIndex = 0;
						lfdCount = 0;
					}

					// Extract and copy the briefing.
					char lfdPath[TFE_MAX_PATH];
					if (briefingIndex >= 0)
					{
						sprintf(lfdPath, "%s%s", modPath, briefingName);
						TFE_Paths::addSingleFilePath("dfbrief.lfd", lfdPath);
					}

					// Extract and copy the LFD.
					for (s32 i = 0; i < lfdCount; i++)
					{
						sprintf(lfdPath, "%s%s", modPath, lfdName[i]);
						TFE_Paths::addSingleFilePath(lfdName[i], lfdPath);
					}
				}
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
		s_mapNumFont = nullptr;
		if (TFE_Paths::getFilePath("map-nums.fnt", &filePath))
		{
			s_mapNumFont = font_load(&filePath);
		}
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
			s_loadScreen = TFE_Jedi::bitmap_load(&filePath, 1);
		}
		if (TFE_Paths::getFilePath("wait.pal", &filePath))
		{
			FileStream::readContents(&filePath, s_loadingScreenPal, 768);
		}
		if (TFE_Paths::getFilePath("secbase.pal", &filePath))
		{
			FileStream::readContents(&filePath, s_escMenuPalette, 768);
		}

		weapon_enableAutomount(s_config.wpnAutoMount);
		s_screenShotSndSrc = sound_Load("scrshot.voc");
		setSoundSourceVolume(s_screenShotSndSrc, 127);
	}

	void loadAgentAndLevelData()
	{
		agent_loadData();
		if (!agent_loadLevelList("jedi.lvl"))
		{
			TFE_System::logWrite(LOG_ERROR, "DarkForcesMain", "Failed to load level list.");
		}
		if (!parseBriefingList(&s_briefingList, "briefing.lst"))
		{
			TFE_System::logWrite(LOG_ERROR, "DarkForcesMain", "Failed to load briefing list.");
		}

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