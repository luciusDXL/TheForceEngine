#include "gameControlMapping.h"
#include <TFE_System/system.h>
#include <TFE_Input/input.h>
#include <assert.h>
#include <vector>
#include <map>
#include <algorithm>

namespace TFE_GameControlMapping
{
	#define INVALID_INDEX 0xffffffff

	struct Binding
	{
		GameControlType type;
		GameControlTrigger trigger;
		u32 primaryId;
		u32 secondaryId;
		f32 scale;
	};

	typedef std::vector<Binding> BindingList;
	struct Action
	{
		u32 id;
		BindingList bindings;
	};

	typedef std::map<u32, u32> ActionToIndexMap;
	typedef std::vector<Action> ActionList;
			
	ActionToIndexMap s_actionMap;
	ActionList s_actionList;

	u32 getActionIndex(u32 id);
	u32 addActionIndex(u32 id);

	void clearActions()
	{
		s_actionMap.clear();
		s_actionList.clear();
	}

	f32 getAction(u32 id)
	{
		// Get the action index and return the default (0.0) if it does not exist.
		const u32 actionIndex = getActionIndex(id);
		if (actionIndex == INVALID_INDEX)
		{
			return 0.0f;
		}

		// Then loop through the bindings, returning at the first hit.
		Action& action = s_actionList[actionIndex];
		const u32 count = (u32)action.bindings.size();
		const Binding* binding = action.bindings.data();
		for (u32 i = 0; i < count; i++, binding++)
		{
			bool activated = false;
			s32 dx, dy;
			switch (binding->type)
			{
				case GCTRL_KEY:
					if (binding->trigger == GTRIGGER_PRESSED)
					{
						activated = TFE_Input::keyPressed((KeyboardCode)binding->primaryId) &&
							(binding->secondaryId == 0 || TFE_Input::keyDown((KeyboardCode)binding->secondaryId));
					}
					else if (binding->trigger == GTRIGGER_DOWN || binding->trigger == GTRIGGER_UPDATE)
					{
						activated = TFE_Input::keyDown((KeyboardCode)binding->primaryId) &&
							(binding->secondaryId == 0 || TFE_Input::keyDown((KeyboardCode)binding->secondaryId));
					}
					if (activated) { return binding->scale != 0.0f ? binding->scale : 1.0f; }
					break;
				case GCTRL_CONTROLLER_BUTTON:
					if (binding->trigger == GTRIGGER_PRESSED)
					{
						activated = TFE_Input::buttonPressed((Button)binding->primaryId) &&
							(binding->secondaryId == 0 || TFE_Input::buttonDown((Button)binding->secondaryId));
					}
					else if (binding->trigger == GTRIGGER_DOWN || binding->trigger == GTRIGGER_UPDATE)
					{
						activated = TFE_Input::buttonDown((Button)binding->primaryId) &&
							(binding->secondaryId == 0 || TFE_Input::buttonDown((Button)binding->secondaryId));
					}
					if (activated) { return binding->scale != 0.0f ? binding->scale : 1.0f; }
					break;
				case GCTRL_MOUSE_BUTTON:
					if (binding->trigger == GTRIGGER_PRESSED)
					{
						activated = TFE_Input::mousePressed((MouseButton)binding->primaryId) &&
							(binding->secondaryId == 0 || TFE_Input::mouseDown((MouseButton)binding->secondaryId));
					}
					else if (binding->trigger == GTRIGGER_DOWN || binding->trigger == GTRIGGER_UPDATE)
					{
						activated = TFE_Input::mouseDown((MouseButton)binding->primaryId) &&
							(binding->secondaryId == 0 || TFE_Input::mouseDown((MouseButton)binding->secondaryId));
					}
					if (activated) { return binding->scale != 0.0f ? binding->scale : 1.0f; }
					break;
				case GCTRL_CONTROLLER_AXIS:
				{
					f32 axis = TFE_Input::getAxis((Axis)binding->primaryId);
					axis = (binding->scale == 0.0f) ? fabsf(axis) : std::max(axis * binding->scale, 0.0f);

					if (axis != 0.0f)
					{
						if (binding->trigger == GTRIGGER_PRESSED || binding->trigger == GTRIGGER_DOWN)
						{
							if (axis > 0.5f) { return (binding->scale != 0.0f ? binding->scale : 1.0f); }
						}
						else // binding->trigger == GTRIGGER_UPDATE
						{
							return axis;
						}
					}
				}	break;
				case GCTRL_MOUSE_AXIS_X:
				{
					TFE_Input::getMouseMove(&dx, &dy);
					f32 axis = f32(dx);
					axis = (binding->scale == 0.0f) ? fabsf(axis) : std::max(axis * binding->scale, 0.0f);

					if (axis != 0.0f)
					{
						if (binding->trigger == GTRIGGER_PRESSED || binding->trigger == GTRIGGER_DOWN)
						{
							if (axis > 0.5f) { return (binding->scale != 0.0f ? binding->scale : 1.0f); }
						}
						else // binding->trigger == GTRIGGER_UPDATE
						{
							return axis;
						}
					}
				}	break;
				case GCTRL_MOUSE_AXIS_Y:
				{
					TFE_Input::getMouseMove(&dx, &dy);
					f32 axis = f32(dy);
					axis = (binding->scale == 0.0f) ? fabsf(axis) : std::max(axis * binding->scale, 0.0f);

					if (axis != 0.0f)
					{
						if (binding->trigger == GTRIGGER_PRESSED || binding->trigger == GTRIGGER_DOWN)
						{
							if (axis > 0.5f) { return (binding->scale != 0.0f ? binding->scale : 1.0f); }
						}
						else // binding->trigger == GTRIGGER_UPDATE
						{
							return axis;
						}
					}
				}	break;
				case GCTRL_MOUSEWHEEL_X:
					TFE_Input::getMouseWheel(&dx, &dy);
					if (dx != 0)
					{
						if (binding->trigger == GTRIGGER_PRESSED || binding->trigger == GTRIGGER_DOWN)
						{
							return binding->scale != 0.0f ? binding->scale : 1.0f;
						}
						else // binding->trigger == GTRIGGER_UPDATE
						{
							return f32(dx) * (binding->scale != 0.0f ? binding->scale : 1.0f);
						}
					}
					break;
				case GCTRL_MOUSEWHEEL_Y:
					TFE_Input::getMouseWheel(&dx, &dy);
					if (dy != 0)
					{
						if (binding->trigger == GTRIGGER_PRESSED || binding->trigger == GTRIGGER_DOWN)
						{
							return binding->scale != 0.0f ? binding->scale : 1.0f;
						}
						else // binding->trigger == GTRIGGER_UPDATE
						{
							return f32(dy) * (binding->scale != 0.0f ? binding->scale : 1.0f);
						}
					}
					break;
			}
		}

		return 0.0f;
	}
		
	void bindAction(u32 id, GameControlType type, GameControlTrigger trigger, u32 primaryId, u32 secondaryId, f32 scale)
	{
		u32 actionIndex = getActionIndex(id);
		if (actionIndex == INVALID_INDEX)
		{
			actionIndex = addActionIndex(id);
		}

		const Binding binding=
		{
			type,
			trigger,
			primaryId,
			secondaryId,
			scale
		};
		s_actionList[actionIndex].bindings.push_back(binding);
	}
		
	///////////////////////////////////////
	// Internal
	///////////////////////////////////////
	u32 getActionIndex(u32 id)
	{
		ActionToIndexMap::iterator iAction = s_actionMap.find(id);
		if (iAction != s_actionMap.end())
		{
			return iAction->second;
		}
		return INVALID_INDEX;
	}

	u32 addActionIndex(u32 id)
	{
		const u32 index = (u32)s_actionList.size();
		s_actionMap[id] = index;

		s_actionList.push_back({ id });
		return index;
	}
}