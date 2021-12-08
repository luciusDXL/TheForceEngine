#pragma once
//////////////////////////////////////////////////////////////////////
// This is TFE specific, but added to TFE_Jedi for convenience.
// 
// Virtual Framebuffer wraps up the screen rect / resolution / virtual
// display handling together to make handling different resolutions in
// the Jedi Renderer and in-game easier.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Jedi/Math/core_math.h>

struct ScreenRect
{
	s32 left;
	s32 top;
	s32 right;
	s32 bot;
};

namespace TFE_Jedi
{
	enum ScreenRectType
	{
		VFB_RECT_RENDER = 0,
		VFB_RECT_UI,
		VFB_RECT_COUNT
	};

	////////////////////////////////////////////////////////////////////////
	// Setup
	// -----------------
	// Note that the current graphics settings are
	// used internally when setting up (GPU palette conversion,etc.)
	////////////////////////////////////////////////////////////////////////
	JBool vfb_setResolution(u32 width, u32 height);
	void vfb_setPalette(const u32* palette);

	////////////////////////////
	// Get Scale Factors
	////////////////////////////
	fixed16_16 vfb_getXScale();
	fixed16_16 vfb_getYScale();
	s32 vfb_getWidescreenOffset();

	////////////////////////////
	// Per-Frame
	////////////////////////////
	// Frame rendering is done, copy the results to GPU memory.
	void vfb_swap();
	void vfb_forceToBlack();

	////////////////////////////
	// Clipping
	////////////////////////////
	void vfb_setScreenRect(ScreenRectType type, ScreenRect* rect);
	void vfb_restoreScreenRect(ScreenRectType type);

	////////////////////////////
	// Query
	////////////////////////////
	// Get the CPU buffer for rendering.
	u8* vfb_getCpuBuffer();
	// Get the valid screen rect for the current mode.
	ScreenRect* vfb_getScreenRect(ScreenRectType type);
	void vfb_getResolution(u32* width, u32* height);
	// Returns the stride for rendering stride
	u32 vfb_getStride();
}  // namespace TFE_Jedi