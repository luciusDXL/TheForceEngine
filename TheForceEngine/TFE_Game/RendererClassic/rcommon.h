#pragma once
#include <TFE_System/types.h>
#include "rwall.h"

struct ColorMap;
struct FlatEdges;
#define LIGHT_SOURCE_LEVELS 128

namespace RendererClassic
{
	// Resolution
	extern s32 s_width;
	extern s32 s_height;
	extern fixed16 s_halfWidth;
	extern fixed16 s_halfHeight;
	extern fixed16 s_halfHeightBase;
	extern s32 s_heightInPixels;
	extern s32 s_heightInPixelsBase;
	extern s32 s_minScreenY;
	extern s32 s_maxScreenY;
	extern s32 s_screenXMid;

	// Projection
	extern fixed16  s_focalLength;
	extern fixed16  s_focalLenAspect;
	extern fixed16  s_eyeHeight;
	extern fixed16* s_depth1d;

	// Camera
	extern fixed16 s_cameraPosX;
	extern fixed16 s_cameraPosZ;
	extern fixed16 s_xCameraTrans;
	extern fixed16 s_zCameraTrans;
	extern fixed16 s_cosYaw;
	extern fixed16 s_sinYaw;
	extern fixed16 s_negSinYaw;

	// Window
	extern s32 s_minScreenX;
	extern s32 s_maxScreenX;
	extern s32 s_windowMinX;
	extern s32 s_windowMaxX;
	extern s32 s_windowMinY;
	extern s32 s_windowMaxY;
	extern fixed16 s_minSegZ;
	
	// Display
	extern u8* s_display;

	// Column Heights
	extern s32* s_columnTop;
	extern s32* s_columnBot;
	extern s32* s_windowTop;
	extern s32* s_windowBot;
	extern fixed16* s_column_Y_Over_X;
	extern fixed16* s_column_X_Over_Y;
	
	// WallSegments
	extern RWallSegment s_wallSegListDst[MAX_SEG];
	extern RWallSegment s_wallSegListSrc[MAX_SEG];

	extern s32 s_nextWall;
	extern s32 s_curWallSeg;
	extern s32 s_drawFrame;

	// Flats
	extern s32 s_flatCount;
	extern fixed16* s_rcp_yMinusHalfHeight;
	extern s32 s_wallMaxCeilY;
	extern s32 s_wallMinFloorY;
	extern s32 s_yMin;
	extern s32 s_yMax;
	extern FlatEdges* s_lowerFlatEdge;
	extern FlatEdges  s_lowerFlatEdgeList[MAX_SEG];

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
