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

#include "rclassicGPU.h"
#include "rsectorGPU.h"
#include "../rcommon.h"

#define PTR_OFFSET(ptr, base) size_t((u8*)ptr - (u8*)base)
using namespace TFE_RenderBackend;

namespace TFE_Jedi
{
	Shader m_wallShader;
	ShaderBuffer m_sectorVertices;
	ShaderBuffer m_sectorWalls;
	ShaderBuffer m_sectors;
	s32 m_cameraPosId;
	s32 m_cameraViewId;
	s32 m_cameraProjId;
	s32 m_sectorDataId;
	s32 m_sectorData2Id;
	s32 m_sectorBoundsId;
	Vec3f m_viewDir;

	struct SectorVertex
	{
		f32 x, y, z;
		f32 u, v;
	};

	struct GPUCachedSector
	{
		s32 vertexOffset;
		s32 wallOffset;
		s32 partCount;
		s32 maxPartCount;

		f32 floorHeight;
		f32 ceilingHeight;
	};

	VertexBuffer m_vertexBuffer;
	IndexBuffer m_indexBuffer;
	static GPUCachedSector* s_cachedSectors;

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
	    * Add TextureBuffer support to the backend - DONE
		* Add UniformBuffer support to the backend.
		* Add shader 330 support - DONE
		* Upload sector data for one sector - DONE
		* Render sector walls - DONE
	 4. Draw Flats in current sector - DONE
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

	struct GPUSourceData
	{
		Vec2f* vertices;
		Vec4ui* walls;
		Vec4f* sectors;

		u32 vertexSize;
		u32 wallSize;
		u32 sectorSize;
	};
	GPUSourceData s_gpuSourceData = { 0 };

	void setupWalls(RSector* curSector)
	{
		GPUCachedSector* cachedSector = &s_cachedSectors[curSector->index];

		s32 partCount = 0;
		Vec4ui* dstWall = &s_gpuSourceData.walls[cachedSector->wallOffset];
		const s32 ambient = floor16(curSector->ambient);
		const RWall* srcWall = curSector->walls;
		const u32 vertexBase = cachedSector->vertexOffset;
		for (s32 w = 0; w < curSector->wallCount; w++, srcWall++)
		{
			u32 i0 = vertexBase + (u32)((u8*)srcWall->v0 - (u8*)curSector->verticesVS) / sizeof(vec2_fixed);
			u32 i1 = vertexBase + (u32)((u8*)srcWall->v1 - (u8*)curSector->verticesVS) / sizeof(vec2_fixed);

			if (srcWall->drawFlags == WDF_MIDDLE && !srcWall->nextSector) // TODO: Fix transparent mid textures.
			{
				dstWall->x = i0;
				dstWall->y = i1;
				dstWall->z = 0;	// part = mid
				dstWall->w = ambient < 31 ? min(31, floor16(srcWall->wallLight) + floor16(curSector->ambient)) : 31;
				dstWall++;
				partCount++;
			}
			if ((srcWall->drawFlags & WDF_TOP) && srcWall->nextSector)
			{
				dstWall->x = i0;
				dstWall->y = i1;
				dstWall->z = 1 | (srcWall->nextSector->index << 16u);	// part = top
				dstWall->w = ambient < 31 ? min(31, floor16(srcWall->wallLight) + floor16(curSector->ambient)) : 31;
				dstWall++;
				partCount++;
			}
			if ((srcWall->drawFlags & WDF_BOT) && srcWall->nextSector)
			{
				dstWall->x = i0;
				dstWall->y = i1;
				dstWall->z = 2 | (srcWall->nextSector->index << 16u);	// part = bot
				dstWall->w = ambient < 31 ? min(31, floor16(srcWall->wallLight) + floor16(curSector->ambient)) : 31;
				dstWall++;
				partCount++;
			}
			// Add Floor
			dstWall->x = i0;
			dstWall->y = i1;
			dstWall->z = 3;	// part = floor
			dstWall->w = ambient;
			dstWall++;
			partCount++;

			// Add Ceiling
			dstWall->x = i0;
			dstWall->y = i1;
			dstWall->z = 4;	// part = ceiling
			dstWall->w = ambient;
			dstWall++;
			partCount++;
		}

		// Floor Cap
		dstWall->x = 0;
		dstWall->y = 0;
		dstWall->z = 5;	// part = floor cap
		dstWall->w = ambient;
		dstWall++;
		partCount++;

		// Ceiling Cap
		dstWall->x = 0;
		dstWall->y = 0;
		dstWall->z = 6;	// part = ceiling cap
		dstWall->w = ambient;
		dstWall++;
		partCount++;

		cachedSector->partCount = partCount;
		assert(partCount <= cachedSector->maxPartCount);
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
			m_sectorDataId = m_wallShader.getVariableId("SectorData");
			m_sectorData2Id = m_wallShader.getVariableId("SectorData2");
			m_sectorBoundsId = m_wallShader.getVariableId("SectorBounds");

			m_wallShader.bindTextureNameToSlot("SectorVertices", 0);
			m_wallShader.bindTextureNameToSlot("SectorWalls", 1);
			m_wallShader.bindTextureNameToSlot("Sectors", 2);

			// Handles up to 65536 quads.
			u16* indices = (u16*)level_alloc(sizeof(u16) * 6 * 65536);
			u16* index = indices;
			for (s32 q = 0; q < 65536; q++, index += 6)
			{
				const s32 i = q * 4;
				index[0] = i + 0;
				index[1] = i + 1;
				index[2] = i + 2;

				index[3] = i + 1;
				index[4] = i + 3;
				index[5] = i + 2;
			}
			m_indexBuffer.create(6 * 65536, sizeof(u16), false, (void*)indices);
			level_free(indices);

			// Let's just cache the current data.
			s_cachedSectors = (GPUCachedSector*)level_alloc(sizeof(GPUCachedSector) * s_sectorCount);
			s32 totalVertices = 0;
			s32 totalWalls = 0;
			for (s32 s = 0; s < s_sectorCount; s++)
			{
				s_cachedSectors[s].vertexOffset = totalVertices;
				s_cachedSectors[s].wallOffset = totalWalls;
				// Pre-allocate enough space for all possible wall parts.
				s32 maxWallCount = 0;
				for (s32 w = 0; w < s_sectors[s].wallCount; w++)
				{
					RWall* wall = &s_sectors[s].walls[w];
					if (wall->nextSector)
					{
						maxWallCount += 2;	// top + bottom
					}
					maxWallCount += 3;		// mid + ceiling + floor
					if (wall->signTex)
					{
						maxWallCount++;		// sign
					}
				}
				maxWallCount += 2;	// caps

				s_cachedSectors[s].maxPartCount = maxWallCount;
				totalVertices += s_sectors[s].vertexCount;
				totalWalls += maxWallCount;
			}
			s_gpuSourceData.vertexSize = sizeof(Vec2f) * totalVertices;
			s_gpuSourceData.vertices = (Vec2f*)level_alloc(s_gpuSourceData.vertexSize);
			memset(s_gpuSourceData.vertices, 0, s_gpuSourceData.vertexSize);
			
			// Wall structure: { i0, i1, partId | sign | light offset, textureId }
			s_gpuSourceData.wallSize = sizeof(Vec4ui) * totalWalls;
			s_gpuSourceData.walls = (Vec4ui*)level_alloc(s_gpuSourceData.wallSize);
			memset(s_gpuSourceData.walls, 0, s_gpuSourceData.wallSize);

			s_gpuSourceData.sectorSize = sizeof(Vec4f) * s_sectorCount;
			s_gpuSourceData.sectors = (Vec4f*)level_alloc(s_gpuSourceData.sectorSize);
			memset(s_gpuSourceData.sectors, 0, s_gpuSourceData.sectorSize);
			
			Vec2f* vertex = s_gpuSourceData.vertices;
			for (s32 s = 0; s < s_sectorCount; s++)
			{
				RSector* curSector = &s_sectors[s];
				GPUCachedSector* cachedSector = &s_cachedSectors[s];
				cachedSector->floorHeight = fixed16ToFloat(curSector->floorHeight);
				cachedSector->ceilingHeight = fixed16ToFloat(curSector->ceilingHeight);

				s_gpuSourceData.sectors[s].x = cachedSector->floorHeight;
				s_gpuSourceData.sectors[s].y = cachedSector->ceilingHeight;
				s_gpuSourceData.sectors[s].z = 0.0f;
				s_gpuSourceData.sectors[s].w = 0.0f;

				for (s32 v = 0; v < curSector->vertexCount; v++, vertex++)
				{
					vertex->x = fixed16ToFloat(curSector->verticesWS[v].x);
					vertex->z = fixed16ToFloat(curSector->verticesWS[v].z);
				}
				setupWalls(curSector);
			}

			ShaderBufferDef bufferDefVertices =
			{
				2,				// 1, 2, 4 channels (R, RG, RGBA)
				sizeof(f32),	// 1, 2, 4 bytes (u8; s16,u16; s32,u32,f32)
				BUF_CHANNEL_FLOAT
			};
			m_sectorVertices.create(totalVertices, bufferDefVertices, true, s_gpuSourceData.vertices);

			ShaderBufferDef bufferDefWalls =
			{
				4,				// 1, 2, 4 channels (R, RG, RGBA)
				sizeof(u32),	// 1, 2, 4 bytes (u8; s16,u16; s32,u32,f32)
				BUF_CHANNEL_UINT
			};
			m_sectorWalls.create(totalWalls, bufferDefWalls, true, s_gpuSourceData.walls);

			ShaderBufferDef bufferDefSectors =
			{
				4,				// 1, 2, 4 channels (R, RG, RGBA)
				sizeof(f32),	// 1, 2, 4 bytes (u8; s16,u16; s32,u32,f32)
				BUF_CHANNEL_FLOAT
			};
			m_sectors.create(s_sectorCount, bufferDefSectors, true, s_gpuSourceData.sectors);
		}
	}

	enum UploadFlags
	{
		UPLOAD_NONE     = 0,
		UPLOAD_SECTORS  = FLAG_BIT(0),
		UPLOAD_VERTICES = FLAG_BIT(1),
		UPLOAD_WALLS    = FLAG_BIT(2),
		UPLOAD_ALL      = UPLOAD_SECTORS | UPLOAD_VERTICES | UPLOAD_WALLS
	};

	void updateCachedWalls(RSector* srcSector, u32 flags, u32& uploadFlags)
	{
		GPUCachedSector* cached = &s_cachedSectors[srcSector->index];
		if (flags & SDF_HEIGHTS)
		{
			uploadFlags |= UPLOAD_WALLS;
			setupWalls(srcSector);
		}

		if (flags & (SDF_WALL_SHAPE | SDF_VERTICES))
		{
			uploadFlags |= UPLOAD_VERTICES;
			Vec2f* vertices = &s_gpuSourceData.vertices[cached->vertexOffset];
			for (s32 i = 0; i < srcSector->vertexCount; i++)
			{
				vertices[i].x = fixed16ToFloat(srcSector->verticesWS[i].x);
				vertices[i].z = fixed16ToFloat(srcSector->verticesWS[i].z);
			}
		}

	#if 0
		for (s32 w = 0; w < srcSector->wallCount; w++)
		{
			RWall* srcWall = &srcSector->walls[w];
			// TODO: SDF_HEIGHTS, SDF_WALL_OFFSETS - Handle texture offsets.
		}
	#endif
	}
	
	void updateCachedSector(RSector* srcSector, u32& uploadFlags)
	{
		u32 flags = srcSector->dirtyFlags;
		if (!flags) { return; }  // Nothing to do.

		GPUCachedSector* cached = &s_cachedSectors[srcSector->index];
		if (flags & SDF_HEIGHTS)
		{
			cached->floorHeight = fixed16ToFloat(srcSector->floorHeight);
			cached->ceilingHeight = fixed16ToFloat(srcSector->ceilingHeight);
			s_gpuSourceData.sectors[srcSector->index].x = cached->floorHeight;
			s_gpuSourceData.sectors[srcSector->index].y = cached->ceilingHeight;
			uploadFlags |= UPLOAD_SECTORS;
		}
		// TODO: Handle texture offsets.
		updateCachedWalls(srcSector, flags, uploadFlags);
		// Clear out dirty flags.
		srcSector->dirtyFlags = SDF_NONE;
	}

	bool addSectorToList(RSector** list, s32& count, RSector* newSector, u32& uploadFlags)
	{
		if (count >= 4096) { return false; }

		for (s32 i = 0; i < count; i++)
		{
			if (list[i] == newSector) { return false; }
		}
		list[count++] = newSector;
		return true;
	}

	struct Frustum
	{
		u32 planeCount;
		Vec4f planes[256];
	};
	static Frustum s_frustumStack[256];
	static u32 s_frustumStackPtr = 0;

	void frustumPush(Frustum& frustum)
	{
		s_frustumStack[s_frustumStackPtr] = frustum;
		s_frustumStackPtr++;
	}

	Frustum* frustumPop()
	{
		s_frustumStackPtr--;
		return &s_frustumStack[s_frustumStackPtr];
	}

	Frustum* frustumGetBack()
	{
		return &s_frustumStack[s_frustumStackPtr - 1];
	}

	bool sphereInFrustum(Vec3f pos, f32 radius)
	{
		pos = { pos.x - s_cameraPos.x, pos.y - s_cameraPos.y, pos.z - s_cameraPos.z };
		Frustum* frustum = frustumGetBack();

		Vec4f* plane = frustum->planes;
		for (s32 i = 0; i < frustum->planeCount; i++)
		{
			f32 dist = plane[i].x * pos.x + plane[i].y * pos.y + plane[i].z * pos.z + plane[i].w;
			if (dist + radius < 0.0f)
			{
				return false;
			}
		}
		return true;
	}

	void addAdjoinSectors(RSector* curSector, RSector** drawList, s32& drawCount, s32& level, u32& uploadFlags)
	{
		if (level > 255)
		{
			return;
		}

		for (s32 w = 0; w < curSector->wallCount; w++)
		{
			RWall* wall = &curSector->walls[w];
			RSector* next = wall->nextSector;
			if (next)
			{
				// Update any potential adjoins even if they are not traversed to make sure the
				// heights and walls settings are handled correctly.
				updateCachedSector(next, uploadFlags);

				const fixed16_16 openTop = max(curSector->ceilingHeight, next->ceilingHeight);
				const fixed16_16 openBot = min(curSector->floorHeight, next->floorHeight);
				const fixed16_16 openSize = openBot - openTop;
				if (openSize > 0)
				{
					f32 x0 = fixed16ToFloat(wall->w0->x);
					f32 x1 = fixed16ToFloat(wall->w1->x);
					f32 z0 = fixed16ToFloat(wall->w0->z);
					f32 z1 = fixed16ToFloat(wall->w1->z);
					f32 y0 = fixed16ToFloat(openTop);
					f32 y1 = fixed16ToFloat(openBot);

					// Is the portal inside the view frustum?
					Vec3f portalPos = { (x0 + x1) * 0.5f, (y0 + y1) * 0.5f, (z0 + z1) * 0.5f };
					Vec3f maxPos = { max(x0, x1), max(y0, y1), max(z0, z1) };
					Vec3f diag = { maxPos.x - portalPos.x, maxPos.y - portalPos.y, maxPos.z - portalPos.z };
					f32 portalRadius = sqrtf(diag.x*diag.x + diag.y*diag.y + diag.z*diag.z);
					if (!sphereInFrustum(portalPos, portalRadius))
					{
						continue;
					}
					// Is the portal facing towards the camera?
					Vec3f portalNormal = { -(z1-z0), 0.0f, x1-x0 };
					Vec3f cameraVec = { portalPos.x - s_cameraPos.x, portalPos.y - s_cameraPos.y, portalPos.z - s_cameraPos.z };
					if (portalNormal.x * cameraVec.x + portalNormal.y * cameraVec.y + portalNormal.z * cameraVec.z < 0.0f)
					{
						continue;
					}

					if (addSectorToList(drawList, drawCount, next, uploadFlags))
					{
						level++;
						addAdjoinSectors(next, drawList, drawCount, level, uploadFlags);
						level--;
					}
				}
			}
		}
	}

	Mat4 transpose4(Mat4 mtx)
	{
		Mat4 out;
		out.m0 = { mtx.m0.x, mtx.m1.x, mtx.m2.x, mtx.m3.x };
		out.m1 = { mtx.m0.y, mtx.m1.y, mtx.m2.y, mtx.m3.y };
		out.m2 = { mtx.m0.z, mtx.m1.z, mtx.m2.z, mtx.m3.z };
		out.m3 = { mtx.m0.w, mtx.m1.w, mtx.m2.w, mtx.m3.w };
		return out;
	}
	
	// Builds a world-space frustum from the camera.
	// We will need this for culling portals and clipping them.
	void buildFrustumFromCamera()
	{
		Mat4 view4;
		view4.m0 = { s_cameraMtx.m0.x, s_cameraMtx.m0.y, s_cameraMtx.m0.z, 0.0f };
		view4.m1 = { s_cameraMtx.m1.x, s_cameraMtx.m1.y, s_cameraMtx.m1.z, 0.0f };
		view4.m2 = { s_cameraMtx.m2.x, s_cameraMtx.m2.y, s_cameraMtx.m2.z, 0.0f };
		view4.m3 = { 0.0f,             0.0f,             0.0f,             1.0f };
		Mat4 viewProj = transpose4(TFE_Math::mulMatrix4(s_cameraProj, view4));
		m_viewDir = s_cameraMtx.m2;

		Vec4f planes[] =
		{
			// Left
			{
				viewProj.m0.w + viewProj.m0.x,
				viewProj.m1.w + viewProj.m1.x,
				viewProj.m2.w + viewProj.m2.x,
				viewProj.m3.w + viewProj.m3.x
			},
			// Right
			{
				viewProj.m0.w - viewProj.m0.x,
				viewProj.m1.w - viewProj.m1.x,
				viewProj.m2.w - viewProj.m2.x,
				viewProj.m3.w - viewProj.m3.x
			},
			// Bottom
			{
				viewProj.m0.w + viewProj.m0.y,
				viewProj.m1.w + viewProj.m1.y,
				viewProj.m2.w + viewProj.m2.y,
				viewProj.m3.w + viewProj.m3.y
			},
			// Top
			{
				viewProj.m0.w - viewProj.m0.y,
				viewProj.m1.w - viewProj.m1.y,
				viewProj.m2.w - viewProj.m2.y,
				viewProj.m3.w - viewProj.m3.y
			},
			// Near
			{
				viewProj.m0.w + viewProj.m0.z,
				viewProj.m1.w + viewProj.m1.z,
				viewProj.m2.w + viewProj.m2.z,
				viewProj.m3.w + viewProj.m3.z
			},
			// Far
			{ 
				viewProj.m0.w - viewProj.m0.z,
				viewProj.m1.w - viewProj.m1.z,
				viewProj.m2.w - viewProj.m2.z,
				viewProj.m3.w - viewProj.m3.z
			}
		};

		Frustum frustum;
		frustum.planeCount = 6;
		for (s32 i = 0; i < 6; i++)
		{
			frustum.planes[i] = planes[i];

			f32 len = sqrtf(frustum.planes[i].x*frustum.planes[i].x + frustum.planes[i].y*frustum.planes[i].y + frustum.planes[i].z*frustum.planes[i].z);
			if (len > FLT_EPSILON)
			{
				f32 scale = 1.0f / len;
				frustum.planes[i].x *= scale;
				frustum.planes[i].y *= scale;
				frustum.planes[i].z *= scale;
				frustum.planes[i].w *= scale;
			}
		}
		frustumPush(frustum);
	}

	static RSector* s_drawList[4096];
	static s32 s_drawCount;

	void buildDrawList(RSector* sector)
	{
		// First build the camera frustum and push it onto the stack.
		buildFrustumFromCamera();

		// Then build the draw list.
		s_drawCount = 1;
		s_drawList[0] = sector;

		s32 level = 0;
		u32 uploadFlags = UPLOAD_NONE;

		updateCachedSector(sector, uploadFlags);
		addAdjoinSectors(sector, s_drawList, s_drawCount, level, uploadFlags);
		frustumPop();

		if (uploadFlags & UPLOAD_SECTORS)
		{
			m_sectors.update(s_gpuSourceData.sectors, s_gpuSourceData.sectorSize);
		}
		if (uploadFlags & UPLOAD_VERTICES)
		{
			m_sectorVertices.update(s_gpuSourceData.vertices, s_gpuSourceData.vertexSize);
		}
		if (uploadFlags & UPLOAD_WALLS)
		{
			m_sectorWalls.update(s_gpuSourceData.walls, s_gpuSourceData.wallSize);
		}
	}

	void TFE_Sectors_GPU::draw(RSector* sector)
	{
		// Build the draw list.
		buildDrawList(sector);

		// State
		TFE_RenderState::setStateEnable(false, STATE_BLEND | STATE_DEPTH_TEST);
		TFE_RenderState::setStateEnable(true, STATE_DEPTH_TEST | STATE_CULLING);

		// Shader and buffers.
		m_wallShader.bind();
		m_indexBuffer.bind();
		m_sectorVertices.bind(0);
		m_sectorWalls.bind(1);
		m_sectors.bind(2);

		// Camera
		m_wallShader.setVariable(m_cameraPosId,  SVT_VEC3,   s_cameraPos.m);
		m_wallShader.setVariable(m_cameraViewId, SVT_MAT3x3, s_cameraMtx.data);
		m_wallShader.setVariable(m_cameraProjId, SVT_MAT4x4, s_cameraProj.data);
				
		// Draw Sectors.
		for (s32 s = 0; s < s_drawCount; s++)
		{
			RSector* curSector = s_drawList[s];
			GPUCachedSector* cachedSector = &s_cachedSectors[curSector->index];

			s32 sectorData1[] = { cachedSector->wallOffset, 0, 0, 0 };
			f32 sectorData2[] = { fixed16ToFloat(curSector->floorHeight), fixed16ToFloat(curSector->ceilingHeight), 0.0f, 0.0f };
			f32 shaderBounds[] = { fixed16ToFloat(curSector->boundsMin.x), fixed16ToFloat(curSector->boundsMin.z), fixed16ToFloat(curSector->boundsMax.x), fixed16ToFloat(curSector->boundsMax.z) };
			m_wallShader.setVariable(m_sectorDataId,  SVT_IVEC4, (f32*)sectorData1);
			m_wallShader.setVariable(m_sectorData2Id, SVT_VEC4, sectorData2);
			m_wallShader.setVariable(m_sectorBoundsId, SVT_VEC4, shaderBounds);

			// Draw.
			TFE_RenderBackend::drawIndexedTriangles(2 * cachedSector->partCount, sizeof(u16));
		}

		// Cleanup.
		m_wallShader.unbind();
		m_vertexBuffer.unbind();
		m_indexBuffer.unbind();
	}

	void TFE_Sectors_GPU::subrendererChanged()
	{
	}
}