#include "rcommon.h"
#include "redgePair.h"

struct SecObject;

namespace TFE_Jedi
{
	// Resolution
	s32 s_width = 320;
	s32 s_height = 200;
	s32 s_screenYMid;
	s32 s_screenYMidBase;
	s32 s_minScreenY;
	s32 s_maxScreenY;
	s32 s_screenXMid;

	// Window
	s32 s_minScreenX_Pixels;
	s32 s_maxScreenX_Pixels;
	s32 s_windowMinX_Pixels;
	s32 s_windowMaxX_Pixels;
	s32 s_windowMinY_Pixels;
	s32 s_windowMaxY_Pixels;
	s32 s_windowMaxCeil;
	s32 s_windowMinFloor;
	s32 s_screenWidth;

	// Display
	u8* s_display;

	// Render
	RSector* s_prevSector;
	s32 s_sectorIndex;
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
	s32* s_windowTopPrev = nullptr;
	s32* s_windowBotPrev = nullptr;

	s32* s_objWindowTop = nullptr;
	s32* s_objWindowBot = nullptr;

	// Segment list.
	s32 s_nextWall;
	s32 s_curWallSeg;
	s32 s_adjoinSegCount;
	s32 s_adjoinDepth;
	s32 s_drawFrame = 0;

	// Flats
	s32 s_flatCount;
	s32 s_wallMaxCeilY;
	s32 s_wallMinFloorY;
		
	// Lighting
	const u8* s_colorMap = nullptr;
	const u8* s_lightSourceRamp = nullptr;
	s32 s_flatAmbient = 0;
	s32 s_sectorAmbient;
	s32 s_scaledAmbient;
	s32 s_cameraLightSource;
	JBool s_enableFlatShading;
	s32 s_worldAmbient;
	s32 s_sectorAmbientFraction;
	s32 s_lightCount = 3;
	JBool s_flatLighting = JFALSE;

	// Debug
	s32 s_maxWallCount;
	s32 s_maxDepthCount;

	s32 s_drawnSpriteCount;
	SecObject* s_drawnSprites[MAX_DRAWN_SPRITE_STORE];
}
