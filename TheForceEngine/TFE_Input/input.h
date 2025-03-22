#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Input Library
// A unified input repository that can be accessed by different
// systems. MAIN updates the raw input and this system organizes the
// data to make it easy for other systems to access.
// 
// Due to the global access of the low level input, this is a
// namespace rather than a static class or "singleton"
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Input/inputEnum.h>

typedef void(*KeyBindingCallback)(f32 value);

namespace TFE_Input
{
	// Call this once at the end of each frame
	// to reset transient key events.
	void endFrame();

	// Set, from the OS
	void setAxis(Axis axis, f32 value);
	void setButtonDown(Button button);
	void setButtonUp(Button button);

	void setKeyDown(KeyboardCode key, bool repeat);
	void setKeyUp(KeyboardCode key);

	void setMouseButtonDown(MouseButton button);
	void setMouseButtonUp(MouseButton button);
	void setMouseWheel(s32 dx, s32 dy);

	void setRelativeMousePos(s32 x, s32 y);
	void setMousePos(s32 x, s32 y);

	void enableRelativeMode(bool enable);

	// Buffered Input
	void setBufferedInput(const char* text);
	void setBufferedKey(KeyboardCode key);

	// Get
	f32 getAxis(Axis axis);
	void getMouseMove(s32* x, s32* y);
	void getMouseMoveAccum(s32* x, s32* y);
	void getAccumulatedMouseMove(s32* x, s32* y);
	void getMousePos(s32* x, s32* y);
	void getMouseWheel(s32* dx, s32* dy);
	bool buttonDown(Button button);
	bool buttonPressed(Button button);
	bool keyDown(KeyboardCode key);
	bool keyPressed(KeyboardCode key);
	bool keyPressedWithRepeat(KeyboardCode key);
	bool keyModDown(KeyModifier keyMod, bool allowAltOnNone = false);
	bool mouseDown(MouseButton button);
	bool mousePressed(MouseButton button);
	bool relativeModeEnabled();
	void setRepeating(bool repeat);
	bool isRepeating();
	void setKeyPress(KeyboardCode key);
	void setKeyPressRepeat(KeyboardCode key);
	void clearKeyPressed(KeyboardCode key);
	void clearMouseButtonPressed(MouseButton btn);
	void clearAccumulatedMouseMove();
	// Buffered Input
	const char* getBufferedText();
	bool bufferedKeyDown(KeyboardCode key);

	KeyboardCode getKeyPressed(bool ignoreModKeys = false);
	KeyModifier  getKeyModifierDown();
	Button getControllerButtonPressed();
	Axis getControllerAnalogDown();
	MouseButton getMouseButtonPressed();
	
	bool loadKeyNames(const char* path);
	const char* getControllerAxisName(Axis axis);
	const char* getControllButtonName(Button button);
	const char* getMouseAxisName(MouseAxis axis);
	const char* getMouseButtonName(MouseButton button);
	const char* getMouseWheelName(MouseWheel axis);
	const char* getKeyboardName(KeyboardCode key);
	const char* getKeyboardModifierName(KeyModifier mod);
};
