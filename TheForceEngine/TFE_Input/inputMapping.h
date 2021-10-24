#pragma once
//////////////////////////////////////////////////////////////////////
// Input Mapping for supported games.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Input/input.h>

namespace TFE_Input
{
	enum InputAction
	{
		// System
		IAS_CONSOLE = 0,
		IAS_SYSTEM_MENU,

		// Dark Forces
		// General
		IADF_MENU_TOGGLE,
		IADF_PDA_TOGGLE,
		IADF_NIGHT_VISION_TOG,
		IADF_CLEATS_TOGGLE,
		IADF_GAS_MASK_TOGGLE,
		IADF_HEAD_LAMP_TOGGLE,
		IADF_HEADWAVE_TOGGLE,
		IADF_HUD_TOGGLE,
		IADF_HOLSTER_WEAPON,
		IADF_AUTOMOUNT_TOGGLE,
		IADF_CYCLEWPN_NEXT,
		IADF_CYCLEWPN_PREV,
		IADF_WPN_PREV,
		IADF_PAUSE,
		IADF_AUTOMAP,
		IADF_DEC_SCREENSIZE,
		IADF_INC_SCREENSIZE,

		// Automap
		IADF_MAP_ZOOM_IN,
		IADF_MAP_ZOOM_OUT,
		IADF_MAP_ENABLE_SCROLL,
		IADF_MAP_SCROLL_UP,
		IADF_MAP_SCROLL_DN,
		IADF_MAP_SCROLL_LT,
		IADF_MAP_SCROLL_RT,
		IADF_MAP_LAYER_UP,
		IADF_MAP_LAYER_DN,
		
		// Player Controls
		IADF_FORWARD,
		IADF_BACKWARD,
		IADF_STRAFE_LT,
		IADF_STRAFE_RT,
		IADF_TURN_LT,
		IADF_TURN_RT,
		IADF_LOOK_UP,
		IADF_LOOK_DN,
		IADF_CENTER_VIEW,
		IADF_RUN,
		IADF_SLOW,
		IADF_CROUCH,
		IADF_JUMP,
		IADF_USE,
		IADF_WEAPON_1,
		IADF_WEAPON_2,
		IADF_WEAPON_3,
		IADF_WEAPON_4,
		IADF_WEAPON_5,
		IADF_WEAPON_6,
		IADF_WEAPON_7,
		IADF_WEAPON_8,
		IADF_WEAPON_9,
		IADF_WEAPON_10,
		IADF_PRIMARY_FIRE,
		IADF_SECONDARY_FIRE,

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

	void inputMapping_startup();
	void inputMapping_shutdown();
		
	void inputMapping_addBinding(InputBinding* binding);
	ActionState inputMapping_getActionState(InputAction action);
	void inputMapping_updateInput();
	void inputMapping_removeState(InputAction action);

	InputConfig* inputMapping_get();
	u32 inputMapping_getBindingsForAction(InputAction action, u32* indices, u32 maxIndices);
	InputBinding* inputMapping_getBindindByIndex(u32 index);
}  // TFE_Input