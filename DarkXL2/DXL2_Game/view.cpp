#include "view.h"
#include "geometry.h"
#include "gameObject.h"
#include "physics.h"
#include "modelRendering.h"
#include "renderCommon.h"
#include <DXL2_Renderer/renderer.h>
#include <DXL2_System/system.h>
#include <DXL2_System/math.h>
#include <DXL2_FileSystem/paths.h>
#include <DXL2_Asset/levelAsset.h>
#include <DXL2_Asset/levelObjectsAsset.h>
#include <DXL2_Game/level.h>
#include <DXL2_Asset/spriteAsset.h>
#include <DXL2_Asset/textureAsset.h>
#include <DXL2_Asset/paletteAsset.h>
#include <DXL2_Asset/colormapAsset.h>
#include <DXL2_Asset/modelAsset.h>
#include <DXL2_LogicSystem/logicSystem.h>
#include <algorithm>
#include <assert.h>

namespace DXL2_View
{
	static f64 s_time = 0.0;

	const u32 c_portalFlag = 1073741824u; // (1 << 30);
	const u32 c_portalMask = 1073741823u; // (1 << 30) - 1

	// 64 bits, making sorting about the same speed as using pointers.
	struct Segment
	{
		// screen coordinates
		s16 x0, x1;
		// wall draw info index.
		u32 index;
	};
	struct WallDrawInfo
	{
		// original projected coordinates
		f32 ox0, odx;
		// original z values.
		f32 rz0, rz1;
		// wall index
		s16 wall;
		// sector id
		s32 sectorId;
		// view coordinates.
		Vec2f v0, v1;
		// clipped u coordinates.
		f32 u0, u1;
	};
	struct SectorDraw
	{
		s32 sectorId;
		s32 prevSectorId;
		s32 windowId;
		s32 win[2];
	};
	struct Range
	{
		s16 i0, i1;
	};
	struct PortalVisit
	{
		u32 frame;				// The frame visited, to avoid the need to clear.
		s32 rangeCount;
		Range range[MAX_RANGE];
	};
	struct MaskWall
	{
		u16 drawId;
		s16 x0, x1;
		u16 pad16;
		s32 maskHeightOffset;
	};

	static MaskWall s_maskWall[MAX_MASK_WALL];
	static s32 s_maskWallCount = 0;
	static s32 s_maskHeightOffset = 0;
		
	static const LevelData* s_level;
	static const LevelObjectData* s_levelObjects;
	static DXL2_Renderer* s_renderer;
	static std::vector<Vec2f> s_transformedVtx;

	static f32 s_halfWidth;
	static f32 s_halfHeight;
	static f32 s_heightScale;
	static s32 s_width;
	static s32 s_height;
	static s32* s_upperHeight = nullptr;
	static s32* s_lowerHeight = nullptr;
	static s16* s_upperHeightMask = nullptr;
	static s16* s_lowerHeightMask = nullptr;

	static f32 s_focalLength = 0.0f;
	static Vec2f s_clipLines[2];
	static f32 s_rot[2];

	static Segment s_segments[SEG_MAX];
	static WallDrawInfo s_drawWalls[WALL_MAX];
	static u32 s_segmentCount;
	static u32 s_wallCount;

	static SectorDraw s_sectorStack[MAX_SECTOR];
	static s32 s_stackRead, s_stackWrite;
		
	static u32 s_frame;
	static std::vector<PortalVisit> s_portalVisList;
	static std::vector<s32> s_wallToPortalMapping;

	static bool s_enableViewStats = false;
	static ViewStats s_viewStats = { 0 };

	static s32 s_prevSectorId;
	static s32 s_windowId;
	static s32 s_pitchOffset = 0;
	static s32 s_pitchSkyOffset = 0;

	// Textures
	static u32 s_textureCount = 0;
	static std::vector<Texture*> s_textures;
	static const Texture* const* s_textureList;
	static const ColorMap* s_colorMap;
	//

	void buildSkyWarpTable(bool useWarp);
	void buildClipLines(s32 x0, s32 x1);
	void drawSky(s32 x, s32 y0, s32 y1, s32 offsetY, const TextureFrame* tex, bool isWall);

	void drawSprite(s32 index);
	void drawMaskWall(s32 index, const Vec3f* cameraPos);

	enum ViewObjectType
	{
		VOBJ_SPRITE = 0,
		VOBJ_MASKWALL,
		VOBJ_3D,
		VOBJ_COUNT
	};

	struct ViewObject
	{
		ViewObjectType type;	// type of object to render.
		s32 index;				// index into object segment array.
	};
	static ViewObject s_viewObjects[MAX_MASK_WALL + MAX_SPRITE_SEG];
	static u32 s_viewObjectCount;
		
	static GameObjectList* s_objects;
	static SectorObjectList* s_sectorObjects;
		
	bool init(const LevelData* level, LevelObjectData* levelObjects, DXL2_Renderer* renderer, s32 w, s32 h, bool enableViewStats)
	{
		s_objects = LevelGameObjects::getGameObjectList();
		s_sectorObjects = LevelGameObjects::getSectorObjectList();

		if (!level)
		{
			s_width = w;
			s_height = h;
			s_halfWidth = f32(s_width >> 1);
			s_halfHeight = f32(s_height >> 1);
			s_heightScale = floorf(s_halfHeight * c_aspect);
			s_renderer = renderer;

			// This should be computed only when the resolution changes.
			s_focalLength = s_halfWidth / tanf(PI*0.25f);
			buildClipLines(0, s_width - 1);
			DXL2_ModelRender::init(s_renderer, w, h, s_focalLength, s_heightScale);
			DXL2_ModelRender::flipVertical();

			DXL2_RenderCommon::init(DXL2_ColorMap::get("SECBASE.CMP"));

			return true;
		}

		delete[] s_upperHeight;
		delete[] s_lowerHeight;

		Palette256* pal = DXL2_Palette::get256(level->palette);
		renderer->setPalette(pal);

		char cmpFile[DXL2_MAX_PATH];
		strcpy(cmpFile, level->palette);
		const size_t nameLen = strlen(cmpFile);
		cmpFile[nameLen - 3] = 'C';
		cmpFile[nameLen - 2] = 'M';
		cmpFile[nameLen - 1] = 'P';
		s_colorMap = DXL2_ColorMap::get(cmpFile);
		
		// If the color map doesn't exist, then a default...
		if (!s_colorMap)
		{
			s_colorMap = DXL2_ColorMap::generateColorMap(cmpFile, pal);
		}
		renderer->setColorMap(s_colorMap);
		DXL2_RenderCommon::init(s_colorMap);
				
		s_width = w;
		s_height = h;
		s_halfWidth = f32(s_width >> 1);
		s_halfHeight = f32(s_height >> 1);
		s_heightScale = floorf(s_halfHeight * c_aspect);
		s_enableViewStats = enableViewStats;

		s_level = level;
		s_levelObjects = levelObjects;
		s_renderer = renderer;
		s_frame = 0;

		buildSkyWarpTable(true);
		
		s_upperHeight = new s32[s_width];
		s_lowerHeight = new s32[s_width];
		if (!s_upperHeightMask)
		{
			s_upperHeightMask = new s16[MAX_MASK_HEIGHT];	// 64Kb
			s_lowerHeightMask = new s16[MAX_MASK_HEIGHT];	// 64Kb
		}

		// Allocate space to mark portals as visited.
		const u32 wallCount = (u32)level->walls.size();
		s_wallToPortalMapping.resize(wallCount);
		const SectorWall* walls = level->walls.data();

		s32 portalCount = 0;
		for (u32 w = 0; w < wallCount; w++)
		{
			if (walls[w].adjoin >= 0)
			{
				s_wallToPortalMapping[w] = portalCount;
				portalCount++;
			}
			else
			{
				s_wallToPortalMapping[w] = -1;
			}
		}
		s_portalVisList.resize(portalCount);

		// This should be computed only when the resolution changes.
		s_focalLength = s_halfWidth / tanf(PI*0.25f);
		buildClipLines(0, s_width - 1);
		DXL2_ModelRender::init(s_renderer, w, h, s_focalLength, s_heightScale);

		// Load Textures.
		s_textureCount = (u32)s_level->textures.size();
		s_textures.resize(s_textureCount);
		Texture* firstValid = nullptr;
		for (u32 t = 0; t < s_textureCount; t++)
		{
			s_textures[t] = s_level->textures[t];
			if (s_textures[t] && !firstValid) { firstValid = s_textures[t]; }
		}
		assert(firstValid);
		
		// Go through and assign null textures with something to avoid crashes.
		for (u32 t = 0; t < s_textureCount; t++)
		{
			if (!s_textures[t]) { s_textures[t] = firstValid; }
		}
		s_textureList = s_textures.data();

		// Add sprites and frames.
		// One object container per sector.
		const u32 sectorCount = (u32)s_level->sectors.size();
		s_sectorObjects->resize(sectorCount);
		for (u32 i = 0; i < sectorCount; i++)
		{
			(*s_sectorObjects)[i].list.clear();
		}

		if (s_levelObjects)
		{
			const u32 objectCount = s_levelObjects->objectCount;
			const LevelObject* object = s_levelObjects->objects.data();
			s_objects->reserve(objectCount + 1024);
			s_objects->resize(objectCount);
			for (u32 i = 0; i < objectCount; i++)
			{
				const ObjectClass oclass = object[i].oclass;
				if (oclass != CLASS_SPRITE && oclass != CLASS_FRAME && oclass != CLASS_3D) { continue; }

				// Get the position and sector.
				Vec3f pos = object[i].pos;
				s32 sectorId = DXL2_Physics::findSector(&pos);
				// Skip objects that are not in sectors.
				if (sectorId < 0) { continue; }

				std::vector<u32>& list = (*s_sectorObjects)[sectorId].list;
				list.push_back(i);

				GameObject* secobject = &(*s_objects)[i];
				secobject->id = i;
				secobject->pos = pos;
				secobject->angles = object[i].orientation;
				secobject->sectorId = sectorId;
				secobject->oclass = oclass;
				secobject->fullbright = false;
				secobject->collisionRadius = 0.0f;
				secobject->collisionHeight = 0.0f;
				secobject->physicsFlags = PHYSICS_GRAVITY;
				secobject->verticalVel = 0.0f;
				secobject->show = true;
				secobject->comFlags = object[i].comFlags;
				secobject->radius = object[i].radius;
				secobject->height = object[i].height;

				secobject->animId = 0;
				secobject->frameIndex = 0;

				if (!object[i].logics.empty() && (object[i].logics[0].type == LOGIC_LIFE || (object[i].logics[0].type >= LOGIC_BLUE && object[i].logics[0].type <= LOGIC_PILE)))
				{
					secobject->fullbright = true;
				}
								
				if (oclass == CLASS_SPRITE)
				{
					// HACK to fix Detention center, for some reason land-mines are mapped to Ewoks.
					if (!object[i].logics.empty() && object[i].logics[0].type == LOGIC_LAND_MINE)
					{
						secobject->oclass = CLASS_FRAME;
						secobject->frame = DXL2_Sprite::getFrame("WMINE.FME");
					}
					else
					{
						secobject->sprite = DXL2_Sprite::getSprite(s_levelObjects->sprites[object[i].dataOffset].c_str());
					}

					// This is just temporary until Logics are implemented.
					if (!object[i].logics.empty() && object[i].logics[0].type >= LOGIC_I_OFFICER && object[i].logics[0].type <= LOGIC_REE_YEES2)
					{
						secobject->animId = 5;
					}
				}
				else if (oclass == CLASS_FRAME)
				{
					// HACK to fix Detention center, for some reason land-mines are mapped to Ewoks.
					if (!object[i].logics.empty() && object[i].logics[0].type == LOGIC_LAND_MINE)
					{
						secobject->frame = DXL2_Sprite::getFrame("WMINE.FME");
					}
					else
					{
						secobject->frame = DXL2_Sprite::getFrame(s_levelObjects->frames[object[i].dataOffset].c_str());
					}
				}
				else if (oclass == CLASS_3D)
				{
					secobject->model = DXL2_Model::get(s_levelObjects->pods[object[i].dataOffset].c_str());
					// 3D objects, by default, have no gravity since they are likely environmental props (like bridges).
					secobject->physicsFlags = PHYSICS_NONE;
				}

				// Register the object logic.
				DXL2_LogicSystem::registerObjectLogics(secobject, object[i].logics, object[i].generators);
			}
		}
		
		return true;
	}

	void shutdown()
	{
		s_level = nullptr;
	}
	
	const ViewStats* getViewStats()
	{
		return &s_viewStats;
	}

	void clearHeightArray()
	{
		for (s32 x = 0; x < s_width; x++)
		{
			s_upperHeight[x] = 0;
			s_lowerHeight[x] = s_height - 1;
		}
	}
		
	void buildClipLines(s32 x0, s32 x1)
	{
		// to reduce precision issues with clipping, push out the planes by 2 pixels
		// otherwise we get 1 pixel black lines at 320x200.
		const f32 scale = 1.0f / s_focalLength;
		s_clipLines[0] = { (f32(x0 - 2) - s_halfWidth) * scale, 1.0f };
		s_clipLines[1] = { (f32(x1 + 2) - s_halfWidth) * scale, 1.0f };
	}

	f32 planeSide(const Vec2f* p, const Vec2f* v0, const Vec2f* v1)
	{
		const Vec2f r = { p->x - v0->x, p->z - v0->z };
		const Vec2f n = { -(v1->z - v0->z), v1->x - v0->x };
		return r.x * n.x + r.z * n.z;
	}

	bool clipLines(Vec2f* v0, Vec2f* v1, f32* u0, f32* u1)
	{
		if (v0->z < c_clip && v1->z < c_clip) { return false; }

		// First clip against the clip lines.
		const Vec2f x0 = { 0.0f, 0.0f };
		f32 s;

		const bool v0Behind0 = planeSide(v0, &x0, &s_clipLines[0]) > 0.0f;
		const bool v1Behind0 = planeSide(v1, &x0, &s_clipLines[0]) > 0.0f;
		// segment completely behind the plane.
		if (v0Behind0 && v1Behind0) { return false; }
		if (v0Behind0 != v1Behind0 && Geometry::lineSegment_LineIntersect(v0, v1, &x0, &s_clipLines[0], &s))
		{
			Vec2f ip = { v0->x + s * (v1->x - v0->x), v0->z + s * (v1->z - v0->z) };
			f32 up = *u0 + s * (*u1 - *u0);
			if (v0Behind0) { *v0 = ip; *u0 = up; }
			else { *v1 = ip; *u1 = up; }
		}

		const bool v0Behind1 = planeSide(v0, &x0, &s_clipLines[1]) < 0.0f;
		const bool v1Behind1 = planeSide(v1, &x0, &s_clipLines[1]) < 0.0f;
		// segment completely behind the plane.
		if (v0Behind1 && v1Behind1) { return false; }
		if (v0Behind1 != v1Behind1 && Geometry::lineSegment_LineIntersect(v0, v1, &x0, &s_clipLines[1], &s))
		{
			Vec2f ip = { v0->x + s * (v1->x - v0->x), v0->z + s * (v1->z - v0->z) };
			f32 up = *u0 + s * (*u1 - *u0);
			if (v0Behind1) { *v0 = ip; *u0 = up; }
			else { *v1 = ip; *u1 = up; }
		}

		// Clip against the near plane.
		// Trivial reject or accept if both vertices are on the same side.
		if (v0->z >= c_clip && v1->z >= c_clip) { return true; }
		if (v0->z < c_clip && v1->z < c_clip) { return false; }

		s = (c_clip - v0->z) / (v1->z - v0->z);
		const Vec2f newVtx = { v0->x + s * (v1->x - v0->x), v0->z + s * (v1->z - v0->z) };
		const f32 up = *u0 + s * (*u1 - *u0);
		if (v0->z < v1->z) { *v0 = newVtx; *u0 = up; }
		else { *v1 = newVtx; *u1 = up; }

		return true;
	}
		
	void insertSegment(s32 index, s32 x0, s32 x1, s32 drawIndex, bool portal)
	{
		const s32 start = std::max((s32)s_segmentCount, index + 1);
		for (s32 i = start; i > index; i--)
		{
			s_segments[i] = s_segments[i - 1];
		}

		s_segments[index].x0 = x0;
		s_segments[index].x1 = x1;
		s_segments[index].index = u32(drawIndex) | (portal ? c_portalFlag : 0);
		s_segmentCount++;
	}

	void splitSegment(s32 index, s32 x0, s32 x1, s32 drawIndex, bool portal)
	{
		s32 x1After = s_segments[index].x1;
		for (s32 i = s_segmentCount + 1; i > index + 1; i--)
		{
			s_segments[i] = s_segments[i - 2];
		}

		s_segments[index].x1 = x0 - 1;

		s_segments[index + 1].x0 = x0;
		s_segments[index + 1].x1 = x1;
		s_segments[index + 1].index = u32(drawIndex) | (portal ? c_portalFlag : 0);

		s_segments[index + 2].x0 = x1 + 1;
		s_segments[index + 2].x1 = x1After;
		s_segments[index + 2].index = s_segments[index].index;

		s_segmentCount+=2;
	}

	// returns true the current seg is closer than segIndex.
	// Adding the 'FLT_EPSILON' offset seems to fix Secbase but isn't very robust.
	// TODO: Convert this test to using the mid-points of the segment overlap rather than the end
	//   where they are most likely very close in some cases.
	bool testSegDepth(s32 x, f32 ox0, f32 odx, f32 rz0, f32 rz1, s32 segIndex)
	{
		// First get the z value from the current seg.
		const f32 s0 = std::max(0.0f, std::min((f32(x) - ox0) * odx, 1.0f));
		const f32 rzCur = rz0 + s0 * (rz1 - rz0);

		// Then the z value for the existing seg.
		const WallDrawInfo& drawInfo = s_drawWalls[s_segments[segIndex].index & c_portalMask];
		const f32 s1 = std::max(0.0f, std::min((f32(x) - drawInfo.ox0) * drawInfo.odx, 1.0f));
		const f32 rz = drawInfo.rz0 + s1 * (drawInfo.rz1 - drawInfo.rz0);

		return rzCur > rz;// -FLT_EPSILON;
	}

	// Same as testSegDepth but picks the max depth point to compare against.
	bool testSegDepthMax(s32 x0, s32 x1, f32 rz, s32 segIndex)
	{
		// The z value for the current seg is 'rz'
		const f32 rzCur = rz;

		// Pick the maximum z value for the existing seg (minimum 1/z)
		const WallDrawInfo& drawInfo = s_drawWalls[s_segments[segIndex].index & c_portalMask];
		const f32 s0 = std::max(0.0f, std::min((f32(x0) - drawInfo.ox0) * drawInfo.odx, 1.0f));
		const f32 s1 = std::max(0.0f, std::min((f32(x1) - drawInfo.ox0) * drawInfo.odx, 1.0f));
		const f32 rz0 = drawInfo.rz0 + s0 * (drawInfo.rz1 - drawInfo.rz0);
		const f32 rz1 = drawInfo.rz0 + s1 * (drawInfo.rz1 - drawInfo.rz0);
		const f32 rzSeg = std::min(rz0, rz1);

		return rzCur > rzSeg;
	}

	bool portalVisited(s32 x0, s32 x1, s32 drawIndex)
	{
		const s32 wallIndex = (s32)s_drawWalls[drawIndex].wall + (s32)s_level->sectors[s_drawWalls[drawIndex].sectorId].wallOffset;
		s32 portalIndex = s_wallToPortalMapping[wallIndex];
		if (portalIndex < 0)
		{
			s32 portalCount = (s32)s_portalVisList.size();
			portalIndex = portalCount;

			s_wallToPortalMapping[wallIndex] = portalCount;
			portalCount++;

			s_portalVisList.push_back({0});
		}
		assert(portalIndex >= 0);

		// for now just skip the whole portal if already visited.
		PortalVisit& visit = s_portalVisList[portalIndex];
		if (visit.frame != s_frame) { return false; }
		
		for (s32 r = 0; r < visit.rangeCount; r++)
		{
			// skip past any segments before the start of the range we care about.
			if (x0 > visit.range[r].i1 || x1 < visit.range[r].i0) { continue; }
			return true;
		}
		return false;
	}

	void insertRange(PortalVisit* visit, s32 index, s32 x0, s32 x1)
	{
		assert(visit->rangeCount < MAX_RANGE);
		for (s32 r = index; r < visit->rangeCount; r++)
		{
			visit->range[r + 1] = visit->range[r];
		}
		visit->range[index] = { s16(x0), s16(x1) };
		visit->rangeCount++;
	}

	void markPortalAsVisited(s32 x0, s32 x1, s32 drawIndex)
	{
		const s32 wallIndex = (s32)s_drawWalls[drawIndex].wall + (s32)s_level->sectors[s_drawWalls[drawIndex].sectorId].wallOffset;
		const s32 portalIndex = s_wallToPortalMapping[wallIndex];
		assert(portalIndex >= 0);

		PortalVisit* visit = &s_portalVisList[portalIndex];
		// Reset the range count if this is the first visit this frame.
		if (visit->frame != s_frame)
		{
			visit->rangeCount = 0;
		}

		visit->frame = s_frame;
		// Does this range already exist?
		for (s32 r = 0; r < visit->rangeCount; r++)
		{
			// skip past any segments before the start of the range we care about.
			if (x0 > visit->range[r].i1+1) { continue; }

			if (x0 <= visit->range[r].i0)
			{
				if (x1 >= visit->range[r].i0 - 1)
				{
					// If the range ranges overlap, then just extend the existing range.
					visit->range[r].i0 = std::min((s16)x0, visit->range[r].i0);
					visit->range[r].i1 = std::max((s16)x1, visit->range[r].i1);
					return;
				}
				else
				{
					// If the ranges do not overlap, then insert the new range before.
					insertRange(visit, r, x0, x1);
					return;
				}
			}
			else // if (x0 <= visit.range[r].i1+1)
			{
				// In this case the ranges are still touching, so extend.
				visit->range[r].i0 = std::min((s16)x0, visit->range[r].i0);
				visit->range[r].i1 = std::max((s16)x1, visit->range[r].i1);
				return;
			}
		}

		// The final case is the new range to the right of the existing range, just add to the end.
		assert(visit->rangeCount < MAX_RANGE);
		if (visit->rangeCount < MAX_RANGE)
		{
			visit->range[visit->rangeCount++] = { s16(x0), s16(x1) };
		}
	}

	struct SpriteSegment
	{
		u16 type;
		const void* data;
		const GameObject* obj;
		u32 heightOffset;
		s32 lightLevel;
		s16 ix0, ix1;
		s16 iy0, iy1;
		f32 x0, dx;
		f32 y0, dy;
		f32 z;
	};
	static u32 s_spriteSegCount;
	SpriteSegment s_spriteSegments[MAX_SPRITE_SEG];

	void clearSpriteSegments()
	{
		s_spriteSegCount = 0;
	}

	SpriteSegment* getSpriteSegment()
	{
		if (s_spriteSegCount >= MAX_SPRITE_SEG)
		{
			assert(0);
			return nullptr;
		}
		const u32 index = s_spriteSegCount;
		s_spriteSegCount++;
		return &s_spriteSegments[index];
	}

	void addSpriteSegment(s32 sx, s32 ex, SpriteSegment* segTemp)
	{
		s32 index = (s32)s_spriteSegCount;
		SpriteSegment* seg = getSpriteSegment();
		// If there isn't enough room then abort.
		if (!seg) { return; }

		*seg = *segTemp;
		seg->ix0 = sx;
		seg->ix1 = ex;
		seg->heightOffset = s_maskHeightOffset;

		// Copy the height mask.
		const s32 count = ex - sx + 1;
		// Make sure we don't overflow mask height data.
		if (s_maskHeightOffset + count > MAX_MASK_HEIGHT)
		{
			// Undo the new segment - there isn't enough room.
			assert(0);
			s_spriteSegCount--;
			return;
		}
		
		for (s32 x = sx; x <= ex; x++, s_maskHeightOffset++)
		{
			s_upperHeightMask[s_maskHeightOffset] = s_upperHeight[x];
			s_lowerHeightMask[s_maskHeightOffset] = s_lowerHeight[x];
		}

		// Add to the overall view list as well.
		if (segTemp->type == 0)
		{
			s_viewObjects[s_viewObjectCount].type = VOBJ_SPRITE;
		}
		else
		{
			s_viewObjects[s_viewObjectCount].type = VOBJ_3D;
		}
		s_viewObjects[s_viewObjectCount].index = index;
		s_viewObjectCount++;
	}
	
	bool clipSpriteToSegments(f32 x0, f32 x1, f32 y0, f32 y1, f32 z, s32 lightLevel, u16 type, const void* data, const GameObject* obj, const s32* win)
	{
		// Create the final rectangle and clip against the segments.
		s32 ix0 = std::max(s32(x0), 0);
		s32 ix1 = std::min(s32(x1), s_width - 1);
		// Cull if the sprite is too small.
		if (ix1 - ix0 < 1) { return false; }

		s32 iy0 = (s32)std::max(s32(y0), 0);
		s32 iy1 = (s32)std::min(s32(y1), s_height - 1);
		// Cull if the sprite is too small.
		if (iy1 - iy0 < 1) { return false; }

		// TODO: Clip parts inside the window and parts outside.
		// Outside parts must be dealt with seperately.
		s32 sx = std::max((s32)x0, win[0]);
		s32 ex = std::min((s32)x1, win[1] + 1) - 1;
		if (ex < sx) { return false; }

		// Compute interpolation and depth gradients.
		const f32 rz = z < FLT_EPSILON ? FLT_MAX : 1.0f / z;

		SpriteSegment segTemp = { 0 };
		segTemp.x0 = x0;
		segTemp.dx = 1.0f / (x1 - x0);
		segTemp.y0 = y0;
		segTemp.dy = 1.0f / (y1 - y0);
		segTemp.type = type;
		segTemp.data = data;
		segTemp.ix0 = ix0;
		segTemp.ix1 = ix1;
		segTemp.iy0 = iy0;
		segTemp.iy1 = iy1;
		segTemp.z = z;
		segTemp.obj = obj;
		segTemp.lightLevel = lightLevel;
		
		// search segments for screen overlap, clipping the remaining part of the segment down as we go.
		for (u32 i = 0; i < s_segmentCount; i++)
		{
			// skip past any segments before the start of the range we care about.
			if (sx > s_segments[i].x1) { continue; }

			// Portal segments have to be treated special -
			// if the sprite is behind the portal and the next sector hasn't been rendered yet
			// then it needs to be added to that sector temporarily.
			const bool isPortalSeg = (s_segments[i].index & c_portalFlag) != 0;

			// If the new segment is clipped, this will be the new start position.
			s32 newEnd = s_segments[i].x1 + 1;
			if (sx < s_segments[i].x0)
			{
				// The segment is entirely before the current segment, so it can just be inserted.
				if (ex < s_segments[i].x0)
				{
					addSpriteSegment(sx, ex, &segTemp);
					return true;
				}

				// now clip the wall to s_segments[i].x0 - 1
				s32 endClip = s_segments[i].x0 - 1;
				
				// test if the new segment is closer to the camera than the current segment.
				s32 maxPoint = std::min(ex, (s32)s_segments[i].x1);
				if (testSegDepthMax(s_segments[i].x0, maxPoint, rz, i))
				{
					// the end of the new segment is either its end point or the current segment end point.
					endClip = std::min(ex, (s32)s_segments[i].x1);
					// the start point of the next segment moves to the end.
					newEnd = s_segments[i].x1 + 1;
				}
				else if (isPortalSeg)
				{
					int x = 0;
					x++;
				}

				addSpriteSegment(sx, endClip, &segTemp);
			}
			else if (sx <= s_segments[i].x1)
			{
				// In this case, the new segment starts at or after the beginning of the current segment.
				// This means that the part overlapping the current segment is either occluded or on top of the existing segment.
				s32 maxPoint = std::min(ex, (s32)s_segments[i].x1);
				if (testSegDepthMax(sx, maxPoint, rz, i))
				{
					// determine which case to use:
					// 0: insert new before existing segment.
					// 1: split the existing segment, with the new segment in the middle.
					// 2: insert the new segment after the existing segment.
					s32 c = 0;
					if (sx == s_segments[i].x0) c = 0;
					else if (sx > s_segments[i].x0 && ex < s_segments[i].x1) c = 1;
					else if (sx > s_segments[i].x0 && ex >= s_segments[i].x1) c = 2;
					else { assert(0); }

					if (c == 0)
					{
						// 0: insert before
						newEnd = std::min(ex, (s32)s_segments[i].x1);
						addSpriteSegment(sx, newEnd, &segTemp);
						newEnd++;
					}
					else if (c == 1)
					{
						// 1: split, which means the entire new segment is accepted and we are done.
						addSpriteSegment(sx, ex, &segTemp);
						return true;
					}
					else
					{
						// 2: insert the new segment after.
						newEnd = s_segments[i].x1 + 1;
						if (i == s_segmentCount - 1)
						{
							// If this is the last segment and we are inserting afterwards - that means the entire
							// new segment is accepted and we are done.
							addSpriteSegment(sx, ex, &segTemp);
							return true;
						}
						else
						{
							// Insert the new segment after the existing segment.
							addSpriteSegment(sx, newEnd - 1, &segTemp);
						}
					}
				}
				else if (isPortalSeg)
				{
					int x = 0;
					x++;
				}
			}
			sx = std::max(sx, newEnd);
			// if the segment was completely contained by the current segment, then we are done.
			if (sx > ex || ex < newEnd) { return true; }
		}

		// If there is nothing left of the segment, then return.
		if (ex < sx) { return true; }

		// There is still segment left... insert it now.
		addSpriteSegment(sx, ex, &segTemp);
		return true;
	}

	void drawModel(s32 index, const Vec3f* cameraPos)
	{
		SpriteSegment* seg = &s_spriteSegments[index];
		const Model* model = (Model*)seg->data;
		const GameObject* obj = seg->obj;

		const s16* upper = &s_upperHeightMask[seg->heightOffset];
		const s16* lower = &s_lowerHeightMask[seg->heightOffset];

		DXL2_ModelRender::setClip(seg->ix0, seg->ix1, upper, lower);
		DXL2_ModelRender::draw(model, &obj->angles, &obj->pos, cameraPos, s_rot, (f32)s_pitchOffset, &s_level->sectors[obj->sectorId]);
	}

	void drawSprite(s32 index)
	{
		SpriteSegment* seg = &s_spriteSegments[index];
		const Frame* frame = (Frame*)seg->data;

		f32 dx = seg->dx;
		f32 v0 = frame->height - 1.0f;
		f32 v1 = 0.0f;

		const s16* upper = &s_upperHeightMask[seg->heightOffset];
		const s16* lower = &s_lowerHeightMask[seg->heightOffset];

		f32 dy = seg->dy;
		f32 y0 = seg->y0;
		f32 x0 = seg->x0;
		s32 start = seg->ix0;
		s32 count = seg->ix1 - seg->ix0 + 1;
		const bool flip = frame->Flip;
		for (s32 x = 0; x < count; x++)
		{
			const s32 iy0 = (s32)std::max((s16)upper[x], seg->iy0);
			const s32 iy1 = (s32)std::min((s16)lower[x], seg->iy1);

			const f32 c0 = (f32(iy0) - y0) * dy;
			const f32 c1 = (f32(iy1) - y0) * dy;
			const f32 v0c = v0 + c0 * (v1 - v0);
			const f32 v1c = v0 + c1 * (v1 - v0);

			const f32 s = std::max(0.0f, std::min((f32(x + start) - x0) * dx, 1.0f));
			s32 U = s32(s * frame->width);
			if (U < 0 || U > frame->width - 1) { continue; }
			if (flip) { U = frame->width - U - 1; }
			
			s_renderer->drawSpriteColumn(x + start, iy0, iy1, v0c, v1c, seg->lightLevel, &frame->image[U*frame->height], frame->height);
		}
	}

	bool addSegment(f32 x0, f32 x1, s32 drawIndex, const s32* win, bool portal)
	{
		if (s_segmentCount >= SEG_MAX) { assert(0);  return false; }

		// Early out if dealing with a backfacing segment.
		s32 sx = std::max((s32)x0, win[0]);
		s32 ex = std::min((s32)x1, win[1]+1) - 1;
		if (ex < sx) { return false; }

		// Compute interpolation and depth gradients.
		const f32 ox0 = s_drawWalls[drawIndex].ox0;
		const f32 odx = s_drawWalls[drawIndex].odx;
		const f32 rz0 = s_drawWalls[drawIndex].rz0;
		const f32 rz1 = s_drawWalls[drawIndex].rz1;

		// If this is a portal, make sure it isn't behind the current window.
		if (portal && portalVisited(sx, ex, drawIndex))
		{
			return false;
		}
		
		// search segments for screen overlap, clipping the remaining part of the segment down as we go.
		for (u32 i = 0; i < s_segmentCount; i++)
		{
			// skip past any segments before the start of the range we care about.
			if (sx > s_segments[i].x1) { continue; }

			// If the new segment is clipped, this will be the new start position.
			s32 newEnd = s_segments[i].x1 + 1;
			if (sx < s_segments[i].x0)
			{
				// The segment is entirely before the current segment, so it can just be inserted.
				if (ex < s_segments[i].x0)
				{
					insertSegment(i, sx, ex, drawIndex, portal);
					return true;
				}

				// now clip the wall to s_segments[i].x0 - 1
				insertSegment(i, sx, s_segments[i].x0 - 1, drawIndex, portal);
				
				// test if the new segment is closer to the camera than the current segment.
				s32 midPoint = (s_segments[i + 1].x0 + std::min(ex, (s32)s_segments[i + 1].x1)) >> 1;
				if (testSegDepth(midPoint, ox0, odx, rz0, rz1, i + 1))
				{
					// the end of the new segment is either its end point or the current segment end point.
					s_segments[i].x1 = std::min(ex, (s32)s_segments[i + 1].x1);
					// the current segment start point is past the new segment end point.
					s_segments[i + 1].x0 = s_segments[i].x1 + 1;
					// if the current segment is no longer valid, delete it.
					if (s_segments[i + 1].x0 > s_segments[i + 1].x1)
					{
						//removeSegment(i + 1);
					}
					// the start point of the next segment moves to the end.
					newEnd = s_segments[i].x1 + 1;
				}
			}
			else if (sx <= s_segments[i].x1)
			{
				// In this case, the new segment starts at or after the beginning of the current segment.
				// This means that the part overlapping the current segment is either occluded or on top of the existing segment.
				s32 midPoint = (sx + std::min(ex, (s32)s_segments[i].x1)) >> 1;
				if (testSegDepth(midPoint, ox0, odx, rz0, rz1, i))
				{
					// determine which case to use:
					// 0: insert new before existing segment.
					// 1: split the existing segment, with the new segment in the middle.
					// 2: insert the new segment after the existing segment.
					s32 c = 0;
					if (sx == s_segments[i].x0) c = 0;
					else if (sx > s_segments[i].x0 && ex < s_segments[i].x1) c = 1;
					else if (sx > s_segments[i].x0 && ex >= s_segments[i].x1) c = 2;
					else { assert(0); }

					if (c == 0)
					{
						// 0: insert before
						newEnd = std::min(ex, (s32)s_segments[i].x1);
						insertSegment(i, sx, newEnd, drawIndex, portal);

						newEnd++;
						s_segments[i + 1].x0 = newEnd;
						if (s_segments[i + 1].x0 > s_segments[i + 1].x1)
						{
							//removeSegment(i + 1);
						}
					}
					else if (c == 1)
					{
						// 1: split, which means the entire new segment is accepted and we are done.
						splitSegment(i, sx, ex, drawIndex, portal);
						return true;
					}
					else
					{
						// 2: insert the new segment after.
						newEnd = s_segments[i].x1 + 1;
						s_segments[i].x1 = sx - 1;
						assert(s_segments[i].x1 >= s_segments[i].x0);

						if (i == s_segmentCount - 1)
						{
							// If this is the last segment and we are inserting afterwards - that means the entire
							// new segment is accepted and we are done.
							insertSegment(i + 1, sx, ex, drawIndex, portal);
							return true;
						}
						else
						{
							// Insert the new segment after the existing segment.
							insertSegment(i + 1, sx, newEnd - 1, drawIndex, portal);
						}
					}
				}
			}
			sx = std::max(sx, newEnd);
			// if the segment was completely contained by the current segment, then we are done.
			if (sx > ex || ex < newEnd) { return true; }
		}

		// If there is nothing left of the segment, then return.
		if (ex < sx) { return true; }

		// There is still segment left... insert it now.
		s_segments[s_segmentCount].x0 = sx;
		s_segments[s_segmentCount].x1 = ex;
		s_segments[s_segmentCount].index = u32(drawIndex) | (portal ? c_portalFlag : 0);
		s_segmentCount++;

		return true;
	}

	f32 wallLength(const SectorWall* wall, const Sector* sector)
	{
		const Vec2f* vtx = s_level->vertices.data() + sector->vtxOffset;
		const Vec2f offset = { vtx[wall->i1].x - vtx[wall->i0].x, vtx[wall->i1].z - vtx[wall->i0].z };
		return sqrtf(offset.x * offset.x + offset.z * offset.z);
	}

	#define SPAN_MAX 4096
	#define SPAN_RANGE_MAX 16
	#define SPAN_TAG_BITS 20
	struct SpanRange	// 4 bytes
	{
		s16 x0, x1;
	};
	struct Span	// 68 bytes
	{
		s8 rangeCount, bot;		// range count, bot = 1 if bottom section.
		s16 y;
		SpanRange ranges[SPAN_RANGE_MAX];	// 64 bytes
	};
	static Span s_span[SPAN_MAX];   // Up to 8K resolution, 68 * 4kb = 272kb
	static u32 s_spanTag = 0;
	static s32 s_spanCount;
	// 12 bits : index, 20 bits: tag
	static u32 s_spanIndex[4096];

	Span* getSpan(s32 y, bool bot)
	{
		const u32 curTag = (s_spanIndex[y] >> 12);
		if (curTag != s_spanTag)
		{
			if (s_spanCount >= SPAN_MAX) { assert(0); return nullptr; }

			s_spanIndex[y] = s_spanCount | (s_spanTag << 12);
			s_span[s_spanCount].rangeCount = 0;
			s_span[s_spanCount].bot = bot ? 1 : 0;
			s_span[s_spanCount].y = y;
			s_spanCount++;
		}
		return &s_span[s_spanIndex[y]&4095];
	}

	const TextureFrame* getTextureFrame(s32 id, u32 frame)
	{
		if (id < 0 || !s_textureList[id]) { return nullptr; }
		if (!s_textureList[id]->frameCount)
		{
			return &s_textureList[id]->frames[0];
		}
		else if (s_textureList[id]->frameCount && s_textureList[id]->frameRate == 0.0f)
		{
			return &s_textureList[id]->frames[frame];
		}

		// Animated texture, so override the frame.
		frame = u32(f32(s_textureList[id]->frameRate) * s_time) % s_textureList[id]->frameCount;
		return &s_textureList[id]->frames[frame];
	}
		
	void addMaskWall(s32 drawId, s32 x0, s32 x1)
	{
		// Make sure we don't overflow mask walls.
		if (s_maskWallCount >= MAX_MASK_WALL)
		{
			assert(0);
			return;
		}
		s32 count = x1 - x0 + 1;
		if (count < 1) { return; }

		// Make sure we don't overflow mask height data.
		if (s_maskHeightOffset + count > MAX_MASK_HEIGHT)
		{
			assert(0);
			return;
		}

		// Create a new mask wall.
		s32 index = (s32)s_maskWallCount;
		MaskWall* maskWall = &s_maskWall[s_maskWallCount];
		s_maskWallCount++;
		
		maskWall->maskHeightOffset = s_maskHeightOffset;
		maskWall->x0 = x0;
		maskWall->x1 = x1;
		maskWall->drawId = drawId;

		// Copy the height mask.
		for (s32 x = 0; x < count; x++, s_maskHeightOffset++)
		{
			s_upperHeightMask[s_maskHeightOffset] = s_upperHeight[x + x0];
			s_lowerHeightMask[s_maskHeightOffset] = s_lowerHeight[x + x0];
		}

		// Add to the overall view list as well.
		s_viewObjects[s_viewObjectCount].type = VOBJ_MASKWALL;
		s_viewObjects[s_viewObjectCount].index = index;
		s_viewObjectCount++;
	}

	// TODO: Figure out why these offsets are required.
	#define TOP_Y_OFFSET 1.0f
	#define BOT_Y_OFFSET 0.5f

	void drawMaskWall(s32 index, const Vec3f* cameraPos)
	{
		MaskWall* maskWall = &s_maskWall[index];
		const WallDrawInfo& drawInfo = s_drawWalls[maskWall->drawId];
		const f32 x0 = drawInfo.ox0;
		const f32 dx = drawInfo.odx;
		const f32 rz0 = drawInfo.rz0;
		const f32 rz1 = drawInfo.rz1;
		const f32 u0 = drawInfo.u0 * rz0;
		const f32 u1 = drawInfo.u1 * rz1;
		const Sector* curSector = s_level->sectors.data() + drawInfo.sectorId;
		const SectorWall* curWall = s_level->walls.data() + drawInfo.wall + curSector->wallOffset;

		const s16* upper = &s_upperHeightMask[maskWall->maskHeightOffset];
		const s16* lower = &s_lowerHeightMask[maskWall->maskHeightOffset];

		const TextureFrame* midFrame = getTextureFrame(curWall->mid.texId, 0);
		if (!midFrame) { return; }

		const s32 start = maskWall->x0;
		const s32 end = maskWall->x1 + 1;

		// peek into the next sector to get the floor and ceiling heights.
		const Sector* next = s_level->sectors.data() + curWall->adjoin;

		// mask walls extend from the bottom of the upper section to the upper part of the lower section.
		f32 bot = curSector->floorAlt;
		f32 top = curSector->ceilAlt;
		s32 botOffset = 0;
		s32 topOffset = 0;
		// Lower section
		if (next->floorAlt < curSector->floorAlt)
		{
			bot = next->floorAlt;
			botOffset = 1;
		}
		// Upper section
		if (next->ceilAlt > curSector->ceilAlt)
		{
			top = next->ceilAlt;
			topOffset = -1;
		}

		// Calculate the floor and ceiling height in view space.
		const f32 botView = (bot - cameraPos->y) * s_heightScale;
		const f32 topView = (top - cameraPos->y) * s_heightScale;

		const s32 midDim[] = { midFrame ? midFrame->width : 0, midFrame ? midFrame->height : 0 };

		const f32 h = (bot - top) * c_worldToTexelScale;
		f32 midV[2];
		midV[1] = curWall->mid.offsetY * c_worldToTexelScale;
		midV[0] = midV[1] + h;
		midV[1] -= midV[0];  // delta from midV[0]

		// Draw the mask wall columns.
		for (s32 x = start; x < end; x++)
		{
			if (upper[x - start] >= lower[x - start]) { continue; }

			const f32 s = std::max(0.0f, std::min((f32(x) - x0) * dx, 1.0f));
			const f32 rz = rz0 + s * (rz1 - rz0);
			// Do this once per column rather than once per feature.
			const f32 z = 1.0f / rz;
			const f32 u = (u0 + s * (u1 - u0)) * z;

			// This is a hack to get the elevator in Secbase working. 
			// It is plausible this was hard coded into the engine and if so it'll need to stay but I don't know for sure.
			const s32 wallLight = curSector->ambient < 31 ? curWall->light : 0;
			const s32 lightLevel = DXL2_RenderCommon::lightFalloff((s32)curSector->ambient + wallLight, z);

			const f32 botProj = botView * rz + s_pitchOffset;
			const f32 topProj = topView * rz + s_pitchOffset;
			const s32 y0 = std::max((s32)upper[x - start], s32(topProj + TOP_Y_OFFSET) + topOffset);
			const s32 y1 = std::min((s32)lower[x - start], s32(botProj + BOT_Y_OFFSET) + botOffset);

			const f32 uWall = (u + curWall->mid.offsetX) * c_worldToTexelScale;
			s32 U = s32(uWall) & (midDim[0] - 1);
			if (curWall->flags[0] & WF1_FLIP_HORIZ) { U = midDim[0] - 1 - U; }

			// clip to the y range.
			const f32 hScale = 1.0f / f32(botProj - topProj);
			const f32 clipV0 = (f32(y0) - topProj) * hScale;
			const f32 clipV1 = (f32(y1) - topProj) * hScale;
			const f32 v0p = midV[0] + clipV0 * midV[1];
			const f32 v1p = midV[0] + clipV1 * midV[1];

			s_renderer->drawMaskTexturedColumn(x, y0, y1, v0p, v1p, lightLevel, &midFrame->image[U*midDim[1]], midDim[1]);
		}
	}

	// View objects of different types must be sorted amongst each other.
	// So this function will draw any of the view object types.
	void drawViewObjects(const Vec3f* cameraPos)
	{
		for (s32 i = (s32)s_viewObjectCount - 1; i >= 0; i--)
		{
			const ViewObject* obj = &s_viewObjects[i];
			if (obj->type == VOBJ_MASKWALL)
			{
				drawMaskWall(obj->index, cameraPos);
			}
			else if (obj->type == VOBJ_SPRITE)
			{
				drawSprite(obj->index);
			}
			else if (obj->type == VOBJ_3D)
			{
				drawModel(obj->index, cameraPos);
			}
			else
			{
				// Invalid.
				assert(0);
			}
		}
	}

	void drawSign(const TextureFrame* signFrame, const Sector* curSector, const SectorWall* curWall, const SectorTexture* baseTex, f32 u, s32 x, s32 y0, s32 y1, f32 floorAlt, f32 ceilAlt, f32 hBase, u8 lightLevel)
	{
		if (!signFrame) { return; }
		if (curWall->flags[0] & WF1_ILLUM_SIGN) { lightLevel = 31; }

		const f32 offsetX = baseTex->offsetX - curWall->sign.offsetX;
		// The way the mid offset Y is factored in is quite strange but has been arrived at by looking at the data...
		const f32 offsetY = -DXL2_Math::fract(std::max(baseTex->offsetY, 0.0f)) + curWall->sign.offsetY;
		const f32 uWall = (u + offsetX) * c_worldToTexelScale;
		const s32 U = s32(uWall);

		// TODO: Fix sign anchoring.
		if (curWall->flags[0] & WF1_SIGN_ANCHORED)
		{
			
		}

		// TODO: Move this out?
		f32 signV[2];
		const f32 h = hBase * c_worldToTexelScale;
		signV[1] = offsetY * c_worldToTexelScale;
		signV[0] = signV[1] + h;
		signV[1] -= signV[0];  // delta from midV[0]

		if (uWall > 0 && uWall < signFrame->width)
		{
			// clip to the y range.
			const f32 hScale = 1.0f / f32(floorAlt - ceilAlt);
			const f32 clipV0 = (f32(y0) - ceilAlt) * hScale;
			const f32 clipV1 = (f32(y1) - ceilAlt) * hScale;
			const f32 v0p = signV[0] + clipV0 * signV[1];
			const f32 v1p = signV[0] + clipV1 * signV[1];

			s_renderer->drawClampedTexturedColumn(x, y0, y1, v0p, v1p, lightLevel, &signFrame->image[U*signFrame->height], signFrame->height, signFrame->height >= 64 ? 0 : 2, U == signFrame->width - 1);
		}
	}

	// Merge Segments with a single pass, though deletion is slower.
	void mergeSegs(const s32* win)
	{
		s32 segToRemove[SEG_MAX];
		s32 segToRemCount = 0;

		s32 start = -1;
		for (u32 i = 0; i < s_segmentCount; i++)
		{
			if (s_segments[i].x1 < win[0]) { continue; }
			start = i;
			break;
		}
		if (start < 0) { return; }

		s32 prev = -1;
		for (s32 i = start; i < (s32)s_segmentCount; i++)
		{
			if (s_segments[i].x0 > s_segments[i].x1) { assert(segToRemCount < SEG_MAX);  segToRemove[segToRemCount++] = i; continue; }
			if (s_segments[i].x0 > win[1]) { break; }

			// Attempt to merge neighboring valid segments.
			if (prev >= 0)
			{
				Segment* seg0 = &s_segments[prev];
				Segment* seg1 = &s_segments[i];
				if (seg0->index == seg1->index && seg0->x1 + 1 == seg1->x0)
				{
					seg0->x1 = seg1->x1;
					segToRemove[segToRemCount++] = i;
					continue;
				}
			}
			prev = i;
		}

		// delete invalid segments and portals.
		for (s32 i = segToRemCount - 1; i >= 0; i--)
		{
			for (s32 j = segToRemove[i]; j < (s32)s_segmentCount; j++)
			{
				s_segments[j] = s_segments[j + 1];
			}
			s_segmentCount--;
		}
	}

	void drawSegs(const Vec3f* cameraPos, const s32* win)
	{
		s32 segToRemove[SEG_MAX];
		s32 segToRemCount = 0;
		
		s32 start = -1;
		for (u32 i = 0; i < s_segmentCount; i++)
		{
			if (s_segments[i].x1 < win[0]) { continue; }
			start = i;
			break;
		}
		if (start < 0) { return; }

		// Constants for drawing spans with world to texel space scaling baking in.
		const f32 xOffset = cameraPos->x * c_worldToTexelScale;
		const f32 zOffset = cameraPos->z * c_worldToTexelScale;
		const f32 r0 = s_rot[0] * c_worldToTexelScale;
		const f32 r1 = s_rot[1] * c_worldToTexelScale;

		const u32 spanTagMask = (1 << SPAN_TAG_BITS) - 1;
		const Sector* curSector = nullptr;
		s_spanCount = 0;
		s_spanTag = (s_spanTag + 1) & spanTagMask;

		for (s32 i = start; i < (s32)s_segmentCount; i++)
		{
			if (s_segments[i].x0 > s_segments[i].x1) { segToRemove[segToRemCount++] = i; continue; }
			if (s_segments[i].x0 > win[1]) { break; }
									
			const WallDrawInfo& drawInfo = s_drawWalls[s_segments[i].index & c_portalMask];
			const f32 x0 = drawInfo.ox0;
			const f32 dx = drawInfo.odx;
			const f32 rz0 = drawInfo.rz0;
			const f32 rz1 = drawInfo.rz1;
			const f32 cU0 = drawInfo.u0;
			const f32 cU1 = drawInfo.u1;
			curSector = s_level->sectors.data() + drawInfo.sectorId;
			const SectorWall* curWall = s_level->walls.data() + drawInfo.wall + curSector->wallOffset;

			const bool exteriorAdj = (curSector->flags[0] & SEC_FLAGS1_EXT_ADJ) != 0;
			const bool exteriorFloorAdj = (curSector->flags[0] & SEC_FLAGS1_EXT_FLOOR_ADJ) != 0;
			const bool exterior = (curSector->flags[0] & SEC_FLAGS1_EXTERIOR) != 0;
			const bool pit = (curSector->flags[0] & SEC_FLAGS1_PIT) != 0;
			const bool fullSky = (curSector->flags[0] & SEC_FLAGS1_NOWALL_DRAW) != 0;

			const f32 floorHeightOffs = pit ? 100.0f * s_heightScale : 0;
			const f32 ceilHeightOffs = exterior ? -100.0f * s_heightScale : 0;

			const s32 start = s_segments[i].x0;
			const s32 end = s_segments[i].x1 + 1;
			const f32 floorAltBase = (curSector->floorAlt - cameraPos->y) * s_heightScale;
			const f32 ceilAltBase = (curSector->ceilAlt - cameraPos->y) * s_heightScale;

			// Handle next sector.
			bool hasLower = false;
			bool hasUpper = false;
			bool hasLowerSky = false;
			bool hasUpperSky = false;
			f32 lowerBase[2];
			f32 upperBase[2];
			f32 lowerHeight, upperHeight;
			const Sector* next = nullptr;
			const bool hasAdjoin = curWall->adjoin >= 0 && curWall->adjoin != drawInfo.sectorId && (curWall->adjoin != s_prevSectorId || curWall->mirror != s_windowId);
			bool skyAdjoin = false, pitAdjoin = false;
			if (hasAdjoin)
			{
				// peek into the next sector to get the floor and ceiling heights.
				next = s_level->sectors.data() + curWall->adjoin;
				const bool nextExterior = (next->flags[0] & SEC_FLAGS1_EXTERIOR) != 0;
				const bool nextPit = (next->flags[0] & SEC_FLAGS1_PIT) != 0;
				const bool nextFull = (next->flags[0] & SEC_FLAGS1_NOWALL_DRAW) != 0;

				if ((curSector->flags[0] & SEC_FLAGS1_EXTERIOR) && (next->flags[0] & SEC_FLAGS1_EXTERIOR))
				{
					skyAdjoin = true;
				}
				if ((curSector->flags[0] & SEC_FLAGS1_PIT) && (next->flags[0] & SEC_FLAGS1_PIT))
				{
					pitAdjoin = true;
				}

				// Lower section
				if (next->floorAlt < curSector->floorAlt)
				{
					hasLower = true;
					hasLowerSky = fullSky || (nextPit && exteriorFloorAdj) || (exterior && nextFull);
					
					if (pitAdjoin && hasLowerSky)
					{
						lowerBase[0] = (curSector->floorAlt + 100.0f - cameraPos->y) * s_heightScale;
						lowerBase[1] = (next->floorAlt + 100.0f - cameraPos->y) * s_heightScale;
					}
					else
					{
						lowerBase[0] = floorAltBase;
						lowerBase[1] = (next->floorAlt - cameraPos->y) * s_heightScale;
					}
					lowerHeight = curSector->floorAlt - next->floorAlt;
				}
				// Upper section
				if (next->ceilAlt > curSector->ceilAlt)
				{
					hasUpper = true;
					hasUpperSky = fullSky || (nextExterior && exteriorAdj) || (exterior && nextFull);
					
					if (skyAdjoin && hasUpperSky)
					{
						upperBase[0] = (next->ceilAlt - 100.0f - cameraPos->y) * s_heightScale;
						upperBase[1] = (curSector->ceilAlt - 100.0f - cameraPos->y) * s_heightScale;
					}
					else
					{
						upperBase[0] = (next->ceilAlt - cameraPos->y) * s_heightScale;
						upperBase[1] = ceilAltBase;
					}
					upperHeight = next->ceilAlt - curSector->ceilAlt;
				}

				// Can we see into the next sector?
				f32 h1 = std::min(curSector->floorAlt, next->floorAlt);
				f32 h0 = std::max(curSector->ceilAlt, next->ceilAlt);
				if (skyAdjoin) { h0 -= 100.0f; }
				if (pitAdjoin) { h1 += 100.0f; }

				assert(s_stackWrite < MAX_SECTOR);
				if (h1 - h0 > 0.0f && s_stackWrite < MAX_SECTOR)
				{
					markPortalAsVisited(start, end - 1, s_segments[i].index & c_portalMask);

					// If the next sector is visible, add it to the stack.
					s_sectorStack[s_stackWrite].sectorId = curWall->adjoin;
					s_sectorStack[s_stackWrite].prevSectorId = drawInfo.sectorId;
					s_sectorStack[s_stackWrite].windowId = drawInfo.wall;
					s_sectorStack[s_stackWrite].win[0] = start;
					s_sectorStack[s_stackWrite].win[1] = end - 1;
					s_stackWrite++;

					segToRemove[segToRemCount++] = i;
				}
			}
						
			const TextureFrame* midFrame = getTextureFrame(curWall->mid.texId, curWall->mid.frame);
			const TextureFrame* topFrame = getTextureFrame(curWall->top.texId, curWall->top.frame);
			const TextureFrame* botFrame = getTextureFrame(curWall->bot.texId, curWall->bot.frame);
			const TextureFrame* signFrame = getTextureFrame(curWall->sign.texId, curWall->sign.frame);
			const TextureFrame* ceilFrame = getTextureFrame(curSector->ceilTexture.texId, 0);
			const TextureFrame* floorFrame = getTextureFrame(curSector->floorTexture.texId, 0);
			
			// Mid Texturing.
			s32 midDim[2], botDim[2], topDim[2];
			f32 midV[2], botV[2], topV[2];
			if (curWall->adjoin < 0 || curWall->adjoin == drawInfo.sectorId)
			{
				midDim[0] = midFrame ? midFrame->width : 0;
				midDim[1] = midFrame ? midFrame->height : 0;

				f32 offsetY = curWall->mid.offsetY * c_worldToTexelScale;
				if (curWall->flags[0] & WF1_TEX_ANCHORED)
				{
					offsetY += (DXL2_Level::getBaseSectorHeight(curSector->id)->floorAlt - curSector->floorAlt) * c_worldToTexelScale;
				}

				const f32 h = (curSector->floorAlt - curSector->ceilAlt) * c_worldToTexelScale;
				midV[1] = offsetY;
				midV[0] = midV[1] + h;

				midV[1] -= midV[0];  // delta from midV[0]
			}
			if (hasLower && botFrame)
			{
				botDim[0] = botFrame->width;
				botDim[1] = botFrame->height;

				f32 offsetY = curWall->bot.offsetY * c_worldToTexelScale;
				if (curWall->flags[0] & WF1_TEX_ANCHORED)
				{
					// Handle next sector moving.
					f32 baseHeight = DXL2_Level::getBaseSectorHeight(next->id)->floorAlt;
					offsetY -= (baseHeight - next->floorAlt) * c_worldToTexelScale;

					// Handle current sector moving.
					baseHeight = DXL2_Level::getBaseSectorHeight(curSector->id)->floorAlt;
					offsetY += (baseHeight - curSector->floorAlt) * c_worldToTexelScale;
				}
				
				const f32 h = (curSector->floorAlt - next->floorAlt) * c_worldToTexelScale;
				botV[1] = offsetY;
				botV[0] = botV[1] + h;

				botV[1] -= botV[0];  // delta from botV[0]
			}
			if (hasUpper && topFrame)
			{
				topDim[0] = topFrame->width;
				topDim[1] = topFrame->height;

				f32 offsetY = curWall->top.offsetY * c_worldToTexelScale;
				if (curWall->flags[0] & WF1_TEX_ANCHORED)
				{
					const f32 h = (next->ceilAlt - curSector->ceilAlt) * c_worldToTexelScale;
					topV[1] = /*f32(topDim[1]) + */offsetY;
					topV[0] = topV[1] + h;
				}
				else
				{
					const f32 h = (next->ceilAlt - curSector->ceilAlt) * c_worldToTexelScale;
					topV[1] = f32(topDim[1]) + offsetY;
					topV[0] = topV[1] + h;
				}

				topV[1] -= topV[0];  // delta from topV[0]
			}

			const f32 u0 = cU0 * rz0;
			const f32 u1 = cU1 * rz1;
			bool wallDrawn = false, floorDrawn = false, ceilDrawn = false, upperDrawn = false, lowerDrawn = false;

			if (hasAdjoin && (curWall->flags[0] & WF1_ADJ_MID_TEX))
			{
				addMaskWall(s_segments[i].index & c_portalMask, start, end - 1);
			}

			// Draw wall segments and build spans to draw flats.
			for (s32 x = start; x < end; x++)
			{
				if (s_upperHeight[x] >= s_lowerHeight[x]) { continue; }
				
				const f32 s = std::max(0.0f, std::min((f32(x) - x0) * dx, 1.0f));
				const f32 rz = rz0 + s * (rz1 - rz0);
				// Do this once per column rather than once per feature.
				const f32 z = 1.0f / rz;
				const f32 u = (u0 + s * (u1 - u0)) * z;

				// This is a hack to get the elevator in Secbase working. 
				// It is plausible this was hard coded into the engine and if so it'll need to stay but I don't know for sure.
				s32 wallLight = curSector->ambient < 31 ? curWall->light : 0;
				s32 lightLevel = DXL2_RenderCommon::lightFalloff((s32)curSector->ambient + wallLight, z);

				f32 floorAlt = floorAltBase * rz + s_pitchOffset;
				f32 ceilAlt = ceilAltBase * rz + s_pitchOffset;
				f32 floorAltSky = (floorAltBase + floorHeightOffs) * rz + s_pitchOffset;
				f32 ceilAltSky = (ceilAltBase + ceilHeightOffs) * rz + s_pitchOffset;

				s32 y0baseSky = s32(ceilAltSky + TOP_Y_OFFSET);
				s32 y1baseSky = s32(floorAltSky + BOT_Y_OFFSET);

				s32 y0base = s32(ceilAlt + TOP_Y_OFFSET);
				s32 y1base = s32(floorAlt+ BOT_Y_OFFSET);

				s32 y0 = std::max(s_upperHeight[x], y0base);
				s32 y1 = std::min(s_lowerHeight[x], y1base);

				s32 y0sky = std::max(s_upperHeight[x], y0baseSky);
				s32 y1sky = std::min(s_lowerHeight[x], y1baseSky);

				// draw the floor
				if (y1sky < s_lowerHeight[x])
				{
					if (curSector->flags[0] & SEC_FLAGS1_PIT)
					{
						drawSky(x, std::max(y1sky + 1, s_upperHeight[x]), s_lowerHeight[x], (s32)curSector->floorTexture.offsetY, floorFrame, false);
					}
					else
					{
						// Add and adjust spans.
						s32 spanStart = std::max(y1 + 1, s_upperHeight[x]);
						s32 spanEnd = s_lowerHeight[x] + 1;
						for (s32 y = spanStart; y < spanEnd; y++)
						{
							Span* span = getSpan(y, true);
							if (!span) { continue; }
							// search for an adjacent range.
							bool insertAtEnd = true;
							const s32 rangeCount = (s32)span->rangeCount;
							for (s32 r = 0; r < rangeCount; r++)
							{
								if (x == span->ranges[r].x1 + 1)
								{
									span->ranges[r].x1++;
									insertAtEnd = false;
									break;
								}
							}
							if (insertAtEnd)
							{
								assert(span->rangeCount < SPAN_RANGE_MAX);
								if (span->rangeCount < SPAN_RANGE_MAX)
								{
									span->ranges[span->rangeCount].x0 = x;
									span->ranges[span->rangeCount].x1 = x;
									span->rangeCount++;
								}
							}
						}
					}

					// Perf stats
					floorDrawn = true;
				}

				// draw the ceiling
				if (y0sky > s_upperHeight[x])
				{
					if (curSector->flags[0] & SEC_FLAGS1_EXTERIOR)
					{
						drawSky(x, s_upperHeight[x], std::min(y0sky - 1, s_lowerHeight[x]), (s32)curSector->ceilTexture.offsetY, ceilFrame, false);
					}
					else
					{
						// Add and adjust spans.
						s32 spanStart = s_upperHeight[x];
						s32 spanEnd = std::min(y0 - 1, s_lowerHeight[x]) + 1;
						for (s32 y = spanStart; y < spanEnd; y++)
						{
							Span* span = getSpan(y, false);
							if (!span) { continue; }

							// search for an adjacent range.
							bool insertAtEnd = true;
							const s32 rangeCount = (s32)span->rangeCount;
							for (s32 r = 0; r < rangeCount; r++)
							{
								if (x == span->ranges[r].x1 + 1)
								{
									span->ranges[r].x1++;
									insertAtEnd = false;
									break;
								}
							}
							if (insertAtEnd)
							{
								assert(span->rangeCount < SPAN_RANGE_MAX);
								if (span->rangeCount < SPAN_RANGE_MAX)
								{
									span->ranges[span->rangeCount].x0 = x;
									span->ranges[span->rangeCount].x1 = x;
									span->rangeCount++;
								}
							}
						}
					}

					// Perf stats
					ceilDrawn = true;
				}

				// draw the wall
				if (!hasAdjoin)
				{
					if (fullSky)
					{
						drawSky(x, y0sky, y1sky, (s32)curSector->ceilTexture.offsetY, ceilFrame, true);
					}
					else
					{
						// If the sky continues on, draw a sky strip to fill the gap.
						if (y0sky < y0)
						{
							drawSky(x, y0sky, std::min(y0-1, s_lowerHeight[x]), (s32)curSector->ceilTexture.offsetY, ceilFrame, true);
						}
						// If the sky goes below (a pit), draw a sky strip.
						if (y1sky > y1)
						{
							drawSky(x, std::max(y1+1, s_upperHeight[x]), y1sky, (s32)curSector->floorTexture.offsetY, floorFrame, true);
						}
						
						///////////////////////////////////////////////////////////////////////////////
						const f32 uWall = (u + curWall->mid.offsetX) * c_worldToTexelScale;
						s32 U = s32(uWall) & (midDim[0] - 1);
						if (curWall->flags[0] & WF1_FLIP_HORIZ) { U = midDim[0] - 1 - U; }

						// clip to the y range.
						const f32 hScale = 1.0f / f32(floorAlt - ceilAlt);
						const f32 clipV0 = (f32(y0) - ceilAlt) * hScale;
						const f32 clipV1 = (f32(y1) - ceilAlt) * hScale;
						const f32 v0p = midV[0] + clipV0 * midV[1];
						const f32 v1p = midV[0] + clipV1 * midV[1];
						///////////////////////////////////////////////////////////////////////////////

						s_renderer->drawTexturedColumn(x, y0, y1, v0p, v1p, lightLevel, &midFrame->image[U*midDim[1]], midDim[1]);
					}

					drawSign(signFrame, curSector, curWall, &curWall->mid, u, x, y0, y1, floorAlt, ceilAlt, curSector->floorAlt - curSector->ceilAlt, lightLevel);

					//s_lowerHeight[x] = s_height>>1;
					//s_upperHeight[x] = s_height>>1;
					// Perf stats
					wallDrawn = true;
				}
				else
				{
					s_lowerHeight[x] = std::min(s_lowerHeight[x], pitAdjoin ? y1sky : y1);
					s_upperHeight[x] = std::max(s_upperHeight[x], skyAdjoin ? y0sky : y0);

					// Lower section
					if (hasLower)
					{
						floorAlt = lowerBase[0] * rz + s_pitchOffset;
						ceilAlt = lowerBase[1] * rz + s_pitchOffset;
						const s32 ly0base = s32(ceilAlt + TOP_Y_OFFSET);
						const s32 ly1base = s32(floorAlt + BOT_Y_OFFSET);
						const s32 ly0 = std::max(s_upperHeight[x], ly0base);
						const s32 ly1 = std::min(s_lowerHeight[x], ly1base);

						if (y1sky > ly1)
						{
							drawSky(x, std::max(ly1+1, s_upperHeight[x]), y1sky, (s32)curSector->floorTexture.offsetY, floorFrame, true);
						}

						if (ly0 >= 0 && ly1 >= 0 && ly0 < s_height && ly1 < s_height && ly0 != ly1)
						{
							if (hasLowerSky)
							{
								drawSky(x, ly0, ly1, (s32)curSector->ceilTexture.offsetY, ceilFrame, true);
							}
							else
							{
								const f32 uBot = (u + curWall->bot.offsetX) * c_worldToTexelScale;
								s32 U = s32(uBot) & (botDim[0] - 1);
								if (curWall->flags[0] & WF1_FLIP_HORIZ) { U = botDim[0] - 1 - U; }

								// clip to the y range.
								const f32 hScale = 1.0f / f32(floorAlt - ceilAlt);
								const f32 clipV0 = f32(ly0 - ceilAlt) * hScale;
								const f32 clipV1 = f32(ly1 - ceilAlt) * hScale;
								const f32 v0p = botV[0] + clipV0 * botV[1];
								const f32 v1p = botV[0] + clipV1 * botV[1];
								///////////////////////////////////////////////////////////////////////////////

								s_renderer->drawTexturedColumn(x, ly0, ly1, v0p, v1p, lightLevel, &botFrame->image[U*botDim[1]], botDim[1]);
							}

							drawSign(signFrame, curSector, curWall, &curWall->bot, u, x, ly0, ly1, floorAlt, ceilAlt, lowerHeight, lightLevel);
							s_lowerHeight[x] = std::min(s_lowerHeight[x], ly0);

							// Perf stats
							lowerDrawn = true;
						}
					}
					else if (pit && y1sky > y1)
					{
						drawSky(x, std::max(y1 + 1, s_upperHeight[x]), y1sky, (s32)curSector->floorTexture.offsetY, floorFrame, true);
					}

					// Upper section
					if (hasUpper)
					{
						floorAlt = upperBase[0] * rz + s_pitchOffset;
						ceilAlt = upperBase[1] * rz + s_pitchOffset;
						const s32 uy0base = s32(ceilAlt + TOP_Y_OFFSET);
						const s32 uy1base = s32(floorAlt + BOT_Y_OFFSET);
						const s32 uy0 = std::max(s_upperHeight[x], uy0base);
						const s32 uy1 = std::min(s_lowerHeight[x], uy1base);

						if (y0sky < uy0)
						{
							drawSky(x, y0sky, std::min(uy0 - 1, s_lowerHeight[x]), (s32)curSector->ceilTexture.offsetY, ceilFrame, true);
						}

						if (uy0 >= 0 && uy1 >= 0 && uy0 < s_height && uy1 < s_height && uy0 != uy1)
						{
							if (hasUpperSky)
							{
								drawSky(x, uy0, uy1, (s32)curSector->ceilTexture.offsetY, ceilFrame, true);
							}
							else
							{
								///////////////////////////////////////////////////////////////////////////////
								// First Pass texturing. Note this is low quality and needs to be improved...
								const f32 uTop = (u + curWall->top.offsetX) * c_worldToTexelScale;
								s32 U = s32(uTop) & (topDim[0] - 1);
								if (curWall->flags[0] & WF1_FLIP_HORIZ) { U = topDim[0] - 1 - U; }

								// clip to the y range.
								const f32 hScale = 1.0f / f32(floorAlt - ceilAlt);
								const f32 clipV0 = f32(uy0 - ceilAlt) * hScale;
								const f32 clipV1 = f32(uy1 - ceilAlt) * hScale;
								const f32 v0p = topV[0] + clipV0 * topV[1];
								const f32 v1p = topV[0] + clipV1 * topV[1];
								///////////////////////////////////////////////////////////////////////////////

								s_renderer->drawTexturedColumn(x, uy0, uy1, v0p, v1p, lightLevel, &topFrame->image[U*topDim[1]], topDim[1]);
							}
							if (!hasLower)
							{
								drawSign(signFrame, curSector, curWall, &curWall->top, u, x, uy0, uy1, floorAlt, ceilAlt, upperHeight, lightLevel);
							}
							s_upperHeight[x] = std::max(s_upperHeight[x], uy1);

							// Perf stats
							upperDrawn = true;
						}
					}
					else if (exterior && y0sky < y0)
					{
						drawSky(x, y0sky, std::min(y0 - 1, s_lowerHeight[x]), (s32)curSector->ceilTexture.offsetY, ceilFrame, true);
					}
				}
			}
			
			if (s_enableViewStats)
			{
				s_viewStats.segWallRendered += (wallDrawn ? 1 : 0);
				s_viewStats.segLowerRendered += (lowerDrawn ? 1 : 0);
				s_viewStats.segUpperRendered += (upperDrawn ? 1 : 0);
				s_viewStats.floorPolyRendered += (floorDrawn ? 1 : 0);
				s_viewStats.ceilPolyRendered += (ceilDrawn ? 1 : 0);
			}
		}

		// Draw Spans - These are the flats attached to this segment.
		// TODO: Convert inner loop to fixed point.
		if (s_spanCount)
		{
			const TextureFrame* frameBot = getTextureFrame(curSector->floorTexture.texId, 0);
			const TextureFrame* frameTop = getTextureFrame(curSector->ceilTexture.texId, 0);
			const bool botPow2 = DXL2_Math::isPow2(frameBot->width) && DXL2_Math::isPow2(frameBot->height);
			const bool topPow2 = DXL2_Math::isPow2(frameTop->width) && DXL2_Math::isPow2(frameTop->height);
			assert(botPow2 && topPow2);

			const f32 floorOffset[] = { curSector->floorTexture.offsetX, curSector->floorTexture.offsetY };
			const f32 ceilOffset[] = { curSector->ceilTexture.offsetX, curSector->ceilTexture.offsetY };
			const f32 botPlaneHeight = fabsf(curSector->floorAlt - cameraPos->y);
			const f32 topPlaneHeight = fabsf(curSector->ceilAlt - cameraPos->y);
			const s32 lightLevelBase = (s32)curSector->ambient;
			for (s32 s = 0; s < s_spanCount; s++)
			{
				const Span* span = &s_span[s];
				const TextureFrame* frame = span->bot ? frameBot : frameTop;
				const f32* texOffsets = span->bot ? floorOffset : ceilOffset;
				const s32 texWidth = frame->width;
				const s32 texHeight = frame->height;

				const f32 planeHeight = span->bot ? botPlaneHeight : topPlaneHeight;
				const f32 yf = (f32)abs(span->y - s_pitchOffset);
				const f32 z = (s_heightScale / yf) * planeHeight;

				const s32 lightLevel = DXL2_RenderCommon::lightFalloff((s32)curSector->ambient, z);

				for (s32 r = 0; r < span->rangeCount; r++)
				{
					const SpanRange* range = &span->ranges[r];
					s_renderer->drawSpan(range->x0, range->x1, span->y, z, r0, r1, xOffset - texOffsets[0] * c_worldToTexelScale, zOffset - texOffsets[1] * c_worldToTexelScale, lightLevel, frame->width, frame->height, frame->image);
				}
			}
		}

		// delete invalid segments and portals.
		for (s32 i = segToRemCount - 1; i >= 0; i--)
		{
			for (s32 j = segToRemove[i]; j < (s32)s_segmentCount; j++)
			{
				s_segments[j] = s_segments[j + 1];
			}
			s_segmentCount--;
		}
	}

	#define MAX_ITER 1024
	static s32 s_iterMax = MAX_ITER;
	void setIterationOverride(s32 iterMax)
	{
		if (iterMax == 0) { s_iterMax = MAX_ITER; }
		else s_iterMax = iterMax;
	}

	static s32 s_skyAngle;
		
	void setupSky(f32 yaw)
	{
		yaw += 0.8f;
		if (yaw < 0.0f) { yaw += PI * 2.0f; }
		else if (yaw >= PI * 2.0f) { yaw -= PI * 2.0f; }

		f32 u = yaw / (PI * 2.0f);
		u = DXL2_Math::fract(1.0f - u);

		s_skyAngle = s32(s_level->parallax[0] * u);
	}

	// The original Dark Forces half width/height values for 320x200.
	#define SKY_HALF_WIDTH 160
	#define SKY_HALF_HEIGHT 100
	static s32* s_skyWarp = nullptr;

	void buildSkyWarpTable(bool useWarp)
	{
		delete[] s_skyWarp;
		s_skyWarp = new s32[s_width];

		if (useWarp)
		{
			for (s32 x = 0; x < s_width; x++)
			{
				f32 xf = f32(x - s_halfWidth) / s_halfWidth;
				s_skyWarp[x] = s32(cosf(xf * 0.6545f) * xf * s_halfWidth + s_halfWidth) * SKY_HALF_WIDTH / (s32)s_halfWidth;
			}
		}
		else
		{
			for (s32 x = 0; x < s_width; x++)
			{
				s_skyWarp[x] = x * SKY_HALF_WIDTH / (s32)s_halfWidth;
			}
		}
	}
	
	void drawSky(s32 x, s32 y0, s32 y1, s32 offsetY, const TextureFrame* tex, bool isWall)
	{
		const s32 u = (s_skyAngle + s_skyWarp[x]) & (tex->width - 1);
		const u8* image = &tex->image[u*tex->height];

		s_renderer->drawSkyColumn(x, y0, y1, (s32)s_halfHeight, offsetY + s_pitchSkyOffset, image, tex->height);
	}

	struct SpriteSegmentToSort
	{
		f32 x0;
		f32 x1;
		f32 y0;
		f32 y1;
		f32 z;
		s16 lightLevel;
		u16 type;
		union
		{
			const Frame* frame;
			const Model* model;
		};
		const GameObject* obj;
	};
	static SpriteSegmentToSort s_segToSort[1024];
	static u32 s_segIndices[1024];
	static u32 s_segInSector;

	s32 segSpriteSort(const void* a, const void* b)
	{
		const u32 i0 = *(u32*)a;
		const u32 i1 = *(u32*)b;
		const SpriteSegmentToSort* seg0 = &s_segToSort[i0];
		const SpriteSegmentToSort* seg1 = &s_segToSort[i1];

		if (seg0->z < seg1->z) { return -1; }
		else if (seg0->z > seg1->z) { return 1; }
		return 0;
	}

	s32 computeSpriteAngle(f32 vx, f32 vz, f32 yaw)
	{
		static const f32 c_angleOffset = 90.0f;
		static const f32 c_angleScale = 1.0f / 360.0f;
		static const f32 c_atanScale = 1.0f / (2.0f * PI);
		// Convert from angles to unit, adjust for angle offset.
		yaw = (yaw + c_angleOffset) * c_angleScale;

		// Is this too expensive? Dark Forces did this using a table, similar to Doom and Build.
		const f32 angle = atan2f(vz, vx) * c_atanScale;
		return s32((angle + yaw) * 32.0f) & 31;
	}

	void addObjectsToView(s32 sectorId, const Vec3f* cameraPos, const s32* win, f32 ca, f32 sa)
	{
		// Clip objects against the segment buffer and add.
		const s32 ambient = s_level->sectors[sectorId].ambient;
		const GameObject* objects = s_objects->data();
		const u32* objIndices = (*s_sectorObjects)[sectorId].list.data();
		const u32 objectCount = (u32)(*s_sectorObjects)[sectorId].list.size();
				
		s_segInSector = 0;
		for (u32 o = 0; o < objectCount; o++)
		{
			const GameObject* obj = &objects[objIndices[o]];
			if (!obj->show) { continue; }

			// Compute the view position.
			const f32 wx = obj->pos.x - cameraPos->x;
			const f32 wy = obj->pos.y - cameraPos->y;
			const f32 wz = obj->pos.z - cameraPos->z;
			const f32 vx =  wx * ca + wz * sa;
			const f32 vz = -wx * sa + wz * ca;

			f32 x0, x1, y0, y1;
			const Frame* frame = nullptr;
			const Model* model = nullptr;
			if ((obj->oclass == CLASS_SPRITE && obj->sprite) || (obj->oclass == CLASS_FRAME && obj->frame))
			{
				// If the sprite is behind the camera, skip.
				if (vz < 1.0f) { continue; }

				// Project
				const f32 rz = 1.0f / vz;
				const f32 scrX = s_halfWidth   + vx * s_focalLength * rz;
				const f32 scrY = s_pitchOffset + wy * s_heightScale * rz;

				// Get the current frame and world dimensions.
				f32 wWidth = 1.0f, wHeight = 1.0f;
				if (obj->oclass == CLASS_SPRITE && obj->sprite)
				{
					const s32 angle = computeSpriteAngle(wx, wz, obj->angles.y);
					const SpriteAngle* spriteAngle = &obj->sprite->anim[obj->animId].angles[angle];

					const u32 frameIndex = spriteAngle->frameIndex[obj->frameIndex % spriteAngle->frameCount];
					frame = &obj->sprite->frames[frameIndex];
					wWidth  = f32(obj->sprite->anim[obj->animId].worldWidth)  * c_spriteWorldScale;
					wHeight = f32(obj->sprite->anim[obj->animId].worldHeight) * c_spriteWorldScale;
				}
				else if (obj->oclass == CLASS_FRAME && obj->frame)
				{
					frame = obj->frame;
				}
				if (!frame) { continue; }

				// Convert world width to screen width based on depth and projection parameters.
				wWidth  *= s_focalLength * rz;
				wHeight *= s_heightScale * rz;

				// Compute the x and y range in screenspace.
				x0 = scrX + wWidth  * frame->rect[0] * c_spriteTexelToWorldScale;
				x1 = scrX + wWidth  * frame->rect[2] * c_spriteTexelToWorldScale;
				y0 = scrY - wHeight * frame->rect[3] * c_spriteTexelToWorldScale;
				y1 = scrY - wHeight * frame->rect[1] * c_spriteTexelToWorldScale;
			}
			else if (obj->oclass == CLASS_3D && obj->model)
			{
				model = obj->model;

				Vec2i screenRect[2];
				if (!DXL2_ModelRender::computeScreenSize(model, &obj->angles, &obj->pos, cameraPos, s_rot, (f32)s_pitchOffset, screenRect))
				{
					continue;
				}
				x0 = (f32)screenRect[0].x;
				y0 = (f32)screenRect[0].z;
				x1 = (f32)screenRect[1].x;
				y1 = (f32)screenRect[1].z;
			}
			if (!frame && !model) { continue; }

			// Cull sprites that do not overlap the view window.
			if (s32(x1) < win[0] || s32(x0) > win[1] || y0 >= s_height || y1 < 0) { continue; }

			// Compute the light level.
			const s32 lightLevel = obj->fullbright ? 31 : DXL2_RenderCommon::lightFalloff(ambient, vz);

			// Store the sprite segment to be clipped after sorting.
			const u32 segIndex = s_segInSector;
			SpriteSegmentToSort* seg = &s_segToSort[s_segInSector];
			s_segInSector++;

			seg->x0 = x0;
			seg->x1 = x1;
			seg->y0 = y0;
			seg->y1 = y1;
			seg->z  = vz;
			seg->obj = obj;
			seg->lightLevel = lightLevel;
			if (frame) { seg->type = 0; seg->frame = frame; }
			else { seg->type = 1; seg->model = model; }
			// Prime the index list for later sorting.
			s_segIndices[segIndex] = segIndex;
		}

		// Sort from front to back based on Z.
		std::qsort(s_segIndices, s_segInSector, sizeof(u32), segSpriteSort);

		// Clip sprite segments against wall segments and add them to the view list (if visible).
		for (u32 i = 0; i < s_segInSector; i++)
		{
			const SpriteSegmentToSort* seg = &s_segToSort[s_segIndices[i]];
			clipSpriteToSegments(seg->x0, seg->x1, seg->y0, seg->y1, seg->z, seg->lightLevel, seg->type, seg->type == 0 ? (void*)seg->frame : (void*)seg->model, seg->obj, win);
		}
	}

	void update(const Vec3f* cameraPos, f32 yaw, f32 pitch, s32 sectorId, LightMode lightMode)
	{
		s_viewStats.pos = *cameraPos;
		s_viewStats.yaw = yaw;
		s_viewStats.pitch = pitch;
		s_viewStats.sectorId = sectorId;

		s_frame++;
		DXL2_RenderCommon::enableLightSource(lightMode);

		// Camera data should be computed once per frame.
		s_rot[0] = cosf(yaw);
		s_rot[1] = sinf(yaw);

		setupSky(yaw);
		s_time += DXL2_System::getDeltaTime();

		s_pitchSkyOffset = s32(-pitch / (2.0f * PI) * s_level->parallax[1]);
		s_pitchOffset = s32(s_halfHeight * pitch * PITCH_PIXEL_SCALE / PITCH_LIMIT) + (s32)s_halfHeight;
		DXL2_RenderCommon::setProjectionParam(s_width, s_height, s_focalLength, s_heightScale, s_pitchOffset);
		DXL2_RenderCommon::setViewParam(s_rot, cameraPos);
		DXL2_ModelRender::buildClipPlanes(s_pitchOffset);
	}
		
	void draw(const Vec3f* cameraPos, s32 sectorId)
	{
		const Sector* sectors = s_level->sectors.data();
		const SectorWall* walls = s_level->walls.data();
		const Vec2f* vertices = s_level->vertices.data();
		clearHeightArray();
		s_stackRead = 0;
		s_stackWrite = 0;
		s_sectorStack[s_stackWrite++] = { sectorId, -1, -1, {0, s_width - 1} };
		s_viewStats = { 0 };
		
		s32 iter = 0;
		s_wallCount = 0;
		s_segmentCount = 0;
		s_maskWallCount = 0;
		s_maskHeightOffset = 0;
		s_viewObjectCount = 0;

		clearSpriteSegments();

		while (s_stackRead < s_stackWrite && iter < s_iterMax)
		{
			if (s_enableViewStats)
			{
				s32 depth = s_stackWrite - s_stackRead;
				s_viewStats.maxTraversalDepth = std::max(s_viewStats.maxTraversalDepth, depth);
			}

			sectorId = s_sectorStack[s_stackRead].sectorId;
			const s32 win[] = { s_sectorStack[s_stackRead].win[0], s_sectorStack[s_stackRead].win[1] };
			s_prevSectorId = s_sectorStack[s_stackRead].prevSectorId;
			s_windowId = s_sectorStack[s_stackRead].windowId;
			s_stackRead++;
			
			// For now just draw the current sector.
			const Sector* curSector = &sectors[sectorId];
			const u32 wallCount = curSector->wallCount;
			const u32 vtxCount = curSector->vtxCount;
			const SectorWall* curWall = walls + curSector->wallOffset;
			const Vec2f* curVtx = vertices + curSector->vtxOffset;

			s_transformedVtx.resize(vtxCount);
			Vec2f* transformed = s_transformedVtx.data();

			// 1. Transform vertices.
			for (u32 v = 0; v < vtxCount; v++)
			{
				const f32 x = curVtx[v].x - cameraPos->x;
				const f32 z = curVtx[v].z - cameraPos->z;
				transformed[v].x =  x * s_rot[0] + z * s_rot[1];
				transformed[v].z = -x * s_rot[1] + z * s_rot[0];
			}

			// 2. Gather visible segments.
			bool wallsVis = false;
			for (u32 w = 0; w < wallCount; w++, curWall++)
			{
				// View space wall vertices.
				Vec2f v0 = transformed[curWall->i0];
				Vec2f v1 = transformed[curWall->i1];
				
				f32 u0 = 0.0f;
				f32 u1 = u0 + wallLength(curWall, curSector);

				// Clip wall to the view frustum, cull the wall if it is outside the frustum.
				if (!clipLines(&v0, &v1, &u0, &u1)) { continue; }

				// Project the vertices.
				const f32 rz0 = 1.0f / v0.z;
				const f32 rz1 = 1.0f / v1.z;
				const f32 x0 = s_halfWidth + v0.x*s_focalLength * rz0;
				const f32 x1 = s_halfWidth + v1.x*s_focalLength * rz1;

				// Cull walls that are backfacing.
				if (x0 >= x1) { continue; }
				// Cull walls that do not overlap the view window.
				if (s32(x1) < win[0] || s32(x0) > win[1]) { continue; }
				// Avoid overflowing the wall buffer.
				if (s_wallCount >= WALL_MAX) { assert(0); continue; }

				// Store values required for perspective correct depth interpolation.
				const u32 wallIndex = s_wallCount;
				s_drawWalls[s_wallCount].ox0 = x0;
				s_drawWalls[s_wallCount].odx = 1.0f / (x1 - x0);
				s_drawWalls[s_wallCount].rz0 = rz0;
				s_drawWalls[s_wallCount].rz1 = rz1;
				// Then store the original wall info, required to draw it later.
				s_drawWalls[s_wallCount].wall = s16(w);
				s_drawWalls[s_wallCount].sectorId = sectorId;
				s_drawWalls[s_wallCount].v0 = v0;
				s_drawWalls[s_wallCount].v1 = v1;
				s_drawWalls[s_wallCount].u0 = u0;
				s_drawWalls[s_wallCount].u1 = u1;

				// Check to see if this is a portal, which means it has an adjoin and it is possible
				// to see into the next sector.
				bool portal = false;
				if (curWall->adjoin >= 0 && curWall->adjoin != sectorId && (curWall->adjoin != s_prevSectorId || curWall->mirror != s_windowId))
				{
					const Sector* next = &sectors[curWall->adjoin];
					f32 h1 = std::min(curSector->floorAlt, next->floorAlt);
					f32 h0 = std::max(curSector->ceilAlt, next->ceilAlt);

					bool skyAdjoin = false, pitAdjoin = false;
					if ((curSector->flags[0] & SEC_FLAGS1_EXTERIOR) && (next->flags[0] & SEC_FLAGS1_EXTERIOR))
					{
						skyAdjoin = true;
					}
					if ((curSector->flags[0] & SEC_FLAGS1_PIT) && (next->flags[0] & SEC_FLAGS1_PIT))
					{
						pitAdjoin = true;
					}
					if (skyAdjoin) { h0 -= 100.0f; }
					if (pitAdjoin) { h1 += 100.0f; }

					if (h1 - h0 > 0.0f) { portal = true; }
				}

				// Add the wall segment to the S-buffer.
				if (addSegment(x0, x1, wallIndex, win, portal))
				{
					s_wallCount++;
					// A new segment has been added, so drawSegs() needs to be called.
					wallsVis = true;
				}
			}

			// Merge segments before adding sprites or drawing.
			mergeSegs(win);

			// 3. Add sprites and 3d objects to the view from the sector + view.
			// These sprites will be rendered later.
			addObjectsToView(sectorId, cameraPos, win, s_rot[0], s_rot[1]);

			// 4. Draw visible segments.
			if (wallsVis && s_segmentCount)
			{
				drawSegs(cameraPos, win);
			}

			iter++;
		}
		if (s_enableViewStats)
		{
			s_viewStats.iterCount = iter;
		}

		drawViewObjects(cameraPos);
	}
}
