#pragma once
//////////////////////////////////////////////////////////////////////
// DarkXL 2 Common Rendering Functions
//////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>
struct ColorMap;

#define SEG_MAX 256
#define WALL_MAX 4096
#define MAX_MASK_WALL 4096
#define MAX_SPRITE_SEG 4096
#define MAX_RANGE 32
#define MAX_SECTOR 2048
#define MAX_MASK_HEIGHT 32768
#define PITCH_PIXEL_SCALE 1.5f
#define PITCH_LIMIT 0.78539816339744830962f
#define CONV_6bitTo8bit(x) (((x)<<2) | ((x)>>4))
#define CONV_5bitTo8bit(x) (((x)<<3) | ((x)>>2))
#define PACK_RGB_666_888(r6, g6, b6) CONV_6bitTo8bit(r6) | (CONV_6bitTo8bit(g6)<<8) | (CONV_6bitTo8bit(b6)<<16) | (0xff << 24)
static const f32 c_clip = 0.0001f;
static const f32 c_aspect = 4.0f / 3.0f;
static const f32 c_pixelAspect = 320.0f / 200.0f;
static const f32 c_spriteWorldScale = 1.0f / 65536.0f;
static const f32 c_worldToTexelScale = 8.0f;
static const f32 c_texelToWorldScale = 1.0f / c_worldToTexelScale;
static const f32 c_spriteTexelToWorldScale = c_texelToWorldScale * c_aspect / c_pixelAspect;

enum LightMode
{
	LIGHT_OFF = 0,
	LIGHT_NORMAL,
	LIGHT_BRIGHT,
	LIGHT_COUNT
};

namespace DXL2_RenderCommon
{
	void init(const ColorMap* cmap);
	void enableLightSource(LightMode mode);
	s32 lightFalloff(s32 level, f32 z);
	Vec3f unproject(const Vec2i& screenPos, f32 viewZ);
	Vec3f project(const Vec3f* worldPos);

	f32 getFocalLength();
	f32 getHeightScale();

	void setProjectionParam(s32 width, s32 height, f32 focalLength, f32 heightScale, s32 pitchOffset);
	void setViewParam(const f32* rot, const Vec3f* cameraPos);
}
