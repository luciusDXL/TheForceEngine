#include "darkForcesMain.h"
#include "actor.h"
#include "gameMessage.h"
#include "hud.h"
#include "item.h"
#include <TFE_System/system.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_Archive/archive.h>
#include <TFE_Jedi/Level/rfont.h>
#include <TFE_Jedi/InfSystem/infSystem.h>
#include <TFE_Jedi/Task/task.h>
#include <TFE_Jedi/Renderer/jediRenderer.h>
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

	/////////////////////////////////////////////
	// Internal State
	/////////////////////////////////////////////
	static JBool s_cutscenesEnabled = JTRUE;
	static JBool s_localMsgLoaded = JFALSE;
	static JBool s_useJediPath = JFALSE;
	static JBool s_hudModeStd = JTRUE;
	static const char* s_launchLevelName = nullptr;
	static GameMessages s_localMessages = { 0 };
	static u8 s_loadingScreenPal[768];
	static TextureData* s_loadScreen = nullptr;
	static Font* s_swFont1 = nullptr;
	static Font* s_mapNumFont = nullptr;

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
		TFE_Jedi::setTaskDefaults();
		TFE_Jedi::setupInitCameraAndLights();
		gameStartup();

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
	}

	void DarkForces::loopGame()
	{
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
		
	void gameStartup()
	{
		hud_loadGraphics();
		hud_loadGameMessages();
		loadMapNumFont();
		inf_loadSounds();
		actor_loadSounds();
		item_loadData();
		//initPlayer();
		inf_loadDefaultSwitchSound();
		//allocatePhysicsActorList();
		//loadCutsceneList();
		//loadProjectiles();
		//loadHitEffects();
		//weaponStartup();

		FilePath filePath;
		TFE_Paths::getFilePath("swfont1.fnt", &filePath);
		s_swFont1 = font_load(&filePath);

		//setVisionEffect(0);
		//setupHeadlamp(s_actionEnableFlatShading, s_headlampActive);

		if (TFE_Paths::getFilePath("wait.bm", &filePath))
		{
			s_loadScreen = TFE_Jedi::bitmap_load(&filePath, 0);
		}
		if (TFE_Paths::getFilePath("wait.pal", &filePath))
		{
			FileStream::readContents(&filePath, s_loadingScreenPal, 768);
		}
		//enableAutomount(s_config.wpnAutoMount);
		//s_screenShotSndSrc = sound_Load("scrshot.voc");
		//setSoundEffectVolume(s_screenShotSndSrc, 127);
	}
}