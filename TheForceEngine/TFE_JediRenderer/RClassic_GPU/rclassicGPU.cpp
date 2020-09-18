#include <TFE_Asset/levelAsset.h>
#include "../rcommon.h"
#include "../fixedPoint.h"

namespace TFE_JediRenderer
{

namespace RClassic_GPU
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
		s_cameraPosZ = z;

		const f32 pitchOffset = sinf(pitch) * s_focalLenAspect;
		s_halfHeight = s_halfHeightBase + pitchOffset;
		s_heightInPixels = s_heightInPixelsBase + floorFloat(pitchOffset);
		computeSkyOffsets();
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
		s_focalLenAspect = s_focalLength;
		s_screenXMid = s_width >> 1;

		s_heightInPixels = s_height;
		s_heightInPixelsBase = s_height;

		// Assume that 200p and 400p are 16:10 in 4:3
		// Otherwise we can pure 4:3 (i.e. undo the 16:10 part)
		s_aspectScale = 1.0f;
		if (height != 200 && height != 400)
		{
			s_aspectScale = 1.2f;
		}
		s_focalLenAspect *= s_aspectScale;

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
			s_skyTable[x] = 512.0f * atanf(f32(x - halfWidth) / f32(halfWidth)) / PI;
		}
	}
}  // RClassic_Float

}  // TFE_JediRenderer