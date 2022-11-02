#include <cstring>

#include <TFE_System/profiler.h>
#include <TFE_System/math.h>
#include <TFE_Asset/modelAsset_jedi.h>
#include <TFE_Game/igame.h>
#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/Level/levelData.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_Jedi/Level/levelTextures.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Settings/settings.h>

#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderBackend/indexBuffer.h>
#include <TFE_RenderBackend/shader.h>
#include <TFE_RenderBackend/shaderBuffer.h>
#include <TFE_RenderShared/texturePacker.h>

#include <TFE_FrontEndUI/console.h>

#include "rclassicGPU.h"
#include "rsectorGPU.h"
#include "modelGPU.h"
#include "renderDebug.h"
#include "debug.h"
#include "frustum.h"
#include "sbuffer.h"
#include "objectPortalPlanes.h"
#include "sectorDisplayList.h"
#include "spriteDisplayList.h"
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
		f32   y0, y1;
		RSector* next;
		Frustum  frustum;
		RWall*   wall;
	};
	struct ShaderInputs
	{
		s32 cameraPosId;
		s32 cameraViewId;
		s32 cameraProjId;
		s32 cameraDirId;
		s32 lightDataId;
		s32 globalLightingId;
	};

	static GPUSourceData s_gpuSourceData = { 0 };

	static TextureGpu*  s_colormapTex = nullptr;
	static ShaderBuffer s_sectorGpuBuffer;
	static ShaderBuffer s_wallGpuBuffer;
	static Shader s_spriteShader;
	static Shader s_wallShader[SECTOR_PASS_COUNT];
	static ShaderInputs s_shaderInputs[SECTOR_PASS_COUNT + 1];
	static s32 s_skyParallaxId[SECTOR_PASS_COUNT];
	static s32 s_skyParamId[SECTOR_PASS_COUNT];
	static s32 s_globalLightingId[SECTOR_PASS_COUNT];
	static s32 s_cameraRightId;

	static IndexBuffer s_indexBuffer;
	static GPUCachedSector* s_cachedSectors;
	static bool s_enableDebug = false;

	static s32 s_gpuFrame;
	static s32 s_portalListCount = 0;
	static s32 s_rangeCount;

	static Portal* s_portalList = nullptr;
	static Vec2f  s_range[2];
	static Vec2f  s_rangeSrc[2];

	static bool s_showWireframe = false;
	static SkyMode s_skyMode = SKYMODE_CYLINDER;
	static RSector* s_clipSector;
	static Vec3f s_clipObjPos;

	extern Mat3  s_cameraMtx;
	extern Mat4  s_cameraProj;
	extern Vec3f s_cameraPos;
	extern Vec3f s_cameraDir;
	extern Vec3f s_cameraRight;
	extern s32   s_displayCurrentPortalId;
	extern ShaderBuffer s_displayListPlanesGPU;
		
	bool loadSpriteShader()
	{
		if (!s_spriteShader.load("Shaders/gpu_render_sprite.vert", "Shaders/gpu_render_sprite.frag", 0, nullptr, SHADER_VER_STD))
		{
			return false;
		}
		s_spriteShader.enableClipPlanes(MAX_PORTAL_PLANES);

		s_shaderInputs[SPRITE_PASS].cameraPosId  = s_spriteShader.getVariableId("CameraPos");
		s_shaderInputs[SPRITE_PASS].cameraViewId = s_spriteShader.getVariableId("CameraView");
		s_shaderInputs[SPRITE_PASS].cameraProjId = s_spriteShader.getVariableId("CameraProj");
		s_shaderInputs[SPRITE_PASS].cameraDirId  = s_spriteShader.getVariableId("CameraDir");
		s_shaderInputs[SPRITE_PASS].lightDataId  = s_spriteShader.getVariableId("LightData");
		s_shaderInputs[SPRITE_PASS].globalLightingId = s_spriteShader.getVariableId("GlobalLightData");
		s_cameraRightId = s_spriteShader.getVariableId("CameraRight");

		s_spriteShader.bindTextureNameToSlot("DrawListPosXZ_Texture", 0);
		s_spriteShader.bindTextureNameToSlot("DrawListPosYU_Texture", 1);
		s_spriteShader.bindTextureNameToSlot("DrawListTexId_Texture", 2);

		s_spriteShader.bindTextureNameToSlot("Colormap",       3);
		s_spriteShader.bindTextureNameToSlot("Palette",        4);
		s_spriteShader.bindTextureNameToSlot("Textures",       5);
		s_spriteShader.bindTextureNameToSlot("TextureTable",   6);
		s_spriteShader.bindTextureNameToSlot("DrawListPlanes", 7);

		return true;
	}
	
	static bool loadShaderVariant(s32 index, s32 defineCount, ShaderDefine* defines)
	{
		// Destroy the existing shader so we aren't duplicating shaders if new settings are used.
		s_wallShader[index].destroy();

		if (!s_wallShader[index].load("Shaders/gpu_render_wall.vert", "Shaders/gpu_render_wall.frag", defineCount, defines, SHADER_VER_STD))
		{
			return false;
		}
		s_wallShader[index].enableClipPlanes(MAX_PORTAL_PLANES);

		s_shaderInputs[index].cameraPosId  = s_wallShader[index].getVariableId("CameraPos");
		s_shaderInputs[index].cameraViewId = s_wallShader[index].getVariableId("CameraView");
		s_shaderInputs[index].cameraProjId = s_wallShader[index].getVariableId("CameraProj");
		s_shaderInputs[index].cameraDirId  = s_wallShader[index].getVariableId("CameraDir");
		s_shaderInputs[index].lightDataId  = s_wallShader[index].getVariableId("LightData");
		s_shaderInputs[index].globalLightingId = s_wallShader[index].getVariableId("GlobalLightData");
		s_skyParallaxId[index] = s_wallShader[index].getVariableId("SkyParallax");
		s_skyParamId[index]    = s_wallShader[index].getVariableId("SkyParam");
		
		s_wallShader[index].bindTextureNameToSlot("Sectors",        0);
		s_wallShader[index].bindTextureNameToSlot("Walls",          1);
		s_wallShader[index].bindTextureNameToSlot("DrawListPos",    2);
		s_wallShader[index].bindTextureNameToSlot("DrawListData",   3);
		s_wallShader[index].bindTextureNameToSlot("DrawListPlanes", 4);
		s_wallShader[index].bindTextureNameToSlot("Colormap",       5);
		s_wallShader[index].bindTextureNameToSlot("Palette",        6);
		s_wallShader[index].bindTextureNameToSlot("Textures",       7);
		s_wallShader[index].bindTextureNameToSlot("TextureTable",   8);

		return true;
	}

	void TFE_Sectors_GPU::destroy()
	{
		free(s_portalList);
		s_spriteShader.destroy();
		s_wallShader[0].destroy();
		s_wallShader[1].destroy();
		s_indexBuffer.destroy();
		s_sectorGpuBuffer.destroy();
		s_wallGpuBuffer.destroy();
		TFE_RenderBackend::freeTexture(s_colormapTex);
		
		s_portalList = nullptr;
		s_cachedSectors = nullptr;
		s_colormapTex = nullptr;

		sdisplayList_destroy();
		sprdisplayList_destroy();
		objectPortalPlanes_destroy();
		model_destroy();
	}

	void TFE_Sectors_GPU::reset()
	{
		m_levelInit = false;
	}
	
	void TFE_Sectors_GPU::prepare()
	{
		if (!m_gpuInit)
		{
			CVAR_BOOL(s_showWireframe, "d_enableWireframe", CVFLAG_DO_NOT_SERIALIZE, "Enable wireframe rendering.");
			TFE_COUNTER(s_wallSegGenerated, "Wall Segments");
			
			m_gpuInit = true;
			s_gpuFrame = 1;
			if (!s_portalList)
			{
				s_portalList = (Portal*)malloc(sizeof(Portal) * MAX_DISP_ITEMS);
			}

			// Read the current graphics settings before compiling shaders.
			TFE_Settings_Graphics* graphics = TFE_Settings::getGraphicsSettings();
			s_skyMode = SkyMode(graphics->skyMode);

			bool result = updateBasePassShader();
			assert(result);

			// Load the transparent version of the shader.
			ShaderDefine defines[] = { "SECTOR_TRANSPARENT_PASS", "1" };
			result = loadShaderVariant(1, TFE_ARRAYSIZE(defines), defines);
			assert(result);

			result = loadSpriteShader();
			assert(result);

			// Handles up to 65536 sector quads in the view.
			u16* indices = (u16*)level_alloc(sizeof(u16) * 6 * MAX_DISP_ITEMS);
			u16* index = indices;
			for (s32 q = 0; q < MAX_DISP_ITEMS; q++, index += 6)
			{
				const s32 i = q * 4;
				index[0] = i + 0;
				index[1] = i + 1;
				index[2] = i + 2;

				index[3] = i + 1;
				index[4] = i + 3;
				index[5] = i + 2;
			}
			s_indexBuffer.create(6 * MAX_DISP_ITEMS, sizeof(u16), false, (void*)indices);
			level_free(indices);

			// Initialize the display list with the GPU buffers.
			s32 posIndex[] = { 2, 2 };
			s32 dataIndex[] = { 3, 3 };
			sdisplayList_init(posIndex, dataIndex, 4);

			// Sprite Shader and buffers...
			sprdisplayList_init(0);
			objectPortalPlanes_init();
			model_init();
		}
		if (!m_levelInit)
		{
			m_levelInit = true;

			// Let's just cache the current data.
			s_cachedSectors = (GPUCachedSector*)level_alloc(sizeof(GPUCachedSector) * s_levelState.sectorCount);
			memset(s_cachedSectors, 0, sizeof(GPUCachedSector) * s_levelState.sectorCount);

			s_gpuSourceData.sectorSize = sizeof(Vec4f) * s_levelState.sectorCount * 2;
			s_gpuSourceData.sectors = (Vec4f*)level_alloc(s_gpuSourceData.sectorSize);
			memset(s_gpuSourceData.sectors, 0, s_gpuSourceData.sectorSize);

			s32 wallCount = 0;
			for (u32 s = 0; s < s_levelState.sectorCount; s++)
			{
				RSector* curSector = &s_levelState.sectors[s];
				GPUCachedSector* cachedSector = &s_cachedSectors[s];
				cachedSector->floorHeight = fixed16ToFloat(curSector->floorHeight);
				cachedSector->ceilingHeight = fixed16ToFloat(curSector->ceilingHeight);
				cachedSector->wallStart = wallCount;

				s_gpuSourceData.sectors[s * 2].x = cachedSector->floorHeight;
				s_gpuSourceData.sectors[s * 2].y = cachedSector->ceilingHeight;
				s_gpuSourceData.sectors[s * 2].z = clamp(fixed16ToFloat(curSector->ambient), 0.0f, 31.0f);
				s_gpuSourceData.sectors[s * 2].w = 0.0f;

				s_gpuSourceData.sectors[s * 2 + 1].x = fixed16ToFloat(curSector->floorOffset.x);
				s_gpuSourceData.sectors[s * 2 + 1].y = fixed16ToFloat(curSector->floorOffset.z);
				s_gpuSourceData.sectors[s * 2 + 1].z = fixed16ToFloat(curSector->ceilOffset.x);
				s_gpuSourceData.sectors[s * 2 + 1].w = fixed16ToFloat(curSector->ceilOffset.z);

				wallCount += curSector->wallCount;
			}

			s_gpuSourceData.wallSize = sizeof(Vec4f) * wallCount * 3;
			s_gpuSourceData.walls = (Vec4f*)level_alloc(s_gpuSourceData.wallSize);
			memset(s_gpuSourceData.walls, 0, s_gpuSourceData.wallSize);

			for (u32 s = 0; s < s_levelState.sectorCount; s++)
			{
				RSector* curSector = &s_levelState.sectors[s];
				GPUCachedSector* cachedSector = &s_cachedSectors[s];

				Vec4f* wallData = &s_gpuSourceData.walls[cachedSector->wallStart * 3];
				const RWall* srcWall = curSector->walls;
				for (s32 w = 0; w < curSector->wallCount; w++, wallData += 3, srcWall++)
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

			if (m_prevSectorCount < (s32)s_levelState.sectorCount || m_prevWallCount < wallCount || !m_gpuBuffersAllocated)
			{
				// Recreate the GPU sector buffers so they are large enough to hold the new data.
				if (m_gpuBuffersAllocated)
				{
					s_sectorGpuBuffer.destroy();
					s_wallGpuBuffer.destroy();
				}

				const ShaderBufferDef bufferDefSectors = { 4, sizeof(f32), BUF_CHANNEL_FLOAT };
				s_sectorGpuBuffer.create(s_levelState.sectorCount * 2, bufferDefSectors, true, s_gpuSourceData.sectors);
				s_wallGpuBuffer.create(wallCount * 3, bufferDefSectors, true, s_gpuSourceData.walls);

				m_gpuBuffersAllocated = true;
				m_prevSectorCount = s_levelState.sectorCount;
				m_prevWallCount = wallCount;
			}
			else
			{
				// Update the GPU sector buffers since they are already large enough.
				s_sectorGpuBuffer.update(s_gpuSourceData.sectors, s_gpuSourceData.sectorSize);
				s_wallGpuBuffer.update(s_gpuSourceData.walls, s_gpuSourceData.wallSize);
			}
			m_prevSectorCount = s_levelState.sectorCount;
			m_prevWallCount = wallCount;
			
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
				TFE_RenderBackend::freeTexture(s_colormapTex);
				s_colormapTex = TFE_RenderBackend::createTexture(256, 32, colormapData);
			}

			// Load textures into GPU memory.
			if (texturepacker_getGlobal())
			{
				texturepacker_discardUnreservedPages(texturepacker_getGlobal());

				texturepacker_pack(level_getLevelTextures, POOL_LEVEL);
				texturepacker_pack(level_getObjectTextures, POOL_LEVEL);
				texturepacker_commit();
			}

			model_loadGpuModels();
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

	void buildSegmentBuffer(bool initSector, RSector* curSector, u32 segCount, Segment* wallSegments, bool forceTreatAsSolid)
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

			sdisplayList_addSegment(curSector, &s_cachedSectors[curSector->index], segment, forceTreatAsSolid);
			s_wallSegGenerated++;
			segment = segment->next;
		}
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
		else
		{
			assert(seg->x0 >= 0.0f && seg->x1 <= 4.0f);
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
		else
		{
			assert(seg2->x0 >= 0.0f && seg2->x1 <= 4.0f);
		}
	}

	bool isWallBehindPlane(Vec2f w0, Vec2f w1, Vec2f p0, Vec2f p1)
	{
		const f32 side0 = (w0.x - p0.x)*(p1.z - p0.z) - (w0.z - p0.z)*(p1.x - p0.x);
		const f32 side1 = (w1.x - p0.x)*(p1.z - p0.z) - (w1.z - p0.z)*(p1.x - p0.x);
		return side0 <= 0.0f || side1 <= 0.0f;
	}

	static Segment s_wallSegments[2048];

	void addPortalAsSky(RSector* curSector, RWall* wall)
	{
		u32 segCount = 0;
		GPUCachedSector* cached = &s_cachedSectors[curSector->index];
		cached->builtFrame = s_gpuFrame;

		// Calculate the vertices.
		const f32 x0 = fixed16ToFloat(wall->w0->x);
		const f32 x1 = fixed16ToFloat(wall->w1->x);
		const f32 z0 = fixed16ToFloat(wall->w0->z);
		const f32 z1 = fixed16ToFloat(wall->w1->z);
		f32 y0 = cached->ceilingHeight;
		f32 y1 = cached->floorHeight;
		f32 portalY0 = y0, portalY1 = y1;

		// Add a new segment.
		Segment* seg = &s_wallSegments[segCount];
		const Vec3f wallNormal = { -(z1 - z0), 0.0f, x1 - x0 };
		Vec2f v0 = { x0, z0 }, v1 = { x1, z1 }, heights = { y0, y1 }, portalHeights = { portalY0, portalY1 };
		if (!createNewSegment(seg, wall->id, false, v0, v1, heights, portalHeights, wallNormal))
		{
			return;
		}
		segCount++;

		// Split segments that cross the modulo boundary.
		if (seg->x1 > 4.0f)
		{
			splitSegment(false, s_wallSegments, segCount, seg, s_range, s_rangeSrc, s_rangeCount);
		}
		else if (!sbuffer_splitByRange(seg, s_range, s_rangeSrc, s_rangeCount))
		{
			// Out of the range, so cancel the segment.
			segCount--;
		}
		else
		{
			assert(seg->x0 >= 0.0f && seg->x1 <= 4.0f);
		}

		buildSegmentBuffer(false, curSector, segCount, s_wallSegments, true/*forceTreatAsSolid*/);
	}
		
	// Build world-space wall segments.
	bool buildSectorWallSegments(RSector* curSector, RSector* prevSector, RWall* portalWall, u32& uploadFlags, bool initSector, Vec2f p0, Vec2f p1, u32& segCount)
	{
		segCount = 0;
		GPUCachedSector* cached = &s_cachedSectors[curSector->index];
		cached->builtFrame = s_gpuFrame;

		// Portal range, all segments must be clipped to this.
		// The actual clip vertices are p0 and p1.
		s_rangeSrc[0] = p0;
		s_rangeSrc[1] = p1;
		s_rangeCount = 0;
		if (!initSector)
		{
			s_range[0].x = sbuffer_projectToUnitSquare(p0);
			s_range[0].z = sbuffer_projectToUnitSquare(p1);
			sbuffer_handleEdgeWrapping(s_range[0].x, s_range[0].z);
			s_rangeCount = 1;

			if (fabsf(s_range[0].x - s_range[0].z) < FLT_EPSILON)
			{
				sbuffer_clear();
				return false;
			}

			if (s_range[0].z > 4.0f)
			{
				s_range[1].x = 0.0f;
				s_range[1].z = s_range[0].z - 4.0f;
				s_range[0].z = 4.0f;
				s_rangeCount = 2;
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
			const Vec3f cameraVec = { x0 - s_cameraPos.x, 0.0f, z0 - s_cameraPos.z };
			if (wallNormal.x*cameraVec.x + wallNormal.z*cameraVec.z < 0.0f)
			{
				continue;
			}

			// Make sure the wall is on the correct side of the portal plane, if not in the initial sector.
			if (!initSector && !isWallBehindPlane({ x0, z0 }, { x1, z1 }, p0, p1))
			{
				continue;
			}

			// Is the wall a portal or is it effectively solid?
			bool isPortal = false;
			if (next)
			{
				bool nextNoWall = (next->flags1 & SEC_FLAGS1_NOWALL_DRAW) != 0;

				// Update any potential adjoins even if they are not traversed to make sure the
				// heights and walls settings are handled correctly.
				updateCachedSector(next, uploadFlags);

				fixed16_16 openTop, openBot;
				// Sky handling
				if ((curSector->flags1 & SEC_FLAGS1_EXTERIOR) && (next->flags1 & SEC_FLAGS1_EXT_ADJ))
				{
					openTop = curSector->ceilingHeight - intToFixed16(100);
					y0 = fixed16ToFloat(openTop);
				}
				// If the next sector has the "NoWall" flag AND this is an exterior adjoin - make the portal opening as large as the
				// the larger sector.
				else if (nextNoWall && (next->flags1 & SEC_FLAGS1_EXT_ADJ))
				{
					openTop = min(next->ceilingHeight, curSector->ceilingHeight);
					y0 = fixed16ToFloat(openTop);
				}
				else
				{
					openTop = min(curSector->floorHeight, max(curSector->ceilingHeight, next->ceilingHeight));
				}

				if ((curSector->flags1 & SEC_FLAGS1_PIT) && (next->flags1 & SEC_FLAGS1_EXT_FLOOR_ADJ))
				{
					openBot = curSector->floorHeight + intToFixed16(100);
					y1 = fixed16ToFloat(openBot);
				}
				// If the next sector has the "NoWall" flag AND this is an exterior adjoin - make the portal opening as large as the
				// the larger sector.
				else if (nextNoWall && (next->flags1 & SEC_FLAGS1_EXT_FLOOR_ADJ))
				{
					openBot = max(next->floorHeight, curSector->floorHeight);
					y1 = fixed16ToFloat(openTop);
				}
				else
				{
					openBot = max(curSector->ceilingHeight, min(curSector->floorHeight, next->floorHeight));
				}
				
				fixed16_16 openSize = openBot - openTop;
				portalY0 = fixed16ToFloat(openTop);
				portalY1 = fixed16ToFloat(openBot);

				if (openSize > 0)
				{
					// Is the portal inside the view frustum?
					// Cull the portal but potentially keep the edge.
					Vec3f qv0 = { x0, portalY0, z0 }, qv1 = { x1, portalY1, z1 };
					isPortal = frustum_quadInside(qv0, qv1);
				}
			}

			// Add a new segment.
			Segment* seg = &s_wallSegments[segCount];
			Vec2f v0 = { x0, z0 }, v1 = { x1, z1 }, heights = { y0, y1 }, portalHeights = { portalY0, portalY1 };
			if (!createNewSegment(seg, w, isPortal, v0, v1, heights, portalHeights, wallNormal))
			{
				continue;
			}
			segCount++;

			// Split segments that cross the modulo boundary.
			if (seg->x1 > 4.0f)
			{
				splitSegment(initSector, s_wallSegments, segCount, seg, s_range, s_rangeSrc, s_rangeCount);
			}
			else if (!initSector && !sbuffer_splitByRange(seg, s_range, s_rangeSrc, s_rangeCount))
			{
				// Out of the range, so cancel the segment.
				segCount--;
			}
			else
			{
				assert(seg->x0 >= 0.0f && seg->x1 <= 4.0f);
			}
		}

		buildSegmentBuffer(initSector, curSector, segCount, s_wallSegments, false/*forceTreatAsSolid*/);
		return true;
	}
		
	// Clip rule called on portal segments.
	// Return true if the segment should clip the incoming segment like a regular wall.
	bool clipRule(s32 id)
	{
		// for now always return false for adjoins.
		assert(id >= 0 && id < s_clipSector->wallCount);
		RWall* wall = &s_clipSector->walls[id];
		assert(wall->nextSector);	// we shouldn't get in here if nextSector is null.
		if (!wall->nextSector)
		{
			return true;
		}
		
		// next verify that there is an opening, if not then treat it as a regular wall.
		RSector* next = wall->nextSector;
		fixed16_16 opening = min(s_clipSector->floorHeight, next->floorHeight) - max(s_clipSector->ceilingHeight, next->ceilingHeight);
		if (opening <= 0)
		{
			return true;
		}

		// if the camera is below the floor, treat it as a wall.
		const f32 floorHeight = fixed16ToFloat(next->floorHeight);
		if (s_cameraPos.y > floorHeight && s_clipObjPos.y <= floorHeight)
		{
			return true;
		}
		const f32 ceilHeight = fixed16ToFloat(next->ceilingHeight);
		if (s_cameraPos.y < ceilHeight && s_clipObjPos.y >= ceilHeight)
		{
			return true;
		}

		return false;
	}

	void clipSpriteToView(RSector* curSector, Vec3f posWS, WaxFrame* frame, void* basePtr, void* objPtr, bool fullbright, u32 portalInfo)
	{
		if (!frame) { return; }
		s_clipSector = curSector;
		s_clipObjPos = posWS;

		// Compute the (x,z) extents of the frame.
		const f32 widthWS  = fixed16ToFloat(frame->widthWS);
		const f32 heightWS = fixed16ToFloat(frame->heightWS);
		const f32 fOffsetX = fixed16ToFloat(frame->offsetX);
		const f32 fOffsetY = fixed16ToFloat(frame->offsetY);

		Vec3f corner0 = { posWS.x - s_cameraRight.x*fOffsetX,  posWS.y + fOffsetY,   posWS.z - s_cameraRight.z*fOffsetX };
		Vec3f corner1 = { corner0.x + s_cameraRight.x*widthWS, corner0.y - heightWS, corner0.z + s_cameraRight.z*widthWS };
		Vec2f points[] =
		{
			{ corner0.x, corner0.z },
			{ corner1.x, corner1.z }
		};
		// Cull sprites outside of the view before clipping.
		if (!frustum_quadInside(corner0, corner1)) { return; }

		// Cull sprites too close to the camera.
		const Vec3f relPos = { posWS.x - s_cameraPos.x, posWS.y - s_cameraPos.y, posWS.z - s_cameraPos.z };
		const f32 z = relPos.x*s_cameraDir.x + relPos.y*s_cameraDir.y + relPos.z*s_cameraDir.z;
		if (z < 1.0f) { return; }

		// Clip against the current wall segments and the portal XZ extents.
		SegmentClipped dstSegs[1024];
		const s32 segCount = sbuffer_clipSegmentToBuffer(points[0], points[1], s_rangeCount, s_range, s_rangeSrc, 1024, dstSegs, clipRule);
		if (!segCount) { return; }

		// Then add the segments to the list.
		SpriteDrawFrame drawFrame =
		{
			basePtr, frame, objPtr,
			points[0], points[1],
			dstSegs[0].v0, dstSegs[0].v1,
			posWS.y,
			curSector,
			fullbright,
			portalInfo
		};
		sprdisplayList_addFrame(&drawFrame);

		for (s32 s = 1; s < segCount; s++)
		{
			drawFrame.c0 = dstSegs[s].v0;
			drawFrame.c1 = dstSegs[s].v1;
			sprdisplayList_addFrame(&drawFrame);
		}
	}
		
	void addSectorObjects(RSector* curSector, RSector* prevSector, s32 portalId, s32 prevPortalId)
	{
		// Decide how to clip objects.
		// Which top and bottom edges are we going to use to clip objects?
		s32 topPortal = portalId;
		s32 botPortal = portalId;

		if (prevSector)
		{
			fixed16_16 nextTop = curSector->ceilingHeight;
			fixed16_16 curTop = min(prevSector->floorHeight, max(nextTop, prevSector->ceilingHeight));
			f32 top = fixed16ToFloat(curTop);
			if (top < s_cameraPos.y && prevSector && prevSector->ceilingHeight <= curSector->ceilingHeight)
			{
				topPortal = prevPortalId;
			}

			fixed16_16 nextBot = curSector->floorHeight;
			fixed16_16 curBot = max(prevSector->ceilingHeight, min(nextBot, prevSector->floorHeight));
			f32 bot = fixed16ToFloat(curBot);
			if (bot > s_cameraPos.y && prevSector && prevSector->floorHeight >= curSector->floorHeight)
			{
				botPortal = prevPortalId;
			}
		}

		// Add the object portals.
		u32 portalInfo = 0u;
		if (topPortal || botPortal)
		{
			Vec4f outPlanes[MAX_PORTAL_PLANES * 2];
			u32 planeCount = 0;
			if (topPortal == botPortal)
			{
				planeCount = sdisplayList_getPlanesFromPortal(topPortal, PLANE_TYPE_BOTH, outPlanes);
			}
			else
			{
				planeCount  = sdisplayList_getPlanesFromPortal(topPortal, PLANE_TYPE_TOP, outPlanes);
				planeCount += sdisplayList_getPlanesFromPortal(botPortal, PLANE_TYPE_BOT, outPlanes + planeCount);
				planeCount = min(MAX_PORTAL_PLANES, planeCount);
			}
			portalInfo = objectPortalPlanes_add(planeCount, outPlanes);
		}

		const f32 ambient = (s_flatLighting) ? f32(s_flatAmbient) : fixed16ToFloat(curSector->ambient);
		const Vec2f floorOffset = { fixed16ToFloat(curSector->floorOffset.x), fixed16ToFloat(curSector->floorOffset.z) };

		SecObject** objIter = curSector->objectList;
		for (s32 i = 0; i < curSector->objectCount; objIter++)
		{
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
							// And finally the frame from the current sequence.
							WaxFrame* frame = WAX_FramePtr(wax, view, obj->frame & 31);
							clipSpriteToView(curSector, posWS, frame, wax, obj, (obj->flags & OBJ_FLAG_FULLBRIGHT) != 0, portalInfo);
						}
					}
					else if (type == OBJ_TYPE_FRAME)
					{
						clipSpriteToView(curSector, posWS, obj->fme, obj->fme, obj, (obj->flags & OBJ_FLAG_FULLBRIGHT) != 0, portalInfo);
					}
				}
				else if (type == OBJ_TYPE_3D)
				{
					model_add(obj->model, posWS, obj->transform, ambient, floorOffset, portalInfo);
				}
			}
		}
	}
		
	void traverseSector(RSector* curSector, RSector* prevSector, RWall* portalWall, s32 prevPortalId, s32& level, u32& uploadFlags, Vec2f p0, Vec2f p1)
	{
		if (level > MAX_ADJOIN_DEPTH_EXT)
		{
			return;
		}
		
		// Mark sector as being rendered for the automap.
		curSector->flags1 |= SEC_FLAGS1_RENDERED;

		// Build the world-space wall segments.
		u32 segCount = 0;
		if (!buildSectorWallSegments(curSector, prevSector, portalWall, uploadFlags, level == 0, p0, p1, segCount))
		{
			return;
		}

		// There is a portal but the sector beyond is degenerate but has a sky.
		// In this case the software renderer will still fill in the sky even though no walls are visible, so the GPU
		// renderer needs to emulate the same behavior.
		if (segCount == 0 && ((curSector->flags1 & SEC_FLAGS1_EXTERIOR) || (curSector->flags1 & SEC_FLAGS1_PIT)))
		{
			addPortalAsSky(prevSector, portalWall);
			return;
		}

		// Determine which objects are visible and add them.
		addSectorObjects(curSector, prevSector, s_displayCurrentPortalId, prevPortalId);

		// Traverse through visible portals.
		s32 parentPortalId = s_displayCurrentPortalId;

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
			if (sdisplayList_addPortal(corner0, corner1, parentPortalId))
			{
				portal->wall->drawFrame = s_gpuFrame;
				traverseSector(portal->next, curSector, portal->wall, parentPortalId, level, uploadFlags, portal->v0, portal->v1);
				portal->wall->drawFrame = 0;
			}

			frustum_pop();
			level--;
		}
	}
						
	bool traverseScene(RSector* sector)
	{
#if 0
		debug_update();
#endif

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
		objectPortalPlanes_clear();

		updateCachedSector(sector, uploadFlags);
		traverseSector(sector, nullptr, nullptr, 0, level, uploadFlags, startView[0], startView[1]);
		frustum_pop();

		sdisplayList_finish();
		sprdisplayList_finish();
		model_drawListFinish();
		objectPortalPlanes_finish();

		// Set the sector ambient for future lighting.
		if (s_flatLighting)
		{
			s_sectorAmbient = s_flatAmbient;
		}
		else
		{
			s_sectorAmbient = round16(sector->ambient);
		}
		s_scaledAmbient = (s_sectorAmbient >> 1) + (s_sectorAmbient >> 2) + (s_sectorAmbient >> 3);
		s_sectorAmbientFraction = s_sectorAmbient << 11;	// fraction of ambient compared to max.

		if (uploadFlags & UPLOAD_SECTORS)
		{
			s_sectorGpuBuffer.update(s_gpuSourceData.sectors, s_gpuSourceData.sectorSize);
		}
		if (uploadFlags & UPLOAD_WALLS)
		{
			s_wallGpuBuffer.update(s_gpuSourceData.walls, s_gpuSourceData.wallSize);
		}

		return sdisplayList_getSize() > 0;
	}

	void drawPass(SectorPass pass)
	{
		if (!sdisplayList_getSize(pass)) { return; }

		TexturePacker* texturePacker = texturepacker_getGlobal();
		const TextureGpu* palette  = TFE_RenderBackend::getPaletteTexture();
		const TextureGpu* textures = texturePacker->texture;
		const ShaderInputs* inputs = &s_shaderInputs[pass];
		const ShaderBuffer* textureTable = &texturePacker->textureTableGPU;

		TFE_RenderState::setStateEnable(true, STATE_DEPTH_WRITE | STATE_DEPTH_TEST);
		TFE_RenderState::setDepthFunction(CMP_LEQUAL);
		
		Shader* shader = &s_wallShader[pass];
		shader->bind();

		s_indexBuffer.bind();
		s_sectorGpuBuffer.bind(0);
		s_wallGpuBuffer.bind(1);
		s_colormapTex->bind(5);
		palette->bind(6);
		textures->bind(7);
		textureTable->bind(8);

		// Camera and lighting.
		const Vec4f lightData = { f32(s_worldAmbient), s_cameraLightSource ? 1.0f : 0.0f, 0.0f, s_showWireframe ? 1.0f : 0.0f };
		shader->setVariable(inputs->cameraPosId,  SVT_VEC3,   s_cameraPos.m);
		shader->setVariable(inputs->cameraViewId, SVT_MAT3x3, s_cameraMtx.data);
		shader->setVariable(inputs->cameraProjId, SVT_MAT4x4, s_cameraProj.data);
		shader->setVariable(inputs->cameraDirId,  SVT_VEC3,   s_cameraDir.m);
		shader->setVariable(inputs->lightDataId,  SVT_VEC4,   lightData.m);

		// Calculte the sky parallax.
		fixed16_16 p0, p1;
		TFE_Jedi::getSkyParallax(&p0, &p1);
		// The values are scaled by 4 to convert from angle to fixed in the original code.
		const f32 parallax[2] = { fixed16ToFloat(p0) * 0.25f, fixed16ToFloat(p1) * 0.25f };
		shader->setVariable(s_skyParallaxId[pass], SVT_VEC2, parallax);
		if (s_skyParamId[pass] >= 0)
		{
			u32 dispWidth, dispHeight;
			vfb_getResolution(&dispWidth, &dispHeight);

			const f32 fourOverTwoPi = 4.0f/6.283185f;	// 4 / 2pi
			const f32 rad45 = 0.785398f;	// 45 degrees in radians.
			const f32 skyParam[4] =
			{
				-atan2f(s_cameraDir.z, s_cameraDir.x) * fourOverTwoPi * parallax[0],
				 clamp(asinf(s_cameraDir.y), -rad45, rad45) * fourOverTwoPi * parallax[1],
				 1.0f / (f32(dispWidth) * 0.5f),
				 200.0f / f32(dispHeight),
			};
			shader->setVariable(s_skyParamId[pass], SVT_VEC4, skyParam);
		}

		if (s_shaderInputs[pass].globalLightingId > 0)
		{
			const f32 globalLighting[] = { s_flatLighting ? 1.0f : 0.0f, (f32)s_flatAmbient, 0.0f, 0.0f };
			shader->setVariable(s_shaderInputs[pass].globalLightingId, SVT_VEC4, globalLighting);
		}

		// Draw the sector display list.
		sdisplayList_draw(pass);
	}

	void drawSprites()
	{
		if (!sprdisplayList_getSize()) { return; }
		// For some reason depth test is required to write, so set the comparison function to always instead.
		TFE_RenderState::setStateEnable(true,  STATE_DEPTH_WRITE | STATE_DEPTH_TEST);
		TFE_RenderState::setDepthFunction(CMP_ALWAYS);

		s_spriteShader.bind();
		s_indexBuffer.bind();
		s_colormapTex->bind(3);

		const TextureGpu* palette = TFE_RenderBackend::getPaletteTexture();
		palette->bind(4);

		TexturePacker* texturePacker = texturepacker_getGlobal();
		const TextureGpu* textures = texturePacker->texture;
		textures->bind(5);

		ShaderBuffer* textureTable = &texturePacker->textureTableGPU;
		textureTable->bind(6);

		// Camera and lighting.
		Vec4f lightData = { f32(s_worldAmbient), s_cameraLightSource ? 1.0f : 0.0f, 0.0f, s_showWireframe ? 1.0f : 0.0f };
		s_spriteShader.setVariable(s_cameraRightId, SVT_VEC3, s_cameraRight.m);
		s_spriteShader.setVariable(s_shaderInputs[SPRITE_PASS].cameraPosId,  SVT_VEC3,   s_cameraPos.m);
		s_spriteShader.setVariable(s_shaderInputs[SPRITE_PASS].cameraViewId, SVT_MAT3x3, s_cameraMtx.data);
		s_spriteShader.setVariable(s_shaderInputs[SPRITE_PASS].cameraProjId, SVT_MAT4x4, s_cameraProj.data);
		s_spriteShader.setVariable(s_shaderInputs[SPRITE_PASS].cameraDirId,  SVT_VEC3,   s_cameraDir.m);
		s_spriteShader.setVariable(s_shaderInputs[SPRITE_PASS].lightDataId,  SVT_VEC4,   lightData.m);

		if (s_shaderInputs[SPRITE_PASS].globalLightingId > 0)
		{
			const f32 globalLighting[] = { s_flatLighting ? 1.0f : 0.0f, (f32)s_flatAmbient, 0.0f, 0.0f };
			s_spriteShader.setVariable(s_shaderInputs[SPRITE_PASS].globalLightingId, SVT_VEC4, globalLighting);
		}

		// Draw the sector display list.
		sprdisplayList_draw();

		s_spriteShader.unbind();
	}

	void draw3d()
	{
		TFE_RenderState::setStateEnable(true, STATE_DEPTH_WRITE | STATE_DEPTH_TEST);
		TFE_RenderState::setDepthFunction(CMP_LEQUAL);

		const TextureGpu* palette = TFE_RenderBackend::getPaletteTexture();
		palette->bind(0);

		s_colormapTex->bind(1);

		TexturePacker* texturePacker = texturepacker_getGlobal();
		const TextureGpu* textures = texturePacker->texture;
		textures->bind(2);

		ShaderBuffer* textureTable = &texturePacker->textureTableGPU;
		textureTable->bind(3);
		objectPortalPlanes_bind(4);
		model_drawList();

		// Cleanup
		TextureGpu::clearSlots(3);
	}
		
	void TFE_Sectors_GPU::draw(RSector* sector)
	{
		// Check to see if a rendering setting has changed
		// (this may require a shader recompile)
		TFE_Settings_Graphics* graphics = TFE_Settings::getGraphicsSettings();
		if (graphics->skyMode != s_skyMode)
		{
			s_skyMode = SkyMode(graphics->skyMode);
			bool result = updateBasePassShader();
			assert(result);
		}

		// Build the draw list.
		if (!traverseScene(sector))
		{
			return;
		}

		// State
		TFE_RenderState::setStateEnable(false, STATE_BLEND);
		TFE_RenderState::setStateEnable(true, STATE_DEPTH_WRITE | STATE_DEPTH_TEST | STATE_CULLING);
		if (s_showWireframe)
		{
			TFE_RenderState::setStateEnable(true, STATE_WIREFRAME);
		}

		for (s32 i = 0; i < SECTOR_PASS_COUNT - 1; i++)
		{
			drawPass(SectorPass(i));
		}
				
		// Draw Sprites.
		drawSprites();

		// Draw transparent pass.
		drawPass(SECTOR_PASS_TRANS);

		// Draw 3D Objects.
		draw3d();
				
		// Cleanup
		s_indexBuffer.unbind();
		s_sectorGpuBuffer.unbind(0);
		s_wallGpuBuffer.unbind(1);
		TextureGpu::clear(5);
		TextureGpu::clear(6);

		TFE_RenderState::setStateEnable(false, STATE_WIREFRAME);
		
		// Debug
		if (s_enableDebug)
		{
			renderDebug_draw();
		}
	}

	TextureGpu* TFE_Sectors_GPU::getColormap()
	{
		return s_colormapTex;
	}

	void TFE_Sectors_GPU::subrendererChanged()
	{
	}

	bool TFE_Sectors_GPU::updateBasePassShader()
	{
		// Load the opaque version of the shader.
		ShaderDefine basePassdefines[1] = {};
		s32 passPassDefineCount = 0;
		if (s_skyMode == SKYMODE_VANILLA)
		{
			basePassdefines[0].name = "SKYMODE_VANILLA";
			basePassdefines[0].value = "1";
			passPassDefineCount = 1;
		}
		return loadShaderVariant(0, passPassDefineCount, basePassdefines);
	}
}