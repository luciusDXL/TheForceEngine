#pragma once
#include <TFE_System/types.h>
#include "rlimits.h"
#include "rwallRender.h"

struct ColorMap;

namespace TFE_Jedi
{
	// Resolution
	extern s32 s_width;
	extern s32 s_height;
	extern s32 s_screenYMidFix;
	extern s32 s_screenYMidFlt;
	extern s32 s_screenYMidBase;
	extern s32 s_minScreenY;
	extern s32 s_maxScreenY;
	extern s32 s_screenXMid;
	
	// Window
	extern s32 s_minScreenX_Pixels;
	extern s32 s_maxScreenX_Pixels;
	extern s32 s_windowMinX_Pixels;
	extern s32 s_windowMaxX_Pixels;
	extern s32 s_windowMinY_Pixels;
	extern s32 s_windowMaxY_Pixels;
	extern s32 s_windowMaxCeil;
	extern s32 s_windowMinFloor;
	extern s32 s_screenWidth;
	
	// Display
	extern u8* s_display;

	// Render
	extern RSector* s_prevSector;
	extern s32 s_sectorIndex;
	extern s32 s_maxAdjoinIndex;
	extern s32 s_adjoinIndex;
	extern s32 s_maxAdjoinDepth;
	extern s32 s_windowX0;
	extern s32 s_windowX1;

	// Column Heights
	extern s32* s_columnTop;
	extern s32* s_columnBot;
	extern s32* s_windowTop_all;
	extern s32* s_windowBot_all;
	extern s32* s_windowTop;
	extern s32* s_windowBot;
	extern s32* s_windowTopPrev;
	extern s32* s_windowBotPrev;

	extern s32* s_objWindowTop;
	extern s32* s_objWindowBot;
	
	// WallSegments
	extern s32 s_nextWall;
	extern s32 s_curWallSeg;
	extern s32 s_adjoinSegCount;
	extern s32 s_adjoinDepth;
	extern s32 s_drawFrame;
		
	// Flats
	extern s32 s_flatCount;
	extern s32 s_wallMaxCeilY;
	extern s32 s_wallMinFloorY;
	
	// Lighting
	extern const u8* s_colorMap;
	extern const u8* s_lightSourceRamp;
	extern s32 s_flatAmbient;
	extern s32 s_sectorAmbient;
	extern s32 s_scaledAmbient;
	extern s32 s_cameraLightSource;
	extern JBool s_enableFlatShading;
	extern s32 s_worldAmbient;
	extern s32 s_sectorAmbientFraction;
	extern s32 s_lightCount;	// Number of directional lights that affect 3D objects.

	extern JBool s_flatLighting;

	// Limits
	extern s32 s_maxAdjoinSegCount;
	extern s32 s_maxAdjoinDepthRecursion;

	// Debug
	extern s32 s_maxWallCount;
	extern s32 s_maxDepthCount;

	// Common functions
	void sprite_decompressColumn(const u8* colData, u8* outBuffer, s32 height);
}
