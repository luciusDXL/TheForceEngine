#include <TFE_Asset/levelAsset.h>
#include "rcommonFixed.h"
#include "../rcommon.h"
#include "../fixedPoint.h"

using namespace FixedPoint;

namespace TFE_JediRenderer
{

namespace RClassic_Fixed
{
	void computeSkyOffsets();

	void setCamera(f32 yaw, f32 pitch, f32 x, f32 y, f32 z, s32 sectorId)
	{
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
		s_cameraPosZ_Fixed = zFixed;

		const fixed16_16 pitchOffset = mul16(sinPitch, s_focalLenAspect_Fixed);
		s_halfHeight_Fixed = s_halfHeightBase_Fixed + pitchOffset;
		s_heightInPixels = s_heightInPixelsBase + floor16(pitchOffset);
		computeSkyOffsets();
	}

	void computeSkyOffsets()
	{
		const LevelData* level = TFE_LevelAsset::getLevelData();

		// angles range from -16384 to 16383; multiply by 4 to convert to [-1, 1) range.
		s_skyYawOffset_Fixed = mul16(s_cameraYaw_Fixed   * ANGLE_TO_FIXED_SCALE, intToFixed16((s32)level->parallax[0]));
		s_skyPitchOffset_Fixed = -mul16(s_cameraPitch_Fixed * ANGLE_TO_FIXED_SCALE, intToFixed16((s32)level->parallax[1]));
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

		// HACK: TODO - compute correctly.
		if (width * 10 / height != 16)
		{
			s_focalLenAspect_Fixed = intToFixed16((height / 2) * 8 / 5);
		}

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
		s_column_Y_Over_X_Fixed = (fixed16_16*)realloc(s_column_Y_Over_X_Fixed, s_width * sizeof(fixed16_16));
		s_column_X_Over_Y_Fixed = (fixed16_16*)realloc(s_column_X_Over_Y_Fixed, s_width * sizeof(fixed16_16));
		s_skyTable_Fixed = (fixed16_16*)realloc(s_skyTable_Fixed, s_width * sizeof(fixed16_16));
		s32 halfWidth = s_width >> 1;
		for (s32 x = 0; x < s_width; x++)
		{
			s_column_Y_Over_X_Fixed[x] = (x != halfWidth) ? div16(s_halfWidth_Fixed, intToFixed16(x - halfWidth)) : s_halfWidth_Fixed;
			s_column_X_Over_Y_Fixed[x] = div16(intToFixed16(x - halfWidth), s_halfWidth_Fixed);

			// This result isn't *exactly* the same as the original, but it is accurate to within 1 decimal point (example: -88.821585 vs -88.8125 @ x=63)
			// The original result is likely from a arc tangent table, which is more approximate. However the end difference is less
			// than a single pixel at 320x200. The more accurate result will look better at higher resolutions as well. :)
			// TODO: Extract the original table to use at 320x200?
			s_skyTable_Fixed[x] = floatToFixed16(512.0f * atanf(f32(x - halfWidth) / f32(halfWidth)) / PI);
		}

		s_rcp_yMinusHalfHeight_Fixed = (fixed16_16*)realloc(s_rcp_yMinusHalfHeight_Fixed, 3 * s_height * sizeof(fixed16_16));
		s32 halfHeight = s_height >> 1;
		for (s32 y = 0; y < s_height * 3; y++)
		{
			fixed16_16 yMinusHalf = fixed16_16(-s_height + y - halfHeight);
			s_rcp_yMinusHalfHeight_Fixed[y] = (yMinusHalf != 0) ? ONE_16 / yMinusHalf : ONE_16;
		}
	}
}  // RClassic_Fixed

}  // TFE_JediRenderer