#include <cstring>

#include "inputMapping.h"
#include <TFE_Game/igame.h>
#include <TFE_FileSystem/paths.h>
#include <assert.h>

namespace TFE_Input
{
	static const char* c_inputRemappingName = "tfe_input_remapping.bin";
	static const char  c_inputRemappingHdr[4] = { 'T', 'F', 'E', 0 };
	static const u32   c_inputRemappingVersion = 0x00010000;

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
		{ IADF_CYCLEWPN_PREV,   ITYPE_KEYBOARD, KEY_F9 },
		{ IADF_CYCLEWPN_NEXT,   ITYPE_KEYBOARD, KEY_F10 },
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
		if (!file.open(fullPath, FileStream::MODE_WRITE))
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
		if (!file.open(fullPath, FileStream::MODE_READ))
		{
			return false;
		}

		char hdr[4];
		u32 version;
		file.readBuffer(hdr, 4);
		file.read(&version);
		if (memcmp(hdr, c_inputRemappingHdr, 4) != 0 || version != c_inputRemappingVersion)
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

		file.read(&s_inputConfig.mouseFlags);
		file.readBuffer(&s_inputConfig.mouseMode, sizeof(MouseMode));
		file.read(s_inputConfig.mouseSensitivity, 2);

		file.close();
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

	void inputMapping_updateInput()
	{
		for (u32 i = 0; i < s_inputConfig.bindCount; i++)
		{
			InputBinding* bind = &s_inputConfig.binds[i];
			switch (bind->type)
			{
				case ITYPE_KEYBOARD:
				{
					if (TFE_Input::keyModDown(bind->keyMod))
					{
						if (TFE_Input::keyPressed(bind->keyCode))
						{
							s_actions[bind->action] = STATE_PRESSED;
						}
						else if (TFE_Input::keyDown(bind->keyCode) && s_actions[bind->action] != STATE_PRESSED)
						{
							s_actions[bind->action] = STATE_DOWN;
						}
					}
				} break;
				case ITYPE_MOUSE:
				{
					if (TFE_Input::keyModDown(bind->keyMod))
					{
						if (TFE_Input::mousePressed(bind->mouseBtn))
						{
							s_actions[bind->action] = STATE_PRESSED;
						}
						else if (TFE_Input::mouseDown(bind->mouseBtn) && s_actions[bind->action] != STATE_PRESSED)
						{
							s_actions[bind->action] = STATE_DOWN;
						}
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
		axisValue *= s_inputConfig.ctrlSensitivity[mappedAxis < AXIS_RIGHT_X ? 0 : 1];

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

	InputBinding* inputMapping_getBindindByIndex(u32 index)
	{
		return &s_inputConfig.binds[index];
	}

	InputConfig* inputMapping_get()
	{
		return &s_inputConfig;
	}
}  // TFE_DarkForces