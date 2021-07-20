#include <TFE_Level/level.h>
#include <TFE_RenderBackend/renderBackend.h>
#include "../rcommon.h"
#include "rlightingFloat.h"
#include "../fixedPoint.h"

namespace TFE_JediRenderer
{

namespace RClassic_Float
{
	void computeSkyOffsets();

	void setCamera(f32 yaw, f32 pitch, f32 x, f32 y, f32 z, s32 sectorId)
	{
		s_cosYaw = cosf(yaw);
		s_sinYaw = sinf(yaw);
		s_negSinYaw = -s_sinYaw;
		s_cameraYaw = yaw;
		s_cameraPitch = pitch;

		s_zCameraTrans = -z*s_cosYaw - x*s_negSinYaw;
		s_xCameraTrans = -x*s_cosYaw - z*s_sinYaw;
		s_eyeHeight = y;

		s_cameraPosX = x;
		s_cameraPosY = y;
		s_cameraPosZ = z;

		s_cameraMtx[0] = s_cosYaw;
		s_cameraMtx[2] = s_negSinYaw;
		s_cameraMtx[4] = 1.0f;
		s_cameraMtx[6] = s_sinYaw;
		s_cameraMtx[8] = s_cosYaw;

		const f32 pitchOffset = sinf(pitch) * s_focalLenAspect;
		s_halfHeight = s_halfHeightBase + pitchOffset;
		s_heightInPixels = s_heightInPixelsBase + floorFloat(pitchOffset);
		computeSkyOffsets();

		const f32 yMaxFixed = f32(s_heightInPixelsBase - 2);
		s_yPlaneBot =  ((yMaxFixed * 0.5f) - pitchOffset) / s_focalLenAspect;
		s_yPlaneTop = -((yMaxFixed * 0.5f) + pitchOffset) / s_focalLenAspect;

		vec3_float worldPoint = { 0, 0, 0 };
		vec3_float viewPoint;
		transformPointByCamera(&worldPoint, &viewPoint);

		// Setup the camera lights
		for (s32 i = 0; i < s_lightCount; i++)
		{
			vec3_float* srcDir = &s_cameraLight[i].lightWS;
			vec3_float* dstDir = &s_cameraLight[i].lightVS;

			vec3_float dst;
			transformPointByCamera(srcDir, &dst);

			dstDir->x = viewPoint.x - dst.x;
			dstDir->y = viewPoint.y - dst.y;
			dstDir->z = viewPoint.z - dst.z;

			normalizeVec3(dstDir, dstDir);
		}
	}

	void computeSkyOffsets()
	{
		const LevelData* level = TFE_LevelAsset::getLevelData();
		s_skyYawOffset   =  s_cameraYaw   / (2.0f * PI) * level->parallax[0];
		s_skyPitchOffset = -s_cameraPitch / (2.0f * PI) * level->parallax[1];
	}

	void setResolution(s32 width, s32 height)
	{
		if (width == s_width && height == s_height) { return; }

		s_width = width;
		s_height = height;
		s_halfWidth = f32(s_width >> 1);
		s_halfHeight = f32(s_height >> 1);
		s_halfHeightBase = s_halfHeight;
		s_focalLength = s_halfWidth;
		if (TFE_RenderBackend::getWidescreen())
		{
			// 200p and 400p get special handling because they are 16:10 resolutions in 4:3.
			if (height == 200 || height == 400)
			{
				s_focalLenAspect = (height == 200) ? 160.0f : 320.0f;
			}
			else
			{
				s_focalLenAspect = (height * 4 / 3) * 0.5f;
			}

			const f32 aspectScale = (s_height == 200 || s_height == 400) ? (10.0f / 16.0f) : (3.0f / 4.0f);
			s_nearPlaneHalfLen = aspectScale * (f32(s_width) / f32(s_height));
			// at low resolution, increase the nearPlaneHalfLen slightly to avoid cutting off the last column.
			if (s_height == 200)
			{
				s_nearPlaneHalfLen += 0.001f;
			}
		}
		else
		{
			s_focalLenAspect = s_focalLength;
			s_nearPlaneHalfLen = 1.0f;
		}
		s_screenXMid = s_width >> 1;

		s_heightInPixels = s_height;
		s_heightInPixelsBase = s_height;

		// Assume that 200p and 400p are 16:10 in 4:3 (rectangular pixels)
		// Otherwise if widescreen is not enabled, assume this is a pure 4:3 resolution (square pixels)
		// And if widescreen is enabled, use the resulting screen aspect ratio while keeping the square or rectangular pixels based on the height.
		s_aspectScaleX = 1.0f;
		s_aspectScaleY = 1.0f;
		if (TFE_RenderBackend::getWidescreen())
		{
			// The (4/3) or (16/10) factor removes the 4:3 or 16:10 aspect ratio already factored in 's_halfWidth' 
			// The (height/width) factor adjusts for the resolution pixel aspect ratio.
			const f32 scaleFactor = (height == 200 || height == 400) ? (16.0f / 10.0f) : (4.0f / 3.0f);
			s_focalLength = s_halfWidth * scaleFactor * f32(height) / f32(width);
		}
		if (height != 200 && height != 400)
		{
			// Scale factor to account for converting from rectangular pixels to square pixels when computing flat texture coordinates.
			// Factor = (16/10) / (4/3)
			s_aspectScaleX = 1.2f;
			s_aspectScaleY = 1.2f;
		}
		s_focalLenAspect *= s_aspectScaleY;

		s_minScreenX = 0;
		s_maxScreenX = s_width - 1;
		s_minScreenY = 1;
		s_maxScreenY = s_height - 1;
		s_windowMinZ = 0;

		s_windowX0 = s_minScreenX;
		s_windowX1 = s_maxScreenX;

		s_columnTop = (s32*)realloc(s_columnTop, s_width * sizeof(s32));
		s_columnBot = (s32*)realloc(s_columnBot, s_width * sizeof(s32));
		s_depth1d_all = (f32*)realloc(s_depth1d_all, s_width * sizeof(f32) * (MAX_ADJOIN_DEPTH + 1));
		s_windowTop_all = (s32*)realloc(s_windowTop, s_width * sizeof(s32) * (MAX_ADJOIN_DEPTH + 1));
		s_windowBot_all = (s32*)realloc(s_windowBot, s_width * sizeof(s32) * (MAX_ADJOIN_DEPTH + 1));

		// Build tables
		s_skyTable = (f32*)realloc(s_skyTable, s_width * sizeof(f32));
		s32 halfWidth = s_width >> 1;
		for (s32 x = 0; x < s_width; x++)
		{
			// This result isn't *exactly* the same as the original, but it is accurate to within 1 decimal point (example: -88.821585 vs -88.8125 @ x=63)
			// The original result is likely from a arc tangent table, which is more approximate. However the end difference is less
			// than a single pixel at 320x200. The more accurate result will look better at higher resolutions as well. :)
			// TODO: Extract the original table to use at 320x200?
			s_skyTable[x] = 512.0f * atanf(f32(x - halfWidth) / s_focalLength) / PI;
		}
	}
}  // RClassic_Float

}  // TFE_JediRenderer