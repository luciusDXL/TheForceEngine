#include <cstring>

#include <TFE_System/math.h>
#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Game/igame.h>
#include "../redgePair.h"
#include "../rcommon.h"

// TODO: FIx
#include "../RClassic_Float/rclassicFloatSharedState.h"

namespace TFE_Jedi
{
	Mat3  s_cameraMtx;
	Mat4  s_cameraProj;
	Vec3f s_cameraPos;
	Vec3f s_cameraDir;
	Vec3f s_cameraRight;
	
namespace RClassic_GPU
{
	void resetState()
	{
	}

	void setupInitCameraAndLights(s32 width, s32 height)
	{
	}

	void changeResolution(s32 width, s32 height)
	{
		s_width = width;
		s_height = height;
		s_rcfltState.halfWidth = f32(width >> 1);
		s_rcfltState.focalLength = s_rcfltState.halfWidth;
		s_rcfltState.focalLenAspect = s_rcfltState.halfWidth;
		s_rcfltState.aspectScaleY = 1.0f;

		if (TFE_RenderBackend::getWidescreen())
		{
			// 200p and 400p get special handling because they are 16:10 resolutions in 4:3.
			if (s_height == 200 || s_height == 400)
			{
				s_rcfltState.focalLenAspect = (s_height == 200) ? 160.0f : 320.0f;
			}
			else
			{
				s_rcfltState.focalLenAspect = (s_height * 4 / 3) * 0.5f;
			}

			// The (4/3) or (16/10) factor removes the 4:3 or 16:10 aspect ratio already factored in 's_halfWidth' 
			// The (height/width) factor adjusts for the resolution pixel aspect ratio.
			const f32 scaleFactor = (s_height == 200 || s_height == 400) ? (16.0f / 10.0f) : (4.0f / 3.0f);
			s_rcfltState.focalLength = s_rcfltState.halfWidth * scaleFactor * f32(s_height) / f32(s_width);
		}
		if (s_height != 200 && s_height != 400)
		{
			// Scale factor to account for converting from rectangular pixels to square pixels when computing flat texture coordinates.
			// Factor = (16/10) / (4/3)
			s_rcfltState.aspectScaleY = 1.2f;
		}
		s_rcfltState.focalLenAspect *= s_rcfltState.aspectScaleY;

		s_cameraProj = TFE_Math::computeProjMatrixExplicit(2.0f*s_rcfltState.focalLength / f32(s_width),
			2.0f*s_rcfltState.focalLenAspect / f32(s_height), 0.01f, 4096.0f);
	}

	void computeCameraTransform(RSector* sector, f32 pitch, f32 yaw, f32 camX, f32 camY, f32 camZ)
	{
		s_cameraPos = { camX, camY, camZ };
		s_cameraProj = TFE_Math::computeProjMatrixExplicit(2.0f*s_rcfltState.focalLength / f32(s_width),
			2.0f*s_rcfltState.focalLenAspect / f32(s_height), 0.01f, 4096.0f);

		f32 sinYaw, cosYaw, sinPitch, cosPitch;
		sinCosFlt(-yaw, &sinYaw, &cosYaw);
		sinCosFlt(-pitch, &sinPitch, &cosPitch);

		s_cameraMtx.m0 = { cosYaw, 0.0f, sinYaw };
		s_cameraMtx.m1 = { sinYaw * sinPitch, cosPitch, -cosYaw * sinPitch };
		s_cameraMtx.m2 = { sinYaw * cosPitch, -sinPitch, -cosYaw * cosPitch };
		s_cameraDir    = { -s_cameraMtx.m2.x, -s_cameraMtx.m2.y, -s_cameraMtx.m2.z };
		s_cameraRight  = {  s_cameraMtx.m0.x,  s_cameraMtx.m0.y,  s_cameraMtx.m0.z };
	}

	void transformPointByCamera(vec3_float* worldPoint, vec3_float* viewPoint)
	{
	}

	void computeSkyOffsets()
	{
	}
}  // RClassic_GPU

}  // TFE_Jedi