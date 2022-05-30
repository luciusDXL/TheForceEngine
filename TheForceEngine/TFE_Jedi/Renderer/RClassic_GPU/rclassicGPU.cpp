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

namespace TFE_Jedi
{
	Mat3  s_cameraMtx;
	Mat4  s_cameraProj;
	Vec3f s_cameraPos;
	
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
		s_cameraProj = TFE_Math::computeProjMatrix(120.0f * PI / 180.0f, 1.3333333f, 0.01f, 1000.0f);

		yaw   = -yaw   / 16383.0f * 2.0f * PI;
		pitch = -pitch*1.9f / 16383.0f * 2.0f * PI;

		Vec3f lookDir = { sinf(yaw) * cosf(pitch), sinf(pitch), cosf(yaw) * cosf(pitch) };
		Vec3f upDir = { 0.0f, -1.0f, 0.0f };
		s_cameraMtx = TFE_Math::computeViewMatrix(&lookDir, &upDir);
	}

	void transformPointByCamera(vec3_float* worldPoint, vec3_float* viewPoint)
	{
	}

	void computeSkyOffsets()
	{
	}
}  // RClassic_GPU

}  // TFE_Jedi