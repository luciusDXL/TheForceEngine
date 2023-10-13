#include "camera.h"
#include "sharedState.h"
#include <TFE_System/math.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Editor/errorMessages.h>
#include <TFE_Editor/EditorAsset/editorTexture.h>
#include <TFE_Editor/EditorAsset/editorFrame.h>
#include <TFE_Editor/EditorAsset/editorSprite.h>

#include <TFE_Ui/imGUI/imgui.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

using namespace TFE_Editor;
using namespace TFE_Jedi;

namespace LevelEditor
{
	extern Vec2i s_viewportSize;
	Camera3d s_camera;
	Vec3f s_viewDir;
	Vec3f s_viewRight;
	Vec3f s_cursor3d;

	static f32 c_editorCameraFov = 1.57079632679489661923f;

	void computeCameraTransform()
	{
		Vec3f upDir = { 0.0f, 1.0f, 0.0f };
		Vec3f lookDir = { sinf(s_camera.yaw) * cosf(s_camera.pitch), sinf(s_camera.pitch), cosf(s_camera.yaw) * cosf(s_camera.pitch) };
		s_camera.viewMtx = TFE_Math::computeViewMatrix(&lookDir, &upDir);
		s_camera.projMtx = TFE_Math::computeProjMatrix(c_editorCameraFov, f32(s_viewportSize.x) / f32(s_viewportSize.z), 0.01f, 5000.0f);

		// Flip the y coordinate.
		s_camera.projMtx.m1.y *= -1.0f;
		
		s_viewDir   = { -s_camera.viewMtx.m2.x, -s_camera.viewMtx.m2.y, -s_camera.viewMtx.m2.z };
		s_viewRight = {  s_camera.viewMtx.m0.x,  s_camera.viewMtx.m0.y,  s_camera.viewMtx.m0.z };
	}
}
