#include "rcommon.h"
#include "redgePair.h"

namespace RendererClassic
{
	// Resolution
	s32 s_width = 0;
	s32 s_height = 0;
	fixed16 s_halfWidth;
	fixed16 s_halfHeight;
	fixed16 s_halfHeightBase;
	s32 s_heightInPixels;
	s32 s_heightInPixelsBase;
	s32 s_minScreenY;
	s32 s_maxScreenY;
	s32 s_screenXMid;

	// Projection
	fixed16 s_focalLength;
	fixed16 s_focalLenAspect;
	fixed16 s_eyeHeight;
	fixed16* s_depth1d_all = nullptr;
	fixed16* s_depth1d = nullptr;

	// Camera
	fixed16 s_cameraPosX;
	fixed16 s_cameraPosZ;
	fixed16 s_xCameraTrans;
	fixed16 s_zCameraTrans;
	fixed16 s_cosYaw;
	fixed16 s_sinYaw;
	fixed16 s_negSinYaw;

	// Window
	s32 s_minScreenX;
	s32 s_maxScreenX;
	s32 s_windowMinX;
	s32 s_windowMaxX;
	s32 s_windowMinY;
	s32 s_windowMaxY;
	s32 s_windowMaxCeil;
	s32 s_windowMinFloor;
	fixed16 s_minSegZ;

	// Display
	u8* s_display;

	// Render
	s32 s_sectorIndex;
	RSector* s_prevSector;
	s32 s_maxAdjoinIndex;
	s32 s_adjoinIndex;
	s32 s_maxAdjoinDepth;
	s32 s_windowX0;
	s32 s_windowX1;

	// Column Heights
	s32* s_columnTop = nullptr;
	s32* s_columnBot = nullptr;
	s32* s_windowTop_all = nullptr;
	s32* s_windowBot_all = nullptr;
	s32* s_windowTop = nullptr;
	s32* s_windowBot = nullptr;
	fixed16* s_column_Y_Over_X = nullptr;
	fixed16* s_column_X_Over_Y = nullptr;

	// Segment list.
	RWallSegment s_wallSegListDst[MAX_SEG];
	RWallSegment s_wallSegListSrc[MAX_SEG];
	RWallSegment** s_adjoinSegment;

	s32 s_nextWall;
	s32 s_curWallSeg;
	s32 s_adjoinSegCount;
	s32 s_adjoinDepth;
	s32 s_drawFrame;

	// Flats
	s32 s_flatCount;
	fixed16* s_rcp_yMinusHalfHeight;
	s32 s_wallMaxCeilY;
	s32 s_wallMinFloorY;
	EdgePair* s_flatEdge;
	EdgePair  s_flatEdgeList[MAX_SEG];
	EdgePair* s_adjoinEdge;
	EdgePair  s_adjoinEdgeList[MAX_ADJOIN_SEG];

	// Lighting
	const u8* s_colorMap;
	const u8* s_lightSourceRamp;
	s32 s_sectorAmbient;
	s32 s_scaledAmbient;
	s32 s_cameraLightSource;
	s32 s_worldAmbient;

	// Debug
	s32 s_maxWallCount;
}
