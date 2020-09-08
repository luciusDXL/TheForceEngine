#pragma once
//////////////////////////////////////////////////////////////////////
// Renderer - Classic
// Classic Dark Forces (DOS) derived renderer.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Asset/colormapAsset.h>

enum TFE_SubRenderer
{
	TSR_CLASSIC_FIXED = 0,	// The Reverse-Engineered DOS Jedi Renderer, using 16.16 fixed point.
	TSR_CLASSIC_FLOAT,		// Derived from the Reverse-Engineered Jedi Renderer but using floating point.
	TSR_CLASSIC_GPU,		// Derived from the Reverse-Engineered Jedi Renderer but using the GPU for low-level rendering.
	TSR_COUNT
};

namespace TFE_JediRenderer
{
	void init();
	void destroy();

	// Set the current sub-renderer.
	// Note that changing the sub-renderer at runtime may result in re-initialization of rendering data, causing a hitch.
	void setSubRenderer(TFE_SubRenderer subRenderer = TSR_CLASSIC_FIXED);

	// Camera parameters: yaw, pitch, position (x, y, z)
	//                    sectorId containing the camera.
	//                    ambient offset (0 = default).
	//                    camera light source.
	void setCamera(f32 yaw, f32 pitch, f32 x, f32 y, f32 z, s32 sectorId, s32 worldAmbient = 0, bool cameraLightSource = false);
	void draw(u8* display, const ColorMap* colormap);

	void setupLevel(s32 width, s32 height);
	void setResolution(s32 width, s32 height);
}