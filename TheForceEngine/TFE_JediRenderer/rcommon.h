#pragma once
#include <TFE_System/types.h>
#include "rlimits.h"
#include "rwall.h"

struct ColorMap;

namespace TFE_JediRenderer
{
	struct EdgePair;

	// Resolution
	extern s32 s_width;
	extern s32 s_height;
	extern f32 s_halfWidth;
	extern f32 s_halfHeight;
	extern f32 s_halfHeightBase;
	extern s32 s_heightInPixels;
	extern s32 s_heightInPixelsBase;
	extern s32 s_minScreenY;
	extern s32 s_maxScreenY;
	extern s32 s_screenXMid;

	// Projection
	extern f32  s_focalLength;
	extern f32  s_focalLenAspect;
	extern f32  s_aspectScaleX;
	extern f32  s_aspectScaleY;
	extern f32  s_eyeHeight;
	extern f32* s_depth1d_all;
	extern f32* s_depth1d;

	// Camera
	extern f32 s_cameraPosX;
	extern f32 s_cameraPosY;
	extern f32 s_cameraPosZ;
	extern f32 s_xCameraTrans;
	extern f32 s_zCameraTrans;
	extern f32 s_cosYaw;
	extern f32 s_sinYaw;
	extern f32 s_negSinYaw;
	extern f32 s_cameraYaw;
	extern f32 s_cameraPitch;
	extern f32 s_yPlaneTop;
	extern f32 s_yPlaneBot;
	extern f32 s_skyYawOffset;
	extern f32 s_skyPitchOffset;
	extern f32 s_nearPlaneHalfLen;
	extern f32* s_skyTable;
	extern f32 s_cameraMtx[9];
	
	// Window
	extern s32 s_minScreenX;
	extern s32 s_maxScreenX;
	extern s32 s_windowMinX;
	extern s32 s_windowMaxX;
	extern s32 s_windowMinY;
	extern s32 s_windowMaxY;
	extern s32 s_windowMaxCeil;
	extern s32 s_windowMinFloor;
	extern f32 s_windowMinZ;
	
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

	extern RWallSegment s_wallSegListDst[MAX_SEG];
	extern RWallSegment s_wallSegListSrc[MAX_SEG];
	extern RWallSegment** s_adjoinSegment;

	// Flats
	extern s32 s_flatCount;
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
	extern s32 s_sectorAmbientFraction;
	extern s32 s_lightCount;	// Number of directional lights that affect 3D objects.

	// Debug
	extern s32 s_maxWallCount;
	extern s32 s_maxDepthCount;
}
