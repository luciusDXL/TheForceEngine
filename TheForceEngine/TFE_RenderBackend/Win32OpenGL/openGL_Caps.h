#pragma once
//////////////////////////////////////////////////////////////////////
// A central place to store OpenGL capabilities for the current device.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>

enum DeviceTier
{
	DEV_TIER_0 = 0,	// Cannot support Gpu blit, use GDI.
	DEV_TIER_1,		// Can support Gpu blit.
	DEV_TIER_2,		// Can support Gpu renderer & Gpu color correction.
	DEV_TIER_3,		// Can support Gpu Compute renderer.
};

namespace OpenGL_Caps
{
	void queryCapabilities();

	bool supportsPbo();
	bool supportsVbo();
	bool supportsFbo();
	bool supportsNonPow2Textures();
	bool supportsTextureArrays();

	bool deviceSupportsGpuBlit();
	bool deviceSupportsGpuColorConversion();
	bool deviceSupportsGpuRenderer();

	u32 getDeviceTier();
	s32 getMaxTextureBufferSize();
	f32 getMaxAnisotropy();
	f32 getAnisotropyFromQuality(f32 quality);
};
