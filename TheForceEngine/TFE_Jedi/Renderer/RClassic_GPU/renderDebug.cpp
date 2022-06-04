#include <cstring>

#include <TFE_System/profiler.h>
#include <TFE_System/math.h>
#include <TFE_Asset/modelAsset_jedi.h>
#include <TFE_Game/igame.h>
#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>

#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderBackend/indexBuffer.h>
#include <TFE_RenderBackend/shader.h>
#include <TFE_RenderBackend/shaderBuffer.h>

#include "renderDebug.h"
#include "rclassicGPU.h"
#include "rsectorGPU.h"
#include "../rcommon.h"

#define PTR_OFFSET(ptr, base) size_t((u8*)ptr - (u8*)base)
using namespace TFE_RenderBackend;

namespace TFE_Jedi
{
	struct LineVertex
	{
		f32 x, y, z;
		u8 color[4];
	};

	static const AttributeMapping c_lineAttrMapping[] =
	{
		{ATTR_POS,   ATYPE_FLOAT, 3, 0, false},
		{ATTR_COLOR, ATYPE_UINT8, 4, 0, true},
	};
	static const u32 c_lineAttrCount = TFE_ARRAYSIZE(c_lineAttrMapping);

	extern Mat3  s_cameraMtx;
	extern Mat4  s_cameraProj;
	extern Vec3f s_cameraPos;

	static bool s_debugInit = false;
	static bool s_debugEnable = false;
	static s32 s_lineCount = 0;
	static const u32 c_maxLineCount = 4096;

	Shader s_lineShader;
	VertexBuffer s_vertexBuffer;
	LineVertex* s_lines = nullptr;

	s32 s_cameraPosId  = -1;
	s32 s_cameraViewId = -1;
	s32 s_cameraProjId = -1;

	void renderDebug_enable(bool enable)
	{
		s_debugEnable = enable;
		s_lineCount = 0;
		if (!s_debugInit)
		{
			s_debugInit = true;
			// Init.
			s_lines = (LineVertex*)level_alloc(sizeof(LineVertex) * c_maxLineCount * 2);
			memset(s_lines, 0, sizeof(LineVertex) * c_maxLineCount * 2);
			s_vertexBuffer.create(c_maxLineCount * 2, sizeof(LineVertex), c_lineAttrCount, c_lineAttrMapping, true, s_lines);

			bool result = s_lineShader.load("Shaders/line3d_debug.vert", "Shaders/line3d_debug.frag", 0, nullptr, SHADER_VER_STD);
			assert(result);
			s_cameraPosId  = s_lineShader.getVariableId("CameraPos");
			s_cameraViewId = s_lineShader.getVariableId("CameraView");
			s_cameraProjId = s_lineShader.getVariableId("CameraProj");
		}
	}

	void renderDebug_addLine(Vec3f v0, Vec3f v1, const u8* color)
	{
		if (s_lineCount >= c_maxLineCount) { return; }
		s_lines[s_lineCount * 2 + 0] = { v0.x, v0.y, v0.z, color[0], color[1], color[2], color[3] };
		s_lines[s_lineCount * 2 + 1] = { v1.x, v1.y, v1.z, color[0], color[1], color[2], color[3] };
		s_lineCount++;
	}

	void renderDebug_addPolygon(s32 count, Vec3f* vtx, Vec4f color)
	{
		if (!s_debugEnable) { return; }
		const u8 u8Color[] = { u8(color.x * 255.0f), u8(color.y * 255.0f), u8(color.z * 255.0f), u8(color.w * 255.0f) };
		for (s32 i = 0; i < count; i++)
		{
			renderDebug_addLine(vtx[i], vtx[(i + 1) % count], u8Color);
		}
	}

	void renderDebug_draw()
	{
		if (!s_lineCount || !s_debugEnable) { return; }

		TFE_RenderState::setStateEnable(false, STATE_CULLING | STATE_DEPTH_TEST | STATE_DEPTH_WRITE);
		TFE_RenderState::setStateEnable(true, STATE_BLEND);
		TFE_RenderState::setBlendMode(BLEND_ONE, BLEND_ONE_MINUS_SRC_ALPHA);

		// Update the vertices
		s_vertexBuffer.update(s_lines, s_lineCount * 2 * sizeof(LineVertex));

		s_lineShader.bind();
		s_vertexBuffer.bind();

		// Camera
		s_lineShader.setVariable(s_cameraPosId, SVT_VEC3, s_cameraPos.m);
		s_lineShader.setVariable(s_cameraViewId, SVT_MAT3x3, s_cameraMtx.data);
		s_lineShader.setVariable(s_cameraProjId, SVT_MAT4x4, s_cameraProj.data);
		
		TFE_RenderBackend::drawLines(s_lineCount);

		s_vertexBuffer.unbind();
		s_lineShader.unbind();
		s_lineCount = 0;
	}
}