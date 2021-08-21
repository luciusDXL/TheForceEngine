#include <TFE_Level/level.h>
#include <TFE_Level/fixedPoint.h>
#include <TFE_Level/core_math.h>
#include <TFE_Level/rtexture.h>
#include "rcommonFixed.h"
#include "rlightingFixed.h"
#include "../rcommon.h"

namespace TFE_JediRenderer
{

namespace RClassic_Fixed
{
	void computeSkyOffsets();

	void transformPointByCamera(vec3_fixed* worldPoint, vec3_fixed* viewPoint)
	{
		viewPoint->x = mul16(worldPoint->x, RClassic_Fixed::s_cosYaw_Fixed) + mul16(worldPoint->z, RClassic_Fixed::s_sinYaw_Fixed) + RClassic_Fixed::s_xCameraTrans_Fixed;
		viewPoint->y = worldPoint->y - RClassic_Fixed::s_eyeHeight_Fixed;
		viewPoint->z = mul16(worldPoint->z, RClassic_Fixed::s_cosYaw_Fixed) + mul16(worldPoint->x, RClassic_Fixed::s_negSinYaw_Fixed) + RClassic_Fixed::s_zCameraTrans_Fixed;
	}

	void setCamera(f32 yaw, f32 pitch, f32 x, f32 y, f32 z, s32 sectorId)
	{
		// TODO: Remap input pitch to match Dark Forces limits, it should range from -1.5315f, 1.5315f (scale 1.95)

		// Convert camera parameters to fixed point.
		const fixed16_16 yawFixed = floatAngleToFixed(yaw);
		const fixed16_16 pchFixed = floatAngleToFixed(pitch);

		const fixed16_16 cosYaw = floatToFixed16(cosf(yaw));
		const fixed16_16 sinYaw = floatToFixed16(sinf(yaw));
		const fixed16_16 sinPitch = floatToFixed16(sinf(pitch));

		const fixed16_16 xFixed = floatToFixed16(x);
		const fixed16_16 yFixed = floatToFixed16(y);
		const fixed16_16 zFixed = floatToFixed16(z);

		s_cosYaw_Fixed = cosYaw;
		s_sinYaw_Fixed = sinYaw;
		s_negSinYaw_Fixed = -sinYaw;
		s_cameraYaw_Fixed = yawFixed;
		s_cameraPitch_Fixed = pchFixed;

		s_zCameraTrans_Fixed = mul16(-zFixed, s_cosYaw_Fixed) + mul16(-xFixed, s_negSinYaw_Fixed);
		s_xCameraTrans_Fixed = mul16(-xFixed, s_cosYaw_Fixed) + mul16(-zFixed, s_sinYaw_Fixed);
		s_eyeHeight_Fixed = yFixed;

		s_cameraPosX_Fixed = xFixed;
		s_cameraPosY_Fixed = yFixed;
		s_cameraPosZ_Fixed = zFixed;

		s_cameraMtx_Fixed[0] = s_cosYaw_Fixed;
		s_cameraMtx_Fixed[2] = s_negSinYaw_Fixed;
		s_cameraMtx_Fixed[4] = ONE_16;
		s_cameraMtx_Fixed[6] = s_sinYaw_Fixed;
		s_cameraMtx_Fixed[8] = s_cosYaw_Fixed;
		
		const fixed16_16 pitchOffset = mul16(sinPitch, s_focalLenAspect_Fixed);
		s_halfHeight_Fixed = s_halfHeightBase_Fixed + pitchOffset;
		s_heightInPixels = s_heightInPixelsBase + floor16(pitchOffset);
		computeSkyOffsets();

		fixed16_16 yMaxFixed = (s_heightInPixelsBase - 2) << 16;	// should be 198
		s_yPlaneBot_Fixed =  div16((yMaxFixed >> 1) - pitchOffset, s_focalLength_Fixed);
		s_yPlaneTop_Fixed = -div16((yMaxFixed >> 1) + pitchOffset, s_focalLength_Fixed);

		vec3_fixed worldPoint = { 0, 0, 0 };
		vec3_fixed viewPoint;
		transformPointByCamera(&worldPoint, &viewPoint);

		// Setup the camera lights
		for (s32 i = 0; i < s_lightCount; i++)
		{
			vec3_fixed* srcDir = &s_cameraLight[i].lightWS;
			vec3_fixed* dstDir = &s_cameraLight[i].lightVS;

			vec3_fixed dst;
			transformPointByCamera(srcDir, &dst);

			dstDir->x = viewPoint.x - dst.x;
			dstDir->y = viewPoint.y - dst.y;
			dstDir->z = viewPoint.z - dst.z;

			normalizeVec3(dstDir, dstDir);
		}
	}

	void computeSkyOffsets()
	{
		fixed16_16 parallax[2];
		TFE_Level::getSkyParallax(&parallax[0], &parallax[1]);

		// angles range from -16384 to 16383; multiply by 4 to convert to [-1, 1) range.
		s_skyYawOffset_Fixed = mul16(s_cameraYaw_Fixed * ANGLE_TO_FIXED_SCALE, parallax[0]);
		s_skyPitchOffset_Fixed = -mul16(s_cameraPitch_Fixed * ANGLE_TO_FIXED_SCALE, parallax[1]);
	}

	void setResolution(s32 width, s32 height)
	{
		if (width == s_width && height == s_height) { return; }

		s_width = width;
		s_height = height;
		s_halfWidth_Fixed = intToFixed16(s_width >> 1);
		s_halfHeight_Fixed = intToFixed16(s_height >> 1);
		s_halfHeightBase_Fixed = s_halfHeight_Fixed;
		s_focalLength_Fixed = s_halfWidth_Fixed;
		s_focalLenAspect_Fixed = s_focalLength_Fixed;
		s_screenXMid = s_width >> 1;

		s_heightInPixels = s_height;
		s_heightInPixelsBase = s_height;

		s_minScreenX = 0;
		s_maxScreenX = s_width - 1;
		s_minScreenY = 1;
		s_maxScreenY = s_height - 1;
		s_windowMinZ_Fixed = 0;

		s_windowX0 = s_minScreenX;
		s_windowX1 = s_maxScreenX;

		s_columnTop = (s32*)realloc(s_columnTop, s_width * sizeof(s32));
		s_columnBot = (s32*)realloc(s_columnBot, s_width * sizeof(s32));
		s_depth1d_all_Fixed = (fixed16_16*)realloc(s_depth1d_all_Fixed, s_width * sizeof(fixed16_16) * (MAX_ADJOIN_DEPTH + 1));
		s_windowTop_all = (s32*)realloc(s_windowTop, s_width * sizeof(s32) * (MAX_ADJOIN_DEPTH + 1));
		s_windowBot_all = (s32*)realloc(s_windowBot, s_width * sizeof(s32) * (MAX_ADJOIN_DEPTH + 1));

		// Build tables
		s_column_Z_Over_X = (fixed16_16*)realloc(s_column_Z_Over_X, s_width * sizeof(fixed16_16));
		s_column_X_Over_Z = (fixed16_16*)realloc(s_column_X_Over_Z, s_width * sizeof(fixed16_16));
		s_skyTable_Fixed = (fixed16_16*)realloc(s_skyTable_Fixed, s_width * sizeof(fixed16_16));
		s32 halfWidth = s_width >> 1;
		for (s32 x = 0; x < s_width; x++)
		{
			s_column_Z_Over_X[x] = (x != halfWidth) ? div16(s_halfWidth_Fixed, intToFixed16(x - halfWidth)) : s_halfWidth_Fixed;
			s_column_X_Over_Z[x] = div16(intToFixed16(x - halfWidth), s_halfWidth_Fixed);

			// This result isn't *exactly* the same as the original, but it is accurate to within 1 decimal point (example: -88.821585 vs -88.8125 @ x=63)
			// The original result is likely from a arc tangent table, which is more approximate. However the end difference is less
			// than a single pixel at 320x200. The more accurate result will look better at higher resolutions as well. :)
			// TODO: Extract the original table to use at 320x200?
			s_skyTable_Fixed[x] = floatToFixed16(512.0f * atanf(f32(x - halfWidth) / f32(halfWidth)) / PI);
		}

		s_rcpY = (fixed16_16*)realloc(s_rcpY, 4 * s_height * sizeof(fixed16_16));
		s32 yMid = s_height / 2;
		s32 startY = -2 * s_height;
		s32 endY = 2 * s_height;
		for (s32 y = startY; y < endY; y++)
		{
			s_rcpY[y + startY] = (y != yMid) ? div16(ONE_16, (y - yMid)) : ONE_16;
		}
	}

	// 2D
	void textureBlitColumn(const u8* image, u8* outBuffer, s32 yPixelCount)
	{
		s32 end = yPixelCount - 1;
		s32 offset = end * s_width;
		for (s32 i = end; i >= 0; i--, offset -= s_width)
		{
			outBuffer[offset] = image[i];
		}
	}

	void blitTextureToScreen(TextureData* texture, s32 x0, s32 y0)
	{
		s32 x1 = x0 + texture->width - 1;
		s32 y1 = y0 + texture->height - 1;

		if (x0 < 0)
		{
			x0 = 0;
		}
		if (y0 < 0)
		{
			y0 = 0;
		}
		if (x1 > s_width - 1)
		{
			x1 = s_width - 1;
		}
		if (y1 > s_height - 1)
		{
			y1 = s_height - 1;
		}
		s32 yPixelCount = y1 - y0 + 1;

		const u8* buffer = texture->image;
		for (s32 x = x0; x <= x1; x++, buffer += texture->height)
		{
			textureBlitColumn(buffer, s_display + y0*s_width + x, yPixelCount);
		}
	}
}  // RClassic_Fixed

}  // TFE_JediRenderer