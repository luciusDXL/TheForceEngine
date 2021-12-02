// main.cpp : Defines the entry point for the application.
#include "version.h"
#include <SDL.h>
#include <TFE_System/types.h>
#include <TFE_System/profiler.h>
#include <TFE_Memory/memoryRegion.h>
#include <TFE_Game/igame.h>
#include <TFE_Jedi/InfSystem/infSystem.h>
//#include <TFE_Editor/editor.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_Audio/audioSystem.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Polygon/polygon.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Input/inputMapping.h>
#include <TFE_Settings/settings.h>
#include <TFE_System/system.h>
#include <TFE_Jedi/Task/task.h>
#include <TFE_Asset/paletteAsset.h>
#include <TFE_Asset/imageAsset.h>
#include <TFE_Ui/ui.h>
#include <TFE_FrontEndUI/frontEndUi.h>
#include <algorithm>
#include <time.h>
#include <sys/types.h>
#include <sys/timeb.h>

// Replace with music system
#include <TFE_Audio/midiPlayer.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#ifdef min
#undef min
#undef max
#endif
#endif

#define PROGRAM_ERROR   1
#define PROGRAM_SUCCESS 0

#pragma comment(lib, "SDL2main.lib")
using namespace TFE_Input;

// Replace with settings.
static bool s_loop  = true;
static f32  s_refreshRate = 0;
static s32  s_displayIndex = 0;
static u32  s_baseWindowWidth = 1280;
static u32  s_baseWindowHeight = 720;
static u32  s_displayWidth = s_baseWindowWidth;
static u32  s_displayHeight = s_baseWindowHeight;
static u32  s_monitorWidth = 1280;
static u32  s_monitorHeight = 720;
static char s_screenshotTime[TFE_MAX_PATH];
static IGame* s_curGame = nullptr;

void parseOption(const char* name, const std::vector<const char*>& values, bool longName);

void handleEvent(SDL_Event& Event)
{
	TFE_Ui::setUiInput(&Event);
	TFE_Settings_Window* windowSettings = TFE_Settings::getWindowSettings();

	switch (Event.type)
	{
		case SDL_QUIT:
		{
			TFE_System::logWrite(LOG_MSG, "Main", "App Quit");
			s_loop = false;
		} break;
		case SDL_WINDOWEVENT:
		{
			if (Event.window.event == SDL_WINDOWEVENT_RESIZED || Event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
			{
				TFE_RenderBackend::resize(Event.window.data1, Event.window.data2);
			}
		} break;
		case SDL_CONTROLLERDEVICEADDED:
		{
			const s32 cIdx = Event.cdevice.which;
			if (SDL_IsGameController(cIdx))
			{
				SDL_GameController* controller = SDL_GameControllerOpen(cIdx);
				SDL_Joystick* j = SDL_GameControllerGetJoystick(controller);
				SDL_JoystickID joyId = SDL_JoystickInstanceID(j);

				//Save the joystick id to used in the future events
				SDL_GameControllerOpen(0);
			}
		} break;
		case SDL_MOUSEBUTTONDOWN:
		{
			TFE_Input::setMouseButtonDown(MouseButton(Event.button.button - SDL_BUTTON_LEFT));
		} break;
		case SDL_MOUSEBUTTONUP:
		{
			TFE_Input::setMouseButtonUp(MouseButton(Event.button.button - SDL_BUTTON_LEFT));
		} break;
		case SDL_MOUSEWHEEL:
		{
			TFE_Input::setMouseWheel(Event.wheel.x, Event.wheel.y);
		} break;
		case SDL_KEYDOWN:
		{
			if (Event.key.keysym.scancode && Event.key.repeat == 0)
			{
				TFE_Input::setKeyDown(KeyboardCode(Event.key.keysym.scancode));
			}

			if (Event.key.keysym.scancode)
			{
				TFE_Input::setBufferedKey(KeyboardCode(Event.key.keysym.scancode));
			}
		} break;
		case SDL_KEYUP:
		{
			if (Event.key.keysym.scancode)
			{
				const KeyboardCode code = KeyboardCode(Event.key.keysym.scancode);
				TFE_Input::setKeyUp(KeyboardCode(Event.key.keysym.scancode));

				// Fullscreen toggle.
				if (code == KeyboardCode::KEY_F11)
				{
					windowSettings->fullscreen = !windowSettings->fullscreen;
					TFE_RenderBackend::enableFullscreen(windowSettings->fullscreen);
				}
				else if (code == KeyboardCode::KEY_PRINTSCREEN)
				{
					static u64 _screenshotIndex = 0;

					char screenshotDir[TFE_MAX_PATH];
					TFE_Paths::appendPath(TFE_PathType::PATH_USER_DOCUMENTS, "Screenshots/", screenshotDir);
										
					char screenshotPath[TFE_MAX_PATH];
					sprintf(screenshotPath, "%stfe_screenshot_%s_%llu.jpg", screenshotDir, s_screenshotTime, _screenshotIndex);
					_screenshotIndex++;

					TFE_RenderBackend::queueScreenshot(screenshotPath);
				}
				else if (code == KeyboardCode::KEY_F2 && (TFE_Input::keyDown(KEY_LALT) || TFE_Input::keyDown(KEY_RALT)))
				{
					static u64 _gifIndex = 0;
					static bool _recording = false;

					if (!_recording)
					{
						char screenshotDir[TFE_MAX_PATH];
						TFE_Paths::appendPath(TFE_PathType::PATH_USER_DOCUMENTS, "Screenshots/", screenshotDir);

						char gifPath[TFE_MAX_PATH];
						sprintf(gifPath, "%stfe_gif_%s_%llu.gif", screenshotDir, s_screenshotTime, _gifIndex);
						_gifIndex++;

						TFE_RenderBackend::startGifRecording(gifPath);
						_recording = true;
					}
					else
					{
						TFE_RenderBackend::stopGifRecording();
						_recording = false;
					}
				}
			}
		} break;
		case SDL_TEXTINPUT:
		{
			TFE_Input::setBufferedInput(Event.text.text);
		} break;
		case SDL_CONTROLLERAXISMOTION:
		{
			const s32 deadzone = 3200;
			if ((Event.caxis.value < -deadzone) || (Event.caxis.value > deadzone))
			{
				if (Event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX)
				{ TFE_Input::setAxis(AXIS_LEFT_X, f32(Event.caxis.value) / 32768.0f); }
				else if (Event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)
				{ TFE_Input::setAxis(AXIS_LEFT_Y, -f32(Event.caxis.value) / 32768.0f); }

				if (Event.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTX)
				{ TFE_Input::setAxis(AXIS_RIGHT_X, f32(Event.caxis.value) / 32768.0f); }
				else if (Event.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTY)
				{ TFE_Input::setAxis(AXIS_RIGHT_Y, -f32(Event.caxis.value) / 32768.0f); }

				if (Event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
				{ TFE_Input::setAxis(AXIS_LEFT_TRIGGER, f32(Event.caxis.value) / 32768.0f); }
				if (Event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
				{ TFE_Input::setAxis(AXIS_RIGHT_TRIGGER, f32(Event.caxis.value) / 32768.0f); }
			}
			else
			{
				if (Event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX)
				{ TFE_Input::setAxis(AXIS_LEFT_X, 0.0f); }
				else if (Event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)
				{ TFE_Input::setAxis(AXIS_LEFT_Y, 0.0f); }

				if (Event.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTX)
				{ TFE_Input::setAxis(AXIS_RIGHT_X, 0.0f); }
				else if (Event.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTY)
				{ TFE_Input::setAxis(AXIS_RIGHT_Y, 0.0f); }

				if (Event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
				{ TFE_Input::setAxis(AXIS_LEFT_TRIGGER, 0.0f); }
				if (Event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
				{ TFE_Input::setAxis(AXIS_RIGHT_TRIGGER, 0.0f); }
			}
		} break;
		case SDL_CONTROLLERBUTTONDOWN:
		{
			if (Event.cbutton.button < CONTROLLER_BUTTON_COUNT)
			{
				TFE_Input::setButtonDown(Button(Event.cbutton.button));
			}
		} break;
		case SDL_CONTROLLERBUTTONUP:
		{
			if (Event.cbutton.button < CONTROLLER_BUTTON_COUNT)
			{
				TFE_Input::setButtonUp(Button(Event.cbutton.button));
			}
		} break;
		default:
		{
		} break;
	}
}

bool sdlInit()
{
	// Audio is handled outside of SDL2.
	// Using the Force Engine Audio system for sound mixing, FluidSynth for Midi handling and rtAudio for audio I/O.
	const int code = SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER);
	if (code != 0) { return false; }

	TFE_Settings_Window* windowSettings = TFE_Settings::getWindowSettings();
	bool fullscreen    = windowSettings->fullscreen;
	s_displayWidth     = windowSettings->width;
	s_displayHeight    = windowSettings->height;
	s_baseWindowWidth  = windowSettings->baseWidth;
	s_baseWindowHeight = windowSettings->baseHeight;

	// Get the displays and their bounds.
	s_displayIndex = TFE_RenderBackend::getDisplayIndex(windowSettings->x, windowSettings->y);
	// Reset the display if the window is out of bounds.
	if (s_displayIndex < 0)
	{
		MonitorInfo mInfo;
		s_displayIndex = 0;
		TFE_RenderBackend::getDisplayMonitorInfo(0, &mInfo);

		windowSettings->x = mInfo.x;
		windowSettings->y = mInfo.y + 32;
		windowSettings->width  = min(windowSettings->width,  mInfo.w);
		windowSettings->height = min(windowSettings->height, mInfo.h);
		windowSettings->baseWidth  = windowSettings->width;
		windowSettings->baseHeight = windowSettings->height;
		TFE_Settings::writeToDisk();

		s_displayWidth     = windowSettings->width;
		s_displayHeight    = windowSettings->height;
		s_baseWindowWidth  = windowSettings->baseWidth;
		s_baseWindowHeight = windowSettings->baseHeight;
	}

	// Determine the display mode settings based on the desktop.
	SDL_DisplayMode mode = {};
	SDL_GetDesktopDisplayMode(s_displayIndex, &mode);
	s_refreshRate = (f32)mode.refresh_rate;

	if (fullscreen)
	{
		s_displayWidth  = mode.w;
		s_displayHeight = mode.h;
	}
	else
	{
		s_displayWidth  = std::min(s_displayWidth,  (u32)mode.w);
		s_displayHeight = std::min(s_displayHeight, (u32)mode.h);
	}

	s_monitorWidth  = mode.w;
	s_monitorHeight = mode.h;

	return true;
}

static AppState s_curState = APP_STATE_UNINIT;

void setAppState(AppState newState, int argc, char* argv[])
{
	const TFE_Settings_Graphics* config = TFE_Settings::getGraphicsSettings();

	if (newState != APP_STATE_EDITOR)
	{
		//TFE_Editor::disable();
	}

	switch (newState)
	{
	case APP_STATE_MENU:
		break;
	case APP_STATE_EDITOR:
		if (TFE_Paths::hasPath(PATH_SOURCE_DATA))
		{
			//renderer->changeResolution(640, 480, false, TFE_Settings::getGraphicsSettings()->asyncFramebuffer, false);
			// TFE_GameUi::updateUiResolution();
			//TFE_Editor::enable(renderer);
		}
		else
		{
			newState = APP_STATE_NO_GAME_DATA;
		}
		break;
	case APP_STATE_GAME:
		if (TFE_Paths::hasPath(PATH_SOURCE_DATA))
		{
			TFE_Game* gameInfo = TFE_Settings::getGame();
			if (!s_curGame || gameInfo->id != s_curGame->id)
			{
				if (s_curGame)
				{
					freeGame(s_curGame);
					s_curGame = nullptr;
				}
				s_curGame = createGame(gameInfo->id);
				if (!s_curGame)
				{
					TFE_System::logWrite(LOG_ERROR, "AppMain", "Cannot create game '%s'.", gameInfo->game);
					newState = APP_STATE_CANNOT_RUN;
				}
				else if (!s_curGame->runGame(argc, (const char**)argv))
				{
					TFE_System::logWrite(LOG_ERROR, "AppMain", "Cannot run game '%s'.", gameInfo->game);
					freeGame(s_curGame);
					s_curGame = nullptr;
					newState = APP_STATE_CANNOT_RUN;
				}
				else
				{
					TFE_Input::enableRelativeMode(true);
				}
			}
		}
		else
		{
			newState = APP_STATE_NO_GAME_DATA;
		}
		break;
	};

	s_curState = newState;
}

bool systemMenuKeyCombo()
{
	return TFE_System::systemUiRequestPosted() || (inputMapping_getActionState(IAS_SYSTEM_MENU) == STATE_PRESSED);
}

void parseCommandLine(s32 argc, char* argv[])
{
	if (argc < 1) { return; }

	const char* curOptionName = nullptr;
	bool longName = false;
	std::vector<const char*> values;
	for (s32 i = 1; i < argc; i++)
	{
		const char* opt = argv[i];
		const size_t len = strlen(opt);

		// Is this an option name or value?
		const char* optValue = nullptr;
		if (len && opt[0] == '-')
		{
			if (curOptionName)
			{
				parseOption(curOptionName, values, longName);
			}
			if (len > 2 && opt[0] == '-' && opt[1] == '-')
			{
				longName = true;
				curOptionName = opt + 2;
			}
			else
			{
				longName = false;
				curOptionName = opt + 1;
			}
			values.clear();
		}
		else if (len && opt[0] != '-')
		{
			values.push_back(opt);
		}
	}
	if (curOptionName)
	{
		parseOption(curOptionName, values, longName);
	}
}

void generateScreenshotTime()
{
	__time64_t time;
	_time64(&time);
	strcpy(s_screenshotTime, _ctime64(&time));

	// Replace ':' with '_'
	size_t len = strlen(s_screenshotTime);
	for (size_t i = 0; i < len; i++)
	{
		if (s_screenshotTime[i] == ':')
		{
			s_screenshotTime[i] = '_';
		}
		else if (s_screenshotTime[i] == ' ')
		{
			s_screenshotTime[i] = '-';
		}
		if (s_screenshotTime[i] == '\n')
		{
			s_screenshotTime[i] = 0;
			break;
		}
	}
}

int main(int argc, char* argv[])
{
	// Paths
	bool pathsSet = true;
	pathsSet &= TFE_Paths::setProgramPath();
	pathsSet &= TFE_Paths::setProgramDataPath("TheForceEngine");
	pathsSet &= TFE_Paths::setUserDocumentsPath("TheForceEngine");
	TFE_System::logOpen("the_force_engine_log.txt");
	TFE_System::logWrite(LOG_MSG, "Main", "The Force Engine %s", c_gitVersion);
	if (!pathsSet)
	{
		TFE_System::logWrite(LOG_ERROR, "Main", "Cannot set paths.");
		return PROGRAM_ERROR;
	}

	// Before loading settings, read in the Input key lists.
	if (!TFE_Input::loadKeyNames("UI_Text/KeyText.txt"))
	{
		TFE_System::logWrite(LOG_ERROR, "Main", "Cannot load key names.");
		return PROGRAM_ERROR;
	}

	// Initialize settings so that the paths can be read.
	if (!TFE_Settings::init())
	{
		TFE_System::logWrite(LOG_ERROR, "Main", "Cannot load settings.");
		return PROGRAM_ERROR;
	}

	// Override settings with command line options.
	parseCommandLine(argc, argv);

	// Setup game paths.
	// Get the current game.
	const TFE_Game* game = TFE_Settings::getGame();
	const TFE_GameHeader* gameHeader = TFE_Settings::getGameHeader(game->game);
	TFE_Paths::setPath(PATH_SOURCE_DATA, gameHeader->sourcePath);
	TFE_Paths::setPath(PATH_EMULATOR, gameHeader->emulatorPath);

	TFE_System::logWrite(LOG_MSG, "Paths", "Program Path: \"%s\"",   TFE_Paths::getPath(PATH_PROGRAM));
	TFE_System::logWrite(LOG_MSG, "Paths", "Program Data: \"%s\"",   TFE_Paths::getPath(PATH_PROGRAM_DATA));
	TFE_System::logWrite(LOG_MSG, "Paths", "User Documents: \"%s\"", TFE_Paths::getPath(PATH_USER_DOCUMENTS));
	TFE_System::logWrite(LOG_MSG, "Paths", "Source Data: \"%s\"",    TFE_Paths::getPath(PATH_SOURCE_DATA));

	// Create a screenshot directory
	char screenshotDir[TFE_MAX_PATH];
	TFE_Paths::appendPath(TFE_PathType::PATH_USER_DOCUMENTS, "Screenshots/", screenshotDir);
	if (!FileUtil::directoryExits(screenshotDir))
	{
		FileUtil::makeDirectory(screenshotDir);
	}

	// Create a mods temporary directory.
	char tempPath[TFE_MAX_PATH];
	sprintf(tempPath, "%sTemp/", TFE_Paths::getPath(PATH_PROGRAM_DATA));
	if (!FileUtil::directoryExits(tempPath))
	{
		FileUtil::makeDirectory(tempPath);
	}
	generateScreenshotTime();

	// Initialize SDL
	if (!sdlInit())
	{
		TFE_System::logWrite(LOG_CRITICAL, "SDL", "Cannot initialize SDL.");
		TFE_System::logClose();
		return PROGRAM_ERROR;
	}
	TFE_Settings_Window* windowSettings = TFE_Settings::getWindowSettings();
	TFE_Settings_Graphics* graphics = TFE_Settings::getGraphicsSettings();
	TFE_System::init(s_refreshRate, graphics->vsync, c_gitVersion);
	
	// Setup the GPU Device and Window.
	u32 windowFlags = 0;
	if (windowSettings->fullscreen) { TFE_System::logWrite(LOG_MSG, "Display", "Fullscreen enabled."); windowFlags |= WINFLAG_FULLSCREEN; }
	if (graphics->vsync) { TFE_System::logWrite(LOG_MSG, "Display", "Vertical Sync enabled."); windowFlags |= WINFLAG_VSYNC; }
	
	WindowState windowState =
	{
		"",
		s_displayWidth,
		s_displayHeight,
		s_baseWindowWidth,
		s_baseWindowHeight,
		s_monitorWidth,
		s_monitorHeight,
		windowFlags,
		s_refreshRate
	};
	sprintf(windowState.name, "The Force Engine  v%s", TFE_System::getVersionString());
	if (!TFE_RenderBackend::init(windowState))
	{
		TFE_System::logWrite(LOG_CRITICAL, "GPU", "Cannot initialize GPU/Window.");
		TFE_System::logClose();
		return PROGRAM_ERROR;
	}
	TFE_FrontEndUI::initConsole();
	TFE_Audio::init();
	TFE_MidiPlayer::init();
	TFE_Polygon::init();
	TFE_Image::init();
	TFE_Jedi::inf_init();
	TFE_Palette::createDefault256();
	TFE_FrontEndUI::init();
	game_init();
	inputMapping_startup();

	// Uncomment to test memory region allocator.
	// TFE_Memory::region_test();

	// Color correction.
	const ColorCorrection colorCorrection = { graphics->brightness, graphics->contrast, graphics->saturation, graphics->gamma };
	TFE_RenderBackend::setColorCorrection(graphics->colorCorrection, &colorCorrection);

	// Game loop
	u32 frame = 0u;
	bool showPerf = false;
	bool relativeMode = false;
	TFE_System::logWrite(LOG_MSG, "Progam Flow", "The Force Engine Game Loop Started");
	while (s_loop && !TFE_System::quitMessagePosted())
	{
		TFE_FRAME_BEGIN();
		
		bool enableRelative = TFE_Input::relativeModeEnabled();
		if (enableRelative != relativeMode)
		{
			relativeMode = enableRelative;
			SDL_SetRelativeMouseMode(relativeMode ? SDL_TRUE : SDL_FALSE);
		}

		// System events
		SDL_Event event;
		while (SDL_PollEvent(&event)) { handleEvent(event); }

		// Handle mouse state.
		s32 mouseX, mouseY;
		s32 mouseAbsX, mouseAbsY;
		u32 state = SDL_GetRelativeMouseState(&mouseX, &mouseY);
		SDL_GetMouseState(&mouseAbsX, &mouseAbsY);
		TFE_Input::setRelativeMousePos(mouseX, mouseY);
		TFE_Input::setMousePos(mouseAbsX, mouseAbsY);
		inputMapping_updateInput();

		AppState appState = TFE_FrontEndUI::update();
		if (appState == APP_STATE_QUIT)
		{
			s_loop = false;
		}
		else if (appState != s_curState)
		{
			if (appState == APP_STATE_EXIT_TO_MENU)	// Return to the menu from the game.
			{
				if (s_curGame)
				{
					freeGame(s_curGame);
					s_curGame = nullptr;
				}
				appState = APP_STATE_MENU;
			}

			char* selectedMod = TFE_FrontEndUI::getSelectedMod();
			if (selectedMod && selectedMod[0] && appState == APP_STATE_GAME)
			{
				char* newArgs[16];
				for (s32 i = 0; i < argc; i++)
				{
					newArgs[i] = argv[i];
				}
				newArgs[argc] = selectedMod;
				setAppState(appState, argc + 1, newArgs);
			}
			else
			{
				setAppState(appState, argc, argv);
			}
		}

		TFE_Ui::begin();
		TFE_System::update();
						
		// Update
		if (TFE_FrontEndUI::uiControlsEnabled() && task_canRun())
		{
			if (inputMapping_getActionState(IAS_CONSOLE) == STATE_PRESSED)
			{
				bool isOpening = TFE_FrontEndUI::toggleConsole();
				if (s_curGame)
				{
					s_curGame->pauseGame(isOpening);
				}
				TFE_Input::enableRelativeMode(!isOpening);
			}
			else if (TFE_Input::keyPressed(KEY_F9) && TFE_Input::keyDown(KEY_LALT))
			{
				showPerf = !showPerf;
			}
			else if (TFE_Input::keyPressed(KEY_F10) && TFE_Input::keyDown(KEY_LALT))
			{
				TFE_FrontEndUI::toggleProfilerView();
			}

			bool toggleSystemMenu = systemMenuKeyCombo();
			if (TFE_FrontEndUI::isConfigMenuOpen() && (toggleSystemMenu || TFE_Input::keyPressed(KEY_ESCAPE)))
			{
				// "Eat" the escape key so it doesn't also open the Escape menu.
				TFE_Input::clearKeyPressed(KEY_ESCAPE);
				inputMapping_clearKeyBinding(KEY_ESCAPE);

				s_curState = TFE_FrontEndUI::menuReturn();
			}
			else if (toggleSystemMenu)
			{
				TFE_FrontEndUI::enableConfigMenu();
				TFE_FrontEndUI::setMenuReturnState(s_curState);
			}
		}

		if (showPerf)
		{
			//TFE_Editor::showPerf(frame);
		}

		const bool isConsoleOpen = TFE_FrontEndUI::isConsoleOpen();
		bool endInputFrame = true;
		if (s_curState == APP_STATE_EDITOR)
		{
			/*
			if (TFE_Editor::update(isConsoleOpen))
			{
				TFE_FrontEndUI::setAppState(APP_STATE_MENU);
			}
			*/
		}
		else if (s_curState == APP_STATE_GAME)
		{
			if (!s_curGame)
			{
				s_curState = APP_STATE_MENU;
			}
			else
			{
				s_curGame->loopGame();
				endInputFrame = TFE_Jedi::task_run() != 0;
			}
		}
		else
		{
			TFE_RenderBackend::clearWindow();
		}
		TFE_FrontEndUI::draw(s_curState == APP_STATE_MENU || s_curState == APP_STATE_NO_GAME_DATA, s_curState == APP_STATE_NO_GAME_DATA);

		bool swap = s_curState != APP_STATE_EDITOR && (s_curState != APP_STATE_MENU || TFE_FrontEndUI::isConfigMenuOpen());
		if (s_curState == APP_STATE_EDITOR)
		{
			//swap = TFE_Editor::render();
		}

		// Blit the frame to the window and draw UI.
		TFE_RenderBackend::swap(swap);

		// Clear transitory input state.
		if (endInputFrame)
		{
			TFE_Input::endFrame();
			inputMapping_endFrame();
		}
		frame++;

		if (endInputFrame)
		{
			TFE_FRAME_END();
		}
	}

	if (s_curGame)
	{
		freeGame(s_curGame);
		s_curGame = nullptr;
	}
	game_destroy();
	inputMapping_shutdown();

	// Cleanup
	TFE_FrontEndUI::shutdown();
	TFE_Audio::shutdown();
	TFE_MidiPlayer::destroy();
	TFE_Polygon::shutdown();
	TFE_Image::shutdown();
	TFE_Jedi::inf_shutdown();
	TFE_Palette::freeAll();
	TFE_RenderBackend::updateSettings();
	TFE_Settings::shutdown();
	//TFE_Renderer::destroy(renderer);
	TFE_RenderBackend::destroy();
	SDL_Quit();
		
	TFE_System::logWrite(LOG_MSG, "Progam Flow", "The Force Engine Game Loop Ended.");
	TFE_System::logClose();
	return PROGRAM_SUCCESS;
}

// TODO: Implement the various options.
void parseOption(const char* name, const std::vector<const char*>& values, bool longName)
{
	if (!longName)	// short names use the same style as the originals.
	{
		if (name[0] == 'g')		// Directly load a game, skipping the titlescreen.
		{
			// -gDARK
			const char* gameToLoad = &name[1];
			TFE_System::logWrite(LOG_MSG, "CommandLine", "Game to load: %s", gameToLoad);
		}
		else if (name[0] == 'u')	// Load a custom archive.
		{
			// -uDARK.GOB
			const char* archiveToLoad = &name[1];
			TFE_System::logWrite(LOG_MSG, "CommandLine", "Archive to load: %s", archiveToLoad);
		}
		else if (name[0] == 'l')	// Directly load a level at medium difficulty.
		{
			// -lSECBASE
			const char* levelToLoad = &name[1];
			TFE_System::logWrite(LOG_MSG, "CommandLine", "Level to load: %s", levelToLoad);
		}
		else if (name[0] == 'c')	// Skip cutscenes and the title screen.
		{
			// -c
			// disable cutscenes and title.
			TFE_System::logWrite(LOG_MSG, "CommandLine", "Disable cutscenes and title screen.");
		}
	}
	else  // long names use the more traditional style of arguments which allow for multiple values.
	{
		if (strcasecmp(name, "game") == 0 && values.size() >= 1)	// Directly load a game, skipping the titlescreen.
		{
			// --game DARK
			const char* gameToLoad = values[0];
			TFE_System::logWrite(LOG_MSG, "CommandLine", "Game to load: %s", gameToLoad);
		}
		else if (strcasecmp(name, "archive") == 0 && values.size() >= 1)	// Load a custom archive.
		{
			// --archive DARK.GOB
			const char* archiveToLoad = values[0];
			TFE_System::logWrite(LOG_MSG, "CommandLine", "Archive to load: %s", archiveToLoad);
		}
		else if (strcasecmp(name, "level") == 0 && values.size() >= 1)		// Directly load a level at medium difficulty.
		{
			// --level SECBASE
			const char* levelToLoad = values[0];
			TFE_System::logWrite(LOG_MSG, "CommandLine", "Level to load: %s", levelToLoad);
		}
		else if (strcasecmp(name, "warp") == 0 && values.size() >= 3)		// Warp to a specific Sector and Location.
		{
			// --warp 15 12.7 203.5
			char* endPtr = nullptr;
			TFE_System::logWrite(LOG_MSG, "CommandLine", "Warp: sectorID: %d, x: %0.2f, z: %0.2f", strtol(values[0], &endPtr, 10), strtof(values[1], &endPtr), strtof(values[2], &endPtr));
		}
		else if (strcasecmp(name, "nocutscenes") == 0)		// Skip cutscenes and the title screen.
		{
			// --nocutscenes
			TFE_System::logWrite(LOG_MSG, "CommandLine", "Disable cutscenes and title screen.");
		}
	}
}