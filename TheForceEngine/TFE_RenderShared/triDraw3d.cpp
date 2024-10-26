#include "triDraw3d.h"
#include <TFE_RenderBackend/shader.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderBackend/indexBuffer.h>
#include <TFE_System/system.h>
#include <TFE_Jedi/Math/core_math.h>
#include <assert.h>
#include <vector>

#define TRI3D_MAX_DRAW_COUNT 65536
#define TRI3D_DRAW_COUNT_RES 4096
#define TRI3D_MAX 65536
#define IDX3D_MAX TRI3D_MAX * 3
#define VTX3D_MAX TRI3D_MAX * 3

namespace TFE_RenderShared
{
	// Vertex Definition
	struct Tri3dVertex
	{
		Vec3f pos;		// 2D position.
		Vec2f uv;		// UV coordinates.
		Vec2f uv1;
		Vec2f uv2;
		u32   color;	// color + opacity.
	};
	struct Tri3dDraw
	{
		TextureGpu* texture;
		DrawMode mode;
		u32 drawFlags;
		s32 vtxOffset;
		s32 idxOffset;
		s32 vtxCount;
		s32 idxCount;
	};
	static const AttributeMapping c_tri3dAttrMapping[]=
	{
		{ATTR_POS,   ATYPE_FLOAT, 3, 0, false},
		{ATTR_UV,    ATYPE_FLOAT, 2, 0, false},
		{ATTR_UV1,   ATYPE_FLOAT, 2, 0, false},
		{ATTR_UV2,   ATYPE_FLOAT, 2, 0, false},
		{ATTR_COLOR, ATYPE_UINT8, 4, 0, true}
	};
	static const u32 c_tri3dAttrCount = TFE_ARRAYSIZE(c_tri3dAttrMapping);

	struct ShaderStateTRIDRAW
	{
		s32 svCameraPos = -1;
		s32 svCameraView = -1;
		s32 svCameraProj = -1;
		s32 svGridScaleOpacity = -1;
		s32 svIsTextured = -1;
		s32 skyParam0Id = -1;
		s32 skyParam1Id = -1;
	};
	static ShaderStateTRIDRAW s_shaderState[TRIMODE_COUNT];

	static Shader s_shader[TRIMODE_COUNT];
	static VertexBuffer s_vertexBuffer;
	static IndexBuffer  s_indexBuffer;

	static Tri3dVertex* s_vertices = nullptr;
	static s32* s_indices = nullptr;
	
	static u32 s_vtxCount;
	static u32 s_idxCount;

	static Tri3dDraw* s_triDraw[TRIMODE_COUNT] = { nullptr };
	static u32 s_triDrawCount[TRIMODE_COUNT];
	static u32 s_triDrawCapacity[TRIMODE_COUNT];

	static DrawMode s_lastDrawMode = TRIMODE_COUNT;
	static Grid s_gridDef = {};

	bool canMergeDraws(DrawMode mode, TextureGpu* texture, u32 drawFlags = TFLAG_NONE);
	u32 setDrawFlags(bool showGrid, bool sky);

	bool tri3d_loadTextureVariant(DrawMode mode, u32 defineCount, ShaderDefine* defines)
	{
		// Transparent
		if (!s_shader[mode].load("Shaders/tri3dWall.vert", "Shaders/tri3dWall.frag", defineCount, defines))
		{
			return false;
		}
		s_shader[mode].bindTextureNameToSlot("Image", 0);

		s_shaderState[mode].svCameraPos = s_shader[mode].getVariableId("CameraPos");
		s_shaderState[mode].svCameraView = s_shader[mode].getVariableId("CameraView");
		s_shaderState[mode].svCameraProj = s_shader[mode].getVariableId("CameraProj");
		s_shaderState[mode].svGridScaleOpacity = s_shader[mode].getVariableId("GridScaleOpacity");
		s_shaderState[mode].svIsTextured = s_shader[mode].getVariableId("isTexturedSky");
		s_shaderState[mode].skyParam0Id = s_shader[mode].getVariableId("SkyParam0");
		s_shaderState[mode].skyParam1Id = s_shader[mode].getVariableId("SkyParam1");
		if (s_shaderState[mode].svCameraPos < 0 || s_shaderState[mode].svCameraView < 0 || s_shaderState[mode].svCameraProj < 0)
		{
			return false;
		}
		return true;
	}
	
	bool tri3d_init()
	{
		ShaderDefine defines[] = { { "TRANS", "1" }, { "TEX_CLAMP", "1" } };
		tri3d_loadTextureVariant(TRIMODE_OPAQUE, 0, nullptr);
		tri3d_loadTextureVariant(TRIMODE_BLEND, 1, defines);
		tri3d_loadTextureVariant(TRIMODE_CLAMP, 2, defines);

		// Create buffers
		// Create vertex and index buffers.
		s_vertices = new Tri3dVertex[VTX3D_MAX];
		s_indices = new s32[IDX3D_MAX];

		s_vertexBuffer.create(VTX3D_MAX, sizeof(Tri3dVertex), c_tri3dAttrCount, c_tri3dAttrMapping, true);
		s_indexBuffer.create(IDX3D_MAX, sizeof(u32), true);

		s_triDraw[TRIMODE_OPAQUE] = (Tri3dDraw*)malloc(sizeof(Tri3dDraw) * TRI3D_DRAW_COUNT_RES);
		s_triDrawCapacity[TRIMODE_OPAQUE] = TRI3D_DRAW_COUNT_RES;

		s_triDraw[TRIMODE_BLEND] = (Tri3dDraw*)malloc(sizeof(Tri3dDraw) * TRI3D_DRAW_COUNT_RES);
		s_triDrawCapacity[TRIMODE_BLEND] = TRI3D_DRAW_COUNT_RES;

		s_triDraw[TRIMODE_CLAMP] = (Tri3dDraw*)malloc(sizeof(Tri3dDraw) * TRI3D_DRAW_COUNT_RES);
		s_triDrawCapacity[TRIMODE_CLAMP] = TRI3D_DRAW_COUNT_RES;

		s_idxCount = 0;
		s_vtxCount = 0;
		s_triDrawCount[TRIMODE_OPAQUE] = 0;
		s_triDrawCount[TRIMODE_BLEND] = 0;
		s_triDrawCount[TRIMODE_CLAMP] = 0;

		return true;
	}

	void tri3d_destroy()
	{
		s_vertexBuffer.destroy();
		s_indexBuffer.destroy();
		delete[] s_vertices;
		delete[] s_indices;
		s_vertices = nullptr;
		s_indices = nullptr;
		for (s32 i = 0; i < TRIMODE_COUNT; i++)
		{
			free(s_triDraw[i]);
			s_triDraw[i] = nullptr;
		}
	}
	
	void triDraw3d_begin(const Grid* gridDef)
	{
		s_idxCount = 0;
		s_vtxCount = 0;
		s_lastDrawMode = TRIMODE_COUNT;
		if (gridDef)
		{
			s_gridDef = *gridDef;
		}
		else
		{
			s_gridDef = {};
		}
		for (s32 i = 0; i < TRIMODE_COUNT; i++)
		{
			s_triDrawCount[i] = 0;
		}
	}
		
	bool triDraw3d_expand(DrawMode pass)
	{
		if (s_triDrawCapacity[pass] >= TRI3D_MAX_DRAW_COUNT)
		{
			return false;
		}
		s_triDrawCapacity[pass] += TRI3D_DRAW_COUNT_RES;
		s_triDraw[pass] = (Tri3dDraw*)realloc(s_triDraw[pass], sizeof(Tri3dDraw) * s_triDrawCapacity[pass]);
		return true;
	}

	Tri3dDraw* getTriDraw(DrawMode pass)
	{
		if (s_triDrawCount[pass] >= s_triDrawCapacity[pass])
		{
			if (!triDraw3d_expand(pass)) { return nullptr; }
		}
		Tri3dDraw* draw = &s_triDraw[pass][s_triDrawCount[pass]];
		s_triDrawCount[pass]++;
		return draw;
	}

	bool canMergeDraws(DrawMode mode, TextureGpu* texture, u32 drawFlags)
	{
		const bool canMerge = (s_triDrawCount[mode] > 0 && s_triDraw[mode][s_triDrawCount[mode] - 1].texture == texture && mode == s_lastDrawMode) &&
			s_triDraw[mode][s_triDrawCount[mode] - 1].drawFlags == drawFlags;
		s_lastDrawMode = mode;
		return canMerge;
	}
		
	void triDraw3d_addQuadTextured(DrawMode pass, Vec3f* corners, const Vec2f* uvCorners, const u32 color, TextureGpu* texture, bool sky)
	{
		if (!s_vertices) { return; }
		const s32 idxOffset = s_idxCount;
		const s32 vtxOffset = s_vtxCount;

		// Compute the corners in grid-space.
		Vec3f gridCorners[] =
		{
			posToGrid(s_gridDef, corners[0]),
			posToGrid(s_gridDef, corners[1])
		};
		const f32 dx = fabsf(gridCorners[1].x - gridCorners[0].x);
		const f32 dz = fabsf(gridCorners[1].z - gridCorners[0].z);

		// Do we have enough room for the vertices and indices?
		if (s_vtxCount + 4 > VTX3D_MAX || s_idxCount + 6 > IDX3D_MAX)
		{
			return;
		}

		// New draw call or add to the existing call?
		u32 drawFlags = setDrawFlags(true, sky);
		if (canMergeDraws(pass, texture))
		{
			// Append to the previous draw call.
			s_triDraw[pass][s_triDrawCount[pass] - 1].vtxCount += 4;
			s_triDraw[pass][s_triDrawCount[pass] - 1].idxCount += 6;
		}
		else
		{
			// Too many draw calls?
			Tri3dDraw* draw = getTriDraw(pass);
			// Add a new draw call.
			if (draw)
			{
				draw->texture = texture;
				draw->mode = pass;
				draw->vtxOffset = vtxOffset;
				draw->idxOffset = idxOffset;
				draw->vtxCount = 4;
				draw->idxCount = 6;
				draw->drawFlags = drawFlags;
			}
			else
			{
				return;
			}
		}

		Tri3dVertex* outVert = &s_vertices[s_vtxCount];
		s32* outIdx = &s_indices[s_idxCount];

		outVert[0].pos = { corners[0].x, corners[0].y, corners[0].z };
		outVert[0].uv = { 0.0f, 0.0f };
		outVert[0].uv1 = { (dx >= dz) ? gridCorners[0].x : gridCorners[0].z, gridCorners[0].y };
		outVert[0].uv2 = { uvCorners[0].x, uvCorners[0].z };
		outVert[0].color = color;

		outVert[1].pos = { corners[1].x, corners[0].y, corners[1].z };
		outVert[1].uv = { 1.0f, 0.0f };
		outVert[1].uv1 = { (dx >= dz) ? gridCorners[1].x : gridCorners[1].z, gridCorners[0].y };
		outVert[1].uv2 = { uvCorners[1].x, uvCorners[0].z };
		outVert[1].color = color;

		outVert[2].pos = { corners[1].x, corners[1].y, corners[1].z };
		outVert[2].uv = { 1.0f, 1.0f };
		outVert[2].uv1 = { (dx >= dz) ? gridCorners[1].x : gridCorners[1].z, gridCorners[1].y };
		outVert[2].uv2 = { uvCorners[1].x, uvCorners[1].z };
		outVert[2].color = color;

		outVert[3].pos = { corners[0].x, corners[1].y, corners[0].z };
		outVert[3].uv = { 0.0f, 1.0f };
		outVert[3].uv1 = { (dx >= dz) ? gridCorners[0].x : gridCorners[0].z, gridCorners[1].y };
		outVert[3].uv2 = { uvCorners[0].x, uvCorners[1].z };
		outVert[3].color = color;

		outIdx[0] = vtxOffset + 0;
		outIdx[1] = vtxOffset + 1;
		outIdx[2] = vtxOffset + 2;

		outIdx[3] = vtxOffset + 0;
		outIdx[4] = vtxOffset + 2;
		outIdx[5] = vtxOffset + 3;

		s_vtxCount += 4;
		s_idxCount += 6;
	}
		
	void triDraw3d_addTextured(DrawMode pass, u32 idxCount, u32 vtxCount, const Vec3f* vertices, const Vec2f* uv, const s32* indices, const u32 color, bool invSide, TextureGpu* texture, bool showGrid, bool sky)
	{
		if (!s_vertices) { return; }
		const s32 idxOffset = s_idxCount;
		const s32 vtxOffset = s_vtxCount;

		// Do we have enough room for the vertices and indices?
		if (s_vtxCount + vtxCount > VTX3D_MAX || s_idxCount + idxCount > IDX3D_MAX)
		{
			return;
		}

		// TODO: Compute the grid vertices here.

		// New draw call or add to the existing call?
		u32 drawFlags = setDrawFlags(showGrid, sky);
		if (canMergeDraws(pass, texture, drawFlags))
		{
			// Append to the previous draw call.
			s_triDraw[pass][s_triDrawCount[pass] - 1].vtxCount += vtxCount;
			s_triDraw[pass][s_triDrawCount[pass] - 1].idxCount += idxCount;
		}
		else
		{
			// Too many draw calls?
			Tri3dDraw* draw = getTriDraw(pass);
			// Add a new draw call.
			if (draw)
			{
				draw->texture = texture;
				draw->mode = pass;
				draw->vtxOffset = vtxOffset;
				draw->idxOffset = idxOffset;
				draw->vtxCount = vtxCount;
				draw->idxCount = idxCount;
				draw->drawFlags = drawFlags;
			}
			else
			{
				return;
			}
		}

		Tri3dVertex* outVert = &s_vertices[s_vtxCount];
		s32* outIdx = &s_indices[s_idxCount];

		for (u32 v = 0; v < vtxCount; v++)
		{
			const Vec2f posXZ = { vertices[v].x, vertices[v].z };

			outVert[v].pos = vertices[v];
			outVert[v].uv = { 0.5f, 0.5f };
			outVert[v].uv1 = posToGrid(s_gridDef, posXZ);
			outVert[v].uv2 = uv[v];
			outVert[v].color = color;
		}

		for (u32 i = 0; i < idxCount; i += 3)
		{
			if (invSide)
			{
				outIdx[i + 0] = indices[i + 0] + vtxOffset;
				outIdx[i + 1] = indices[i + 2] + vtxOffset;
				outIdx[i + 2] = indices[i + 1] + vtxOffset;
			}
			else
			{
				outIdx[i + 0] = indices[i + 0] + vtxOffset;
				outIdx[i + 1] = indices[i + 1] + vtxOffset;
				outIdx[i + 2] = indices[i + 2] + vtxOffset;
			}
		}

		s_vtxCount += vtxCount;
		s_idxCount += idxCount;
	}

	void triDraw3d_addQuadColored(DrawMode pass, Vec3f* corners, const u32 color)
	{
		if (!s_vertices) { return; }
		const s32 idxOffset = s_idxCount;
		const s32 vtxOffset = s_vtxCount;

		// Compute the corners in grid-space.
		Vec3f gridCorners[] =
		{
			posToGrid(s_gridDef, corners[0]),
			posToGrid(s_gridDef, corners[1])
		};
		const f32 dx = fabsf(gridCorners[1].x - gridCorners[0].x);
		const f32 dz = fabsf(gridCorners[1].z - gridCorners[0].z);

		// Do we have enough room for the vertices and indices?
		if (s_vtxCount + 4 > VTX3D_MAX || s_idxCount + 6 > IDX3D_MAX)
		{
			return;
		}

		// New draw call or add to the existing call?
		if (canMergeDraws(pass, nullptr))
		{
			// Append to the previous draw call.
			s_triDraw[pass][s_triDrawCount[pass] - 1].vtxCount += 4;
			s_triDraw[pass][s_triDrawCount[pass] - 1].idxCount += 6;
		}
		else
		{
			// Too many draw calls?
			Tri3dDraw* draw = getTriDraw(pass);
			// Add a new draw call.
			if (draw)
			{
				draw->texture = nullptr;
				draw->mode = pass;
				draw->vtxOffset = vtxOffset;
				draw->idxOffset = idxOffset;
				draw->vtxCount = 4;
				draw->idxCount = 6;
				draw->drawFlags = TFLAG_NONE;
			}
			else
			{
				return;
			}
		}

		Tri3dVertex* outVert = &s_vertices[s_vtxCount];
		s32* outIdx = &s_indices[s_idxCount];

		outVert[0].pos = { corners[0].x, corners[0].y, corners[0].z };
		outVert[0].uv = { 0.0f, 0.0f };
		outVert[0].uv1 = { (dx >= dz) ? gridCorners[0].x : gridCorners[0].z, gridCorners[0].y };
		outVert[0].color = color;

		outVert[1].pos = { corners[1].x, corners[0].y, corners[1].z };
		outVert[1].uv = { 1.0f, 0.0f };
		outVert[1].uv1 = { (dx >= dz) ? gridCorners[1].x : gridCorners[1].z, gridCorners[0].y };
		outVert[1].color = color;

		outVert[2].pos = { corners[1].x, corners[1].y, corners[1].z };
		outVert[2].uv = { 1.0f, 1.0f };
		outVert[2].uv1 = { (dx >= dz) ? gridCorners[1].x : gridCorners[1].z, gridCorners[1].y };
		outVert[2].color = color;

		outVert[3].pos = { corners[0].x, corners[1].y, corners[0].z };
		outVert[3].uv = { 0.0f, 1.0f };
		outVert[3].uv1 = { (dx >= dz) ? gridCorners[0].x : gridCorners[0].z, gridCorners[1].y };
		outVert[3].color = color;

		outIdx[0] = vtxOffset + 0;
		outIdx[1] = vtxOffset + 1;
		outIdx[2] = vtxOffset + 2;

		outIdx[3] = vtxOffset + 0;
		outIdx[4] = vtxOffset + 2;
		outIdx[5] = vtxOffset + 3;

		s_vtxCount += 4;
		s_idxCount += 6;
	}

	void triDraw3d_addColored(DrawMode pass, u32 idxCount, u32 vtxCount, const Vec3f* vertices, const s32* indices, const u32 color, bool invSide, bool showGrid)
	{
		if (!s_vertices) { return; }
		const s32 idxOffset = s_idxCount;
		const s32 vtxOffset = s_vtxCount;

		// Do we have enough room for the vertices and indices?
		if (s_vtxCount + vtxCount > VTX3D_MAX || s_idxCount + idxCount > IDX3D_MAX)
		{
			return;
		}

		// New draw call or add to the existing call?
		u32 drawFlags = setDrawFlags(showGrid, false);
		if (canMergeDraws(pass, nullptr, drawFlags))
		{
			// Append to the previous draw call.
			s_triDraw[pass][s_triDrawCount[pass] - 1].vtxCount += vtxCount;
			s_triDraw[pass][s_triDrawCount[pass] - 1].idxCount += idxCount;
		}
		else
		{
			// Too many draw calls?
			Tri3dDraw* draw = getTriDraw(pass);
			// Add a new draw call.
			if (draw)
			{
				draw->texture = nullptr;
				draw->mode = pass;
				draw->vtxOffset = vtxOffset;
				draw->idxOffset = idxOffset;
				draw->vtxCount = vtxCount;
				draw->idxCount = idxCount;
				draw->drawFlags = drawFlags;
			}
			else
			{
				return;
			}
		}

		Tri3dVertex* outVert = &s_vertices[s_vtxCount];
		s32* outIdx = &s_indices[s_idxCount];

		for (u32 v = 0; v < vtxCount; v++)
		{
			const Vec2f posXZ = { vertices[v].x, vertices[v].z };

			outVert[v].pos = vertices[v];
			outVert[v].uv = { 0.5f, 0.5f };
			outVert[v].uv1 = posToGrid(s_gridDef, posXZ);
			outVert[v].color = color;
		}

		for (u32 i = 0; i < idxCount; i+=3)
		{
			if (invSide)
			{
				outIdx[i + 0] = indices[i + 0] + vtxOffset;
				outIdx[i + 1] = indices[i + 2] + vtxOffset;
				outIdx[i + 2] = indices[i + 1] + vtxOffset;
			}
			else
			{
				outIdx[i + 0] = indices[i + 0] + vtxOffset;
				outIdx[i + 1] = indices[i + 1] + vtxOffset;
				outIdx[i + 2] = indices[i + 2] + vtxOffset;
			}
		}

		s_vtxCount += vtxCount;
		s_idxCount += idxCount;
	}

	void triDraw3d_draw(const Camera3d* camera, f32 width, f32 height, f32 gridScale, f32 gridOpacity, bool depthTest, bool culling)
	{
		if (s_vtxCount < 1 || s_idxCount < 1) { return; }

		s_vertexBuffer.update(s_vertices, s_vtxCount * sizeof(Tri3dVertex));
		s_indexBuffer.update(s_indices, s_idxCount * sizeof(s32));

		TFE_RenderState::setStateEnable(culling, STATE_CULLING);
		TFE_RenderState::setStateEnable(depthTest, STATE_DEPTH_TEST | STATE_DEPTH_WRITE);
		TFE_RenderState::setDepthFunction(CMP_LEQUAL);

		// Enable blending.
		TFE_RenderState::setBlendMode(BLEND_ONE, BLEND_ONE_MINUS_SRC_ALPHA);

		// Sky settings.
		// Compute the camera yaw from the camera direction and rotate it 90 degrees.
		// This generates a value from 0 to 1.
		Vec3f dir = { camera->viewMtx.m2.x, camera->viewMtx.m2.y, camera->viewMtx.m2.z };
		f32 cameraYaw = fmodf(-atan2f(dir.z, dir.x) / (2.0f*PI) + 0.25f, 1.0f);
		cameraYaw = cameraYaw < 0.0f ? cameraYaw + 1.0f : cameraYaw;
		f32 cameraPitch = asinf(dir.y);

		DisplayInfo info;
		TFE_RenderBackend::getDisplayInfo(&info);

		// TODO: Make this adjustable.
		f32 parallax[] = { 1024.0f, 1024.0f };
		const f32 oneOverTwoPi = 1.0f / 6.283185f;
		const f32 rad45 = 0.785398f;	// 45 degrees in radians.
		const f32 skyParam0[4] =
		{
			cameraYaw * parallax[0],
			TFE_Jedi::clamp(cameraPitch, -rad45, rad45) * parallax[1] * oneOverTwoPi,
			parallax[0] * oneOverTwoPi,
			200.0f / height,
		};
		const f32 aspectScale = 3.0f / 4.0f;
		const f32 nearPlaneHalfLen = aspectScale * (width / height);
		f32 skyParam1[4] =
		{
		   -nearPlaneHalfLen,
		    nearPlaneHalfLen * 2.0f / width,
			1.0f, 1.0f
		};

		bool blendEnable[] = { false, true };
		for (s32 i = 0; i < TRIMODE_COUNT; i++)
		{
			if (s_triDrawCount[i] < 1) { continue; }

			s_shader[i].bind();
			// Bind Uniforms & Textures.
			s_shader[i].setVariable(s_shaderState[i].svCameraPos, SVT_VEC3, camera->pos.m);
			s_shader[i].setVariable(s_shaderState[i].svCameraView, SVT_MAT3x3, camera->viewMtx.data);
			s_shader[i].setVariable(s_shaderState[i].svCameraProj, SVT_MAT4x4, camera->projMtx.data);

			f32 gridScaleOpacityNone[] = { 0.0f, 0.0f };
			f32 gridScaleOpacity[] = { gridScale, gridOpacity };
			f32 gridScaleOpacityTex[] = { gridScale, gridOpacity };

			// Bind vertex/index buffers and setup attributes for BlitVert
			s_vertexBuffer.bind();
			s_indexBuffer.bind();

			TFE_RenderState::setStateEnable(blendEnable[i], STATE_BLEND);
			s_shader[i].setVariable(s_shaderState[i].skyParam0Id, SVT_VEC4, skyParam0);
			
			// Draw.
			s32 isTexPrev = -1;
			bool prevGrid = true;
			for (u32 t = 0; t < s_triDrawCount[i]; t++)
			{
				Tri3dDraw* draw = &s_triDraw[i][t];
				s32 isTexturedSky[] = { draw->texture ? 1 : 0, (draw->drawFlags & TFLAG_SKY) ? 1 : 0 };
				bool showGrid = !(draw->drawFlags & TFLAG_NO_GRID);
				if (isTexturedSky[1])
				{
					skyParam1[2] = 1.0f / f32(draw->texture->getWidth());
					skyParam1[3] = 1.0f / f32(draw->texture->getHeight());
					s_shader[i].setVariable(s_shaderState[i].skyParam1Id, SVT_VEC4, skyParam1);
				}
				s_shader[i].setVariable(s_shaderState[i].svIsTextured, SVT_IVEC2, isTexturedSky);
				if (isTexturedSky[0])
				{
					draw->texture->bind(0);
					if (isTexPrev != isTexturedSky[0])
					{
						s_shader[i].setVariable(s_shaderState[i].svGridScaleOpacity, SVT_VEC2, showGrid ? gridScaleOpacityTex : gridScaleOpacityNone);
					}
				}
				else if (isTexPrev != isTexturedSky[0] || prevGrid != showGrid)
				{
					s_shader[i].setVariable(s_shaderState[i].svGridScaleOpacity, SVT_VEC2, showGrid ? gridScaleOpacity : gridScaleOpacityNone);
				}
				isTexPrev = isTexturedSky[0];
				prevGrid = showGrid;

				TFE_RenderBackend::drawIndexedTriangles(draw->idxCount / 3, sizeof(u32), draw->idxOffset);
			}
		}

		// Cleanup.
		s_shader[0].unbind();
		s_vertexBuffer.unbind();
		s_indexBuffer.unbind();
		TFE_RenderState::setDepthBias();

		// Clear
		for (s32 i = 0; i < TRIMODE_COUNT; i++)
		{
			s_triDrawCount[i] = 0;
		}
	}

	u32 setDrawFlags(bool showGrid, bool sky)
	{
		u32 drawFlags = 0;
		if (!showGrid) { drawFlags |= TFLAG_NO_GRID; }
		if (sky) { drawFlags |= TFLAG_SKY; }
		return drawFlags;
	}
}
