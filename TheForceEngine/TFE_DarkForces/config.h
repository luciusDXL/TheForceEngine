#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Config
// Replacement for the Jedi Config for TFE.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Input/input.h>

namespace TFE_DarkForces
{
	enum InputAction
	{
		// General
		IA_MENU_TOGGLE = 0,
		IA_PDA_TOGGLE,
		IA_NIGHT_VISION_TOG,
		IA_CLEATS_TOGGLE,
		IA_GAS_MASK_TOGGLE,
		IA_HEAD_LAMP_TOGGLE,
		IA_HEADWAVE_TOGGLE,
		IA_HUD_TOGGLE,
		IA_HOLSTER_WEAPON,
		IA_AUTOMOUNT_TOGGLE,
		IA_CYCLEWPN_NEXT,
		IA_CYCLEWPN_PREV,
		IA_WPN_PREV,
		IA_PAUSE,
		IA_AUTOMAP,
		IA_DEC_SCREENSIZE,
		IA_INC_SCREENSIZE,

		// Automap
		IA_MAP_ZOOM_IN,
		IA_MAP_ZOOM_OUT,
		IA_MAP_ENABLE_SCROLL,
		IA_MAP_SCROLL_UP,
		IA_MAP_SCROLL_DN,
		IA_MAP_SCROLL_LT,
		IA_MAP_SCROLL_RT,
		IA_MAP_LAYER_UP,
		IA_MAP_LAYER_DN,
		
		// Player Controls
		IA_FORWARD,
		IA_BACKWARD,
		IA_STRAFE_LT,
		IA_STRAFE_RT,
		IA_TURN_LT,
		IA_TURN_RT,
		IA_LOOK_UP,
		IA_LOOK_DN,
		IA_CENTER_VIEW,
		IA_RUN,
		IA_SLOW,
		IA_CROUCH,
		IA_JUMP,
		IA_USE,
		IA_WEAPON_1,
		IA_WEAPON_2,
		IA_WEAPON_3,
		IA_WEAPON_4,
		IA_WEAPON_5,
		IA_WEAPON_6,
		IA_WEAPON_7,
		IA_WEAPON_8,
		IA_WEAPON_9,
		IA_WEAPON_10,
		IA_PRIMARY_FIRE,
		IA_SECONDARY_FIRE,
		IA_COUNT,
	};

	enum AnalogAxis
	{
		AA_LOOK_HORZ = 0,
		AA_LOOK_VERT,
		AA_MOVE,
		AA_STRAFE,
	};

	enum InputType
	{
		ITYPE_KEYBOARD = 0,
		ITYPE_MOUSE,
		ITYPE_CONTROLLER,
		ITYPE_CONTROLLER_AXIS,
		ITYPE_COUNT
	};

	enum ActionState
	{
		STATE_UP      = 0,
		STATE_DOWN    = 1,
		STATE_PRESSED = 2,
		STATE_ACTIVE  = STATE_DOWN | STATE_PRESSED
	};

	struct InputBinding
	{
		InputAction action;
		InputType   type;
		union
		{
			u32          code;
			KeyboardCode keyCode;
			MouseButton  mouseBtn;
			Button       ctrlBtn;
			Axis         axis;
		};
		KeyModifier keyMod = KEYMOD_NONE;
	};

	struct InputConfig
	{
		u32 bindCount;
		u32 bindCapacity;
		InputBinding* binds;
	};

	struct GameConfig
	{
		JBool headwave;
		JBool wpnAutoMount;
		JBool mouseTurnEnabled;
		JBool mouseLookEnabled;
		JBool superShield;
	};

	void configStartup();
	void configShutdown();
		
	void addInputBinding(InputBinding* binding);
	ActionState getActionState(InputAction action);
	void config_updateInput();
	void config_removeState(InputAction action);

	extern GameConfig s_config;
}  // TFE_DarkForces