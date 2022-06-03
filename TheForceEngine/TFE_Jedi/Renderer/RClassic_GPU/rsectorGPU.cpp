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

#include <TFE_Input/input.h>

#include "rclassicGPU.h"
#include "rsectorGPU.h"
#include "renderDebug.h"
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

	struct WallSegment
	{
		Vec3f normal;
		Vec2f v0, v1;
		f32 y0, y1;
		f32 portalY0, portalY1;
		bool portal;
		s32 id;
		// positions projected onto the camera plane, used for sorting and overlap tests.
		f32 x0, x1;
	};

	struct WallSegBuffer
	{
		WallSegBuffer* prev;
		WallSegBuffer* next;
		WallSegment* seg;

		// Adjusted values when segments need to be split.
		f32 x0, x1;
		Vec2f v0, v1;
	};

	struct GPUCachedSector
	{
		s32 vertexOffset;
		s32 wallOffset;
		s32 partCount;
		s32 maxPartCount;

		f32 floorHeight;
		f32 ceilingHeight;

		u64 builtFrame;
		s32 portalSegCount;
		s32 maxPortalSegCount;
		WallSegment* portalSeg;
	};

	VertexBuffer m_vertexBuffer;
	IndexBuffer m_indexBuffer;
	static GPUCachedSector* s_cachedSectors;
	static bool s_enableDebug = true;
	static u64 s_gpuFrame;

	static const AttributeMapping c_sectorAttrMapping[] =
	{
		{ATTR_POS, ATYPE_FLOAT, 3, 0, false},
		{ATTR_UV,  ATYPE_FLOAT, 2, 0, false},
	};
	static const u32 c_sectorAttrCount = TFE_ARRAYSIZE(c_sectorAttrMapping);

	// Sector elements are generated in depth-first traversal order and are rendered as:
	// 1. Portal -> Stencil Buffer (parentSectorIndex, adjoinIndex); write 'level' as the ref value, but only where 'level-1' exists.
	// 2. Sector (sectorIndex); write only where 'level' exists.
	struct SectorElement
	{
		s32 parentSectorIndex;	// -1 or index of the parent sector.
		s32 adjoinIndex;		// -1 or index of the adjoin that we are viewing through, this belongs to the parent.
		s32 sectorIndex;		// index of the sector to be rendered through the adjoin.
		s32 level;				// 0 = top level
	};
	static SectorElement s_sectorElements[4096];
	static s32 s_sectorElemCount;

	static RSector* s_drawList[4096];
	static s32 s_drawCount;

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
			s_gpuFrame = 1;
			// Load Shaders.
			bool result = m_wallShader.load("Shaders/gpu_render_wall.vert", "Shaders/gpu_render_wall.frag", 0, nullptr, SHADER_VER_STD);
			assert(result);
			m_cameraPosId = m_wallShader.getVariableId("CameraPos");
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
				s_cachedSectors[s].builtFrame = 0;
				// Pre-allocate enough space for all possible wall parts.
				s32 maxWallCount = 0;
				s32 maxPortalCount = 0;
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
					if (wall->nextSector)
					{
						maxPortalCount++;
					}
				}
				maxWallCount += 2;	// caps

				s_cachedSectors[s].maxPortalSegCount = maxPortalCount * 2;
				s_cachedSectors[s].portalSegCount = 0;
				s_cachedSectors[s].portalSeg = (WallSegment*)level_alloc(sizeof(WallSegment) * (maxPortalCount * 2));
				assert(s_cachedSectors[s].portalSeg || !maxPortalCount);
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
		else
		{
			s_gpuFrame++;
		}

		renderDebug_enable(s_enableDebug);
	}

	enum UploadFlags
	{
		UPLOAD_NONE = 0,
		UPLOAD_SECTORS = FLAG_BIT(0),
		UPLOAD_VERTICES = FLAG_BIT(1),
		UPLOAD_WALLS = FLAG_BIT(2),
		UPLOAD_ALL = UPLOAD_SECTORS | UPLOAD_VERTICES | UPLOAD_WALLS
	};

	void updateCachedWalls(RSector* srcSector, u32 flags, u32& uploadFlags)
	{
		GPUCachedSector* cached = &s_cachedSectors[srcSector->index];
		if (flags & (SDF_HEIGHTS | SDF_AMBIENT))
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

	bool addSectorToList(RSector** list, s32& count, RSector* newSector)
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

	Frustum* frustumGetFront()
	{
		return &s_frustumStack[0];
	}

	// useCamera: use the main camera frustum if true, otherwise use the current frustum.
	bool sphereInFrustum(Vec3f pos, f32 radius, bool useCamera)
	{
		pos = { pos.x - s_cameraPos.x, pos.y - s_cameraPos.y, pos.z - s_cameraPos.z };
		Frustum* frustum = useCamera ? frustumGetFront() : frustumGetBack();

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

	/*
	0 -> 1
	^    V
	3 <- 2
	*/
	struct Polygon
	{
		s32 vertexCount;
		Vec3f vtx[256];
	};

	void createQuad(Vec3f corner0, Vec3f corner1, Polygon* poly)
	{
		for (s32 v = 0; v < 4; v++)
		{
			Vec3f pos;
			pos.x = (v == 0 || v == 3) ? corner0.x : corner1.x;
			pos.y = (v < 2) ? corner0.y : corner1.y;
			pos.z = (v == 0 || v == 3) ? corner0.z : corner1.z;

			poly->vtx[v] = pos;
		}
		poly->vertexCount = 4;
	}

	f32 planeDist(const Vec4f* plane, const Vec3f* pos)
	{
		return (pos->x - s_cameraPos.x) * plane->x + (pos->y - s_cameraPos.y) * plane->y + (pos->z - s_cameraPos.z) * plane->z + plane->w;
	}

	// A small epsilon value to make point vs. plane side determinations more robust to numerical error.
	const f32 c_planeEps = 0.0001f;
	const f32 c_minSqArea = FLT_EPSILON;

	// Return false if the polygon is outside of the frustum, else true.
	// This forms a quad from corner0, corner1 and clips against the "back" frustum on the stack.
	// Returns false if the polygon is not inside the frustum, otherwise returns true with 'output' being the clipped polygon.
	bool clipPortalToFrustum(Vec3f corner0, Vec3f corner1, Polygon* output)
	{
		Frustum* frustum = frustumGetBack();
		s32 count = frustum->planeCount;
		Vec4f* plane = frustum->planes;

		Polygon poly[2];
		Polygon* cur = &poly[0];
		Polygon* next = &poly[1];
		createQuad(corner0, corner1, cur);

		f32 vtxDist[256];
		for (s32 p = 0; p < count; p++, plane++)
		{
			s32 positive = 0, negative = 0;
			next->vertexCount = 0;
			assert(cur->vertexCount <= 256);

			// Process the vertices.
			for (s32 v = 0; v < cur->vertexCount; v++)
			{
				vtxDist[v] = planeDist(plane, &cur->vtx[v]);
				if (vtxDist[v] >= c_planeEps)
				{
					positive++;
				}
				else if (vtxDist[v] <= -c_planeEps)
				{
					negative++;
				}
				else
				{
					// Point is on the plane.
					vtxDist[v] = 0.0f;
				}
			}

			// Return early or skip plane.
			if (positive == cur->vertexCount)
			{
				// Nothing to clip, moving on.
				continue;
			}
			else if (negative == cur->vertexCount)
			{
				// The entire polygon is behind the plane, we are done here.
				return false;
			}

			// Process the edges.
			for (s32 v = 0; v < cur->vertexCount; v++)
			{
				const s32 a = v;
				const s32 b = (v + 1) % cur->vertexCount;

				f32 d0 = vtxDist[a], d1 = vtxDist[b];
				if (d0 < 0.0f && d1 < 0.0f)
				{
					// Cull the edge.
					continue;
				}
				if (d0 >= 0.0f && d1 >= 0.0f)
				{
					// The edge does not need clipping.
					next->vtx[next->vertexCount++] = cur->vtx[a];
					continue;
				}
				// Calculate the edge intersection with the plane.
				const f32 t = -d0 / (d1 - d0);
				Vec3f intersect = { (1.0f - t)*cur->vtx[a].x + t * cur->vtx[b].x, (1.0f - t)*cur->vtx[a].y + t * cur->vtx[b].y,
									(1.0f - t)*cur->vtx[a].z + t * cur->vtx[b].z };
				if (d0 > 0.0f)
				{
					// The first vertex of the edge is inside and should be included.
					next->vtx[next->vertexCount++] = cur->vtx[a];
				}
				// The intersection should be included.
				if (t < 1.0f)
				{
					next->vtx[next->vertexCount++] = intersect;
				}
			}
			// Swap polygons for the next plane.
			std::swap(cur, next);
			// If the new polygon has less than 3 vertices, than it has been culled.
			if (cur->vertexCount < 3)
			{
				return false;
			}
		}

		// Output the clipped polygon.
		assert(cur->vertexCount <= 256);
		output->vertexCount = cur->vertexCount;
		for (s32 i = 0; i < output->vertexCount; i++)
		{
			output->vtx[i] = cur->vtx[i];
		}
		return true;
	}

	// Build a new frustum such that each polygon edge becomes a plane that passes through
	// the camera.
	void computePortalFrustum(const Polygon* clippedPortal)
	{
		s32 count = clippedPortal->vertexCount;
		Frustum newFrustum;
		newFrustum.planeCount = 0;

		// Sides.
		for (s32 i = 0; i < count; i++)
		{
			s32 e0 = i, e1 = (i + 1) % count;
			Vec3f S = { clippedPortal->vtx[e0].x - s_cameraPos.x, clippedPortal->vtx[e0].y - s_cameraPos.y, clippedPortal->vtx[e0].z - s_cameraPos.z };
			Vec3f T = { clippedPortal->vtx[e1].x - s_cameraPos.x, clippedPortal->vtx[e1].y - s_cameraPos.y, clippedPortal->vtx[e1].z - s_cameraPos.z };
			Vec3f N = TFE_Math::cross(&S, &T);
			if (TFE_Math::dot(&N, &N) <= 0.0f)
			{
				continue;
			}
			N = TFE_Math::normalize(&N);
			newFrustum.planes[newFrustum.planeCount++] = { N.x, N.y, N.z, 0.0f };
		}

		// Near plane.
		Vec3f O = { clippedPortal->vtx[0].x - s_cameraPos.x, clippedPortal->vtx[0].y - s_cameraPos.y, clippedPortal->vtx[0].z - s_cameraPos.z };
		Vec3f S = { clippedPortal->vtx[1].x - clippedPortal->vtx[0].x, clippedPortal->vtx[1].y - clippedPortal->vtx[0].y, clippedPortal->vtx[1].z - clippedPortal->vtx[0].z };
		Vec3f T = { clippedPortal->vtx[2].x - clippedPortal->vtx[0].x, clippedPortal->vtx[2].y - clippedPortal->vtx[0].y, clippedPortal->vtx[2].z - clippedPortal->vtx[0].z };
		Vec3f N = TFE_Math::cross(&S, &T);
		N = TFE_Math::normalize(&N);
		f32 d = -TFE_Math::dot(&N, &O);
		newFrustum.planes[newFrustum.planeCount++] = { N.x, N.y, N.z, d };

		frustumPush(newFrustum);
	}

	Vec4f debug_interpolateVec4(Vec4f a, Vec4f b, f32 t)
	{
		Vec4f res;
		res.x = a.x + t * (b.x - a.x);
		res.y = a.y + t * (b.y - a.y);
		res.z = a.z + t * (b.z - a.z);
		res.w = a.w + t * (b.w - a.w);
		return res;
	}

	Vec4f debug_getColorFromLevel(s32 level)
	{
		Vec4f colors[5] =
		{
			{ 0.0f, 1.0f, 0.0f, 1.0f }, // GREEN
			{ 1.0f, 1.0f, 0.0f, 1.0f }, // YELLOW
			{ 1.0f, 0.0f, 0.0f, 1.0f }, // RED
			{ 1.0f, 0.0f, 1.0f, 1.0f }, // PURPLE
			{ 0.0f, 0.0f, 1.0f, 1.0f }, // BLUE
		};

		f32 t = 0.0f;
		s32 a = 0, b = 0;
		if (level < 16)
		{
			t = f32(level) / 16.0f;
			a = 0; b = 1;
		}
		else if (level < 32)
		{
			t = f32(level - 16) / 16.0f;
			a = 1; b = 2;
		}
		else if (level < 64)
		{
			t = f32(level - 32) / 32.0f;
			a = 2; b = 3;
		}
		else
		{
			t = f32(level - 64) / 190.0f;
			a = 3; b = 4;
		}

		return debug_interpolateVec4(colors[a], colors[b], t);
	}

	// Hacky...
	static s32 s_maxPortals = 4096;
	static s32 s_maxWallSeg = 4096;
	static s32 s_portalsTraversed;
	static s32 s_wallSegGenerated;
	void debug_update()
	{
		if (TFE_Input::keyPressed(KEY_LEFTBRACKET))
		{
			if (TFE_Input::getKeyModifierDown() == KEYMOD_SHIFT)
			{
				s_maxWallSeg = max(0, s_maxWallSeg - 1);
			}
			else
			{
				s_maxPortals = max(0, s_maxPortals - 1);
			}
		}
		else if (TFE_Input::keyPressed(KEY_RIGHTBRACKET))
		{
			if (TFE_Input::getKeyModifierDown() == KEYMOD_SHIFT)
			{
				s_maxWallSeg = min(4096, s_maxWallSeg + 1);
			}
			else
			{
				s_maxPortals = min(4096, s_maxPortals + 1);
			}
		}
		else if (TFE_Input::keyPressed(KEY_C))
		{
			if (TFE_Input::getKeyModifierDown() == KEYMOD_SHIFT)
			{
				s_maxWallSeg = 4096;
			}
			else
			{
				s_maxPortals = 4096;
			}
		}
		else if (TFE_Input::keyPressed(KEY_V))
		{
			if (TFE_Input::getKeyModifierDown() == KEYMOD_SHIFT)
			{
				s_maxWallSeg = 0;
			}
			else
			{
				s_maxPortals = 0;
			}
		}
	}

	WallSegBuffer s_bufferPool[1024];
	WallSegBuffer* s_bufferHead;
	WallSegBuffer* s_bufferTail;
	WallSegBuffer* s_portalHead;
	WallSegBuffer* s_portalTail;
	s32 s_bufferPoolCount = 0;

	// Project a 2D coordinate onto the unit square centered around the camera.
	// This returns back a single value where 0.5 = +x,0; 1.5 = 0,+z; 2.5 = -x,0; 3.5 = 0,-z
	// This is done for fast camera relative overlap tests for segments on the XZ axis in a way that
	// is independent of the camera orientation.
	// Once done, segments can be tested for overlap simply by comparing these values - note some care
	// must be taken to pitch the "shortest route" as 4.0 == 0.0
	f32 projectToUnitSquare(Vec2f coord)
	{
		coord.x -= s_cameraPos.x;
		coord.z -= s_cameraPos.z;

		// Find the largest axis.
		s32 axis = 0;
		if (fabsf(coord.x) > fabsf(coord.z))
		{
			axis = coord.x < 0.0f ? 2 : 0;
		}
		else
		{
			axis = coord.z < 0.0f ? 3 : 1;
		}
		f32 axisScale = coord.m[axis & 1] != 0.0f ? 1.0f / fabsf(coord.m[axis & 1]) : 0.0f;
		f32 value = coord.m[(1 + axis) & 1] * axisScale * 0.5f + 0.5f;
		if (axis == 1 || axis == 2) { value = 1.0f - value; }

		return fmodf(f32(axis) + value, 4.0f);	// this wraps around to 0.0 at 4.0
	}

	void testProjections()
	{
		Vec2f testCoords[] =
		{
			// +X
			{ 1.0f,  0.0f },
			{ 1.0f,  0.5f },
			{ 1.0f, -0.5f },
			{ 1.0f,  1.0f },
			{ 1.0f, -1.0f },

			// +Z
			{  0.0f, 1.0f, },
			{  0.5f, 1.0f, },
			{ -0.5f, 1.0f, },
			{  1.0f, 1.0f, },
			{ -1.0f, 1.0f, },

			// -X
			{ -1.0f,  0.0f },
			{ -1.0f,  0.5f },
			{ -1.0f, -0.5f },
			{ -1.0f,  1.0f },
			{ -1.0f, -1.0f },

			// -Z
			{  0.0f, -1.0f, },
			{  0.5f, -1.0f, },
			{ -0.5f, -1.0f, },
			{  1.0f, -1.0f, },
			{ -1.0f, -1.0f, },
		};

		f32 expectedResults[] =
		{
			// +X
			0.5f,  // { 1.0f,  0.0f },
			0.75f, // { 1.0f,  0.5f },
			0.25f, // { 1.0f, -0.5f },
			1.0f,  // { 1.0f,  1.0f },
			0.0f,  // { 1.0f, -1.0f },

			// +Z
			1.5f,  // {  0.0f, 1.0f, },
			1.25f, // {  0.5f, 1.0f, },
			1.75f, // { -0.5f, 1.0f, },
			1.0f,  // {  1.0f, 1.0f, },
			2.0f,  // { -1.0f, 1.0f, },

			// -X
			2.5f,  // { -1.0f,  0.0f },
			2.25f, // { -1.0f,  0.5f },
			2.75f, // { -1.0f, -0.5f },
			2.0f,  // { -1.0f,  1.0f },
			3.0f,  // { -1.0f, -1.0f },

			// -Z
			3.5f,  // {  0.0f, -1.0f, },
			3.75f, // {  0.5f, -1.0f, },
			3.25f, // { -0.5f, -1.0f, },
			0.0f,  // {  1.0f, -1.0f, },
			3.0f,  // { -1.0f, -1.0f, },
		};

		s_cameraPos = { 0.0f, 0.0f };
		for (s32 i = 0; i < TFE_ARRAYSIZE(testCoords); i++)
		{
			const f32 result = projectToUnitSquare(testCoords[i]);
			assert(fabsf(result - expectedResults[i]) < FLT_EPSILON);
		}
	}

	WallSegBuffer* getBufferSeg(WallSegment* seg)
	{
		if (s_bufferPoolCount >= 1024)
		{
			return nullptr;
		}
		WallSegBuffer* segBuffer = &s_bufferPool[s_bufferPoolCount];
		segBuffer->prev = nullptr;
		segBuffer->next = nullptr;
		segBuffer->seg = seg;
		if (seg)
		{
			segBuffer->x0 = seg->x0;
			segBuffer->x1 = seg->x1;
			segBuffer->v0 = seg->v0;
			segBuffer->v1 = seg->v1;
		}
		s_bufferPoolCount++;
		return segBuffer;
	}

	const f32 c_sideEps = 0.0001f;

	// Returns true if 'w1' is in front of 'w0'
	bool wallInFront(Vec2f w0left, Vec2f w0right, Vec3f w0nrml, Vec2f w1left, Vec2f w1right, Vec3f w1nrml)
	{
		// Are w1left and w1right both on the same side of w0nrml as the camera position?
		const Vec2f left0   = { w1left.x  - w0left.x, w1left.z  - w0left.z };
		const Vec2f right0  = { w1right.x - w0left.x, w1right.z - w0left.z };
		const Vec2f camera0 = { s_cameraPos.x - w0left.x, s_cameraPos.z - w0left.z };
		f32 sideLeft0   = w0nrml.x*left0.x  + w0nrml.z*left0.z;
		f32 sideRight0  = w0nrml.x*right0.x + w0nrml.z*right0.z;
		f32 cameraSide0 = w0nrml.x*camera0.x + w0nrml.z*camera0.z;

		// Handle floating point precision.
		if (fabsf(sideLeft0) < c_sideEps) { sideLeft0 = 0.0f; }
		if (fabsf(sideRight0) < c_sideEps) { sideRight0 = 0.0f; }
		if (fabsf(cameraSide0) < c_sideEps) { cameraSide0 = 0.0f; }

		// both vertices of 'w1' are on the same side of 'w0'
		// 'w1' is in front if the camera is on the same side.
		if (sideLeft0 <= 0.0f && sideRight0 <= 0.0f)
		{
			return cameraSide0 <= 0.0f;
		}
		else if (sideLeft0 >= 0.0f && sideRight0 >= 0.0f)
		{
			return cameraSide0 >= 0.0f;
		}

		// Are w0left and w0right both on the same side of w1nrml as the camera position?
		const Vec2f left1 = { w0left.x - w1left.x, w0left.z - w1left.z };
		const Vec2f right1 = { w0right.x - w1left.x, w0right.z - w1left.z };
		const Vec2f camera1 = { s_cameraPos.x - w1left.x, s_cameraPos.z - w1left.z };
		f32 sideLeft1   = w1nrml.x*left1.x + w1nrml.z*left1.z;
		f32 sideRight1  = w1nrml.x*right1.x + w1nrml.z*right1.z;
		f32 cameraSide1 = w1nrml.x*camera1.x + w1nrml.z*camera1.z;

		// Handle floating point precision.
		if (fabsf(sideLeft1) < c_sideEps) { sideLeft1 = 0.0f; }
		if (fabsf(sideRight1) < c_sideEps) { sideRight1 = 0.0f; }
		if (fabsf(cameraSide1) < c_sideEps) { cameraSide1 = 0.0f; }

		// both vertices of 'w0' are on the same side of 'w1'
		// 'w0' is in front if the camera is on the same side.
		if (sideLeft1 <= 0.0f && sideRight1 <= 0.0f)
		{
			return !(cameraSide1 <= 0.0f);
		}
		else if (sideLeft1 >= 0.0f && sideRight1 >= 0.0f)
		{
			return !(cameraSide1 >= 0.0f);
		}

		// If we still get here, just do a quick distance check.
		f32 distSq0 = camera0.x*camera0.x + camera0.z*camera0.z;
		f32 distSq1 = camera1.x*camera1.x + camera1.z*camera1.z;
		return distSq1 < distSq0;
	}

	Vec2f clipSegment(Vec2f v0, Vec2f v1, Vec2f pointOnPlane)
	{
		Vec2f p1 = { pointOnPlane.x - s_cameraPos.x, pointOnPlane.z - s_cameraPos.z };
		Vec2f planeNormal = { -p1.z, p1.x };
		f32 lenSq = p1.x*p1.x + p1.z*p1.z;
		f32 scale = (lenSq > FLT_EPSILON) ? 1.0f / sqrtf(lenSq) : 0.0f;

		f32 d0 = ((v0.x - s_cameraPos.x)*planeNormal.x + (v0.z - s_cameraPos.z)*planeNormal.z) * scale;
		f32 d1 = ((v1.x - s_cameraPos.x)*planeNormal.x + (v1.z - s_cameraPos.z)*planeNormal.z) * scale;
		f32 t = -d0 / (d1 - d0);

		Vec2f intersection;
		intersection.x = (1.0f - t)*v0.x + t * v1.x;
		intersection.z = (1.0f - t)*v0.z + t * v1.z;
		return intersection;
	}

	void insertSegmentBefore(WallSegBuffer* cur, WallSegBuffer* seg, WallSegBuffer*& head = s_bufferHead);
	void insertSegmentAfter(WallSegBuffer* cur, WallSegBuffer* seg, WallSegBuffer*& tail = s_bufferTail);
	void deleteSegment(WallSegBuffer* cur, WallSegBuffer*& head = s_bufferHead, WallSegBuffer*& tail = s_bufferTail);
	WallSegBuffer* mergeSegments(WallSegBuffer* a, WallSegBuffer* b, WallSegBuffer*& tail = s_bufferTail);

	void insertSegmentBefore(WallSegBuffer* cur, WallSegBuffer* seg, WallSegBuffer*& head)
	{
		WallSegBuffer* curPrev = cur->prev;
		if (curPrev)
		{
			curPrev->next = seg;
		}
		else
		{
			head = seg;
		}
		seg->prev = curPrev;
		seg->next = cur;
		cur->prev = seg;
	}

	void insertSegmentAfter(WallSegBuffer* cur, WallSegBuffer* seg, WallSegBuffer*& tail)
	{
		WallSegBuffer* curNext = cur->next;
		if (curNext)
		{
			curNext->prev = seg;
		}
		else
		{
			tail = seg;
		}
		seg->next = curNext;
		seg->prev = cur;
		cur->next = seg;
	}

	void deleteSegment(WallSegBuffer* cur, WallSegBuffer*& head, WallSegBuffer*& tail)
	{
		WallSegBuffer* curPrev = cur->prev;
		WallSegBuffer* curNext = cur->next;
		if (curPrev)
		{
			curPrev->next = curNext;
		}
		else
		{
			head = curNext;
		}

		if (curNext)
		{
			curNext->prev = curPrev;
		}
		else
		{
			tail = curPrev;
		}
	}

	WallSegBuffer* mergeSegments(WallSegBuffer* a, WallSegBuffer* b, WallSegBuffer*& tail)
	{
		a->next = b->next;
		a->x1 = b->x1;
		a->v1 = b->v1;

		if (a->next)
		{
			a->next->prev = a;
		}
		else
		{
			s_bufferTail = a;
		}
		return a;
	}

	void swapSegments(WallSegBuffer* a, WallSegBuffer* b, WallSegBuffer*& head, WallSegBuffer*& tail)
	{
		WallSegBuffer* prev = a->prev;
		WallSegBuffer* next = b->next;
		b->prev = prev;
		if (prev)
		{
			prev->next = b;
		}
		else
		{
			head = b;
		}
		b->next = a;
		a->prev = b;
		a->next = next;
		if (next)
		{
			next->prev = a;
		}
		else
		{
			tail = a;
		}
	}

	void mergeSegmentsInBuffer()
	{
		WallSegBuffer* cur = s_bufferHead;
		while (cur)
		{
			WallSegBuffer* curNext = cur->next;
			while (curNext && cur->x1 == curNext->x0 && cur->seg->id == curNext->seg->id)
			{
				cur = mergeSegments(cur, curNext, s_bufferTail);
				curNext = cur->next;
			}
			cur = cur->next;
		}
	}

	void mergePortalsInBuffer()
	{
		WallSegBuffer* cur = s_portalHead;
		while (cur)
		{
			WallSegBuffer* curNext = cur->next;
			while (curNext && cur->x1 == curNext->x0 && cur->seg->id == curNext->seg->id)
			{
				cur = mergeSegments(cur, curNext, s_portalTail);
				curNext = cur->next;
			}
			cur = cur->next;
		}

		// Try to merge the head and tail because they might have been split on the modulo line.
		if (s_portalHead != s_portalTail)
		{
			if (s_portalHead->x0 == 0.0f && s_portalTail->x1 == 4.0f && s_portalHead->seg->id == s_portalTail->seg->id)
			{
				s_portalTail->x1 = s_portalHead->x1 + 4.0f;
				s_portalTail->v1 = s_portalHead->v1;
				s_portalHead = s_portalHead->next;
				s_portalHead->prev = nullptr;
			}
		}
	}

	bool segmentsOverlap(f32 ax0, f32 ax1, f32 bx0, f32 bx1)
	{
		return (ax1 > bx0 && ax0 < bx1) || (bx1 > ax0 && bx0 < ax1);// || (ax1 - 4.0f > bx0 && ax0 - 4.0f < bx1) || (bx1 - 4.0f > ax0 && bx0 - 4.0f < ax1);
	}
			
	void insertPortal(WallSegBuffer* portal)
	{
		if (!s_portalHead)
		{
			s_portalHead = portal;
			s_portalTail = portal;
			return;
		}
		// Hack
		//insertSegmentBefore(s_portalTail, portal, s_portalHead);
		//return;

		WallSegBuffer* cur = s_portalHead;
		bool inserted = false;
		while (cur)
		{
			if (segmentsOverlap(portal->x0, portal->x1, cur->x0, cur->x1))
			{
				if (!inserted)
				{
					insertSegmentBefore(cur, portal, s_portalHead);
					inserted = true;
				}

				bool curInFront = wallInFront(portal->v0, portal->v1, portal->seg->normal, cur->v0, cur->v1, cur->seg->normal);
				if (curInFront)
				{
					// Make sure that portal is *after* cur
					swapSegments(portal, cur, s_portalHead, s_portalTail);
					cur = portal;
				}
			}
			else if (portal->x1 <= cur->x0)
			{
				if (!inserted)
				{
					insertSegmentBefore(cur, portal, s_portalHead);
				}
				return;
			}

			cur = cur->next;
		}
		if (!inserted)
		{
			insertSegmentAfter(s_portalTail, portal, s_portalTail);
		}
	}

	// Inserts portal segments into the buffer:
	// * Clips portal segments against the solid segments, discard hidden parts.
	// * A single span of space may hold multiple portals - but they are sorted front to back.
	void insertPortalIntoBuffer(WallSegment* seg)
	{
		WallSegBuffer* cur = s_bufferHead;
		while (cur)
		{
			// Do the segments overlap?
			if (segmentsOverlap(seg->x0, seg->x1, cur->x0, cur->x1))
			{
				bool curInFront = wallInFront(seg->v0, seg->v1, seg->normal, cur->v0, cur->v1, cur->seg->normal);
				if (curInFront)
				{
					// Seg can be discarded, which means we are done here.
					if (seg->x0 >= cur->x0 && seg->x1 <= cur->x1) { return; }

					// Clip the portal and insert.
					s32 clipFlags = 0;
					if (seg->x0 < cur->x0) { clipFlags |= 1; } // Seg sticks out of the left side and should be clipped.
					if (seg->x1 > cur->x1) { clipFlags |= 2; } // Seg sticks out of the right side and should be clipped.

					assert(clipFlags);
					if (!clipFlags) { return; }

					// If the left side needs to be clipped, it can be added without continue with loop.
					if (clipFlags & 1)
					{
						WallSegBuffer* portal = getBufferSeg(seg);
						portal->x1 = cur->x0;
						portal->v1 = clipSegment(seg->v0, seg->v1, cur->v0);
						insertPortal(portal);
					}
					// If the right side is clipped, then we must continue with the loop.
					if (clipFlags & 2)
					{
						seg->x0 = cur->x1;
						seg->v0 = clipSegment(seg->v0, seg->v1, cur->v1);
					}
					else
					{
						return;
					}
				}
				else
				{
					if (seg->x1 > cur->x1)
					{
						// New segment that gets clipped by the edge of 'cur'.
						WallSegBuffer* portal = getBufferSeg(seg);
						portal->x1 = cur->x1;
						portal->v1 = clipSegment(seg->v0, seg->v1, cur->v1);
						insertPortal(portal);

						// Left over part for the rest of the loop.
						seg->x0 = portal->x1;
						seg->v0 = portal->v1;
					}
					else  // Insert the full new segment.
					{
						insertPortal(getBufferSeg(seg));
						return;
					}
				}
			}
			else if (seg->x1 <= cur->x0)  // Left
			{
				insertPortal(getBufferSeg(seg));
				return;
			}
			cur = cur->next;
		}
		insertPortal(getBufferSeg(seg));
	}
		
	// Inserts a solid segment into the buffer, which has the following properties:
	// * Only keeps front segments.
	// * Segments clip, so only one segment exists in any span of space.
	// * This is basically an 'S-buffer' - but wrapping around the camera in world space.
	void insertSegmentIntoBuffer(WallSegment* seg)
	{
		if (!s_bufferHead)
		{
			s_bufferHead = getBufferSeg(seg);
			s_bufferTail = s_bufferHead;
			return;
		}

		// Go through and see if there is an overlap.
		WallSegBuffer* cur = s_bufferHead;
		while (cur)
		{
			// Do the segments overlap?
			if (segmentsOverlap(seg->x0, seg->x1, cur->x0, cur->x1))
			{
				bool curInFront = wallInFront(seg->v0, seg->v1, seg->normal, cur->v0, cur->v1, cur->seg->normal);
				if (curInFront)
				{
					// Seg can be discarded, which means we are done here.
					if (seg->x0 >= cur->x0 && seg->x1 <= cur->x1) { return; }

					s32 clipFlags = 0;
					if (seg->x0 < cur->x0) { clipFlags |= 1; } // Seg sticks out of the left side and should be clipped.
					if (seg->x1 > cur->x1) { clipFlags |= 2; } // Seg sticks out of the right side and should be clipped.
					if (!clipFlags) { return; }

					// If the left side needs to be clipped, it can be added without continue with loop.
					if (clipFlags & 1)
					{
						WallSegBuffer* newEntry = getBufferSeg(seg);
						newEntry->x1 = cur->x0;
						newEntry->v1 = clipSegment(seg->v0, seg->v1, cur->v0);

						insertSegmentBefore(cur, newEntry, s_bufferHead);
					}
					// If the right side is clipped, then we must continue with the loop.
					if (clipFlags & 2)
					{
						seg->x0 = cur->x1;
						seg->v0 = clipSegment(seg->v0, seg->v1, cur->v1);
					}
					else
					{
						return;
					}
				}
				else
				{
					s32 clipFlags = 0;
					if (cur->x0 < seg->x0) { clipFlags |= 1; } // Current segment is partially on the left.
					if (cur->x1 > seg->x1) { clipFlags |= 2; } // Current segment is partially on the right.
					if (seg->x1 > cur->x1) { clipFlags |= 4; } // Create a new segment on the right side of the current segment.

					// Save so the current segment can be modified.
					const f32 curX1 = cur->x1;
					const Vec2f curV1 = cur->v1;

					if (clipFlags & 1)
					{
						// [cur | segEntry ...]
						cur->x1 = seg->x0;
						cur->v1 = clipSegment(cur->v0, cur->v1, seg->v0);
					}
					if (clipFlags & 2)
					{
						WallSegBuffer* segEntry = getBufferSeg(seg);
						WallSegBuffer* curRight = getBufferSeg(cur->seg);
						curRight->x0 = seg->x1;
						curRight->v0 = clipSegment(cur->v0, curV1, seg->v1);
						curRight->x1 = curX1;
						curRight->v1 = curV1;

						insertSegmentAfter(cur, segEntry, s_bufferTail);
						insertSegmentAfter(segEntry, curRight, s_bufferTail);
						if (!(clipFlags & 1))
						{
							deleteSegment(cur, s_bufferHead, s_bufferTail);
						}
						return;
					}
					else if (clipFlags & 4)
					{
						// New segment that gets clipped by the edge of 'cur'.
						WallSegBuffer* segEntry = getBufferSeg(seg);
						segEntry->x1 = curX1;
						segEntry->v1 = clipSegment(seg->v0, seg->v1, curV1);

						// Left over part for the rest of the loop.
						seg->x0 = segEntry->x1;
						seg->v0 = segEntry->v1;

						insertSegmentAfter(cur, segEntry, s_bufferTail);
						if (!(clipFlags & 1))
						{
							deleteSegment(cur, s_bufferHead, s_bufferTail);
						}

						// continue with loop...
						cur = segEntry;
					}
					else  // Insert the full new segment.
					{
						insertSegmentAfter(cur, getBufferSeg(seg), s_bufferTail);
						if (!(clipFlags & 1))
						{
							deleteSegment(cur, s_bufferHead, s_bufferTail);
						}
						return;
					}
				}
			}
			else if (seg->x1 <= cur->x0) // Left
			{
				insertSegmentBefore(cur, getBufferSeg(seg), s_bufferHead);
				return;
			}

			cur = cur->next;
		}
		// The new segment is to the right of everything.
		insertSegmentAfter(s_bufferTail, getBufferSeg(seg), s_bufferTail);
	}

	// Build world-space wall segments.
	void buildSectorWallSegments(RSector* curSector, u32& uploadFlags, bool debugDrawWallSegs)
	{
		static WallSegment solidSegments[1024];
		static WallSegment portalSegments[1024];

		u32 solid = 0, portal = 0;
		GPUCachedSector* cached = &s_cachedSectors[curSector->index];
		// Already built this frame.
		if (cached->builtFrame == s_gpuFrame)
		{
			return;
		}
		cached->builtFrame = s_gpuFrame;
						
		// Build segments, skipping any backfacing walls or any that are outside of the camera frustum.
		// Identify walls as solid or portals.
		for (s32 w = 0; w < curSector->wallCount; w++)
		{
			RWall* wall = &curSector->walls[w];
			RSector* next = wall->nextSector;

			if (s_wallSegGenerated >= s_maxWallSeg)
			{
				break;
			}

			const f32 x0 = fixed16ToFloat(wall->w0->x);
			const f32 x1 = fixed16ToFloat(wall->w1->x);
			const f32 z0 = fixed16ToFloat(wall->w0->z);
			const f32 z1 = fixed16ToFloat(wall->w1->z);
			f32 y0 = cached->ceilingHeight;
			f32 y1 = cached->floorHeight;
			f32 portalY0, portalY1;
						
			// Is the wall a portal or is it effectively solid?
			bool isPortal = false;
			if (next)
			{
				// Update any potential adjoins even if they are not traversed to make sure the
				// heights and walls settings are handled correctly.
				updateCachedSector(next, uploadFlags);

				const fixed16_16 openTop = min(curSector->floorHeight,   max(curSector->ceilingHeight, next->ceilingHeight));
				const fixed16_16 openBot = max(curSector->ceilingHeight, min(curSector->floorHeight,   next->floorHeight));
				const fixed16_16 openSize = openBot - openTop;
				portalY0 = fixed16ToFloat(openTop);
				portalY1 = fixed16ToFloat(openBot);

				if (openSize > 0)
				{
					// Is the portal inside the view frustum?
					const Vec3f portalPos = { (x0 + x1) * 0.5f, (portalY0 + portalY1) * 0.5f, (z0 + z1) * 0.5f };
					const Vec3f maxPos = { max(x0, x1), max(portalY0, portalY1), max(z0, z1) };
					const Vec3f diag = { maxPos.x - portalPos.x, maxPos.y - portalPos.y, maxPos.z - portalPos.z };
					const f32 portalRadius = sqrtf(diag.x*diag.x + diag.y*diag.y + diag.z*diag.z);
					if (!sphereInFrustum(portalPos, portalRadius, true))
					{
						continue;
					}
					isPortal = true;
				}
			}

			// Only apply backfacing to portals.
			// Is the wall facing towards the camera?
			const Vec3f wallNormal = { -(z1 - z0), 0.0f, x1 - x0 };
			if (isPortal)
			{
				const Vec3f cameraVec = { x0 - s_cameraPos.x, (y0 + y1)*0.5f - s_cameraPos.y, z0 - s_cameraPos.z };
				if (wallNormal.x*cameraVec.x + wallNormal.y*cameraVec.y + wallNormal.z*cameraVec.z < 0.0f)
				{
					continue;
				}
			}

			// Is the wall inside the view frustum?
			WallSegment* seg;
			if (!isPortal)
			{
				Vec3f wallPos = { (x0 + x1) * 0.5f, (y0 + y1) * 0.5f, (z0 + z1) * 0.5f };
				Vec3f maxPos = { max(x0, x1), max(y0, y1), max(z0, z1) };
				Vec3f diag = { maxPos.x - wallPos.x, maxPos.y - wallPos.y, maxPos.z - wallPos.z };
				f32 portalRadius = sqrtf(diag.x*diag.x + diag.y*diag.y + diag.z*diag.z);
				if (!sphereInFrustum(wallPos, portalRadius, true))
				{
					continue;
				}

				seg = &solidSegments[solid];
				solid++;
			}
			else
			{
				seg = &portalSegments[portal];
				portal++;
			}

			seg->id = w;
			seg->v0 = { x0, z0 };
			seg->v1 = { x1, z1 };
			seg->x0 = 4.0f - projectToUnitSquare(seg->v0);
			seg->x1 = 4.0f - projectToUnitSquare(seg->v1);

			// This means both vertices map to the same point on the unit square, in other words, the edge isn't actually visible.
			if (fabsf(seg->x0 - seg->x1) < FLT_EPSILON)
			{
				if (isPortal) { portal--; }
				else { solid--; }
				continue;
			}

			// Make sure the segment is the correct length.
			if (fabsf(seg->x1 - seg->x0) > 2.0f)
			{
				if (seg->x0 < 1.0f) { seg->x0 += 4.0f; }
				else { seg->x1 += 4.0f; }
			}
			// Handle wrapping.
			while (seg->x0 < 0.0f || seg->x1 < 0.0f)
			{
				seg->x0 += 4.0f;
				seg->x1 += 4.0f;
			}
			while (seg->x0 >= 4.0f && seg->x1 >= 4.0f)
			{
				seg->x0 = fmodf(seg->x0, 4.0f);
				seg->x1 = fmodf(seg->x1, 4.0f);
			}
			// For backfacing solid segments, we need to flip them.
			// Note: portals should *never* be backfacing since they are discarded above.
			if (seg->x0 > seg->x1)
			{
				if (!seg->portal)
				{
					std::swap(seg->x0, seg->x1);
					std::swap(seg->v0, seg->v1);
				}
				else // back face culling.
				{
					continue;
				}
			}
			// Check again for zero-length walls in case the fix-ups above caused it (for example, x0 = 0.0, x1 = 4.0).
			if (fabsf(seg->x0 - seg->x1) < FLT_EPSILON || seg->x0 == seg->x1)
			{
				if (isPortal) { portal--; }
				else { solid--; }

				continue;
			}
			assert(seg->x1 - seg->x0 > 0.0f && seg->x1 - seg->x0 <= 2.0f);

			seg->normal = wallNormal;
			seg->portal = isPortal;
			seg->y0 = y0;
			seg->y1 = y1;
			seg->portalY0 = isPortal ? portalY0 : y0;
			seg->portalY1 = isPortal ? portalY1 : y1;

			// Create a duplicate segment when crossing the modolo boundary.
			if (seg->x1 > 4.0f)
			{
				const f32 sx1 = seg->x1;
				const Vec2f sv1 = seg->v1;

				// Split the segment at the modulus border.
				seg->v1 = clipSegment(seg->v0, seg->v1, { 1.0f + s_cameraPos.x, -1.0f + s_cameraPos.z });
				seg->x1 = 4.0f;
				s_wallSegGenerated++;
				
				if (s_wallSegGenerated < s_maxWallSeg)
				{
					WallSegment* seg2;
					if (seg->portal)
					{
						seg2 = &portalSegments[portal];
						portal++;
					}
					else
					{
						seg2 = &solidSegments[solid];
						solid++;
					}

					*seg2 = *seg;
					seg2->x0 = 0.0f;
					seg2->x1 = sx1 - 4.0f;
					seg2->v0 = seg->v1;
					seg2->v1 = sv1;
				}
			}

			assert(!seg->portal || next);
			s_wallSegGenerated++;
		}

		// Next insert solid segments into the segment buffer one at a time.
		s_bufferPoolCount = 0;
		s_bufferHead = nullptr;
		s_bufferTail = nullptr;
		for (s32 i = 0; i < solid; i++)
		{
			insertSegmentIntoBuffer(&solidSegments[i]);
		}
		mergeSegmentsInBuffer();

		if (debugDrawWallSegs)
		{
			WallSegBuffer* wallSeg = s_bufferHead;
			while (wallSeg)
			{
				Vec3f vtx[4];
				vtx[0] = { wallSeg->v0.x, wallSeg->seg->y0, wallSeg->v0.z };
				vtx[1] = { wallSeg->v1.x, wallSeg->seg->y0, wallSeg->v1.z };
				vtx[2] = { wallSeg->v1.x, wallSeg->seg->y1, wallSeg->v1.z };
				vtx[3] = { wallSeg->v0.x, wallSeg->seg->y1, wallSeg->v0.z };
				renderDebug_addPolygon(4, vtx, { 1.0f, 0.0f, 1.0f, 1.0f });

				wallSeg = wallSeg->next;
			}
		}

		// Next insert portals into the segment list.
		s_portalHead = nullptr;
		s_portalTail = nullptr;
		for (s32 i = 0; i < portal; i++)
		{
			insertPortalIntoBuffer(&portalSegments[i]);
		}
		mergePortalsInBuffer();

		// Insert.
		cached->portalSegCount = 0;
		WallSegBuffer* portalSeg = s_portalHead;
		while (portalSeg)
		{
			WallSegment* seg = &cached->portalSeg[cached->portalSegCount];
			cached->portalSegCount++;
			assert(cached->portalSegCount <= cached->maxPortalSegCount);

			seg->id = portalSeg->seg->id;
			seg->normal = portalSeg->seg->normal;
			seg->portalY0 = portalSeg->seg->portalY0;
			seg->portalY1 = portalSeg->seg->portalY1;
			seg->v0 = portalSeg->v0;
			seg->v1 = portalSeg->v1;
			assert(portalSeg->seg->portal);

			portalSeg = portalSeg->next;
		}
	}

	void traverseAdjoin(RSector* curSector, RSector** drawList, s32& drawCount, s32& level, u32& uploadFlags)
	{
		//if (level >= 255)
		if (level >= 64)
		{
			return;
		}

		// Build the world-space wall segments.
		buildSectorWallSegments(curSector, uploadFlags, level == 0);

		// Then loop through the portals and recurse.
		GPUCachedSector* cached = &s_cachedSectors[curSector->index];
		for (s32 p = 0; p < cached->portalSegCount; p++)
		{
			if (s_portalsTraversed >= s_maxPortals)
			{
				break;
			}
			
			WallSegment* portal = &cached->portalSeg[p];

			RWall* wall = &curSector->walls[portal->id];
			RSector* next = wall->nextSector;
			assert(next);

			f32 x0 = portal->v0.x;
			f32 x1 = portal->v1.x;
			f32 z0 = portal->v0.z;
			f32 z1 = portal->v1.z;
			f32 y0 = portal->portalY0;
			f32 y1 = portal->portalY1;

			// Is the portal inside the view frustum?
			Vec3f portalPos = { (x0 + x1) * 0.5f, (y0 + y1) * 0.5f, (z0 + z1) * 0.5f };
			Vec3f maxPos = { max(x0, x1), max(y0, y1), max(z0, z1) };
			Vec3f diag = { maxPos.x - portalPos.x, maxPos.y - portalPos.y, maxPos.z - portalPos.z };
			f32 portalRadius = sqrtf(diag.x*diag.x + diag.y*diag.y + diag.z*diag.z);
			if (!sphereInFrustum(portalPos, portalRadius, false))
			{
				continue;
			}

			// Clip the portal by the current frustum, and return if it is culled.
			Polygon clippedPortal;
			if (!clipPortalToFrustum({ x0, y0, z0 }, { x1, y1, z1 }, &clippedPortal))
			{
				continue;
			}
			s_portalsTraversed++;

			// Debug
			if (s_enableDebug)
			{
				s_portalsTraversed++;
				if (s_portalsTraversed == s_maxPortals)
				{
					Vec4f curPortalColor = { 1.0f, 0.0f, 0.0f, 1.0f };
					renderDebug_addPolygon(clippedPortal.vertexCount, clippedPortal.vtx, curPortalColor);
				}
				else
				{
					renderDebug_addPolygon(clippedPortal.vertexCount, clippedPortal.vtx, debug_getColorFromLevel(level));
				}
			}

			// Add the sector to the list to draw.
			addSectorToList(drawList, drawCount, next);
			// Then compute the new frustum, and continue to traverse.
			computePortalFrustum(&clippedPortal);
			level++;

			// Sector elements.
			if (s_sectorElemCount < 4096)
			{
				s_sectorElements[s_sectorElemCount].parentSectorIndex = curSector->index;
				s_sectorElements[s_sectorElemCount].adjoinIndex = portal->id;
				s_sectorElements[s_sectorElemCount].sectorIndex = next->index;
				s_sectorElements[s_sectorElemCount].level = level;
				s_sectorElemCount++;
			}

			traverseAdjoin(next, drawList, drawCount, level, uploadFlags);

			level--;
			// Pop the frustum.
			frustumPop();
		}

	#if 0
		for (s32 w = 0; w < curSector->wallCount; w++)
		{
			RWall* wall = &curSector->walls[w];
			RSector* next = wall->nextSector;
			if (next)
			{
				// Update any potential adjoins even if they are not traversed to make sure the
				// heights and walls settings are handled correctly.
				updateCachedSector(next, uploadFlags);

				if (s_portalsTraversed >= s_maxPortals)
				{
					continue;
				}

				// Determine the size of the opening, if it is too small no traversal is necessary.
				const fixed16_16 openTop = min(curSector->floorHeight,   max(curSector->ceilingHeight, next->ceilingHeight));
				const fixed16_16 openBot = max(curSector->ceilingHeight, min(curSector->floorHeight, next->floorHeight));
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
					if (!sphereInFrustum(portalPos, portalRadius, false))
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

					// Clip the portal by the current frustum, and return if it is culled.
					Polygon clippedPortal;
					if (!clipPortalToFrustum({ x0, y0, z0 }, { x1, y1, z1 }, &clippedPortal))
					{
						continue;
					}

					// Debug
					if (s_enableDebug)
					{
						s_portalsTraversed++;
						if (s_portalsTraversed == s_maxPortals)
						{
							Vec4f curPortalColor = { 1.0f, 0.0f, 0.0f, 1.0f };
							renderDebug_addPolygon(clippedPortal.vertexCount, clippedPortal.vtx, curPortalColor);
						}
						else
						{
							renderDebug_addPolygon(clippedPortal.vertexCount, clippedPortal.vtx, debug_getColorFromLevel(level));
						}
					}

					// Add the sector to the list to draw.
					addSectorToList(drawList, drawCount, next);
					// Then compute the new frustum, and continue to traverse.
					computePortalFrustum(&clippedPortal);
					level++;

					// Sector elements.
					if (s_sectorElemCount < 4096)
					{
						s_sectorElements[s_sectorElemCount].parentSectorIndex = curSector->index;
						s_sectorElements[s_sectorElemCount].adjoinIndex = w;
						s_sectorElements[s_sectorElemCount].sectorIndex = next->index;
						s_sectorElements[s_sectorElemCount].level = level;
						s_sectorElemCount++;
					}

					traverseAdjoin(next, drawList, drawCount, level, uploadFlags);

					level--;
					// Pop the frustum.
					frustumPop();
				}
			}
		}
	#endif
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

		// Scale the projection slightly, so the frustum is slightly bigger than needed (i.e. something like a guard band).
		// This is done by exact clipping isn't actually necessary for rendering, clipping is only used for portal testing.
		f32 superScale = 0.98f;
		Mat4 proj = s_cameraProj;
		proj.m0.x *= superScale;
		proj.m1.y *= superScale;

		// Move the near plane to 0, which is allowed since clipping is done in world space.
		proj.m2.z = -1.0f;
		proj.m2.w =  0.0f;

		Mat4 viewProj = transpose4(TFE_Math::mulMatrix4(proj, view4));
		m_viewDir = s_cameraMtx.m2;

		// TODO: properly handle widening the frustum slightly to avoid clipping artifacts.

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
		frustum.planeCount = 5;  // Ignore the far plane for now.
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
		
	void buildDrawList(RSector* sector)
	{
		debug_update();

		// First build the camera frustum and push it onto the stack.
		buildFrustumFromCamera();

		// Sector elements.
		s_sectorElemCount = 1;
		s_sectorElements[0].parentSectorIndex = -1;
		s_sectorElements[0].adjoinIndex = -1;
		s_sectorElements[0].sectorIndex = sector->index;
		s_sectorElements[0].level = 0;

		// Then build the draw list.
		s_drawCount = 1;
		s_drawList[0] = sector;

		s32 level = 0;
		u32 uploadFlags = UPLOAD_NONE;
		s_portalsTraversed = 0;
		s_wallSegGenerated = 0;
				
		updateCachedSector(sector, uploadFlags);
		traverseAdjoin(sector, s_drawList, s_drawCount, level, uploadFlags);
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

	void drawSector(RSector* curSector, GPUCachedSector* cachedSector)
	{
		s32 sectorData1[] = { cachedSector->wallOffset, 0, 0, 0 };
		f32 sectorData2[] = { fixed16ToFloat(curSector->floorHeight), fixed16ToFloat(curSector->ceilingHeight), 0.0f, 0.0f };
		f32 shaderBounds[] = { fixed16ToFloat(curSector->boundsMin.x), fixed16ToFloat(curSector->boundsMin.z), fixed16ToFloat(curSector->boundsMax.x), fixed16ToFloat(curSector->boundsMax.z) };
		m_wallShader.setVariable(m_sectorDataId, SVT_IVEC4, (f32*)sectorData1);
		m_wallShader.setVariable(m_sectorData2Id, SVT_VEC4, sectorData2);
		m_wallShader.setVariable(m_sectorBoundsId, SVT_VEC4, shaderBounds);

		// Draw.
		TFE_RenderBackend::drawIndexedTriangles(2 * cachedSector->partCount, sizeof(u16));
	}

	void TFE_Sectors_GPU::draw(RSector* sector)
	{
		// testProjections();

		// Build the draw list.
		buildDrawList(sector);

		// State
		TFE_RenderState::setStateEnable(false, STATE_BLEND);
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
	#if 0
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
	#endif	

		TFE_RenderState::setStateEnable(true, STATE_STENCIL_TEST | STATE_STENCIL_WRITE);
		TFE_RenderState::setStencilFunction(CMP_ALWAYS, 0);
		TFE_RenderState::setStencilOp(OP_KEEP, OP_KEEP, OP_KEEP);
				
		// Draw sector fragments.
		SectorElement* fragment = s_sectorElements;
		for (s32 s = 0; s < s_sectorElemCount; s++, fragment++)
		{
			// For now just draw the sector - note this will generate a lot of overdraw...
			RSector* curSector = &s_sectors[fragment->sectorIndex];
			GPUCachedSector* cachedSector = &s_cachedSectors[curSector->index];

			if (fragment->parentSectorIndex >= 0)
			{
				// First draw the adjoin portal to the stencil buffer.
				TFE_RenderState::setStencilFunction(CMP_EQUAL, fragment->level - 1);
				TFE_RenderState::setStencilOp(OP_KEEP, OP_KEEP, OP_INC);
				TFE_RenderState::setColorMask(CMASK_NONE);
				TFE_RenderState::setStateEnable(false, STATE_DEPTH_WRITE);
				// Draw portal quad here.
				RSector* parent = &s_sectors[fragment->parentSectorIndex];
				GPUCachedSector* pCached = &s_cachedSectors[fragment->parentSectorIndex];
				RWall* srcWall = &parent->walls[fragment->adjoinIndex];
				s32 i0 = pCached->vertexOffset + (s32)((u8*)srcWall->v0 - (u8*)parent->verticesVS) / sizeof(vec2_fixed);
				s32 i1 = pCached->vertexOffset + (s32)((u8*)srcWall->v1 - (u8*)parent->verticesVS) / sizeof(vec2_fixed);

				s32 sectorData1[] = { pCached->wallOffset, 1 + i0, i1, curSector->index };
				f32 sectorData2[] = { pCached->floorHeight, pCached->ceilingHeight, 0.0f, 0.0f };
				m_wallShader.setVariable(m_sectorDataId, SVT_IVEC4, (f32*)sectorData1);
				m_wallShader.setVariable(m_sectorData2Id, SVT_VEC4, sectorData2);
				TFE_RenderBackend::drawIndexedTriangles(2, sizeof(u16));

				// Then render the sector, reading from the stencil values.
				TFE_RenderState::setColorMask(CMASK_ALL);
				TFE_RenderState::setStateEnable(true, STATE_DEPTH_WRITE);
				TFE_RenderState::setStencilFunction(CMP_EQUAL, fragment->level);
				TFE_RenderState::setStencilOp(OP_KEEP, OP_KEEP, OP_KEEP);
			}
			// Draw sector here.
			drawSector(curSector, cachedSector);
		}

		// Cleanup.
		TFE_RenderState::setColorMask(CMASK_ALL);
		TFE_RenderState::setStateEnable(false, STATE_STENCIL_TEST | STATE_STENCIL_WRITE);
		TFE_RenderState::setStencilFunction(CMP_ALWAYS, 0);
		TFE_RenderState::setStencilOp(OP_KEEP, OP_KEEP, OP_KEEP);

		m_wallShader.unbind();
		m_vertexBuffer.unbind();
		m_indexBuffer.unbind();

		// Debug
		if (s_enableDebug)
		{
			renderDebug_draw();
		}
	}

	void TFE_Sectors_GPU::subrendererChanged()
	{
	}
}