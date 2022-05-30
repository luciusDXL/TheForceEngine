#include <cstring>

#include <TFE_System/profiler.h>
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

#include "rclassicGPU.h"
#include "rsectorGPU.h"
#include "../rcommon.h"

#define PTR_OFFSET(ptr, base) size_t((u8*)ptr - (u8*)base)
using namespace TFE_RenderBackend;

namespace TFE_Jedi
{
	Shader m_wallShader;
	s32 m_cameraPosId;
	s32 m_cameraViewId;
	s32 m_cameraProjId;

	struct SectorVertex
	{
		f32 x, y, z;
		f32 u, v;
	};

	VertexBuffer m_vertexBuffer;
	IndexBuffer m_indexBuffer;

	static const AttributeMapping c_sectorAttrMapping[] =
	{
		{ATTR_POS, ATYPE_FLOAT, 3, 0, false},
		{ATTR_UV,  ATYPE_FLOAT, 2, 0, false},
	};
	static const u32 c_sectorAttrCount = TFE_ARRAYSIZE(c_sectorAttrMapping);

	extern Mat3  s_cameraMtx;
	extern Mat4  s_cameraProj;
	extern Vec3f s_cameraPos;

	/*****************************************
	 First Steps:
	 1. Render Target setup - DONE
	 2. Initial Camera and test geo - DONE
	 3. Draw Walls in current sector:
	    * Add TextureBuffer support to the backend.
		* Add UniformBuffer support to the backend.
		* Add shader 330 support - DONE
		* Upload sector data for one sector.
		* Render sector walls.
	 4. Draw Flats in current sector.
	 5. Draw Adjoin openings in current sector (top and bottom parts).

	 Initial Sector Data (GPU):
	 vec2 vertices[] - store all vertices in the level.

	 struct WallData
	 {
		int2 indices;	// left and right indices.
	 }

	 struct Sector
	 {
		int2 geoOffsets;	// vertexOffset, wallOffset
		vec2 heights;		// floor, ceiling
		vec4 bounds;		// (min, max) XZ bounds.
	 }
	 *****************************************/

	void TFE_Sectors_GPU::reset()
	{
	}

	void TFE_Sectors_GPU::prepare()
	{
		if (!m_gpuInit)
		{
			m_gpuInit = true;
			// Load Shaders.
			bool result = m_wallShader.load("Shaders/gpu_render_wall.vert", "Shaders/gpu_render_wall.frag", 0, nullptr, SHADER_VER_STD);
			assert(result);
			m_cameraPosId  = m_wallShader.getVariableId("CameraPos");
			m_cameraViewId = m_wallShader.getVariableId("CameraView");
			m_cameraProjId = m_wallShader.getVariableId("CameraProj");

			const Vec3f basePos = { -144.0f, -152.0f, 287.0f };
			const f32 testSize = 500.0f;

			// Setup Buffers.
			// Upload vertex/index buffers
			const SectorVertex vertices[] =
			{
				{basePos.x - testSize, basePos.y - 5.0f, basePos.z - testSize, 0.0f, 0.0f},
				{basePos.x + testSize, basePos.y - 5.0f, basePos.z - testSize, 1.0f, 0.0f},
				{basePos.x + testSize, basePos.y - 5.0f, basePos.z + testSize, 1.0f, 1.0f},
				{basePos.x - testSize, basePos.y - 5.0f, basePos.z + testSize, 0.0f, 1.0f},
			};
			const u16 indices[] =
			{
				0, 1, 2,
				0, 2, 3
			};
			m_vertexBuffer.create(4, sizeof(SectorVertex), c_sectorAttrCount, c_sectorAttrMapping, false, (void*)vertices);
			m_indexBuffer.create(6, sizeof(u16), false, (void*)indices);
		}
	}

	void TFE_Sectors_GPU::draw(RSector* sector)
	{
		// State
		TFE_RenderState::setStateEnable(false, STATE_CULLING | STATE_DEPTH_TEST | STATE_BLEND);

		// Shader and buffers.
		m_wallShader.bind();
		m_vertexBuffer.bind();
		m_indexBuffer.bind();
				
		// Camera
		m_wallShader.setVariable(m_cameraPosId,  SVT_VEC3,   s_cameraPos.m);
		m_wallShader.setVariable(m_cameraViewId, SVT_MAT3x3, s_cameraMtx.data);
		m_wallShader.setVariable(m_cameraProjId, SVT_MAT4x4, s_cameraProj.data);

		// Draw.
		TFE_RenderBackend::drawIndexedTriangles(2, sizeof(u16));

		// Cleanup.
		m_wallShader.unbind();
		m_vertexBuffer.unbind();
		m_indexBuffer.unbind();
	}

	void TFE_Sectors_GPU::subrendererChanged()
	{
	}
}