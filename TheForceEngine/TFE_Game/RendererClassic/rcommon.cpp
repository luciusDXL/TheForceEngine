#include "rcommon.h"
#include "rflat.h"

namespace RendererClassic
{
	// Resolution
	s32 s_width;
	s32 s_height;
	s32 s_halfWidth;
	s32 s_halfHeight;
	s32 s_minScreenY;
	s32 s_maxScreenY;
	s32 s_screenXMid;

	// Projection
	s32 s_focalLength;
	s32 s_eyeHeight;
	s32* s_depth1d = nullptr;

	// Camera
	s32 s_cameraPosX;
	s32 s_cameraPosZ;
	s32 s_xCameraTrans;
	s32 s_zCameraTrans;
	s32 s_cosYaw;
	s32 s_sinYaw;
	s32 s_negSinYaw;

	// Window
	s32 s_minScreenX;
	s32 s_maxScreenX;
	s32 s_windowMinX;
	s32 s_windowMaxX;
	s32 s_windowMinY;
	s32 s_windowMaxY;
	s32 s_minSegZ;

	// Display
	u8* s_display;

	// Column Heights
	s32* s_columnTop = nullptr;
	s32* s_columnBot = nullptr;
	u8* s_windowTop = nullptr;
	u8* s_windowBot = nullptr;
	s32* s_column_Y_Over_X = nullptr;
	s32* s_column_X_Over_Y = nullptr;

	// Segment list.
	RWallSegment s_wallSegListDst[MAX_SEG];
	RWallSegment s_wallSegListSrc[MAX_SEG];

	s32 s_nextWall;

	// Flats
	s32 s_flatCount;
	s32* s_rcp_yMinusHalfHeight;
	s32 s_wallMaxCeilY;
	s32 s_wallMinFloorY;
	s32 s_yMin;
	s32 s_yMax;
	FlatEdges* s_lowerFlatEdge;
	FlatEdges  s_lowerFlatEdgeList[MAX_SEG];

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
