#include "rcommon.h"

namespace RendererClassic
{
	// Resolution
	s32 s_width;
	s32 s_height;
	s32 s_halfWidth;
	s32 s_halfHeight;

	// Projection
	s32 s_focalLength;
	s32 s_eyeHeight;
	s32* s_depth1d = nullptr;

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
	u8* s_columnTop = nullptr;
	u8* s_columnBot = nullptr;
	u8* s_windowTop = nullptr;
	u8* s_windowBot = nullptr;
	s32* s_column_Y_Over_X = nullptr;
	s32* s_column_X_Over_Y = nullptr;

	// Segment list.
	RWallSegment s_wallSegListDst[MAX_SEG];
	RWallSegment s_wallSegListSrc[MAX_SEG];

	s32 s_nextWall;

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
