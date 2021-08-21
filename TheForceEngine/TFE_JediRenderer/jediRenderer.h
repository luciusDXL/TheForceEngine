#pragma once
//////////////////////////////////////////////////////////////////////
// Jedi Renderer
// Classic Dark Forces (DOS) Jedi derived renderer.
//
// Copyright note:
// While the project as a whole is licensed under GPL-2.0, some of the
// code under TFE_JediRenderer/ was derived from reverse-engineered
// code from "Dark Forces" (DOS) which is copyrighted by LucasArts.
// The original renderer is contained in RClassic_Fixed/ and the
// RClassic_Float/ and RClassic_GPU/ sub-renderers are derived from
// that code.
//
// I consider the reverse-engineering to be "Fair Use" - a means of 
// supporting the games on other platforms and to improve support on
// existing platforms without claiming ownership of the games
// themselves or their IPs.
//
// That said using this code in a commercial project is risky without
// permission of the original copyright holders (LucasArts).
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Asset/colormapAsset.h>
#include <TFE_Level/core_math.h>
#include <TFE_Level/rtexture.h>

enum TFE_SubRenderer
{
	TSR_CLASSIC_FIXED = 0,	// The Reverse-Engineered DOS Jedi Renderer, using 16.16 fixed point.
	TSR_CLASSIC_FLOAT,		// Derived from the Reverse-Engineered Jedi Renderer but using floating point.
	TSR_CLASSIC_GPU,		// Derived from the Reverse-Engineered Jedi Renderer but using the GPU for low-level rendering.
	TSR_COUNT,
	TSR_INVALID,			// Invalid sub-renderer.
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
	//                    ambient base or offset (default = MAX_LIGHT_LEVEL).
	//                    camera light source - true if there is a camera based light source, in which case worldAmbient is treated as an offset.
	//											false if there is no camera light source, worldAmbient is then the base ambient.
	void setCamera(f32 yaw, f32 pitch, f32 x, f32 y, f32 z, s32 sectorId, s32 worldAmbient = 0, bool cameraLightSource = false);
	// Draw the scene to the passed in display using the colormap for shading.
	void draw(u8* display, const ColorMap* colormap);

	// Setup the currently loaded level for rendering at the specified resolution.
	void setupLevel(s32 width, s32 height);
	// Set the current resolution to render, this may involve regenerating lookup-tables, depending on the sub-renderer.
	void setResolution(s32 width, s32 height);

	// 2D
	void blitTextureToScreen(TextureData* texture, s32 x0, s32 y0);
}