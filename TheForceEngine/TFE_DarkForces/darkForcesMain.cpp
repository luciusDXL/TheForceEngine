#include "darkForcesMain.h"
#include "gameMessage.h"
#include <TFE_System/system.h>
#include <TFE_FileSystem/paths.h>

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
	static const char* s_launchLevelName = nullptr;
	static GameMessages s_localMessages = { 0 };

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

	/////////////////////////////////////////////
	// API
	/////////////////////////////////////////////
		
	// This is the equivalent of the initial part of main() in Dark Forces DOS.
	// This part loads and sets up the game.
	bool DarkForces::runGame(s32 argCount, const char* argv[])
	{
		printGameInfo();
		processCommandLineArgs(argCount, argv);
		loadLocalMessages();
		buildSearchPaths();
		openGobFiles();


		
		return true;
	}

	void DarkForces::exitGame()
	{
		s32 count = s_localMessages.count;
		GameMessage* msg = s_localMessages.msgList;
		for (s32 i = 0; i < count; i++, msg++)
		{
			free(msg->text);
			msg->text = nullptr;
		}
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
	// Many no longer make sense and in some cases will also be available (such as screenshots).
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
		// TODO
		// Add custom GOB to list of archives.
	}

	s32 loadLocalMessages()
	{
		if (s_localMsgLoaded)
		{
			return 1;
		}

		char path[TFE_MAX_PATH];
		TFE_Paths::appendPath(PATH_SOURCE_DATA, "local.msg", path);
		return parseMessageFile(&s_localMessages, path, 0);
	}

	void buildSearchPaths()
	{
		// Add PATH_SOURCE_DATA
		// Add PATH_SOURCE_DATA/LFD
		// Add GOBS to list of archives.
	}

	void openGobFiles()
	{
		for (s32 i = 0; i < 4; i++)
		{
			// getFilePath(c_gobFileNames[i], path);
			// add GOB
		}
	}
}