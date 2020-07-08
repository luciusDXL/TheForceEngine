#include "renderCommon.h"
#include <TFE_Asset/colormapAsset.h>
#include <algorithm>

namespace TFE_RenderCommon
{
	static LightMode s_lightMode = LIGHT_OFF;
	static const ColorMap* s_colorMap = nullptr;
	static bool s_nightVision = false;
	static bool s_grayScale = false;
		
	void init(const ColorMap* cmap)
	{
		s_colorMap = cmap;
		s_lightMode = LIGHT_OFF;
	}

	void enableLightSource(LightMode mode)
	{
		s_lightMode = mode;
	}

	void enableNightVision(bool enable)
	{
		s_nightVision = enable;
	}
		
	void enableGrayScale(bool enable)
	{
		s_grayScale = enable;
		s_nightVision = false;
	}

	LightMode getLightMode()
	{
		return s_lightMode;
	}

	bool isNightVisionEnabled()
	{
		return s_nightVision;
	}

	bool isGrayScaleEnabled()
	{
		return s_grayScale;
	}
		
	s32 lightFalloff(s32 level, f32 z, bool brightObj)
	{
		s32 maxFalloff = 3;
		//level = level - std::min(maxFalloff, std::max(0, s32((z - 6) / 8.0f)));

		if (s_nightVision)
		{
			level = brightObj ? 31 : 18;
		}

		f32 scale = f32(level) / 256.0f;
		level = level - std::min(maxFalloff, std::max(0, s32((z - 4) * scale)));

		// Add in the headlamp if enabled.
		if (s_lightMode == LIGHT_NORMAL && !s_nightVision)
		{
			s32 lightZ = 127 - std::max(0, std::min(s32((z - 2)*6.0f), 127));
			level += s_colorMap->lightSourceRamp[lightZ];
		}
		else if (s_lightMode == LIGHT_BRIGHT && !s_nightVision)
		{
			s32 lightZ = 127 - std::max(0, std::min(s32((z - 2)*3.0f), 127));
			level += s_colorMap->lightSourceRamp[lightZ];
		}

		return std::max(0, std::min(level, 31));
	}

	struct Projection
	{
		s32 w, h;
		s32 hw, hh;
		f32 focalLength;
		f32 heightScale;
		s32 pitchOffset;
	};
	struct View
	{
		f32 rot[2];
		Vec3f cameraPos;
	};
	static Projection s_proj;
	static View s_view;

	void setProjectionParam(s32 width, s32 height, f32 focalLength, f32 heightScale, s32 pitchOffset)
	{
		s_proj.w = width;
		s_proj.h = height;
		s_proj.focalLength = focalLength;
		s_proj.heightScale = heightScale;
		s_proj.pitchOffset = pitchOffset;

		s_proj.hw = s_proj.w / 2;
		s_proj.hh = s_proj.h / 2;
	}

	void setViewParam(const f32* rot, const Vec3f* cameraPos)
	{
		s_view.rot[0] = rot[0];
		s_view.rot[1] = rot[1];
		s_view.cameraPos = *cameraPos;
	}

	Vec3f unproject(const Vec2i& screenPos, f32 viewZ)
	{
		// Convert to view space.
		f32 vx = f32(screenPos.x - s_proj.hw) * viewZ / s_proj.focalLength;
		f32 vy = f32(screenPos.z - s_proj.pitchOffset) * viewZ / s_proj.heightScale;
		f32 vz = viewZ;

		// Then convert to world space.
		Vec3f worldPos;
		worldPos.x = vx * s_view.rot[0] - vz * s_view.rot[1] + s_view.cameraPos.x;
		worldPos.z = vx * s_view.rot[1] + vz * s_view.rot[0] + s_view.cameraPos.z;
		worldPos.y = vy + s_view.cameraPos.y;

		return worldPos;
	}

	f32 getFocalLength()
	{
		return s_proj.focalLength;
	}

	f32 getHeightScale()
	{
		return s_proj.heightScale;
	}

	Vec3f project(const Vec3f* worldPos)
	{
		// first rotate based on camera.
		f32 x = worldPos->x - s_view.cameraPos.x;
		f32 y = worldPos->y - s_view.cameraPos.y;
		f32 z = worldPos->z - s_view.cameraPos.z;

		f32 vx =  x * s_view.rot[0] + z * s_view.rot[1];
		f32 vy =  y;
		f32 vz = -x * s_view.rot[1] + z * s_view.rot[0];

		if (vz >= 1.0f)
		{
			f32 rz = 1.0f / vz;
			f32 x = s_proj.hw + vx*s_proj.focalLength * rz;
			f32 y = s_proj.pitchOffset + vy*s_proj.heightScale * rz;
			return { x, y, rz };
		}
		return { -1, -1, -1 };
	}
}
