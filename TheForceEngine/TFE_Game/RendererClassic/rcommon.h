#pragma once
#include <TFE_System/types.h>
#include "rwall.h"
#include "rlimits.h"

struct ColorMap;
struct EdgePair;

namespace RendererClassic
{
	// Settings
	extern bool s_enableHighPrecision;

	// Resolution
	extern s32 s_width;
	extern s32 s_height;
	extern fixed16_16 s_halfWidth;
	extern fixed16_16 s_halfHeight;
	extern fixed16_16 s_halfHeightBase;
	extern s32 s_heightInPixels;
	extern s32 s_heightInPixelsBase;
	extern s32 s_minScreenY;
	extern s32 s_maxScreenY;
	extern s32 s_screenXMid;

	// Projection
	extern fixed16_16  s_focalLength;
	extern fixed16_16  s_focalLenAspect;
	extern fixed16_16  s_eyeHeight;
	extern fixed16_16* s_depth1d_all;
	extern fixed16_16* s_depth1d;

	// Camera
	extern fixed16_16 s_cameraPosX;
	extern fixed16_16 s_cameraPosZ;
	extern fixed16_16 s_xCameraTrans;
	extern fixed16_16 s_zCameraTrans;
	extern fixed16_16 s_cosYaw;
	extern fixed16_16 s_sinYaw;
	extern fixed16_16 s_negSinYaw;
	extern fixed16_16 s_cameraYaw;
	extern fixed16_16 s_cameraPitch;
	extern fixed16_16 s_skyYawOffset;
	extern fixed16_16 s_skyPitchOffset;
	extern fixed16_16* s_skyTable;
	
	// Window
	extern s32 s_minScreenX;
	extern s32 s_maxScreenX;
	extern s32 s_windowMinX;
	extern s32 s_windowMaxX;
	extern s32 s_windowMinY;
	extern s32 s_windowMaxY;
	extern s32 s_windowMaxCeil;
	extern s32 s_windowMinFloor;
	extern fixed16_16 s_windowMinZ;
	
	// Display
	extern u8* s_display;

	// Render
	extern s32 s_sectorIndex;
	extern RSector* s_prevSector;
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
	extern fixed16_16* s_column_Y_Over_X;
	extern fixed16_16* s_column_X_Over_Y;
	
	// WallSegments
	extern RWallSegment s_wallSegListDst[MAX_SEG];
	extern RWallSegment s_wallSegListSrc[MAX_SEG];
	extern RWallSegment** s_adjoinSegment;

	extern s32 s_nextWall;
	extern s32 s_curWallSeg;
	extern s32 s_adjoinSegCount;
	extern s32 s_adjoinDepth;
	extern s32 s_drawFrame;

	// Flats
	extern s32 s_flatCount;
	extern fixed16_16* s_rcp_yMinusHalfHeight;
	extern s32 s_wallMaxCeilY;
	extern s32 s_wallMinFloorY;
	extern EdgePair* s_flatEdge;
	extern EdgePair  s_flatEdgeList[MAX_SEG];
	extern EdgePair* s_adjoinEdge;
	extern EdgePair  s_adjoinEdgeList[MAX_ADJOIN_SEG];

	// Lighting
	extern const u8* s_colorMap;
	extern const u8* s_lightSourceRamp;
	extern s32 s_sectorAmbient;
	extern s32 s_scaledAmbient;
	extern s32 s_cameraLightSource;
	extern s32 s_worldAmbient;

	// Debug
	extern s32 s_maxWallCount;
	extern s32 s_maxDepthCount;
}
