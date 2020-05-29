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

	// Window
	s32 s_minScreenX;
	s32 s_maxScreenX;

	// Segment list.
	RWallSegment s_wallSegListDst[MAX_SEG];
	RWallSegment s_wallSegListSrc[MAX_SEG];

	s32 s_nextWall;
}
