#pragma once
#include <TFE_System/types.h>
#include "rwall.h"

struct ColorMap;
#define LIGHT_SOURCE_LEVELS 128

namespace RendererClassic
{
	// Resolution
	extern s32 s_width;
	extern s32 s_height;
	extern s32 s_halfWidth;
	extern s32 s_halfHeight;

	// Projection
	extern s32  s_focalLength;
	extern s32  s_eyeHeight;
	extern s32* s_depth1d;

	// Window
	extern s32 s_minScreenX;
	extern s32 s_maxScreenX;
	extern s32 s_windowMinX;
	extern s32 s_windowMaxX;
	extern s32 s_windowMinY;
	extern s32 s_windowMaxY;
	extern s32 s_minSegZ;
	
	// Display
	extern u8* s_display;

	// Column Heights
	extern u8* s_columnTop;
	extern u8* s_columnBot;
	extern u8* s_windowTop;
	extern u8* s_windowBot;
	extern s32* s_column_Y_Over_X;
	extern s32* s_column_X_Over_Y;
	
	// WallSegments
	extern RWallSegment s_wallSegListDst[MAX_SEG];
	extern RWallSegment s_wallSegListSrc[MAX_SEG];

	extern s32 s_nextWall;

	// Flats
	extern s32 s_wallCount;
	extern s32* s_rcp_yMinusHalfHeight;

	// Lighting
	extern const u8* s_colorMap;
	extern const u8* s_lightSourceRamp;
	extern s32 s_sectorAmbient;
	extern s32 s_scaledAmbient;
	extern s32 s_cameraLightSource;
	extern s32 s_worldAmbient;

	// Debug
	extern s32 s_maxWallCount;
}
