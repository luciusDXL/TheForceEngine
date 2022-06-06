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
	}

	void computeCameraTransform(RSector* sector, f32 pitch, f32 yaw, f32 camX, f32 camY, f32 camZ)
	{
		s_cameraPos = { camX, camY, camZ };
		s_cameraProj = TFE_Math::computeProjMatrixExplicit(2.0f*s_rcfltState.focalLength / f32(s_width),
			2.0f*s_rcfltState.focalLenAspect / f32(s_height), 0.01f, 1000.0f);

		f32 sinYaw, cosYaw, sinPitch, cosPitch;
		sinCosFlt(-yaw, &sinYaw, &cosYaw);
		sinCosFlt(-pitch, &sinPitch, &cosPitch);

		s_cameraMtx.m0 = { cosYaw, 0.0f, sinYaw };
		s_cameraMtx.m1 = { sinYaw * sinPitch, cosPitch, -cosYaw * sinPitch };
		s_cameraMtx.m2 = { sinYaw * cosPitch, -sinPitch, -cosYaw * cosPitch };
		s_cameraDir = { -s_cameraMtx.m2.x, -s_cameraMtx.m2.y, -s_cameraMtx.m2.z };
	}

	void transformPointByCamera(vec3_float* worldPoint, vec3_float* viewPoint)
	{
	}

	void computeSkyOffsets()
	{
	}
}  // RClassic_GPU

}  // TFE_Jedi