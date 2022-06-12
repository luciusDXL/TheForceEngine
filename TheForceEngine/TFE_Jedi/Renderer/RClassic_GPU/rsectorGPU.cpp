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
#include "modelGPU.h"
#include "renderDebug.h"
#include "debug.h"
#include "frustum.h"
#include "sbuffer.h"
#include "sectorDisplayList.h"
#include "spriteDisplayList.h"
#include "texturePacker.h"
#include "../rcommon.h"

#define PTR_OFFSET(ptr, base) size_t((u8*)ptr - (u8*)base)
using namespace TFE_RenderBackend;

namespace TFE_Jedi
{
	enum UploadFlags
	{
		UPLOAD_NONE     = 0,
		UPLOAD_SECTORS  = FLAG_BIT(0),
		UPLOAD_VERTICES = FLAG_BIT(1),
		UPLOAD_WALLS    = FLAG_BIT(2),
		UPLOAD_ALL      = UPLOAD_SECTORS | UPLOAD_VERTICES | UPLOAD_WALLS
	};

	enum Constants
	{
		SPRITE_PASS = SECTOR_PASS_COUNT
	};
		
	struct GPUSourceData
	{
		Vec4f* sectors;
		Vec4f* walls;
		u32 sectorSize;
		u32 wallSize;
	};

	struct Portal
	{
		Vec2f v0, v1;
		f32 y0, y1;
		RSector* next;
		Frustum frustum;
		RWall* wall;
	};
		
	GPUSourceData s_gpuSourceData = { 0 };

	TextureGpu* s_colormapTex;
	Shader m_wallShader[SECTOR_PASS_COUNT];
	Shader m_spriteShader;
	ShaderBuffer m_sectors;
	ShaderBuffer m_walls;
	s32 m_cameraPosId[SECTOR_PASS_COUNT+1];
	s32 m_cameraViewId[SECTOR_PASS_COUNT+1];
	s32 m_cameraProjId[SECTOR_PASS_COUNT+1];
	s32 m_cameraDirId[SECTOR_PASS_COUNT+1];
	s32 m_lightDataId[SECTOR_PASS_COUNT+1];
	s32 m_skyParallaxId[SECTOR_PASS_COUNT];
	s32 m_cameraRightId;
	Vec3f m_viewDir;
	
	IndexBuffer m_indexBuffer;
	static GPUCachedSector* s_cachedSectors;
	static bool s_enableDebug = false;
	static s32 s_gpuFrame;

	static Portal s_portalList[2048];
	static s32 s_portalListCount = 0;

	static TexturePacker* s_levelTextures = nullptr;
	static TexturePacker* s_objectTextures = nullptr;
		
	extern Mat3  s_cameraMtx;
	extern Mat4  s_cameraProj;
	extern Vec3f s_cameraPos;
	extern Vec3f s_cameraDir;
	extern Vec3f s_cameraRight;

	bool loadSpriteShader()
	{
		if (!m_spriteShader.load("Shaders/gpu_render_sprite.vert", "Shaders/gpu_render_sprite.frag", 0, nullptr, SHADER_VER_STD))
		{
			return false;
		}

		m_cameraPosId[SPRITE_PASS]   = m_spriteShader.getVariableId("CameraPos");
		m_cameraViewId[SPRITE_PASS]  = m_spriteShader.getVariableId("CameraView");
		m_cameraRightId              = m_spriteShader.getVariableId("CameraRight");
		m_cameraProjId[SPRITE_PASS]  = m_spriteShader.getVariableId("CameraProj");
		m_cameraDirId[SPRITE_PASS]   = m_spriteShader.getVariableId("CameraDir");
		m_lightDataId[SPRITE_PASS]   = m_spriteShader.getVariableId("LightData");

		m_spriteShader.bindTextureNameToSlot("DrawListPosTexture",  0);
		m_spriteShader.bindTextureNameToSlot("DrawListScaleOffset", 1);
		m_spriteShader.bindTextureNameToSlot("Colormap",     2);
		m_spriteShader.bindTextureNameToSlot("Palette",      3);
		m_spriteShader.bindTextureNameToSlot("Textures",     4);
		m_spriteShader.bindTextureNameToSlot("TextureTable", 5);

		return true;
	}
	
	bool loadShaderVariant(s32 index, s32 defineCount, ShaderDefine* defines)
	{
		if (!m_wallShader[index].load("Shaders/gpu_render_wall.vert", "Shaders/gpu_render_wall.frag", defineCount, defines, SHADER_VER_STD))
		{
			return false;
		}

		m_cameraPosId[index]   = m_wallShader[index].getVariableId("CameraPos");
		m_cameraViewId[index]  = m_wallShader[index].getVariableId("CameraView");
		m_cameraProjId[index]  = m_wallShader[index].getVariableId("CameraProj");
		m_cameraDirId[index]   = m_wallShader[index].getVariableId("CameraDir");
		m_lightDataId[index]   = m_wallShader[index].getVariableId("LightData");
		m_skyParallaxId[index] = m_wallShader[index].getVariableId("SkyParallax");

		m_wallShader[index].bindTextureNameToSlot("Sectors", 0);
		m_wallShader[index].bindTextureNameToSlot("Walls", 1);
		m_wallShader[index].bindTextureNameToSlot("DrawListPos", 2);
		m_wallShader[index].bindTextureNameToSlot("DrawListData", 3);
		m_wallShader[index].bindTextureNameToSlot("DrawListPlanes", 4);
		m_wallShader[index].bindTextureNameToSlot("Colormap", 5);
		m_wallShader[index].bindTextureNameToSlot("Palette", 6);
		m_wallShader[index].bindTextureNameToSlot("Textures", 7);
		m_wallShader[index].bindTextureNameToSlot("TextureTable", 8);

		return true;
	}

	void TFE_Sectors_GPU::reset()
	{
	}

	void TFE_Sectors_GPU::prepare()
	{
		if (!m_gpuInit)
		{
			m_gpuInit = true;
			s_gpuFrame = 1;

			// Load the opaque version of the shader.
			bool result = loadShaderVariant(0, 0, nullptr);
			assert(result);

			// Load the transparent version of the shader.
			ShaderDefine defines[] = { "SECTOR_TRANSPARENT_PASS", "1" };
			result = loadShaderVariant(1, TFE_ARRAYSIZE(defines), defines);
			assert(result);

			result = loadSpriteShader();
			assert(result);

			// Handles up to 65536 sector quads in the view.
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
			memset(s_cachedSectors, 0, sizeof(GPUCachedSector) * s_sectorCount);

			s_gpuSourceData.sectorSize = sizeof(Vec4f) * s_sectorCount * 2;
			s_gpuSourceData.sectors = (Vec4f*)level_alloc(s_gpuSourceData.sectorSize);
			memset(s_gpuSourceData.sectors, 0, s_gpuSourceData.sectorSize);

			s32 wallCount = 0;
			for (u32 s = 0; s < s_sectorCount; s++)
			{
				RSector* curSector = &s_sectors[s];
				GPUCachedSector* cachedSector = &s_cachedSectors[s];
				cachedSector->floorHeight   = fixed16ToFloat(curSector->floorHeight);
				cachedSector->ceilingHeight = fixed16ToFloat(curSector->ceilingHeight);
				cachedSector->wallStart = wallCount;

				s_gpuSourceData.sectors[s*2].x = cachedSector->floorHeight;
				s_gpuSourceData.sectors[s*2].y = cachedSector->ceilingHeight;
				s_gpuSourceData.sectors[s*2].z = clamp(fixed16ToFloat(curSector->ambient), 0.0f, 31.0f);
				s_gpuSourceData.sectors[s*2].w = 0.0f;

				s_gpuSourceData.sectors[s*2 + 1].x = fixed16ToFloat(curSector->floorOffset.x);
				s_gpuSourceData.sectors[s*2 + 1].y = fixed16ToFloat(curSector->floorOffset.z);
				s_gpuSourceData.sectors[s*2 + 1].z = fixed16ToFloat(curSector->ceilOffset.x);
				s_gpuSourceData.sectors[s*2 + 1].w = fixed16ToFloat(curSector->ceilOffset.z);

				wallCount += curSector->wallCount;
			}

			s_gpuSourceData.wallSize = sizeof(Vec4f) * wallCount * 3;
			s_gpuSourceData.walls = (Vec4f*)level_alloc(s_gpuSourceData.wallSize);
			memset(s_gpuSourceData.walls, 0, s_gpuSourceData.wallSize);

			for (u32 s = 0; s < s_sectorCount; s++)
			{
				RSector* curSector = &s_sectors[s];
				GPUCachedSector* cachedSector = &s_cachedSectors[s];

				Vec4f* wallData = &s_gpuSourceData.walls[cachedSector->wallStart*3];
				const RWall* srcWall = curSector->walls;
				for (s32 w = 0; w < curSector->wallCount; w++, wallData+=3, srcWall++)
				{
					wallData[0].x = fixed16ToFloat(srcWall->w0->x);
					wallData[0].y = fixed16ToFloat(srcWall->w0->z);

					Vec2f offset = { fixed16ToFloat(srcWall->w1->x) - wallData->x, fixed16ToFloat(srcWall->w1->z) - wallData->y };
					wallData[0].z = fixed16ToFloat(srcWall->length) / sqrtf(offset.x*offset.x + offset.z*offset.z);
					wallData[0].w = 0.0f;

					// Texture offsets.
					wallData[1].x = fixed16ToFloat(srcWall->midOffset.x);
					wallData[1].y = fixed16ToFloat(srcWall->midOffset.z);
					wallData[1].z = fixed16ToFloat(srcWall->signOffset.x);
					wallData[1].w = fixed16ToFloat(srcWall->signOffset.z);

					wallData[2].x = fixed16ToFloat(srcWall->botOffset.x);
					wallData[2].y = fixed16ToFloat(srcWall->botOffset.z);
					wallData[2].z = fixed16ToFloat(srcWall->topOffset.x);
					wallData[2].w = fixed16ToFloat(srcWall->topOffset.z);

					// Now handle the sign offset.
					if (srcWall->signTex)
					{
						if (srcWall->drawFlags & WDF_BOT)
						{
							wallData[1].z = wallData[2].x - wallData[1].z;
						}
						else if (srcWall->drawFlags & WDF_TOP)
						{
							wallData[1].z = wallData[2].z - wallData[1].z;
						}
						else
						{
							wallData[1].z = wallData[1].x - wallData[1].z;
						}
					}
				}
			}

			ShaderBufferDef bufferDefSectors =
			{
				4,				// 1, 2, 4 channels (R, RG, RGBA)
				sizeof(f32),	// 1, 2, 4 bytes (u8; s16,u16; s32,u32,f32)
				BUF_CHANNEL_FLOAT
			};
			m_sectors.create(s_sectorCount*2, bufferDefSectors, true, s_gpuSourceData.sectors);

			m_walls.create(wallCount*3, bufferDefSectors, true, s_gpuSourceData.walls);

			// Initialize the display list with the GPU buffers.
			s32 posIndex[]  = { 2, 2 };
			s32 dataIndex[] = { 3, 3 };
			sdisplayList_init(posIndex, dataIndex, 4);

			// Sprite Shader and buffers...
			sprdisplayList_init(0, 1);

			// Build the color map.
			if (s_colorMap && s_lightSourceRamp)
			{
				u32 colormapData[256 * 32];
				for (s32 i = 0; i < 256 * 32; i++)
				{
					u8* data = (u8*)&colormapData[i];
					data[0] = s_colorMap[i];
					if (i < 128)
					{
						data[1] = s_lightSourceRamp[i];
					}
					else
					{
						data[1] = 0;
					}
					data[2] = data[3] = 0;
				}
				s_colormapTex = TFE_RenderBackend::createTexture(256, 32, colormapData);
			}

			// Load textures into GPU memory.
			if (!s_levelTextures) { s_levelTextures = texturepacker_init("LevelTextures", 4096, 4096); }
			if (s_levelTextures)  { texturepacker_packLevelTextures(s_levelTextures); }

			if (!s_objectTextures) { s_objectTextures = texturepacker_init("Objects", 4096, 4096); }
			if (s_objectTextures)  { texturepacker_packObjectTextures(s_objectTextures); }

			model_init();
			model_loadLevelModels();
		}
		else
		{
			s_gpuFrame++;
		}

		renderDebug_enable(s_enableDebug);
	}
	
	void updateCachedWalls(RSector* srcSector, u32 flags, u32& uploadFlags)
	{
		GPUCachedSector* cached = &s_cachedSectors[srcSector->index];
		if (flags & (SDF_HEIGHTS | SDF_AMBIENT))
		{
			uploadFlags |= UPLOAD_WALLS;
		}
		if (flags & (SDF_VERTICES | SDF_WALL_CHANGE | SDF_WALL_OFFSETS | SDF_WALL_SHAPE))
		{
			uploadFlags |= UPLOAD_WALLS;
			Vec4f* wallData = &s_gpuSourceData.walls[cached->wallStart*3];
			const RWall* srcWall = srcSector->walls;
			for (s32 w = 0; w < srcSector->wallCount; w++, wallData+=3, srcWall++)
			{
				wallData[0].x = fixed16ToFloat(srcWall->w0->x);
				wallData[0].y = fixed16ToFloat(srcWall->w0->z);

				Vec2f offset = { fixed16ToFloat(srcWall->w1->x) - wallData->x, fixed16ToFloat(srcWall->w1->z) - wallData->y };
				wallData->z = fixed16ToFloat(srcWall->length) / sqrtf(offset.x*offset.x + offset.z*offset.z);

				// Texture offsets.
				wallData[1].x = fixed16ToFloat(srcWall->midOffset.x);
				wallData[1].y = fixed16ToFloat(srcWall->midOffset.z);
				wallData[1].z = fixed16ToFloat(srcWall->signOffset.x);
				wallData[1].w = fixed16ToFloat(srcWall->signOffset.z);

				wallData[2].x = fixed16ToFloat(srcWall->botOffset.x);
				wallData[2].y = fixed16ToFloat(srcWall->botOffset.z);
				wallData[2].z = fixed16ToFloat(srcWall->topOffset.x);
				wallData[2].w = fixed16ToFloat(srcWall->topOffset.z);

				// Now handle the sign offset.
				if (srcWall->signTex)
				{
					if (srcWall->drawFlags & WDF_BOT)
					{
						wallData[1].z = wallData[2].x - wallData[1].z;
					}
					else if (srcWall->drawFlags & WDF_TOP)
					{
						wallData[1].z = wallData[2].z - wallData[1].z;
					}
					else
					{
						wallData[1].z = wallData[1].x - wallData[1].z;
					}
				}
			}
		}
	}

	void updateCachedSector(RSector* srcSector, u32& uploadFlags)
	{
		u32 flags = srcSector->dirtyFlags;
		if (!flags) { return; }  // Nothing to do.

		GPUCachedSector* cached = &s_cachedSectors[srcSector->index];
		if (flags & (SDF_HEIGHTS | SDF_FLAT_OFFSETS | SDF_AMBIENT))
		{
			cached->floorHeight   = fixed16ToFloat(srcSector->floorHeight);
			cached->ceilingHeight = fixed16ToFloat(srcSector->ceilingHeight);
			s_gpuSourceData.sectors[srcSector->index*2].x = cached->floorHeight;
			s_gpuSourceData.sectors[srcSector->index*2].y = cached->ceilingHeight;
			s_gpuSourceData.sectors[srcSector->index*2].z = clamp(fixed16ToFloat(srcSector->ambient), 0.0f, 31.0f);

			s_gpuSourceData.sectors[srcSector->index*2+1].x = fixed16ToFloat(srcSector->floorOffset.x);
			s_gpuSourceData.sectors[srcSector->index*2+1].y = fixed16ToFloat(srcSector->floorOffset.z);
			s_gpuSourceData.sectors[srcSector->index*2+1].z = fixed16ToFloat(srcSector->ceilOffset.x);
			s_gpuSourceData.sectors[srcSector->index*2+1].w = fixed16ToFloat(srcSector->ceilOffset.z);

			uploadFlags |= UPLOAD_SECTORS;
		}
		updateCachedWalls(srcSector, flags, uploadFlags);
		srcSector->dirtyFlags = SDF_NONE;
	}

	s32 traversal_addPortals(RSector* curSector)
	{
		// Add portals to the list to process for the sector.
		SegmentClipped* segment = sbuffer_get();
		s32 count = 0;
		while (segment)
		{
			if (!segment->seg->portal)
			{
				segment = segment->next;
				continue;
			}
			
			SegmentClipped* portal = segment;
			RWall* wall = &curSector->walls[portal->seg->id];
			RSector* next = wall->nextSector;
			assert(next);

			Vec3f p0 = { portal->v0.x, portal->seg->portalY0, portal->v0.z };
			Vec3f p1 = { portal->v1.x, portal->seg->portalY1, portal->v1.z };

			// Clip the portal by the current frustum, and return if it is culled.
			Polygon clippedPortal;
			if (frustum_clipQuadToFrustum(p0, p1, &clippedPortal))
			{
				Portal* portalOut = &s_portalList[s_portalListCount];
				s_portalListCount++;

				frustum_buildFromPolygon(&clippedPortal, &portalOut->frustum);
				portalOut->v0 = portal->v0;
				portalOut->v1 = portal->v1;
				portalOut->y0 = p0.y;
				portalOut->y1 = p1.y;
				portalOut->next = next;
				portalOut->wall = &curSector->walls[portal->seg->id];
				assert(portalOut->next);

				count++;
			}
			segment = segment->next;
		}
		return count;
	}

	void buildSegmentBuffer(bool initSector, RSector* curSector, u32 segCount, Segment* wallSegments)
	{
		// Next insert solid segments into the segment buffer one at a time.
		sbuffer_clear();
		for (u32 i = 0; i < segCount; i++)
		{
			sbuffer_insertSegment(&wallSegments[i]);
		}
		sbuffer_mergeSegments();

		// Build the display list.
		SegmentClipped* segment = sbuffer_get();
		while (segment && s_wallSegGenerated < s_maxWallSeg)
		{
			// DEBUG
			debug_addQuad(segment->v0, segment->v1, segment->seg->y0, segment->seg->y1,
				          segment->seg->portalY0, segment->seg->portalY1, segment->seg->portal);

			sdisplayList_addSegment(curSector, &s_cachedSectors[curSector->index], segment);
			s_wallSegGenerated++;
			segment = segment->next;
		}
		if (initSector) { sdisplayList_addCaps(curSector); }
	}

	bool createNewSegment(Segment* seg, s32 id, bool isPortal, Vec2f v0, Vec2f v1, Vec2f heights, Vec2f portalHeights, Vec3f normal)
	{
		seg->id = id;
		seg->portal = isPortal;
		seg->v0 = v0;
		seg->v1 = v1;
		seg->x0 = sbuffer_projectToUnitSquare(seg->v0);
		seg->x1 = sbuffer_projectToUnitSquare(seg->v1);

		// This means both vertices map to the same point on the unit square, in other words, the edge isn't actually visible.
		if (fabsf(seg->x0 - seg->x1) < FLT_EPSILON)
		{
			return false;
		}

		// Project the edge.
		sbuffer_handleEdgeWrapping(seg->x0, seg->x1);
		// Check again for zero-length walls in case the fix-ups above caused it (for example, x0 = 0.0, x1 = 4.0).
		if (seg->x0 >= seg->x1 || seg->x1 - seg->x0 < FLT_EPSILON)
		{
			return false;
		}
		assert(seg->x1 - seg->x0 > 0.0f && seg->x1 - seg->x0 <= 2.0f);

		seg->normal = normal;
		seg->portal = isPortal;
		seg->y0 = heights.x;
		seg->y1 = heights.z;
		seg->portalY0 = isPortal ? portalHeights.x : heights.x;
		seg->portalY1 = isPortal ? portalHeights.z : heights.z;
		return true;
	}
		
	void splitSegment(bool initSector, Segment* segList, u32& segCount, Segment* seg, Vec2f* range, Vec2f* points, s32 rangeCount)
	{
		const f32 sx1 = seg->x1;
		const Vec2f sv1 = seg->v1;

		// Split the segment at the modulus border.
		seg->v1 = sbuffer_clip(seg->v0, seg->v1, { 1.0f + s_cameraPos.x, -1.0f + s_cameraPos.z });
		seg->x1 = 4.0f;
		Vec2f newV1 = seg->v1;

		if (!initSector && !sbuffer_splitByRange(seg, range, points, rangeCount))
		{
			segCount--;
		}

		Segment* seg2;
		seg2 = &segList[segCount];
		segCount++;

		*seg2 = *seg;
		seg2->x0 = 0.0f;
		seg2->x1 = sx1 - 4.0f;
		seg2->v0 = newV1;
		seg2->v1 = sv1;

		if (!initSector && !sbuffer_splitByRange(seg2, range, points, rangeCount))
		{
			segCount--;
		}
	}

	// Build world-space wall segments.
	void buildSectorWallSegments(RSector* curSector, u32& uploadFlags, bool initSector, Vec2f p0, Vec2f p1)
	{
		static Segment wallSegments[2048];

		u32 segCount = 0;
		GPUCachedSector* cached = &s_cachedSectors[curSector->index];
		cached->builtFrame = s_gpuFrame;

		// Portal range, all segments must be clipped to this.
		// The actual clip vertices are p0 and p1.
		Vec2f range[2] = { 0 };
		Vec2f points[2] = { p0, p1 };
		s32 rangeCount = 0;
		if (!initSector)
		{
			range[0].x = sbuffer_projectToUnitSquare(p0);
			range[0].z = sbuffer_projectToUnitSquare(p1);
			sbuffer_handleEdgeWrapping(range[0].x, range[0].z);
			rangeCount = 1;

			if (range[0].z > 4.0f)
			{
				range[1].x = 0.0f;
				range[1].z = range[0].z - 4.0f;
				range[0].z = 4.0f;
				rangeCount = 2;
			}
		}
			
		// Build segments, skipping any backfacing walls or any that are outside of the camera frustum.
		// Identify walls as solid or portals.
		for (s32 w = 0; w < curSector->wallCount; w++)
		{
			RWall* wall = &curSector->walls[w];
			RSector* next = wall->nextSector;

			// Wall already processed.
			if (wall->drawFrame == s_gpuFrame)
			{
				continue;
			}
			
			// Calculate the vertices.
			const f32 x0 = fixed16ToFloat(wall->w0->x);
			const f32 x1 = fixed16ToFloat(wall->w1->x);
			const f32 z0 = fixed16ToFloat(wall->w0->z);
			const f32 z1 = fixed16ToFloat(wall->w1->z);
			f32 y0 = cached->ceilingHeight;
			f32 y1 = cached->floorHeight;
			f32 portalY0 = y0, portalY1 = y1;

			// Check if the wall is backfacing.
			const Vec3f wallNormal = { -(z1 - z0), 0.0f, x1 - x0 };
			const Vec3f cameraVec = { x0 - s_cameraPos.x, (y0 + y1)*0.5f - s_cameraPos.y, z0 - s_cameraPos.z };
			if (wallNormal.x*cameraVec.x + wallNormal.y*cameraVec.y + wallNormal.z*cameraVec.z < 0.0f)
			{
				continue;
			}

			// Check if the wall is outside of the view frustum.
			Vec3f qv0 = { x0, y0 - 200.0f, z0 }, qv1 = { x1, y1 + 200.0f, z1 };
			if (!frustum_quadInside(qv0, qv1))
			{
				continue;
			}

			// Is the wall a portal or is it effectively solid?
			bool isPortal = false;
			if (next)
			{
				// Update any potential adjoins even if they are not traversed to make sure the
				// heights and walls settings are handled correctly.
				updateCachedSector(next, uploadFlags);

				fixed16_16 openTop, openBot;
				// Sky handling
				// TODO: This isn't *quite* right, the top and bottom need to extend to the top/bot of the view frustum.
				if (((curSector->flags1 & SEC_FLAGS1_EXTERIOR) || (next->flags1 & SEC_FLAGS1_EXT_ADJ)) && (next->flags1 & SEC_FLAGS1_EXTERIOR))
				{
					openTop = min(curSector->ceilingHeight, next->ceilingHeight);
					y0 = fixed16ToFloat(openTop);
				}
				else
				{
					openTop = min(curSector->floorHeight, max(curSector->ceilingHeight, next->ceilingHeight));
				}
				if (((curSector->flags1 & SEC_FLAGS1_PIT) || (next->flags1 & SEC_FLAGS1_EXT_FLOOR_ADJ)) && (next->flags1 & SEC_FLAGS1_PIT))
				{
					openBot = max(curSector->floorHeight, next->floorHeight);
					y1 = fixed16ToFloat(openBot);
				}
				else
				{
					openBot = max(curSector->ceilingHeight, min(curSector->floorHeight, next->floorHeight));
				}
				// TODO: Handle sectors with the "no walls" flag.

				fixed16_16 openSize = openBot - openTop;
				portalY0 = fixed16ToFloat(openTop);
				portalY1 = fixed16ToFloat(openBot);

				if (openSize > 0)
				{
					// Is the portal inside the view frustum?
					const Vec3f portalPos = { (x0 + x1) * 0.5f, (portalY0 + portalY1) * 0.5f, (z0 + z1) * 0.5f };
					const Vec3f maxPos = { max(x0, x1), max(portalY0, portalY1), max(z0, z1) };
					const Vec3f diag = { maxPos.x - portalPos.x, maxPos.y - portalPos.y, maxPos.z - portalPos.z };
					const f32 portalRadius = sqrtf(diag.x*diag.x + diag.y*diag.y + diag.z*diag.z);
					// Cull the portal but potentially keep the edge.
					Vec3f qv0 = { x0, portalY0, z0 }, qv1 = { x1, portalY1, z1 };
					isPortal = frustum_quadInside(qv0, qv1);
				}
			}

			// Add a new segment.
			Segment* seg = &wallSegments[segCount];
			Vec2f v0 = { x0, z0 }, v1 = { x1, z1 }, heights = { y0, y1 }, portalHeights = { portalY0, portalY1 };
			if (!createNewSegment(seg, w, isPortal, v0, v1, heights, portalHeights, wallNormal))
			{
				continue;
			}
			segCount++;

			// Split segments that cross the modulo boundary.
			if (seg->x1 > 4.0f)
			{
				splitSegment(initSector, wallSegments, segCount, seg, range, points, rangeCount);
			}
			else if (!initSector && !sbuffer_splitByRange(seg, range, points, rangeCount))
			{
				// Out of the range, so cancel the segment.
				segCount--;
			}
		}

		buildSegmentBuffer(initSector, curSector, segCount, wallSegments);
	}

	void addSectorObjects(RSector* curSector)
	{
		SecObject** objIter = curSector->objectList;
		f32 ambient = fixed16ToFloat(curSector->ambient);
		Vec2f floorOffset = { fixed16ToFloat(curSector->floorOffset.x), fixed16ToFloat(curSector->floorOffset.z) };
		for (s32 i = 0; i < curSector->objectCount; objIter++)
		{
			// TODO: Add an object draw frame.

			SecObject* obj = *objIter;
			if (!obj) { continue; }
			i++;

			if (obj->flags & OBJ_FLAG_NEEDS_TRANSFORM)
			{
				const s32 type = obj->type;
				Vec3f posWS = { fixed16ToFloat(obj->posWS.x), fixed16ToFloat(obj->posWS.y), fixed16ToFloat(obj->posWS.z) };
				if (type == OBJ_TYPE_SPRITE || type == OBJ_TYPE_FRAME)
				{
					if (type == OBJ_TYPE_SPRITE)
					{
						f32 dx = s_cameraPos.x - posWS.x;
						f32 dz = s_cameraPos.z - posWS.z;
						angle14_16 angle = vec2ToAngle(dx, dz);

						// Angles range from [0, 16384), divide by 512 to get 32 even buckets.
						s32 angleDiff = (angle - obj->yaw) >> 9;
						angleDiff &= 31;	// up to 32 views

						// Get the animation based on the object state.
						Wax* wax = obj->wax;
						WaxAnim* anim = WAX_AnimPtr(wax, obj->anim & 31);
						if (anim)
						{
							// Then get the Sequence from the angle difference.
							WaxView* view = WAX_ViewPtr(wax, anim, 31 - angleDiff);
							// And finall the frame from the current sequence.
							WaxFrame* frame = WAX_FramePtr(wax, view, obj->frame & 31);
							sprdisplayList_addFrame(wax, frame, posWS, curSector, (obj->flags & OBJ_FLAG_FULLBRIGHT) != 0);
						}
					}
					else if (type == OBJ_TYPE_FRAME)
					{
						sprdisplayList_addFrame(obj->fme, obj->fme, posWS, curSector, (obj->flags & OBJ_FLAG_FULLBRIGHT) != 0);
					}
				}
				else if (type == OBJ_TYPE_3D)
				{
					model_add(obj->model, posWS, obj->transform, ambient, floorOffset);
				}
			}
		}
	}

	void traverseSector(RSector* curSector, s32& level, u32& uploadFlags, Vec2f p0, Vec2f p1)
	{
		//if (level >= 255)
		if (level >= 64)
		{
			return;
		}

		// Build the world-space wall segments.
		buildSectorWallSegments(curSector, uploadFlags, level == 0, p0, p1);

		// Determine which objects are visible and add them.
		addSectorObjects(curSector);

		// Traverse through visible portals.
		const s32 portalStart = s_portalListCount;
		const s32 portalCount = traversal_addPortals(curSector);
		Portal* portal = &s_portalList[portalStart];
		for (s32 p = 0; p < portalCount && s_portalsTraversed < s_maxPortals; p++, portal++)
		{
			frustum_push(portal->frustum);
			level++;
			s_portalsTraversed++;

			// Add a portal to the display list.
			Vec3f corner0 = { portal->v0.x, portal->y0, portal->v0.z };
			Vec3f corner1 = { portal->v1.x, portal->y1, portal->v1.z };
			sdisplayList_addPortal(corner0, corner1);

			portal->wall->drawFrame = s_gpuFrame;
			traverseSector(portal->next, level, uploadFlags, portal->v0, portal->v1);
			portal->wall->drawFrame = 0;

			frustum_pop();
			level--;
		}
	}
						
	bool traverseScene(RSector* sector)
	{
		debug_update();

		// First build the camera frustum and push it onto the stack.
		frustum_buildFromCamera();

		s32 level = 0;
		u32 uploadFlags = UPLOAD_NONE;
		s_portalsTraversed = 0;
		s_portalListCount = 0;
		s_wallSegGenerated = 0;
		Vec2f startView[] = { {0,0}, {0,0} };

		sdisplayList_clear();
		sprdisplayList_clear();
		model_drawListClear();

		updateCachedSector(sector, uploadFlags);
		traverseSector(sector, level, uploadFlags, startView[0], startView[1]);
		frustum_pop();

		sdisplayList_finish();
		sprdisplayList_finish();
		model_drawListFinish();

		if (uploadFlags & UPLOAD_SECTORS)
		{
			m_sectors.update(s_gpuSourceData.sectors, s_gpuSourceData.sectorSize);
		}
		if (uploadFlags & UPLOAD_WALLS)
		{
			m_walls.update(s_gpuSourceData.walls, s_gpuSourceData.wallSize);
		}

		return sdisplayList_getSize() > 0;
	}

	void drawPass(SectorPass pass)
	{
		if (!sdisplayList_getSize(pass)) { return; }

		m_wallShader[pass].bind();
		m_indexBuffer.bind();
		m_sectors.bind(0);
		m_walls.bind(1);
		s_colormapTex->bind(5);

		const TextureGpu* palette = TFE_RenderBackend::getPaletteTexture();
		palette->bind(6);

		const TextureGpu* textures = s_levelTextures->texture;
		textures->bind(7);

		ShaderBuffer* textureTable = &s_levelTextures->textureTableGPU;
		textureTable->bind(8);

		// Camera and lighting.
		Vec4f lightData = { f32(s_worldAmbient), s_cameraLightSource ? 1.0f : 0.0f, 0.0f, 0.0f };
		m_wallShader[pass].setVariable(m_cameraPosId[pass],  SVT_VEC3, s_cameraPos.m);
		m_wallShader[pass].setVariable(m_cameraViewId[pass], SVT_MAT3x3, s_cameraMtx.data);
		m_wallShader[pass].setVariable(m_cameraProjId[pass], SVT_MAT4x4, s_cameraProj.data);
		m_wallShader[pass].setVariable(m_cameraDirId[pass],  SVT_VEC3, s_cameraDir.m);
		m_wallShader[pass].setVariable(m_lightDataId[pass],  SVT_VEC4, lightData.m);

		// Calculte the sky parallax.
		fixed16_16 p0, p1;
		TFE_Jedi::getSkyParallax(&p0, &p1);
		f32 parallax[2] =
		{
			fixed16ToFloat(p0) * 0.25f,	// The values are scaled by 4 to convert from angle to fixed in the original code.
			fixed16ToFloat(p1) * 0.25f 	// The values are scaled by 4 to convert from angle to fixed in the original code.
		};
		m_wallShader[pass].setVariable(m_skyParallaxId[pass], SVT_VEC2, parallax);

		// Draw the sector display list.
		sdisplayList_draw(pass);

		m_wallShader[pass].unbind();
	}

	void drawSprites()
	{
		if (!sprdisplayList_getSize()) { return; }

		m_spriteShader.bind();
		m_indexBuffer.bind();
		s_colormapTex->bind(2);

		const TextureGpu* palette = TFE_RenderBackend::getPaletteTexture();
		palette->bind(3);

		const TextureGpu* textures = s_objectTextures->texture;
		textures->bind(4);

		ShaderBuffer* textureTable = &s_objectTextures->textureTableGPU;
		textureTable->bind(5);

		// Camera and lighting.
		Vec4f lightData = { f32(s_worldAmbient), s_cameraLightSource ? 1.0f : 0.0f, 0.0f, 0.0f };
		m_spriteShader.setVariable(m_cameraRightId, SVT_VEC3, s_cameraRight.m);
		m_spriteShader.setVariable(m_cameraPosId[SPRITE_PASS],  SVT_VEC3, s_cameraPos.m);
		m_spriteShader.setVariable(m_cameraViewId[SPRITE_PASS], SVT_MAT3x3, s_cameraMtx.data);
		m_spriteShader.setVariable(m_cameraProjId[SPRITE_PASS], SVT_MAT4x4, s_cameraProj.data);
		m_spriteShader.setVariable(m_cameraDirId[SPRITE_PASS],  SVT_VEC3, s_cameraDir.m);
		m_spriteShader.setVariable(m_lightDataId[SPRITE_PASS],  SVT_VEC4, lightData.m);

		// Draw the sector display list.
		sprdisplayList_draw();

		m_spriteShader.unbind();
	}

	void draw3d()
	{
		const TextureGpu* palette = TFE_RenderBackend::getPaletteTexture();
		palette->bind(0);

		s_colormapTex->bind(1);

		const TextureGpu* textures = s_objectTextures->texture;
		textures->bind(2);

		ShaderBuffer* textureTable = &s_objectTextures->textureTableGPU;
		textureTable->bind(3);

		model_drawList();
	}

	void TFE_Sectors_GPU::draw(RSector* sector)
	{
		// Build the draw list.
		if (!traverseScene(sector))
		{
			return;
		}

		// State
		TFE_RenderState::setStateEnable(false, STATE_BLEND);
		TFE_RenderState::setStateEnable(true, STATE_DEPTH_WRITE | STATE_DEPTH_TEST | STATE_CULLING);

		for (s32 i = 0; i < SECTOR_PASS_COUNT; i++)
		{
			drawPass(SectorPass(i));
		}

		// Draw 3D Objects.
		draw3d();

		// Draw Sprites.
		drawSprites();
				
		// Cleanup
		m_indexBuffer.unbind();
		m_sectors.unbind(0);
		m_walls.unbind(1);
		TextureGpu::clear(5);
		TextureGpu::clear(6);

		// TFE_RenderState::setStateEnable(false, STATE_WIREFRAME);
		
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