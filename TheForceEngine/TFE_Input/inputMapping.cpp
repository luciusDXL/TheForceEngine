#include <cstring>

#include "inputMapping.h"
#include <TFE_Game/igame.h>
#include <TFE_FileSystem/paths.h>
#include <assert.h>
#include <TFE_System/system.h>
#include <array>
#include <TFE_Settings/settings.h>
#include <sdl.h>
#include <TFE_Input/replay.h>
#include <TFE_DarkForces/hud.h>
#include <TFE_DarkForces/player.h>

namespace TFE_Input
{
	enum InputVersion
	{
		INPUT_INIT_VER      = 0x00010000,
		INPUT_ADD_QUICKSAVE = 0x00020001,
		INPUT_ADD_DEADZONE  = 0x00020002,
		INPUT_ADD_HIGH_DEF = 0x00020003,
		INPUT_CUR_VERSION = INPUT_ADD_HIGH_DEF
	};

	static const char* c_inputRemappingName = "tfe_input_remapping.bin";
	static const char  c_inputRemappingHdr[4] = { 'T', 'F', 'E', 0 };
	static const u32   c_inputRemappingVersion = INPUT_CUR_VERSION;
		
	static InputBinding s_defaultKeyboardBinds[] =
	{
		// System
		{ IAS_CONSOLE,         ITYPE_KEYBOARD, KEY_GRAVE },
		{ IAS_SYSTEM_MENU,     ITYPE_KEYBOARD, KEY_F1, KEYMOD_ALT },

		// General
		{ IADF_MENU_TOGGLE,     ITYPE_KEYBOARD, KEY_ESCAPE },
		{ IADF_PDA_TOGGLE,      ITYPE_KEYBOARD, KEY_F1 },
		{ IADF_NIGHT_VISION_TOG,ITYPE_KEYBOARD, KEY_F2 },
		{ IADF_CLEATS_TOGGLE,   ITYPE_KEYBOARD, KEY_F3 },
		{ IADF_GAS_MASK_TOGGLE, ITYPE_KEYBOARD, KEY_F4 },
		{ IADF_HEAD_LAMP_TOGGLE,ITYPE_KEYBOARD, KEY_F5 },
		{ IADF_HEADWAVE_TOGGLE, ITYPE_KEYBOARD, KEY_F6 },
		{ IADF_HUD_TOGGLE,      ITYPE_KEYBOARD, KEY_F7 },
		{ IADF_HOLSTER_WEAPON,  ITYPE_KEYBOARD, KEY_F8 },
		{ IADF_AUTOMOUNT_TOGGLE,ITYPE_KEYBOARD, KEY_F8, KEYMOD_ALT },
		{ IADF_CYCLEWPN_PREV,   ITYPE_MOUSEWHEEL, MOUSEWHEEL_DOWN },
		{ IADF_CYCLEWPN_NEXT,   ITYPE_MOUSEWHEEL, MOUSEWHEEL_UP },
		{ IADF_WPN_PREV,        ITYPE_KEYBOARD, KEY_BACKSPACE },
		{ IADF_PAUSE,           ITYPE_KEYBOARD, KEY_PAUSE },
		{ IADF_AUTOMAP,         ITYPE_KEYBOARD, KEY_TAB },
		{ IADF_DEC_SCREENSIZE,  ITYPE_KEYBOARD, KEY_MINUS, KEYMOD_ALT },
		{ IADF_INC_SCREENSIZE,  ITYPE_KEYBOARD, KEY_EQUALS, KEYMOD_ALT },

		// Automap
		{ IADF_MAP_ZOOM_IN,     ITYPE_KEYBOARD, KEY_EQUALS },
		{ IADF_MAP_ZOOM_OUT,    ITYPE_KEYBOARD, KEY_MINUS  },
		{ IADF_MAP_ENABLE_SCROLL, ITYPE_KEYBOARD, KEY_INSERT },
		{ IADF_MAP_SCROLL_UP,   ITYPE_KEYBOARD, KEY_UP },
		{ IADF_MAP_SCROLL_DN,   ITYPE_KEYBOARD, KEY_DOWN },
		{ IADF_MAP_SCROLL_LT,   ITYPE_KEYBOARD, KEY_LEFT },
		{ IADF_MAP_SCROLL_RT,   ITYPE_KEYBOARD, KEY_RIGHT },
		{ IADF_MAP_LAYER_UP,    ITYPE_KEYBOARD, KEY_RIGHTBRACKET },
		{ IADF_MAP_LAYER_DN,    ITYPE_KEYBOARD, KEY_LEFTBRACKET },

		// Player Controls
		{ IADF_FORWARD,         ITYPE_KEYBOARD, KEY_W },
		{ IADF_BACKWARD,        ITYPE_KEYBOARD, KEY_S },
		{ IADF_STRAFE_LT,       ITYPE_KEYBOARD, KEY_A },
		{ IADF_STRAFE_RT,       ITYPE_KEYBOARD, KEY_D },
		{ IADF_TURN_LT,         ITYPE_KEYBOARD, KEY_LEFT },
		{ IADF_TURN_RT,         ITYPE_KEYBOARD, KEY_RIGHT },
		{ IADF_LOOK_UP,         ITYPE_KEYBOARD, KEY_PAGEUP },
		{ IADF_LOOK_DN,         ITYPE_KEYBOARD, KEY_PAGEDOWN },
		{ IADF_CENTER_VIEW,     ITYPE_KEYBOARD, KEY_C },
		{ IADF_RUN,             ITYPE_KEYBOARD, KEY_LSHIFT },
		{ IADF_SLOW,            ITYPE_KEYBOARD, KEY_CAPSLOCK },
		{ IADF_CROUCH,          ITYPE_KEYBOARD, KEY_LCTRL },
		{ IADF_JUMP,            ITYPE_KEYBOARD, KEY_SPACE },
		{ IADF_USE,             ITYPE_KEYBOARD, KEY_E },
		{ IADF_WEAPON_1,        ITYPE_KEYBOARD, KEY_1 },
		{ IADF_WEAPON_2,        ITYPE_KEYBOARD, KEY_2 },
		{ IADF_WEAPON_3,        ITYPE_KEYBOARD, KEY_3 },
		{ IADF_WEAPON_4,        ITYPE_KEYBOARD, KEY_4 },
		{ IADF_WEAPON_5,        ITYPE_KEYBOARD, KEY_5 },
		{ IADF_WEAPON_6,        ITYPE_KEYBOARD, KEY_6 },
		{ IADF_WEAPON_7,        ITYPE_KEYBOARD, KEY_7 },
		{ IADF_WEAPON_8,        ITYPE_KEYBOARD, KEY_8 },
		{ IADF_WEAPON_9,        ITYPE_KEYBOARD, KEY_9 },
		{ IADF_WEAPON_10,       ITYPE_KEYBOARD, KEY_0 },
		{ IADF_PRIMARY_FIRE,    ITYPE_MOUSE, MBUTTON_LEFT },
		{ IADF_SECONDARY_FIRE,  ITYPE_MOUSE, MBUTTON_RIGHT },

		// Saving
		{ IAS_QUICK_SAVE, ITYPE_KEYBOARD, KEY_F5, KEYMOD_ALT },
		{ IAS_QUICK_LOAD, ITYPE_KEYBOARD, KEY_F9, KEYMOD_ALT },

		// HD Asset
		{ IADF_HD_ASSET_TOGGLE, ITYPE_KEYBOARD, KEY_F3, KEYMOD_ALT },
		{ IADF_SCREENSHOT, ITYPE_KEYBOARD, KEY_PRINTSCREEN },
		{ IADF_GIF_RECORD, ITYPE_KEYBOARD, KEY_F2, KEYMOD_ALT},
		{ IADF_GIF_RECORD_NO_COUNTDOWN, ITYPE_KEYBOARD, KEY_F2, KEYMOD_CTRL},
		{ IADF_DEMO_RECORD, ITYPE_KEYBOARD, KEY_F6, KEYMOD_ALT},	
	};

	static InputBinding s_defaultControllerBinds[] =
	{
		{ IAS_SYSTEM_MENU, ITYPE_CONTROLLER, CONTROLLER_BUTTON_RIGHTSTICK },

		{ IADF_JUMP,   ITYPE_CONTROLLER, CONTROLLER_BUTTON_A },
		{ IADF_RUN,    ITYPE_CONTROLLER, CONTROLLER_BUTTON_B },
		{ IADF_CROUCH, ITYPE_CONTROLLER, CONTROLLER_BUTTON_X },
		{ IADF_USE,    ITYPE_CONTROLLER, CONTROLLER_BUTTON_Y },

		{ IADF_MENU_TOGGLE, ITYPE_CONTROLLER, CONTROLLER_BUTTON_GUIDE },

		{ IADF_PRIMARY_FIRE,   ITYPE_CONTROLLER_AXIS, AXIS_RIGHT_TRIGGER },
		{ IADF_SECONDARY_FIRE, ITYPE_CONTROLLER_AXIS, AXIS_LEFT_TRIGGER },
		
		{ IADF_HEAD_LAMP_TOGGLE, ITYPE_CONTROLLER, CONTROLLER_BUTTON_DPAD_RIGHT },
		{ IADF_NIGHT_VISION_TOG, ITYPE_CONTROLLER, CONTROLLER_BUTTON_DPAD_LEFT },
		{ IADF_AUTOMAP,          ITYPE_CONTROLLER, CONTROLLER_BUTTON_DPAD_UP },
		{ IADF_GAS_MASK_TOGGLE,  ITYPE_CONTROLLER, CONTROLLER_BUTTON_DPAD_DOWN },

		{ IADF_CYCLEWPN_PREV, ITYPE_CONTROLLER, CONTROLLER_BUTTON_LEFTSHOULDER },
		{ IADF_CYCLEWPN_NEXT, ITYPE_CONTROLLER, CONTROLLER_BUTTON_RIGHTSHOULDER },
	};

	static InputConfig s_inputConfig = { 0 };
	static ActionState s_actions[IA_COUNT];
	int inputCounter = 0;
	int maxInputCounter = 0;
	vector <KeyboardCode> currentKeys;
	vector <MouseButton> currentMouse;
		
	void addDefaultControlBinds();
			   
	void inputMapping_startup()
	{
		// First try to restore from disk.
		if (inputMapping_restore())
		{
			return;
		}

		// If that fails, setup the defaults.
		inputMapping_resetToDefaults();

		// Once the defaults are setup, write out the file to disk for next time.
		inputMapping_serialize();
		TFE_Settings::getGameSettings()->df_enableRecording;
	}

	void inputMapping_shutdown()
	{
		free(s_inputConfig.binds);
		s_inputConfig.bindCount = 0;
		s_inputConfig.bindCapacity = 0;
		s_inputConfig.binds = nullptr;
	}

	void inputMapping_resetToDefaults()
	{
		// If that fails, setup the defaults.
		s_inputConfig.bindCount = 0;
		s_inputConfig.bindCapacity = IA_COUNT * 2;
		s_inputConfig.binds = (InputBinding*)realloc(s_inputConfig.binds, sizeof(InputBinding)*s_inputConfig.bindCapacity);

		// Controller
		s_inputConfig.controllerFlags = CFLAG_ENABLE;
		s_inputConfig.axis[AA_LOOK_HORZ] = AXIS_RIGHT_X;
		s_inputConfig.axis[AA_LOOK_VERT] = AXIS_RIGHT_Y;
		s_inputConfig.axis[AA_STRAFE] = AXIS_LEFT_X;
		s_inputConfig.axis[AA_MOVE] = AXIS_LEFT_Y;
		s_inputConfig.ctrlSensitivity[0] = 1.0f;
		s_inputConfig.ctrlSensitivity[1] = 1.0f;
		s_inputConfig.ctrlDeadzone[0] = 0.1f;
		s_inputConfig.ctrlDeadzone[1] = 0.1f;

		// Mouse
		s_inputConfig.mouseFlags = 0;
		s_inputConfig.mouseMode = MMODE_LOOK;
		s_inputConfig.mouseSensitivity[0] = 1.0f;
		s_inputConfig.mouseSensitivity[1] = 1.0f;

		memset(s_actions, 0, sizeof(ActionState) * IA_COUNT);
		addDefaultControlBinds();
	}
		
	bool inputMapping_serialize()
	{
		const char* path = TFE_Paths::getPath(PATH_USER_DOCUMENTS);
		char fullPath[TFE_MAX_PATH];

		sprintf(fullPath, "%s%s", path, c_inputRemappingName);
		FileStream file;
		if (!file.open(fullPath, Stream::MODE_WRITE))
		{
			return false;
		}

		file.writeBuffer(c_inputRemappingHdr, 4);
		file.write(&c_inputRemappingVersion);

		file.write(&s_inputConfig.bindCount);
		file.write(&s_inputConfig.bindCapacity);
		file.writeBuffer(s_inputConfig.binds, sizeof(InputBinding), s_inputConfig.bindCount);

		file.write(&s_inputConfig.controllerFlags);
		file.writeBuffer(s_inputConfig.axis, sizeof(Axis), AA_COUNT);
		file.write(s_inputConfig.ctrlSensitivity, 2);

		// version: INPUT_ADD_DEADZONE
		{
			file.write(s_inputConfig.ctrlDeadzone, 2);
		}

		file.write(&s_inputConfig.mouseFlags);
		file.writeBuffer(&s_inputConfig.mouseMode, sizeof(MouseMode));
		file.write(s_inputConfig.mouseSensitivity, 2);

		file.close();
		return true;
	}

	bool inputMapping_restore()
	{
		const char* path = TFE_Paths::getPath(PATH_USER_DOCUMENTS);
		char fullPath[TFE_MAX_PATH];

		sprintf(fullPath, "%s%s", path, c_inputRemappingName);
		FileStream file;
		if (!file.open(fullPath, Stream::MODE_READ))
		{
			return false;
		}

		char hdr[4];
		u32 version;
		file.readBuffer(hdr, 4);
		file.read(&version);
		if (memcmp(hdr, c_inputRemappingHdr, 4) != 0)
		{
			file.close();
			return false;
		}

		file.read(&s_inputConfig.bindCount);
		file.read(&s_inputConfig.bindCapacity);
		s_inputConfig.binds = (InputBinding*)realloc(s_inputConfig.binds, sizeof(InputBinding) * s_inputConfig.bindCapacity);
		file.readBuffer(s_inputConfig.binds, sizeof(InputBinding), s_inputConfig.bindCount);

		file.read(&s_inputConfig.controllerFlags);
		file.readBuffer(s_inputConfig.axis, sizeof(Axis), AA_COUNT);
		file.read(s_inputConfig.ctrlSensitivity, 2);

		if (version >= INPUT_ADD_DEADZONE)
		{
			file.read(s_inputConfig.ctrlDeadzone, 2);
		}
		else
		{
			s_inputConfig.ctrlDeadzone[0] = 0.1f;
			s_inputConfig.ctrlDeadzone[1] = 0.1f;
		}

		file.read(&s_inputConfig.mouseFlags);
		file.readBuffer(&s_inputConfig.mouseMode, sizeof(MouseMode));
		file.read(s_inputConfig.mouseSensitivity, 2);

		file.close();

		if (version < INPUT_ADD_QUICKSAVE)
		{
			inputMapping_addBinding(&s_defaultKeyboardBinds[IAS_QUICK_SAVE]);
			inputMapping_addBinding(&s_defaultKeyboardBinds[IAS_QUICK_LOAD]);
		}

		// Add HD and other buttons
		if (version < INPUT_ADD_HIGH_DEF)
		{
			inputMapping_addBinding(&s_defaultKeyboardBinds[IADF_HD_ASSET_TOGGLE]);
			inputMapping_addBinding(&s_defaultKeyboardBinds[IADF_SCREENSHOT]);
			inputMapping_addBinding(&s_defaultKeyboardBinds[IADF_GIF_RECORD]);
			inputMapping_addBinding(&s_defaultKeyboardBinds[IADF_GIF_RECORD_NO_COUNTDOWN]);
			inputMapping_addBinding(&s_defaultKeyboardBinds[IADF_DEMO_RECORD]);
		}

		return true;
	}

	void inputMapping_addBinding(InputBinding* binding)
	{
		u32 index = s_inputConfig.bindCount;
		s_inputConfig.bindCount++;

		if (s_inputConfig.bindCount > s_inputConfig.bindCapacity)
		{
			s_inputConfig.bindCapacity += IA_COUNT;
			s_inputConfig.binds = (InputBinding*)realloc(s_inputConfig.binds, sizeof(InputBinding)*s_inputConfig.bindCapacity);
		}

		s_inputConfig.binds[index] = *binding;
	}

	void addDefaultControlBinds()
	{
		for (s32 i = 0; i < TFE_ARRAYSIZE(s_defaultKeyboardBinds); i++)
		{
			inputMapping_addBinding(&s_defaultKeyboardBinds[i]);
		}
		for (s32 i = 0; i < TFE_ARRAYSIZE(s_defaultControllerBinds); i++)
		{
			inputMapping_addBinding(&s_defaultControllerBinds[i]);
		}
	}

	void inputMapping_endFrame()
	{
		for (u32 i = 0; i < IA_COUNT; i++)
		{
			s_actions[i] = STATE_UP;
		}
	}

	bool inputMapping_isMovementAction(InputAction action)
	{
		return action >= IADF_FORWARD && action <= IADF_LOOK_DN;
	}

	void resetCounter()
	{
		inputCounter = 0;
	}

	int getCounter()
	{
		return inputCounter;
	}

	void setMaxInputCounter(int counter)
	{
		maxInputCounter = counter;
	}

	void inputMapping_updateInput()
	{

		std::vector<s32> keysDown;
		std::vector<s32> mouseDown;
		int keyIndex = 0;
		int mouseIndex = 0;

		ReplayEvent event = TFE_Input::inputEvents[inputCounter];

		if (TFE_Input::isDemoPlayback())
		{

			if (maxInputCounter > inputCounter)
			{
				// Replay Keys
				keysDown = event.keysDown;
				for (int i = 0; i < keysDown.size(); i++)
				{
					KeyboardCode key = (KeyboardCode)keysDown[i];
					TFE_Input::setKeyDown(key, false);
					currentKeys.push_back(key);
				}
				for (int i = 0; i < currentKeys.size(); i++)
				{
					KeyboardCode key = (KeyboardCode)currentKeys[i];
					if (std::find(keysDown.begin(), keysDown.end(), key) == keysDown.end())
					{
						TFE_Input::setKeyUp(key);
						currentKeys.erase(std::remove(currentKeys.begin(), currentKeys.end(), key), currentKeys.end());
					}
				}

				// Replay Mouse buttons
				mouseDown = event.mouseDown;
				for (int i = 0; i < mouseDown.size(); i++)
				{
					MouseButton key = (MouseButton)mouseDown[i];
					TFE_Input::setMouseButtonDown(key);
					currentMouse.push_back(key);
				}
				for (int i = 0; i < currentMouse.size(); i++)
				{
					MouseButton key = (MouseButton)currentMouse[i];
					if (std::find(mouseDown.begin(), mouseDown.end(), key) == mouseDown.end())
					{
						TFE_Input::setMouseButtonUp(key);
						currentMouse.erase(std::remove(currentMouse.begin(), currentMouse.end(), key), currentMouse.end());
					}
				}
			}

			// Clear all keys after playback.
			//else if (maxInputCounter <= inputCounter) inputMapping_endFrame();
		}

		for (u32 i = 0; i < s_inputConfig.bindCount; i++)
		{
			InputBinding* bind = &s_inputConfig.binds[i];
			switch (bind->type)
			{
				case ITYPE_KEYBOARD:
				{
					const bool keyIsMod = (s32)bind->keyMod == (s32)bind->keyCode || bind->keyMod == KEYMOD_NONE;
					const bool keyIsAlt = bind->keyCode == KEY_LALT || bind->keyCode == KEY_RALT;
					if (TFE_Input::keyModDown(bind->keyMod, inputMapping_isMovementAction(bind->action)) || (keyIsMod && keyIsAlt))
					{
						if (TFE_Input::keyPressed(bind->keyCode))
						{
							s_actions[bind->action] = STATE_PRESSED;
						}
						else if (TFE_Input::keyDown(bind->keyCode) && s_actions[bind->action] != STATE_PRESSED)
						{
							s_actions[bind->action] = STATE_DOWN;

							// Fore recording keys
							//TFE_System::logWrite(LOG_MSG, "INPUT PUSH BACK", "INPUT UPDATE %d", bind->keyCode);
							if (TFE_Input::isRecording())
							{
								if (keysDown.size() > 0)
								{
									int x = 2;
									x = x + 3;
								}
								// Do not record Escape
								if (bind->keyCode != KEY_ESCAPE)
								{
									keysDown.push_back(bind->keyCode);
								}
							}
							keyIndex++;
						}
					}
				} break;
				case ITYPE_MOUSE:
				{
					if (TFE_Input::keyModDown(bind->keyMod, true))
					{
						if (TFE_Input::mousePressed(bind->mouseBtn))
						{
							s_actions[bind->action] = STATE_PRESSED;
						}
						else if (TFE_Input::mouseDown(bind->mouseBtn) && s_actions[bind->action] != STATE_PRESSED)
						{
							s_actions[bind->action] = STATE_DOWN;
						}
						if (TFE_Input::isRecording() && TFE_Input::mouseDown(bind->mouseBtn))
						{
							mouseDown.push_back(bind->mouseBtn);
						}
						mouseIndex++;
					}
				} break;
				case ITYPE_MOUSEWHEEL:
				{
					s32 dx, dy;
					TFE_Input::getMouseWheel(&dx, &dy);
					if ((bind->mouseWheel == MOUSEWHEEL_LEFT  && dx < 0) || 
						(bind->mouseWheel == MOUSEWHEEL_RIGHT && dx > 0) ||
						(bind->mouseWheel == MOUSEWHEEL_UP    && dy > 0) ||
						(bind->mouseWheel == MOUSEWHEEL_DOWN  && dy < 0))
					{
						s_actions[bind->action] = STATE_PRESSED;
					}
				} break;
				case ITYPE_CONTROLLER:
				{
					if (!(s_inputConfig.controllerFlags & CFLAG_ENABLE))
					{
						break;
					}

					if (TFE_Input::buttonPressed(bind->ctrlBtn))
					{
						s_actions[bind->action] = STATE_PRESSED;
					}
					else if (TFE_Input::buttonDown(bind->ctrlBtn) && s_actions[bind->action] != STATE_PRESSED)
					{
						s_actions[bind->action] = STATE_DOWN;
					}
				} break;
				case ITYPE_CONTROLLER_AXIS:
				{
					if (!(s_inputConfig.controllerFlags & CFLAG_ENABLE))
					{
						break;
					}

					if (TFE_Input::getAxis(bind->axis) > 0.5f)
					{
						s_actions[bind->action] = STATE_DOWN;
					}
				} break;
			}
		}
		if (TFE_Input::isRecording())
		{
			if (keyIndex > 0)
			{
				event.keysDown = keysDown;
			}
			if (mouseIndex > 0)
			{
				event.mouseDown = mouseDown;
			}
			inputEvents[inputCounter] = event;
		}
	}

	void inputMapping_removeState(InputAction action)
	{
		s_actions[action] = STATE_UP;
	}
	
	ActionState inputMapping_getActionState(InputAction action)
	{
		return s_actions[action];
	}

	void inputMapping_clearKeyBinding(KeyboardCode key)
	{
		// Search through bindings and clear any actions with this key.
		for (u32 i = 0; i < s_inputConfig.bindCount; i++)
		{
			InputBinding* bind = &s_inputConfig.binds[i];
			if (bind->type == ITYPE_KEYBOARD && bind->keyCode == key)
			{
				inputMapping_removeState(bind->action);
				return;
			}
		}
	}

	f32 inputMapping_getAnalogAxis(AnalogAxis axis)
	{
		if (!(s_inputConfig.controllerFlags & CFLAG_ENABLE))
		{
			return 0.0f;
		}

		Axis mappedAxis = s_inputConfig.axis[axis];
		f32 axisValue = TFE_Input::getAxis(mappedAxis);
		if (s_inputConfig.controllerFlags & (1 << (mappedAxis + 1)))
		{
			axisValue = -axisValue;
		}

		const f32 sensitivity = s_inputConfig.ctrlSensitivity[mappedAxis < AXIS_RIGHT_X ? 0 : 1];
		f32 deadzone = s_inputConfig.ctrlDeadzone[mappedAxis < AXIS_RIGHT_X ? 0 : 1];
		if (fabsf(axisValue) <= deadzone)
		{
			axisValue = 0.0f;
		}
		else if (deadzone > FLT_EPSILON)
		{
			deadzone *= ((axisValue < 0.0f) ? -1.0f : 1.0f);
			// sensitivity is adjusted by the new range, so larger deadzones don't decrease the maximum output.
			axisValue = (axisValue - deadzone) * sensitivity / (1.0f - fabsf(deadzone));
		}
		else
		{
			axisValue = axisValue * sensitivity;
		}

		return axisValue;
	}
		
	f32 inputMapping_getHorzMouseSensitivity()
	{
		return s_inputConfig.mouseSensitivity[0] * ((s_inputConfig.mouseFlags & MFLAG_INVERT_HORZ) ? -1.0f : 1.0f);
	}

	f32 inputMapping_getVertMouseSensitivity()
	{
		return s_inputConfig.mouseSensitivity[1] * ((s_inputConfig.mouseFlags & MFLAG_INVERT_VERT) ? -1.0f : 1.0f);
	}

	u32 inputMapping_getBindingsForAction(InputAction action, u32* indices, u32 maxIndices)
	{
		assert(indices);
		u32 count = 0;
		for (u32 i = 0; i < s_inputConfig.bindCount; i++)
		{
			if (s_inputConfig.binds[i].action == action)
			{
				indices[count++] = i;
				if (count >= maxIndices)
				{
					return count;
				}
			}
		}
		return count;
	}

	void inputMapping_removeBinding(u32 index)
	{
		for (u32 i = index; i < s_inputConfig.bindCount; i++)
		{
			s_inputConfig.binds[i] = s_inputConfig.binds[i + 1];
		}
		s_inputConfig.bindCount--;
	}

	InputBinding* inputMapping_getBindingByIndex(u32 index)
	{
		return &s_inputConfig.binds[index];
	}

	InputConfig* inputMapping_get()
	{
		return &s_inputConfig;
	}

	void handleInputs()
	{
		//  Allow escape during playback
		if (keyPressed(KEY_ESCAPE))
		{
			if (isDemoPlayback())
			{
			
				TFE_Input::setDemoPlayback(false);
			}
			if (isRecording())
			{
				setDemoPlayback(false);
				string msg = "DEMO Playback Complete!";
				TFE_DarkForces::hud_sendTextMessage(msg.c_str(), 1, false);
				TFE_Input::endRecording();
			}
		}


		/*
		if (inputCounter == 0)
		{
			if (isDemoPlayback())
			{
				loadTiming();
			}
			if (isRecording())
			{
				recordTiming();
			}		
		}*/
		std::vector<s32> mousePos;
		s32 mouseX, mouseY;
		s32 mouseAbsX, mouseAbsY;
		u32 state = SDL_GetRelativeMouseState(&mouseX, &mouseY);
		SDL_GetMouseState(&mouseAbsX, &mouseAbsY);

	
		string hudData = "";
		string keys = "";
		string mouse = "";
		string mouseP = "";
		// Wipe current keys and mouse buttons
		if (isDemoPlayback())
		{
			if (inputCounter >= maxInputCounter)
			{
				setDemoPlayback(false);
				string msg = "DEMO Playback Complete!";
				TFE_DarkForces::hud_sendTextMessage(msg.c_str(), 1, false);

			}
			else
			{
				string msg = "DEMO Playback [" + std::to_string(inputCounter) + " out of " + std::to_string(maxInputCounter) + "] ...";
				TFE_DarkForces::hud_sendTextMessage(msg.c_str(), 1, true);

				inputMapping_endFrame();
				ReplayEvent event = TFE_Input::inputEvents[inputCounter];
				mousePos = event.mousePos;
				if (mousePos.size() == 4)
				{
					mouseX = mousePos[0];
					mouseY = mousePos[1];
					mouseAbsX = mousePos[2];
					mouseAbsY = mousePos[3];
				}
				mouseP = convertToString(event.mousePos);
				//event.keysDown;
			}
		}

		else if (isRecording())
		{
			string msg = "DEMO Recording [" + std::to_string(inputCounter) + "] ...";
			TFE_DarkForces::hud_sendTextMessage(msg.c_str(), 1, true);

			ReplayEvent event = TFE_Input::inputEvents[inputCounter];
			mousePos = event.mousePos;
			mousePos.clear();
			mousePos.push_back(mouseX);
			mousePos.push_back(mouseY);
			mousePos.push_back(mouseAbsX);
			mousePos.push_back(mouseAbsY);
			event.mousePos = mousePos;
			mouseP = convertToString(event.mousePos);
			TFE_Input::inputEvents[inputCounter] = event;
		}
		bool rec = isRecording();
		bool play = isDemoPlayback();
		bool recAllow = TFE_Settings::getGameSettings()->df_enableRecording;
		bool playAllow = TFE_Settings::getGameSettings()->df_enableReplay;

		TFE_Input::setRelativeMousePos(mouseX, mouseY);
		TFE_Input::setMousePos(mouseAbsX, mouseAbsY);
		inputMapping_updateInput();

		if ((isRecording() || isDemoPlayback()) && TFE_DarkForces::s_playerEye)
		{
			ReplayEvent event = TFE_Input::inputEvents[inputCounter];
			hudData = TFE_DarkForces::hud_getDataStr(true);
			keys = convertToString(event.keysDown);
			mouse = convertToString(event.mouseDown);
		}

		if ((isRecording() || isDemoPlayback()))
		{
			string hudDataStr = "Rec=%d, Play=%d, RecAllow=%d, PlayAllow=%d, upd=%d ";
			if (hudData.size() != 0) hudDataStr += hudData;
			if (keys.size() != 0) hudDataStr += " Keys: " + keys;
			if (mouse.size() != 0) hudDataStr += " Mouse: " + mouse;
			if (mouseP.size() != 0) hudDataStr += " MousePos " + mouseP;

			TFE_System::logWrite(LOG_MSG, "LOG", hudDataStr.c_str(), rec, play, recAllow, playAllow, inputCounter);
		}
		
		inputCounter++;
	}

}  // TFE_DarkForces