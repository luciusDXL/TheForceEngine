// main.cpp : Defines the entry point for the application.
#include <SDL.h>
#include <DXL2_System/types.h>
#include <DXL2_ScriptSystem/scriptSystem.h>
#include <DXL2_InfSystem/infSystem.h>
#include <DXL2_Editor/editor.h>
#include <DXL2_Game/level.h>
#include <DXL2_FileSystem/paths.h>
#include <DXL2_Polygon/polygon.h>
#include <DXL2_RenderBackend/renderBackend.h>
#include <DXL2_Input/input.h>
#include <DXL2_Renderer/renderer.h>
#include <DXL2_Settings/settings.h>
#include <DXL2_System/system.h>
#include <DXL2_Asset/paletteAsset.h>
#include <DXL2_Asset/imageAsset.h>
#include <DXL2_Ui/ui.h>
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#ifdef min
#undef min
#undef max
#endif
#endif

#define PROGRAM_ERROR 1
#define PROGRAM_SUCCESS 0

#pragma comment(lib, "SDL2main.lib")

// Replace with settings.
static bool s_fullscreen = false;
static bool s_vsync = true;
static bool s_loop  = true;
static f32  s_refreshRate = 0;
static u32  s_baseWindowWidth = 1280;
static u32  s_baseWindowHeight = 720;
static u32  s_displayWidth = s_baseWindowWidth;
static u32  s_displayHeight = s_baseWindowHeight;
static u32  s_monitorWidth = 1280;
static u32  s_monitorHeight = 720;

void handleEvent(SDL_Event& Event)
{
	DXL2_Ui::setUiInput(&Event);

	switch (Event.type)
	{
		case SDL_QUIT:
		{
			s_loop = false;
		} break;
		case SDL_WINDOWEVENT:
		{
			if (Event.window.event == SDL_WINDOWEVENT_RESIZED || Event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
			{
				DXL2_RenderBackend::resize(Event.window.data1, Event.window.data2);
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
			DXL2_Input::setMouseButtonDown(MouseButton(Event.button.button - SDL_BUTTON_LEFT));
		} break;
		case SDL_MOUSEBUTTONUP:
		{
			DXL2_Input::setMouseButtonUp(MouseButton(Event.button.button - SDL_BUTTON_LEFT));
		} break;
		case SDL_MOUSEWHEEL:
		{
			DXL2_Input::setMouseWheel(Event.wheel.x, Event.wheel.y);
		} break;
		case SDL_KEYDOWN:
		{
			if (Event.key.keysym.scancode && Event.key.repeat == 0)
			{
				DXL2_Input::setKeyDown(KeyboardCode(Event.key.keysym.scancode));
			}
		} break;
		case SDL_KEYUP:
		{
			if (Event.key.keysym.scancode)
			{
				const KeyboardCode code = KeyboardCode(Event.key.keysym.scancode);
				DXL2_Input::setKeyUp(KeyboardCode(Event.key.keysym.scancode));

				// Fullscreen toggle.
				if (code == KeyboardCode::KEY_F11)
				{
					s_fullscreen = !s_fullscreen;
					DXL2_RenderBackend::enableFullscreen(s_fullscreen);
				}
			}
		} break;
		case SDL_CONTROLLERAXISMOTION:
		{
			const s32 deadzone = 3200;
			if ((Event.caxis.value < -deadzone) || (Event.caxis.value > deadzone))
			{
				if (Event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX)
				{ DXL2_Input::setAxis(AXIS_LEFT_X, f32(Event.caxis.value) / 32768.0f); }
				else if (Event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)
				{ DXL2_Input::setAxis(AXIS_LEFT_Y, -f32(Event.caxis.value) / 32768.0f); }

				if (Event.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTX)
				{ DXL2_Input::setAxis(AXIS_RIGHT_X, f32(Event.caxis.value) / 32768.0f); }
				else if (Event.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTY)
				{ DXL2_Input::setAxis(AXIS_RIGHT_Y, -f32(Event.caxis.value) / 32768.0f); }

				if (Event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
				{ DXL2_Input::setAxis(AXIS_LEFT_TRIGGER, f32(Event.caxis.value) / 32768.0f); }
				if (Event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
				{ DXL2_Input::setAxis(AXIS_RIGHT_TRIGGER, -f32(Event.caxis.value) / 32768.0f); }
			}
			else
			{
				if (Event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX)
				{ DXL2_Input::setAxis(AXIS_LEFT_X, 0.0f); }
				else if (Event.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY)
				{ DXL2_Input::setAxis(AXIS_LEFT_Y, 0.0f); }

				if (Event.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTX)
				{ DXL2_Input::setAxis(AXIS_RIGHT_X, 0.0f); }
				else if (Event.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTY)
				{ DXL2_Input::setAxis(AXIS_RIGHT_Y, 0.0f); }

				if (Event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
				{ DXL2_Input::setAxis(AXIS_LEFT_TRIGGER, 0.0f); }
				if (Event.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)
				{ DXL2_Input::setAxis(AXIS_RIGHT_TRIGGER, 0.0f); }
			}
		} break;
		case SDL_CONTROLLERBUTTONDOWN:
		{
			if (Event.cbutton.button < CONTROLLER_BUTTON_COUNT)
			{
				DXL2_Input::setButtonDown(Button(Event.cbutton.button));
			}
		} break;
		case SDL_CONTROLLERBUTTONUP:
		{
			if (Event.cbutton.button < CONTROLLER_BUTTON_COUNT)
			{
				DXL2_Input::setButtonUp(Button(Event.cbutton.button));
			}
		} break;
		default:
		{
		} break;
	}
}

bool sdlInit()
{
	int code = SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_EVENTS |
		                SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_GAMECONTROLLER | SDL_INIT_SENSOR);
	if (code != 0) { return false; }

	// Determine the display mode settings based on the desktop.
	SDL_DisplayMode mode = {};
	SDL_GetDesktopDisplayMode(0, &mode);
	s_refreshRate = (f32)mode.refresh_rate;

	DXL2_Settings_Window* windowSettings = DXL2_Settings::getWindowSettings();
	s_fullscreen = windowSettings->fullscreen;
	s_displayWidth = windowSettings->width;
	s_displayHeight = windowSettings->height;
	s_baseWindowWidth = windowSettings->baseWidth;
	s_baseWindowHeight = windowSettings->baseHeight;

	if (s_fullscreen)
	{
		s_displayWidth = mode.w;
		s_displayHeight = mode.h;
	}
	else
	{
		s_displayWidth = std::min(s_displayWidth, (u32)mode.w);
		s_displayHeight = std::min(s_displayHeight, (u32)mode.h);
	}

	s_monitorWidth = mode.w;
	s_monitorHeight = mode.h;

	return true;
}

enum AppState
{
	APP_STATE_MENU = 0,
	APP_STATE_EDITOR,
	APP_STATE_DARK_FORCES,
	APP_STATE_COUNT
};
static AppState s_appState = APP_STATE_MENU;

void setAppState(AppState newState, DXL2_Renderer* renderer)
{
	if (newState != APP_STATE_EDITOR)
	{
		DXL2_Editor::disable();
	}

	switch (newState)
	{
	case APP_STATE_MENU:
		break;
	case APP_STATE_EDITOR:
		renderer->changeResolution(640, 480);
		DXL2_Editor::enable(renderer);
		break;
	case APP_STATE_DARK_FORCES:
		renderer->changeResolution(320, 200);
		break;
	};

	s_appState = newState;
}

int main(int argc, char* argv[])
{
	// Paths
	bool pathsSet = true;
	pathsSet &= DXL2_Paths::setProgramPath();
	pathsSet &= DXL2_Paths::setProgramDataPath("DarkXL2");
	pathsSet &= DXL2_Paths::setUserDocumentsPath("DarkXL2");
	if (!pathsSet)
	{
		return PROGRAM_ERROR;
	}

	// Initialize settings so that the paths can be read.
	if (!DXL2_Settings::init())
	{
		return PROGRAM_ERROR;
	}

	// TODO: The data path is hard coded and should be set.
	DXL2_Paths::setPath(PATH_SOURCE_DATA, "D:\\Program Files (x86)\\Steam\\steamapps\\common\\dark forces\\Game\\");
	// DosBox is only required for the level editor - if running the Dos version to test.
	DXL2_Paths::setPath(PATH_DOSBOX, "D:\\Program Files (x86)\\Steam\\steamapps\\common\\dark forces\\");

	DXL2_System::logOpen("darkxl2_log.txt");
	DXL2_System::logWrite(LOG_MSG, "Paths", "Program Path: \"%s\"", DXL2_Paths::getPath(PATH_PROGRAM));
	DXL2_System::logWrite(LOG_MSG, "Paths", "Program Data: \"%s\"", DXL2_Paths::getPath(PATH_PROGRAM_DATA));
	DXL2_System::logWrite(LOG_MSG, "Paths", "User Documents: \"%s\"", DXL2_Paths::getPath(PATH_USER_DOCUMENTS));
	DXL2_System::logWrite(LOG_MSG, "Paths", "Source Data: \"%s\"", DXL2_Paths::getPath(PATH_SOURCE_DATA));

	// Initialize SDL
	if (!sdlInit())
	{
		DXL2_System::logWrite(LOG_CRITICAL, "SDL", "Cannot initialize SDL.");
		DXL2_System::logClose();
		return PROGRAM_ERROR;
	}

	// Setup the GPU Device and Window.
	u32 windowFlags = 0;
	if (s_fullscreen) { DXL2_System::logWrite(LOG_MSG, "Display", "Fullscreen enabled."); windowFlags |= WINFLAG_FULLSCREEN; }
	if (s_vsync) { DXL2_System::logWrite(LOG_MSG, "Display", "Vertical Sync enabled."); windowFlags |= WINFLAG_VSYNC; }
		
	const WindowState windowState =
	{
		"DarkXL 2: A New Hope",
		s_displayWidth,
		s_displayHeight,
		s_baseWindowWidth,
		s_baseWindowHeight,
		s_monitorWidth,
		s_monitorHeight,
		windowFlags,
		s_refreshRate
	};
	if (!DXL2_RenderBackend::init(windowState))
	{
		DXL2_System::logWrite(LOG_CRITICAL, "GPU", "Cannot initialize GPU/Window.");
		DXL2_System::logClose();
		return PROGRAM_ERROR;
	}
	DXL2_System::init(s_refreshRate, s_vsync);
	DXL2_Polygon::init();
	DXL2_Image::init();
	DXL2_ScriptSystem::init();
	DXL2_InfSystem::init();
	DXL2_Level::init();
	DXL2_Palette::createDefault256();

	DXL2_Renderer* renderer = DXL2_Renderer::create(DXL2_RENDERER_SOFTWARE_CPU);
	if (!renderer)
	{
		DXL2_System::logWrite(LOG_CRITICAL, "Renderer", "Cannot create software renderer.");
		DXL2_System::logClose();
		return PROGRAM_ERROR;
	}
	if (!renderer->init())
	{
		DXL2_System::logWrite(LOG_CRITICAL, "Renderer", "Cannot initialize software renderer.");
		DXL2_System::logClose();
		return PROGRAM_ERROR;
	}
					
	// Game loop
	DXL2_System::logWrite(LOG_MSG, "Progam Flow", "DarkXL 2 Game Loop Started");

	// For now enable the editor by default.
	setAppState(APP_STATE_EDITOR, renderer);
	   
	u32 frame = 0u;
	bool showPerf = false;
	bool relativeMode = false;
	while (s_loop)
	{
		bool enableRelative = DXL2_Input::relativeModeEnabled();
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
		DXL2_Input::setRelativeMousePos(mouseX, mouseY);
		DXL2_Input::setMousePos(mouseAbsX, mouseAbsY);
		DXL2_Ui::begin();
		
		// Update
		if (DXL2_Input::keyPressed(KEY_F9))
		{
			showPerf = !showPerf;
		}

		DXL2_System::update();
		if (showPerf)
		{
			DXL2_Editor::showPerf(frame);
		}

		if (s_appState == APP_STATE_MENU)
		{
		}
		else if (s_appState == APP_STATE_EDITOR)
		{
			DXL2_Editor::update();
		}
		else if (s_appState == APP_STATE_DARK_FORCES)
		{
		}

		// Render
		renderer->begin();
		// Do stuff
		bool swap = s_appState != APP_STATE_EDITOR;
		if (s_appState == APP_STATE_EDITOR)
		{
			swap = DXL2_Editor::render();
		}
		renderer->end();

		// Blit the frame to the window and draw UI.
		DXL2_RenderBackend::swap(swap);

		// Clear transitory input state.
		DXL2_Input::endFrame();
		frame++;
	}

	// Cleanup
	DXL2_Polygon::shutdown();
	DXL2_Image::shutdown();
	DXL2_Level::shutdown();
	DXL2_InfSystem::shutdown();
	DXL2_ScriptSystem::shutdown();
	DXL2_Palette::freeAll();
	DXL2_RenderBackend::updateSettings();
	DXL2_Settings::shutdown();
	DXL2_Renderer::destroy(renderer);
	DXL2_RenderBackend::destroy();
	SDL_Quit();

	DXL2_System::logWrite(LOG_MSG, "Progam Flow", "DarkXL 2 Game Loop Ended.");
	DXL2_System::logClose();
	return PROGRAM_SUCCESS;
}
