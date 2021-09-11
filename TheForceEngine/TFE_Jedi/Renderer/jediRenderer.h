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
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Level/rtexture.h>

enum TFE_SubRenderer
{
	TSR_CLASSIC_FIXED = 0,	// The Reverse-Engineered DOS Jedi Renderer, using 16.16 fixed point.
	TSR_CLASSIC_FLOAT,		// Derived from the Reverse-Engineered Jedi Renderer but using floating point.
	TSR_CLASSIC_GPU,		// Derived from the Reverse-Engineered Jedi Renderer but using the GPU for low-level rendering.
	TSR_COUNT,
	TSR_INVALID,			// Invalid sub-renderer.
};

struct ScreenRect
{
	s32 left;
	s32 top;
	s32 right;
	s32 bot;
};

namespace TFE_Jedi
{
	void renderer_init();
	void renderer_destroy();
	void setupInitCameraAndLights();

	// Set the current sub-renderer.
	// Note that changing the sub-renderer at runtime may result in re-initialization of rendering data, causing a hitch.
	void setSubRenderer(TFE_SubRenderer subRenderer = TSR_CLASSIC_FIXED);

	// Camera parameters: yaw, pitch, position (x, y, z)
	//                    sectorId containing the camera.
	//                    ambient base or offset (default = MAX_LIGHT_LEVEL).
	//                    camera light source - true if there is a camera based light source, in which case worldAmbient is treated as an offset.
	//											false if there is no camera light source, worldAmbient is then the base ambient.
	//void setCamera(f32 yaw, f32 pitch, f32 x, f32 y, f32 z, s32 sectorId, s32 worldAmbient = 0, bool cameraLightSource = false);
	// Draw the scene to the passed in display using the colormap for shading.
	void drawWorld(u8* display, RSector* sector, const u8* colormap, const u8* lightSourceRamp);

	// Setup the currently loaded level for rendering at the specified resolution.
	void setupLevel(s32 width, s32 height);
	// Set the current resolution to render, this may involve regenerating lookup-tables, depending on the sub-renderer.
	void setResolution(s32 width, s32 height);

	void renderer_setVisionEffect(s32 effect);
	void renderer_setupCameraLight(JBool flatShading, JBool headlamp);
	void renderer_setWorldAmbient(s32 value);

	// 2D
	void blitTextureToScreen(TextureData* texture, s32 x0, s32 y0);
	void clear3DView(u8* framebuffer);

	extern ScreenRect s_screenRect;
	extern s32 s_drawnSpriteCount;
	extern SecObject* s_drawnSprites[];
}