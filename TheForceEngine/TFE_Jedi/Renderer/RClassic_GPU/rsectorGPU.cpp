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
#include "frustum.h"
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

	Shader m_wallShader;
	ShaderBuffer m_sectors;
	ShaderBuffer m_drawListPos;
	ShaderBuffer m_drawListData;
	s32 m_cameraPosId;
	s32 m_cameraViewId;
	s32 m_cameraProjId;
	Vec3f m_viewDir;

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
		f32 floorHeight;
		f32 ceilingHeight;
		u64 builtFrame;
	};

	struct GPUSourceData
	{
		Vec4f* sectors;
		u32 sectorSize;
	};
	
	GPUSourceData s_gpuSourceData = { 0 };
	Vec4f  s_displayListPos[1024];
	Vec4ui s_displayListData[1024];
	s32 s_displayListCount = 0;

	IndexBuffer m_indexBuffer;
	static GPUCachedSector* s_cachedSectors;
	static bool s_enableDebug = true;
	static u64 s_gpuFrame;
		
	extern Mat3  s_cameraMtx;
	extern Mat4  s_cameraProj;
	extern Vec3f s_cameraPos;

	void TFE_Sectors_GPU::reset()
	{
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
			
			m_wallShader.bindTextureNameToSlot("Sectors", 0);
			m_wallShader.bindTextureNameToSlot("DrawListPos", 1);
			m_wallShader.bindTextureNameToSlot("DrawListData", 2);

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

			s_gpuSourceData.sectorSize = sizeof(Vec4f) * s_sectorCount;
			s_gpuSourceData.sectors = (Vec4f*)level_alloc(s_gpuSourceData.sectorSize);
			memset(s_gpuSourceData.sectors, 0, s_gpuSourceData.sectorSize);

			for (u32 s = 0; s < s_sectorCount; s++)
			{
				RSector* curSector = &s_sectors[s];
				GPUCachedSector* cachedSector = &s_cachedSectors[s];
				cachedSector->floorHeight   = fixed16ToFloat(curSector->floorHeight);
				cachedSector->ceilingHeight = fixed16ToFloat(curSector->ceilingHeight);

				s_gpuSourceData.sectors[s].x = cachedSector->floorHeight;
				s_gpuSourceData.sectors[s].y = cachedSector->ceilingHeight;
				s_gpuSourceData.sectors[s].z = 0.0f;
				s_gpuSourceData.sectors[s].w = 0.0f;
			}

			ShaderBufferDef bufferDefSectors =
			{
				4,				// 1, 2, 4 channels (R, RG, RGBA)
				sizeof(f32),	// 1, 2, 4 bytes (u8; s16,u16; s32,u32,f32)
				BUF_CHANNEL_FLOAT
			};
			m_sectors.create(s_sectorCount, bufferDefSectors, true, s_gpuSourceData.sectors);

			ShaderBufferDef bufferDefDrawListPos =
			{
				4,				// 1, 2, 4 channels (R, RG, RGBA)
				sizeof(f32),	// 1, 2, 4 bytes (u8; s16,u16; s32,u32,f32)
				BUF_CHANNEL_FLOAT
			};
			m_drawListPos.create(1024, bufferDefDrawListPos, true);

			ShaderBufferDef bufferDefDrawListData =
			{
				4,				// 1, 2, 4 channels (R, RG, RGBA)
				sizeof(u32),	// 1, 2, 4 bytes (u8; s16,u16; s32,u32,f32)
				BUF_CHANNEL_UINT
			};
			m_drawListData.create(1024, bufferDefDrawListData, true);
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
	}

	void updateCachedSector(RSector* srcSector, u32& uploadFlags)
	{
		u32 flags = srcSector->dirtyFlags;
		if (!flags) { return; }  // Nothing to do.

		GPUCachedSector* cached = &s_cachedSectors[srcSector->index];
		if (flags & SDF_HEIGHTS)
		{
			cached->floorHeight   = fixed16ToFloat(srcSector->floorHeight);
			cached->ceilingHeight = fixed16ToFloat(srcSector->ceilingHeight);
			s_gpuSourceData.sectors[srcSector->index].x = cached->floorHeight;
			s_gpuSourceData.sectors[srcSector->index].y = cached->ceilingHeight;
			uploadFlags |= UPLOAD_SECTORS;
		}
		updateCachedWalls(srcSector, flags, uploadFlags);
		srcSector->dirtyFlags = SDF_NONE;
	}

	// TODO: Move to debug module.
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

	// TODO: Move to debug module.
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

	// TODO: Move to S-Buffer module.
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

		// Switch the direction to match the wall direction.
		return 4.0f - fmodf(f32(axis) + value, 4.0f);	// this wraps around to 0.0 at 4.0
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

		// Try to merge the head and tail because they might have been split on the modulo line.
		if (s_bufferHead != s_bufferTail)
		{
			if (s_bufferHead->x0 == 0.0f && s_bufferTail->x1 == 4.0f && s_bufferHead->seg->id == s_bufferTail->seg->id)
			{
				s_bufferTail->x1 = s_bufferHead->x1 + 4.0f;
				s_bufferTail->v1 = s_bufferHead->v1;
				s_bufferHead = s_bufferHead->next;
				s_bufferHead->prev = nullptr;
			}
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
		return (ax1 > bx0 && ax0 < bx1) || (bx1 > ax0 && bx0 < ax1);
	}
			
	void insertPortal(WallSegBuffer* portal)
	{
		if (!s_portalHead)
		{
			s_portalHead = portal;
			s_portalTail = portal;
			return;
		}

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
	
	// TODO: Move to sector display list module.
	void clearDisplayList()
	{
		s_displayListCount = 0;
	}

	void finishDisplayList()
	{
		if (!s_displayListCount) { return; }
		m_drawListPos.update(s_displayListPos, sizeof(Vec4f) * s_displayListCount);
		m_drawListData.update(s_displayListData, sizeof(Vec4ui) * s_displayListCount);
	}

	void addCapsToDisplayList(RSector* curSector)
	{
		// TODO: Constrain to portal frustum.
		Vec4f pos   = { fixed16ToFloat(curSector->boundsMin.x), fixed16ToFloat(curSector->boundsMin.z), fixed16ToFloat(curSector->boundsMax.x), fixed16ToFloat(curSector->boundsMax.z) };
		Vec4ui data = { 0, (u32)curSector->index/*sectorId*/, (u32)floor16(curSector->ambient), 0u/*textureId*/ };

		s_displayListPos[s_displayListCount]  = pos;
		s_displayListData[s_displayListCount] = data;
		s_displayListData[s_displayListCount].x = 5;	// part = floor cap;
		s_displayListCount++;

		s_displayListPos[s_displayListCount] = pos;
		s_displayListData[s_displayListCount] = data;
		s_displayListData[s_displayListCount].x = 6;	// part = ceiling cap;
		s_displayListCount++;
	}

	void addSegToDisplayList(RSector* curSector, WallSegBuffer* wallSeg)
	{
		s32 wallId = wallSeg->seg->id;
		RWall* srcWall = &curSector->walls[wallId];
		s32 ambient = floor16(curSector->ambient);

		Vec4f pos   = { wallSeg->v0.x, wallSeg->v0.z, wallSeg->v1.x, wallSeg->v1.z };
		Vec4ui data = { (srcWall->nextSector ? u32(srcWall->nextSector->index)<<16u : 0u)/*partId | nextSector*/, (u32)curSector->index/*sectorId*/,
			            ambient < 31 ? (u32)min(31, floor16(srcWall->wallLight) + floor16(curSector->ambient)) : 31u/*ambient*/, 0u/*textureId*/ };

		// Wall Flags.
		if (srcWall->drawFlags == WDF_MIDDLE && !srcWall->nextSector) // TODO: Fix transparent mid textures.
		{
			s_displayListPos[s_displayListCount]  = pos;
			s_displayListData[s_displayListCount] = data;
			s_displayListData[s_displayListCount].x |= 0;	// part = mid;
			s_displayListCount++;
		}
		if ((srcWall->drawFlags & WDF_TOP) && srcWall->nextSector)
		{
			s_displayListPos[s_displayListCount] = pos;
			s_displayListData[s_displayListCount] = data;
			s_displayListData[s_displayListCount].x |= 1;	// part = top;
			s_displayListCount++;
		}
		if ((srcWall->drawFlags & WDF_BOT) && srcWall->nextSector)
		{
			s_displayListPos[s_displayListCount] = pos;
			s_displayListData[s_displayListCount] = data;
			s_displayListData[s_displayListCount].x |= 2;	// part = bottom;
			s_displayListCount++;
		}
		// Add Floor
		s_displayListPos[s_displayListCount] = pos;
		s_displayListData[s_displayListCount] = data;
		s_displayListData[s_displayListCount].x |= 3;	// part = floor;
		s_displayListData[s_displayListCount].z = ambient;
		s_displayListCount++;

		// Add Ceiling
		s_displayListPos[s_displayListCount] = pos;
		s_displayListData[s_displayListCount] = data;
		s_displayListData[s_displayListCount].x |= 4;	// part = floor;
		s_displayListData[s_displayListCount].z = ambient;
		s_displayListCount++;
	}

	struct Portal
	{
		Vec2f v0, v1;
		f32 y0, y1;
		RSector* next;
		Frustum frustum;
	};
	static Portal s_portalList[2048];
	static s32 s_portalListCount = 0;

	void handleEdgeWrapping(f32& x0, f32& x1)
	{
		// Make sure the segment is the correct length.
		if (fabsf(x1 - x0) > 2.0f)
		{
			if (x0 < 1.0f) { x0 += 4.0f; }
			else { x1 += 4.0f; }
		}
		// Handle wrapping.
		while (x0 < 0.0f || x1 < 0.0f)
		{
			x0 += 4.0f;
			x1 += 4.0f;
		}
		while (x0 >= 4.0f && x1 >= 4.0f)
		{
			x0 = fmodf(x0, 4.0f);
			x1 = fmodf(x1, 4.0f);
		}
	}

	bool splitByRange(WallSegment* seg, Vec2f* range, Vec2f* points, s32 rangeCount)
	{
		// Clip a single segment against one or two ranges...
		bool inside = false;
		for (s32 r = 0; r < rangeCount; r++)
		{
			if (seg->x1 <= range[r].x || seg->x0 >= range[r].z)
			{
				continue;
			}
			inside = true;

			if (seg->x0 < range[r].x && seg->x1 > range[r].x)
			{
				seg->v0 = clipSegment(seg->v0, seg->v1, points[0]);
				seg->x0 = range[r].x;
			}
			else if (seg->x1 > range[r].z && seg->x0 < range[r].z)
			{
				seg->v1 = clipSegment(seg->v0, seg->v1, points[1]);
				seg->x1 = range[r].z;
			}
		}

		return inside;
	}

	// Build world-space wall segments.
	void buildSectorWallSegments(RSector* curSector, u32& uploadFlags, bool initSector, Vec2f p0, Vec2f p1)
	{
		static WallSegment wallSegments[2048];

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
			range[0].x = projectToUnitSquare(p0);
			range[0].z = projectToUnitSquare(p1);
			handleEdgeWrapping(range[0].x, range[0].z);
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

				const fixed16_16 openTop = min(curSector->floorHeight, max(curSector->ceilingHeight, next->ceilingHeight));
				const fixed16_16 openBot = max(curSector->ceilingHeight, min(curSector->floorHeight, next->floorHeight));
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
					isPortal = true;
					// Cull the portal but potentially keep the edge.
					if (!frustum_sphereInside(portalPos, portalRadius))
					{
						isPortal = false;
					}
				}
			}

			// Check if the wall is backfacing.
			const Vec3f wallNormal = { -(z1 - z0), 0.0f, x1 - x0 };
			const Vec3f cameraVec = { x0 - s_cameraPos.x, (y0 + y1)*0.5f - s_cameraPos.y, z0 - s_cameraPos.z };
			if (wallNormal.x*cameraVec.x + wallNormal.y*cameraVec.y + wallNormal.z*cameraVec.z < 0.0f)
			{
				continue;
			}

			// Is the wall inside the view frustum?
			WallSegment* seg;
			Vec3f wallPos = { (x0 + x1) * 0.5f, (y0 + y1) * 0.5f, (z0 + z1) * 0.5f };
			Vec3f maxPos = { max(x0, x1), max(y0, y1) + 200.0f, max(z0, z1) };	// account for floor and ceiling extensions, to do - a non-crappy way of handling this.
			Vec3f diag = { maxPos.x - wallPos.x, maxPos.y - wallPos.y, maxPos.z - wallPos.z };
			f32 portalRadius = sqrtf(diag.x*diag.x + diag.y*diag.y + diag.z*diag.z);
			if (!frustum_sphereInside(wallPos, portalRadius))
			{
				continue;
			}

			seg = &wallSegments[segCount];
			segCount++;

			seg->id = w;
			seg->portal = isPortal;
			seg->v0 = { x0, z0 };
			seg->v1 = { x1, z1 };
			seg->x0 = projectToUnitSquare(seg->v0);
			seg->x1 = projectToUnitSquare(seg->v1);

			// This means both vertices map to the same point on the unit square, in other words, the edge isn't actually visible.
			if (fabsf(seg->x0 - seg->x1) < FLT_EPSILON)
			{
				segCount--;
				continue;
			}

			// Project the edge.
			handleEdgeWrapping(seg->x0, seg->x1);
			// Check again for zero-length walls in case the fix-ups above caused it (for example, x0 = 0.0, x1 = 4.0).
			if (seg->x0 >= seg->x1 || seg->x1 - seg->x0 < FLT_EPSILON)
			{
				segCount--;
				continue;
			}
			assert(seg->x1 - seg->x0 > 0.0f && seg->x1 - seg->x0 <= 2.0f);

			seg->normal = wallNormal;
			seg->portal = isPortal;
			seg->y0 = y0;
			seg->y1 = y1;
			seg->portalY0 = isPortal ? portalY0 : y0;
			seg->portalY1 = isPortal ? portalY1 : y1;

			// Split segments that cross the modulo boundary.
			if (seg->x1 > 4.0f)
			{
				const f32 sx1 = seg->x1;
				const Vec2f sv1 = seg->v1;
				
				// Split the segment at the modulus border.
				seg->v1 = clipSegment(seg->v0, seg->v1, { 1.0f + s_cameraPos.x, -1.0f + s_cameraPos.z });
				seg->x1 = 4.0f;
				Vec2f newV1 = seg->v1;

				if (!initSector && !splitByRange(seg, range, points, rangeCount))
				{
					// Out of the range, so cancel the segment.
					segCount--;
				}
				else
				{
					s_wallSegGenerated++;
				}
				
				if (s_wallSegGenerated < s_maxWallSeg)
				{
					WallSegment* seg2;
					seg2 = &wallSegments[segCount];
					segCount++;

					*seg2 = *seg;
					seg2->x0 = 0.0f;
					seg2->x1 = sx1 - 4.0f;
					seg2->v0 = newV1;
					seg2->v1 = sv1;

					if (!initSector && !splitByRange(seg2, range, points, rangeCount))
					{
						// Out of the range, so cancel the segment.
						segCount--;
					}
					else
					{
						s_wallSegGenerated++;
					}
				}
			}
			else if (!initSector && !splitByRange(seg, range, points, rangeCount))
			{
				// Out of the range, so cancel the segment.
				segCount--;
			}
			else
			{
				s_wallSegGenerated++;
			}
		}

		// Next insert solid segments into the segment buffer one at a time.
		s_bufferPoolCount = 0;
		s_bufferHead = nullptr;
		s_bufferTail = nullptr;
		for (u32 i = 0; i < segCount; i++)
		{
			insertSegmentIntoBuffer(&wallSegments[i]);
		}
		mergeSegmentsInBuffer();

		if (initSector)
		{
			WallSegBuffer* wallSeg = s_bufferHead;
			while (wallSeg)
			{
				Vec3f vtx[4];
				vtx[0] = { wallSeg->v0.x, wallSeg->seg->y0, wallSeg->v0.z };
				vtx[1] = { wallSeg->v1.x, wallSeg->seg->y0, wallSeg->v1.z };
				vtx[2] = { wallSeg->v1.x, wallSeg->seg->y1, wallSeg->v1.z };
				vtx[3] = { wallSeg->v0.x, wallSeg->seg->y1, wallSeg->v0.z };

				Vec4f colorSolid  = { 1.0f, 0.0f, 1.0f, 1.0f };
				Vec4f colorPortal = { 0.0f, 1.0f, 1.0f, 1.0f };

				renderDebug_addPolygon(4, vtx, colorSolid);
				if (wallSeg->seg->portal)
				{
					vtx[0].y = wallSeg->seg->portalY0;
					vtx[1].y = wallSeg->seg->portalY0;
					vtx[2].y = wallSeg->seg->portalY1;
					vtx[3].y = wallSeg->seg->portalY1;
					renderDebug_addPolygon(4, vtx, colorPortal);
				}

				wallSeg = wallSeg->next;
			}
		}
				
		// Build the display list.
		WallSegBuffer* wallSeg = s_bufferHead;
		if (initSector) { clearDisplayList(); }
		while (wallSeg)
		{
			addSegToDisplayList(curSector, wallSeg);
			wallSeg = wallSeg->next;
		}
		if (initSector) { addCapsToDisplayList(curSector); }

		// Push portals onto the stack.
		wallSeg = s_bufferHead;
		while (wallSeg)
		{
			if (!wallSeg->seg->portal)
			{
				wallSeg = wallSeg->next;
				continue;
			}
			if (s_portalsTraversed >= s_maxPortals)
			{
				break;
			}

			WallSegBuffer* portal = wallSeg;
			RWall* wall = &curSector->walls[portal->seg->id];
			RSector* next = wall->nextSector;
			assert(next);

			f32 x0 = portal->v0.x;
			f32 x1 = portal->v1.x;
			f32 z0 = portal->v0.z;
			f32 z1 = portal->v1.z;
			f32 y0 = portal->seg->portalY0;
			f32 y1 = portal->seg->portalY1;
						
			// Clip the portal by the current frustum, and return if it is culled.
			Polygon clippedPortal;
			if (frustum_clipQuadToFrustum({ x0, y0, z0 }, { x1, y1, z1 }, &clippedPortal))
			{
				frustum_buildFromPolygon(&clippedPortal, &s_portalList[s_portalListCount].frustum);
				
				s_portalList[s_portalListCount].v0 = portal->v0;
				s_portalList[s_portalListCount].v1 = portal->v1;
				s_portalList[s_portalListCount].y0 = y0;
				s_portalList[s_portalListCount].y1 = y1;
				s_portalList[s_portalListCount].next = next;

				s_portalsTraversed++;
				s_portalListCount++;
			}
			wallSeg = wallSeg->next;
		}
	}

	void traverseAdjoin(RSector* curSector, s32& level, u32& uploadFlags, Vec2f p0, Vec2f p1)
	{
		//if (level >= 255)
		if (level >= 64)
		{
			return;
		}

		if (level == 0)
		{
			s_portalListCount = 0;
		}

		// Build the world-space wall segments.
		s32 portalStart = s_portalListCount;
		buildSectorWallSegments(curSector, uploadFlags, level == 0, p0, p1);
		s32 portalCount = s_portalListCount - portalStart;

		Portal* portal = &s_portalList[portalStart];
		for (s32 p = 0; p < portalCount; p++, portal++)
		{
			frustum_push(portal->frustum);
			level++;

			traverseAdjoin(portal->next, level, uploadFlags, portal->v0, portal->v1);

			frustum_pop();
			level--;
		}
	}
		
	// TODO: Move to display list module.
	bool buildDrawList(RSector* sector)
	{
		debug_update();

		// First build the camera frustum and push it onto the stack.
		frustum_buildFromCamera();

		s32 level = 0;
		u32 uploadFlags = UPLOAD_NONE;
		s_portalsTraversed = 0;
		s_wallSegGenerated = 0;
		Vec2f startView[] = { {0,0}, {0,0} };

		updateCachedSector(sector, uploadFlags);
		traverseAdjoin(sector, level, uploadFlags, startView[0], startView[1]);
		frustum_pop();

		finishDisplayList();

		if (uploadFlags & UPLOAD_SECTORS)
		{
			m_sectors.update(s_gpuSourceData.sectors, s_gpuSourceData.sectorSize);
		}

		return s_displayListCount > 0;
	}

	void TFE_Sectors_GPU::draw(RSector* sector)
	{
		// Build the draw list.
		if (!buildDrawList(sector))
		{
			return;
		}

		// State
		TFE_RenderState::setStateEnable(false, STATE_BLEND);
		TFE_RenderState::setStateEnable(true, STATE_DEPTH_WRITE | STATE_DEPTH_TEST | STATE_CULLING);

		// Shader and buffers.
		m_wallShader.bind();
		m_indexBuffer.bind();
		m_sectors.bind(0);
		m_drawListPos.bind(1);
		m_drawListData.bind(2);

		// Camera
		m_wallShader.setVariable(m_cameraPosId,  SVT_VEC3,   s_cameraPos.m);
		m_wallShader.setVariable(m_cameraViewId, SVT_MAT3x3, s_cameraMtx.data);
		m_wallShader.setVariable(m_cameraProjId, SVT_MAT4x4, s_cameraProj.data);
				
		// Draw the sector display list.
		TFE_RenderBackend::drawIndexedTriangles(2 * s_displayListCount, sizeof(u16));

		// Cleanup.
		m_wallShader.unbind();
		m_indexBuffer.unbind();
		m_sectors.unbind(0);
		m_drawListPos.unbind(1);
		m_drawListData.unbind(2);

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