#include "virtualFrameBuffer.h"
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Settings/settings.h>

namespace TFE_Jedi
{
	static u8  s_frameBuffer320x200[320 * 200];
	static u8* s_frameBuffer = nullptr;
	static u8* s_curFrameBuffer = nullptr;

	static u32 s_width  = 0;
	static u32 s_height = 0;
	static u32 s_prevWidth = 0;
	static u32 s_prevHeight = 0;

	static ScreenRect s_screenRect[VFB_RECT_COUNT];

	void vfb_createVirtualDisplay(u32 width, u32 height);
		
	////////////////////////////////////////////////////////////////////////
	// Setup
	// -----------------
	// Note that the current graphics settings are
	// used internally when setting up (GPU palette conversion,etc.)
	////////////////////////////////////////////////////////////////////////
	void vfb_setResolution(u32 width, u32 height)
	{
		if (width == s_width && height == s_height)
		{
			return;
		}

		if (width == 320 && height == 200)
		{
			s_curFrameBuffer = s_frameBuffer320x200;
			s_width  = width;
			s_height = height;
			vfb_createVirtualDisplay(width, height);
		}
		else
		{
			s_width = width;
			s_height = height;
			s_prevWidth = s_width;
			s_prevHeight = s_height;

			free(s_frameBuffer);
			s_frameBuffer = (u8*)malloc(s_width * s_height);
			s_curFrameBuffer = s_frameBuffer;
			vfb_createVirtualDisplay(width, height);
		}

		s_screenRect[VFB_RECT_UI] =
		{
			0,
			0,
			s32(s_width) - 1,
			s32(s_height) - 1,
		};

		s_screenRect[VFB_RECT_RENDER] =
		{
			0,
			1,
			s32(s_width) - 1,
			s32(s_height) - 2,
		};
	}

	void vfb_setPalette(const u32* palette)
	{
		TFE_RenderBackend::setPalette(palette);
	}

	////////////////////////////
	// Per-Frame
	////////////////////////////
	// Frame rendering is done, copy the results to GPU memory.
	void vfb_swap()
	{
		TFE_RenderBackend::updateVirtualDisplay(s_curFrameBuffer, s_width * s_height);
	}

	////////////////////////////
	// Query
	////////////////////////////
	// Get the CPU buffer for rendering.
	u8* vfb_getCpuBuffer()
	{
		return s_curFrameBuffer;
	}

	// Get the valid screen rect for the current mode.
	ScreenRect* vfb_getScreenRect(ScreenRectType type)
	{
		return &s_screenRect[type];
	}

	// Returns 'true' if using square pixels, otherwise 'false'.
	void vfb_getResolution(u32* width, u32* height)
	{
		*width  = s_width;
		*height = s_height;
	}

	// Returns the stride for rendering stride
	u32 vfb_getStride()
	{
		return s_width;
	}

	////////////////////////////
	// Internal
	////////////////////////////
	void vfb_createVirtualDisplay(u32 width, u32 height)
	{
		// Setup or update the virtual display.
		TFE_Settings_Graphics* graphics = TFE_Settings::getGraphicsSettings();
		u32 vdispFlags = 0;
		if (graphics->asyncFramebuffer)
		{
			vdispFlags |= VDISP_ASYNC_FRAMEBUFFER;
		}
		if (graphics->gpuColorConvert)
		{
			vdispFlags |= VDISP_GPU_COLOR_CONVERT;
		}

		// TFE Specific: always use 320x200 when rendering in-game UI (for now).
		VirtualDisplayInfo vdisp =
		{
			DMODE_ASPECT_CORRECT,	// Output display mode.
			vdispFlags,				// See VirtualDisplayFlags.

			width,	// full width
			height,	// full height
			width,	// width for 2D game UI and cutscenes.
			height,	// width for 3D drawing.
		};
		TFE_RenderBackend::createVirtualDisplay(vdisp);
	}
}  // namespace TFE_Jedi