#include "physics.h"
#include "geometry.h"
#include "gameObject.h"
#include <DXL2_System/math.h>
#include <DXL2_LogicSystem/logicSystem.h>
#include <DXL2_InfSystem/infSystem.h>
#include <assert.h>
#include <algorithm>

namespace DXL2_Physics
{
	#define MOVE_ITER_END 4
	#define HEIGHT_EPS 0.5f
	const f32 c_stepSize = 3.5f;
	const f32 c_minHeight = 3.0f;
	const f32 c_minStandHeight = 6.80f;

	static LevelData* s_level = nullptr;
	// Transient local state.
	static u32 s_overlapSectorCnt;
	static s32 s_searchIndex;
	static f32 s_height;
	static const Sector* s_overlapSectors[256];
	static const Sector* s_searchSectors[256];
	
	bool canCrossWall(f32 yPos, const SectorWall* wall);

	bool init(LevelData* level)
	{
		s_level = level;
		s_overlapSectorCnt = 0;
		s_searchIndex = 0;
		s_height = 0.0f;

		return (level != nullptr);
	}

	void shutdown()
	{
	}

	bool addSectorToSearchList(const Sector* sector)
	{
		for (u32 s = 0; s < s_overlapSectorCnt; s++)
		{
			if (sector == s_overlapSectors[s]) { return false; }
		}
		s_overlapSectors[s_overlapSectorCnt++] = sector;
		return true;
	}

	s32 getCurrentSectorId(const Vec3f* pos)
	{
		Vec2f posXZ = { pos->x, pos->z };
		for (u32 s = 0; s < s_overlapSectorCnt; s++)
		{
			const Sector* sector = s_overlapSectors[s];
			if (Geometry::pointInSector(&posXZ, sector->vtxCount, s_level->vertices.data() + sector->vtxOffset, sector->wallCount, s_level->walls.data() + sector->wallOffset))
			{
				return sector->id;
			}
		}

		return -1;
	}

	bool tryMove(const Vec3f* newPos, s32 sectorId, f32 radius)
	{
		const Sector* sectors = s_level->sectors.data();
		const Sector* curSector = sectors + sectorId;

		const SectorWall* walls = s_level->walls.data() + curSector->wallOffset;
		const Vec2f* vertices = s_level->vertices.data() + curSector->vtxOffset;

		const u32 wallCount = curSector->wallCount;
		Vec2f newPosXZ = { newPos->x, newPos->z };

		// First see if this is still in the current sector.
		if (!Geometry::pointInSector(&newPosXZ, curSector->vtxCount, vertices, wallCount, walls))
		{
			return false;
		}

		// If it is the same sector, check collision against objects and walls.
		const f32 rSq = radius * radius + FLT_EPSILON;
		for (u32 w = 0; w < wallCount; w++)
		{
			Vec2f point;
			Geometry::closestPointOnLineSegment(vertices[walls[w].i0], vertices[walls[w].i1], newPosXZ, &point);
			Vec2f offset = { point.x - newPosXZ.x, point.z - newPosXZ.z };
			f32 distSq = DXL2_Math::dot(&offset, &offset);
			if (distSq < rSq)
			{
				return false;
			}
		}

		// Then check collision against objects.
		const GameObjectList* objectList = LevelGameObjects::getGameObjectList();
		const GameObject* object = objectList->data();
		const SectorObjectList* secObjs = LevelGameObjects::getSectorObjectList();
		const size_t objCount = (*secObjs)[sectorId].list.size();
		const u32* indices = (*secObjs)[sectorId].list.data();

		bool collided = false;
		for (size_t i = 0; i < objCount; i++)
		{
			const GameObject* curObj = &object[indices[i]];
			if (curObj->collisionFlags == COLLIDE_NONE || curObj->collisionRadius <= 0.0f || curObj->collisionHeight <= 0.0f)
			{
				continue;
			}
			
			if (curObj->collisionFlags & COLLIDE_PLAYER)
			{
				// overlap collision
				const Vec2f objPosXZ = { curObj->pos.x, curObj->pos.z };
				if (DXL2_Math::distanceSq(&newPosXZ, &objPosXZ) < rSq + curObj->collisionRadius*curObj->collisionRadius)
				{
					// do heights overlap?
					f32 h0[] = { newPos->y, newPos->y - 6.0f };
					f32 h1[] = { curObj->pos.y, curObj->pos.y - curObj->collisionHeight };
					if (h0[0] > h1[1] && h1[0] > h0[1])
					{
						if (curObj->collisionFlags & COLLIDE_TRIGGER)
						{
							DXL2_LogicSystem::sendPlayerCollisionTrigger(curObj);
						}
						else
						{
							collided = true;
						}
					}
				}
			}
		}

		return !collided;
	}

	bool tryMoveSecList(const Vec3f* newPos, f32 radius)
	{
		const Vec2f newPosXZ = { newPos->x, newPos->z };

		// First see if this is still in any sector in the list.
		bool foundInSector = false;
		for (u32 s = 0; s < s_overlapSectorCnt; s++)
		{
			const Sector* curSector = s_overlapSectors[s];
			if (Geometry::pointInSector(&newPosXZ, curSector->vtxCount, s_level->vertices.data() + curSector->vtxOffset, curSector->wallCount, s_level->walls.data() + curSector->wallOffset))
			{
				foundInSector = true;
				break;
			}
		}
		if (!foundInSector) { return false; }

		bool collided = false;
		for (u32 s = 0; s < s_overlapSectorCnt; s++)
		{
			const Sector* curSector = s_overlapSectors[s];
			const SectorWall* walls = s_level->walls.data() + curSector->wallOffset;
			const Vec2f* vertices = s_level->vertices.data() + curSector->vtxOffset;
			const u32 wallCount = curSector->wallCount;

			// If it is the same sector, check collision against objects and walls.
			const f32 rSq = radius * radius + FLT_EPSILON;
			for (u32 w = 0; w < wallCount; w++)
			{
				if (canCrossWall(newPos->y, &walls[w])) { continue; }

				Vec2f point;
				Geometry::closestPointOnLineSegment(vertices[walls[w].i0], vertices[walls[w].i1], newPosXZ, &point);
				Vec2f offset = { point.x - newPosXZ.x, point.z - newPosXZ.z };
				f32 distSq = DXL2_Math::dot(&offset, &offset);
				if (distSq < rSq)
				{
					return false;
				}
			}

			// Then check collision against objects.
			const GameObjectList* objectList = LevelGameObjects::getGameObjectList();
			const GameObject* object = objectList->data();
			const SectorObjectList* secObjs = LevelGameObjects::getSectorObjectList();
			const size_t objCount = (*secObjs)[curSector->id].list.size();
			const u32* indices = (*secObjs)[curSector->id].list.data();

			for (size_t i = 0; i < objCount; i++)
			{
				const GameObject* curObj = &object[indices[i]];
				if (curObj->collisionFlags == COLLIDE_NONE || curObj->collisionRadius <= 0.0f || curObj->collisionHeight <= 0.0f)
				{
					continue;
				}

				if (curObj->collisionFlags & COLLIDE_PLAYER)
				{
					// overlap collision
					const Vec2f objPosXZ = { curObj->pos.x, curObj->pos.z };
					if (DXL2_Math::distanceSq(&newPosXZ, &objPosXZ) < rSq + curObj->collisionRadius*curObj->collisionRadius)
					{
						// do heights overlap?
						f32 h0[] = { newPos->y, newPos->y - 6.0f };
						f32 h1[] = { curObj->pos.y, curObj->pos.y - curObj->collisionHeight };
						if (h0[0] > h1[1] && h1[0] > h0[1])
						{
							if (curObj->collisionFlags & COLLIDE_TRIGGER)
							{
								DXL2_LogicSystem::sendPlayerCollisionTrigger(curObj);
							}
							else
							{
								collided = true;
							}
						}
					}
				}
			}
		}

		return !collided;
	}
		
	bool canCrossWall(f32 yPos, const SectorWall* wall)
	{
		if (wall->adjoin < 0 || (wall->flags[2]&WF3_SOLID_WALL)) { return false; }
		const Sector* nextSector = s_level->sectors.data() + wall->adjoin;
		const f32 floorHeight = (yPos < nextSector->floorAlt + nextSector->secAlt + 0.01f) || nextSector->secAlt > 0.0f ? nextSector->floorAlt + nextSector->secAlt : nextSector->floorAlt;
		const f32 ceilHeight  = nextSector->secAlt < 0.0f && yPos > nextSector->floorAlt + nextSector->secAlt ? nextSector->floorAlt + nextSector->secAlt : nextSector->ceilAlt;

		// can it fit?
		if (std::min(floorHeight, yPos) - ceilHeight < s_height)
		{
			return false;
		}

		// ledge
		const bool canStep = (yPos - floorHeight <= c_stepSize);
		if (!canStep && !(wall->flags[2]&WF3_ALWAYS_WALK))
		{
			return false;
		}
				
		return true;
	}
	
	void gatherAllOverlappingSectors(const Sector* start, const Vec3f* pos, f32 radius, bool incBlockedAdjoins)
	{
		// If not successful, try a "slide move"
		const Sector* sectors = s_level->sectors.data();
		s_searchIndex = 0;
		for (u32 s = 0; s < s_overlapSectorCnt; s++)
		{
			s_searchSectors[s_searchIndex++] = s_overlapSectors[s];
		}
				
		const Vec2f posXZ = { pos->x, pos->z };
		const f32 rSq = radius * radius + FLT_EPSILON;
		while (s_searchIndex)
		{
			s_searchIndex--;
			const Sector* curSector = s_searchSectors[s_searchIndex];
			const SectorWall* walls = s_level->walls.data() + curSector->wallOffset;
			const Vec2f* vertices = s_level->vertices.data() + curSector->vtxOffset;

			const u32 wallCount = curSector->wallCount;
			for (u32 w = 0; w < wallCount; w++)
			{
				bool passable = canCrossWall(pos->y, &walls[w]);
				if ((!incBlockedAdjoins && !passable) || walls[w].adjoin < 0) { continue; }

				Vec2f point;
				Geometry::closestPointOnLineSegment(vertices[walls[w].i0], vertices[walls[w].i1], posXZ, &point);
				const Vec2f offset = { point.x - posXZ.x, point.z - posXZ.z };
				const f32 distSq = DXL2_Math::dot(&offset, &offset);
				if (distSq >= rSq) { continue; }

				const Sector* nextSector = s_level->sectors.data() + walls[w].adjoin;
				if (addSectorToSearchList(nextSector))
				{
					if (passable) { s_searchSectors[s_searchIndex++] = nextSector; }
				}
			}
		}
	}

	u32 getOverlappingLines(const Vec3f* pos, f32 radius, bool includePassable, u32* lines, u32 maxCount)
	{
		// Get all of the walls that overlap the radius, regardless of the type.
		const Vec2f posXZ = { pos->x, pos->z };
		const f32 rSq = radius * radius;
		u32 lineCount = 0;
		for (u32 s = 0; s < s_overlapSectorCnt; s++)
		{
			const Sector* curSector = s_overlapSectors[s];
			const SectorWall* walls = s_level->walls.data() + curSector->wallOffset;
			const Vec2f* vertices = s_level->vertices.data() + curSector->vtxOffset;

			const u32 wallCount = curSector->wallCount;
			for (u32 w = 0; w < wallCount; w++)
			{
				Vec2f point;
				Geometry::closestPointOnLineSegment(vertices[walls[w].i0], vertices[walls[w].i1], posXZ, &point);
				const Vec2f offset = { point.x - posXZ.x, point.z - posXZ.z };
				const f32 distSq = DXL2_Math::dot(&offset, &offset);
				if (distSq >= rSq) { continue; }

				// If includePassable = false then do not include any passable walls.
				if (!includePassable && canCrossWall(pos->y, &walls[w])) { continue; }

				lines[lineCount++] = curSector->id | (w << 16u);
				if (lineCount >= maxCount) { return lineCount; }
			}
		}
		return lineCount;
	}

	const Sector** getOverlappingSectors(const Vec3f* pos, s32 curSectorId, f32 radius, u32* outSectorCount)
	{
		const Sector* curSector = s_level->sectors.data() + curSectorId;
		s_overlapSectorCnt = 0;
		s_overlapSectors[s_overlapSectorCnt++] = curSector;

		gatherAllOverlappingSectors(curSector, pos, radius, true);
		*outSectorCount = s_overlapSectorCnt;
		return s_overlapSectors;
	}

	void getOverlappingLinesAndSectors(const Vec3f* pos, s32 curSectorId, f32 radius, u32 maxCount, u32* lines, u32* sectors, u32* outLineCount, u32* outSectorCount)
	{
		const Sector* curSector = s_level->sectors.data() + curSectorId;
		s_overlapSectorCnt = 0;
		s_overlapSectors[s_overlapSectorCnt++] = curSector;

		// Then go through all of the adjoins and add to the sector list.
		gatherAllOverlappingSectors(curSector, pos, radius, true);
		*outSectorCount = std::min(s_overlapSectorCnt, maxCount);
		for (u32 i = 0; i < s_overlapSectorCnt && i < maxCount; i++)
		{
			sectors[i] = s_overlapSectors[i]->id;
		}

		*outLineCount = getOverlappingLines(pos, radius, true, lines, maxCount);
	}

	void getValidHeightRange(const Vec3f* pos, s32 curSectorId, f32* floorHeight, f32* visualFloorHeight, f32* ceilHeight)
	{
		const f32 radius = c_playerRadius;	// Tuned based on texture width, so might need more tuning if FOV changes at all.
		const Sector* curSector = s_level->sectors.data() + curSectorId;
		s_overlapSectorCnt = 0;
		s_overlapSectors[s_overlapSectorCnt++] = curSector;

		// Then go through all of the adjoins and add to the sector list.
		gatherAllOverlappingSectors(curSector, pos, radius, false);

		// Get the minimum floor height.
		f32 minHeight = FLT_MAX;
		for (u32 s = 0; s < s_overlapSectorCnt; s++)
		{
			if (pos->y < s_overlapSectors[s]->floorAlt + s_overlapSectors[s]->secAlt + 0.01f || s_overlapSectors[s]->secAlt > 0.0f)
			{
				minHeight = std::min(s_overlapSectors[s]->floorAlt + s_overlapSectors[s]->secAlt, minHeight);
			}
			else
			{
				minHeight = std::min(s_overlapSectors[s]->floorAlt, minHeight);
			}
		}

		// Get the minimum VISUAL floor height
		f32 minHeightVis = FLT_MAX;
		for (u32 s = 0; s < s_overlapSectorCnt; s++)
		{
			if (pos->y < s_overlapSectors[s]->floorAlt + s_overlapSectors[s]->secAlt + 0.01f && s_overlapSectors[s]->secAlt <= 0.0f)
			{
				minHeightVis = std::min(s_overlapSectors[s]->floorAlt + s_overlapSectors[s]->secAlt, minHeightVis);
			}
			else
			{
				minHeightVis = std::min(s_overlapSectors[s]->floorAlt, minHeightVis);
			}
		}

		// Get the minimum floor height.
		f32 maxHeight = -FLT_MAX;
		for (u32 s = 0; s < s_overlapSectorCnt; s++)
		{
			if (s_overlapSectors[s]->secAlt < 0.0f && pos->y > s_overlapSectors[s]->floorAlt + s_overlapSectors[s]->secAlt)
			{
				maxHeight = std::max(s_overlapSectors[s]->floorAlt + s_overlapSectors[s]->secAlt, maxHeight);
			}
			else
			{
				maxHeight = std::max(s_overlapSectors[s]->ceilAlt, maxHeight);
			}
		}

		*floorHeight = minHeight;
		*visualFloorHeight = minHeightVis;
		*ceilHeight = maxHeight;
	}

	void checkWallCrossing(const Vec3f* prevPos, const Vec3f* curPos, f32 radius, s32 curSectorId)
	{
		if (curSectorId < 0) { return; }

		const f32 diff = DXL2_Math::distance(prevPos, curPos);
		Sector* curSector = s_level->sectors.data() + curSectorId;

		s_overlapSectorCnt = 0;
		s_overlapSectors[s_overlapSectorCnt++] = curSector;
		gatherAllOverlappingSectors(curSector, curPos, radius, false);
		u32 lines[256];
		u32 lineCount = getOverlappingLines(curPos, radius + diff * 0.5f, true, lines, 256);

		for (u32 l = 0; l < lineCount; l++)
		{
			// Get sector and wallId
			u32 sectorId = lines[l] & 0xffffu;
			u32 wallId = lines[l] >> 16u;
			const Sector* sector = s_level->sectors.data() + sectorId;
			const Vec2f* vertices = s_level->vertices.data() + sector->vtxOffset;
			const SectorWall* wall = s_level->walls.data() + wallId + sector->wallOffset;
			if (wall->adjoin < 0) { continue; }

			const Vec2f v0 = vertices[wall->i0];
			const Vec2f v1 = vertices[wall->i1];
			Vec2f dir = { v1.x - v0.x, v1.z - v0.z };
			// Normal is reversed due to winding order. This way normals point "in" towards the collision.
			Vec2f nrm = { dir.z, -dir.x };

			// has the player crossed?
			Vec2f relPrev = { prevPos->x - v0.x, prevPos->z - v0.z };
			Vec2f relCur = { curPos->x - v0.x, curPos->z - v0.z };
			f32 distPrev = DXL2_Math::dot(&relPrev, &nrm);
			f32 distCur = DXL2_Math::dot(&relCur, &nrm);
			if (DXL2_Math::sign(distPrev) != DXL2_Math::sign(distCur))
			{
				// The player has crossed this wall.
				DXL2_InfSystem::firePlayerEvent(distPrev >= 0.0f ? INF_EVENT_CROSS_LINE_FRONT : INF_EVENT_CROSS_LINE_BACK, sectorId, wallId);
			}
		}
	}

	bool correctPosition(Vec3f* pos, s32* curSectorId, f32 radius, bool sendInfMsg)
	{
		bool correctionNeeded = false;
		bool invalidSector = false;
		const Sector* curSector = s_level->sectors.data() + (*curSectorId);
		
		// Make sure the sectorId itself is valid first.
		const Vec2f posXZ = { pos->x, pos->z };
		if (!Geometry::pointInSector(&posXZ, curSector->vtxCount, s_level->vertices.data() + curSector->vtxOffset, curSector->wallCount, s_level->walls.data() + curSector->wallOffset))
		{
			correctionNeeded = true;
			invalidSector = true;
		}

		// Then go through all of the adjoins and add to the sector list.
		s_overlapSectorCnt = 0;
		s_overlapSectors[s_overlapSectorCnt++] = curSector;
		gatherAllOverlappingSectors(curSector, pos, radius, false);

		u32 lines[256];
		u32 lineCount = getOverlappingLines(pos, radius, false, lines, 256);

		// First try to move and see if there are potential collisions.
		correctionNeeded |= (!tryMoveSecList(pos, radius));
		if (!correctionNeeded) { return false; }

		Vec3f prevPos = *pos;
		s32 prevSector = *curSectorId;

		// Apply corrections by pushing the position away from walls as little as possible to get a new valid position.
		s32 iter = 0;
		f32 rSq = radius * radius + FLT_EPSILON;
		while (correctionNeeded && lineCount && iter <= MOVE_ITER_END)
		{
			// First push away from walls.
			for (u32 l = 0; l < lineCount; l++)
			{
				// Get sector and wallId
				u32 sectorId = lines[l] & 0xffffu;
				u32 wallId = lines[l] >> 16u;
				const Sector* sector = s_level->sectors.data() + sectorId;
				const Vec2f* vertices = s_level->vertices.data() + sector->vtxOffset;
				const SectorWall* wall = s_level->walls.data() + wallId + sector->wallOffset;

				const Vec2f v0 = vertices[wall->i0];
				const Vec2f v1 = vertices[wall->i1];
				// Get the distance from the wall.
				Vec2f point;
				Vec2f posXZ = { pos->x, pos->z };
				Geometry::closestPointOnLineSegment(v0, v1, posXZ, &point);
				Vec2f offset = { point.x - posXZ.x, point.z - posXZ.z };
				const f32 distSq = DXL2_Math::dot(&offset, &offset);
				if (distSq >= rSq) { continue; }
				// Get the "push" normal - current position -> closestPoint
				Vec2f nrm = { pos->x - point.x, pos->z - point.z };
				nrm = DXL2_Math::normalize(&nrm);
				// Now push out.
				f32 overlap = radius - sqrtf(distSq);
				f32 pushDist = std::max(0.0f, overlap) + 0.0001f;
				pos->x += nrm.x * pushDist;
				pos->z += nrm.z * pushDist;
			}

			// Is this good enough?
			correctionNeeded = !tryMoveSecList(pos, radius);
			// Update the overlapping lines if needed
			if (correctionNeeded) { lineCount = getOverlappingLines(pos, radius, false, lines, 256); }
			iter++;
		}

		// Now find out the correct sector.
		*curSectorId = getCurrentSectorId(pos);
		assert(*curSectorId >= 0);

		// Finally check to see if the player has crossed any walls (adjoins).
		if (sendInfMsg && (prevPos.x != pos->x || prevPos.z != pos->z || prevSector != *curSectorId))
		{
			checkWallCrossing(&prevPos, pos, radius, *curSectorId);
		}

		return true;
	}

	bool move(const Vec3f* startPoint, const Vec3f* desiredMove, s32 curSectorId, Vec3f* actualMove, s32* newSectorId, f32 height, bool sendInfMsg)
	{
		const f32 radius = c_playerRadius;
		s_height = fabsf(height);
		// If there is no movement then early out.
		if (desiredMove->x == 0.0f && desiredMove->z == 0.0f)
		{
			*actualMove = *desiredMove;
			*newSectorId = curSectorId;
			return true;
		}
				
		Vec3f startPos = *startPoint;
		Vec3f endPos = { startPoint->x + desiredMove->x, startPoint->y + desiredMove->y, startPoint->z + desiredMove->z };
		Vec3f move = *desiredMove;
		s32 iter = 0;
		s32 baseSectorId = curSectorId;

		// Try to move to the end position, if it works then no collision needed.
		// This will usually be the case.
		if (tryMove(&endPos, curSectorId, radius))
		{
			*actualMove = *desiredMove;
			*newSectorId = curSectorId;
			return true;
		}

		// Iterate as long as movement remains, sliding along walls and objects.
		// Note the iteration count is capped to avoid corner cases that can cause infinite loops (such as corners).
		const Sector* curSector = s_level->sectors.data() + curSectorId;
		s_overlapSectorCnt = 0;
		s_overlapSectors[s_overlapSectorCnt++] = curSector;

		const f32 rSq = radius * radius + FLT_EPSILON;
		while (iter <= MOVE_ITER_END)
		{
			// Then go through all of the adjoins and add to the sector list.
			gatherAllOverlappingSectors(curSector, &endPos, radius, false);

			// First try to move and see if there are potential collisions.
			if (tryMoveSecList(&endPos, radius))
			{
				break;
			}
			
			// Calculate the movement direction.
			Vec2f moveDirNrm = { endPos.x - startPos.x, endPos.z - startPos.z };
			moveDirNrm = DXL2_Math::normalize(&moveDirNrm);
						
			// find the wall with the least amount of overlap.
			f32 minIntersect = FLT_MAX;
			s32 minOverlapWall = -1;
			const Sector* minOverlapSector = nullptr;
			Vec2f minWallNrm = { 0 };
			Vec2f minWallPt = { 0 };

			f32 secMinIntersect = FLT_MAX;
			s32 secMinOverlapWall = -1;
			const Sector* secMinOverlapSector = nullptr;
			Vec2f secWallNrm = { 0 };
			Vec2f secWallPt = { 0 };

			const Vec2f newPosXZ = { endPos.x, endPos.z };
			for (u32 s = 0; s < s_overlapSectorCnt; s++)
			{
				// If not successful, try a "slide move"
				const Sector* curSector = s_overlapSectors[s];
				const SectorWall* walls = s_level->walls.data() + curSector->wallOffset;
				const Vec2f* vertices = s_level->vertices.data() + curSector->vtxOffset;
				const u32 wallCount = curSector->wallCount;
				
				for (u32 w = 0; w < wallCount; w++)
				{
					// Adjoins are ignored for collision.
					if (canCrossWall(endPos.y, &walls[w])) { continue; }

					const Vec2f v0 = vertices[walls[w].i0];
					const Vec2f v1 = vertices[walls[w].i1];

					Vec2f point;
					Geometry::closestPointOnLineSegment(v0, v1, newPosXZ, &point);
					Vec2f offset = { point.x - newPosXZ.x, point.z - newPosXZ.z };
					const f32 distSq = DXL2_Math::dot(&offset, &offset);
					if (distSq >= rSq) { continue; }
												
					Vec2f dir = { v1.x - v0.x, v1.z - v0.z };
					// Normal is reversed due to winding order. This way normals point "in" towards the collision.
					Vec2f nrm = { dir.z, -dir.x };
					offset = { startPos.x - v0.x, startPos.z - v0.z };
					// Ignore walls that have normals facing towards the direction of movement.
					f32 side = move.x*nrm.x + move.z*nrm.z;
					if (side > 0.0f) { continue; }

					// Determine if the cylinder overlaps the line. If not, then no need to consider it further.
					f32 sign = Geometry::isLeft(v0, v1, newPosXZ) > 0.0f ? -1.0f : 1.0f;
					f32 dist = sqrtf(distSq);
					f32 overlap = sign < 0.0f ? radius + dist : radius - dist;
					if (overlap <= 0.0f) { continue; }

					// We only care about edges that our path will intersect.
					nrm = DXL2_Math::normalize(&nrm);
					Vec2f leadingPoint = { startPos.x - nrm.x*radius, startPos.z - nrm.z*radius };
					Vec2f endingPoint = { leadingPoint.x + move.x, leadingPoint.z + move.z };
					f32 s = FLT_MAX, t = FLT_MAX;
					f32 intersect = FLT_MAX;
					if (Geometry::lineSegmentIntersect(&leadingPoint, &endingPoint, &v0, &v1, &s, &t))
					{
						intersect = s;
					}
					else if (s > -0.1f && s < 1.1f && t < FLT_MAX)
					{
						intersect = s;
					}

					// Keep track of the closest intersecting line and second closest - in order to handle corners.
					if (intersect < minIntersect)
					{
						secMinIntersect = minIntersect;
						secMinOverlapSector = minOverlapSector;
						secMinOverlapWall = minOverlapWall;
						secWallNrm = minWallNrm;

						minIntersect = intersect;
						minOverlapWall = s32(w);
						minOverlapSector = curSector;
						minWallNrm = nrm;
						minWallPt = { v0.x + t * (v1.x - v0.x), v0.z + t * (v1.z - v0.z) };
					}
					else if (intersect < secMinIntersect)
					{
						secMinIntersect = intersect;
						secMinOverlapWall = s32(w);
						secMinOverlapSector = curSector;
						secWallNrm = nrm;
						secWallPt = { v0.x + t * (v1.x - v0.x), v0.z + t * (v1.z - v0.z) };
					}
				}
			}

			if (minOverlapWall >= 0)
			{
				// Find the point where the path intersects the wall.
				const SectorWall* wall = s_level->walls.data() + minOverlapWall + minOverlapSector->wallOffset;
				const SectorWall* wall1 = secMinOverlapSector ? s_level->walls.data() + secMinOverlapWall + secMinOverlapSector->wallOffset : nullptr;
				Vec2f nrm = minWallNrm;

				// a small bias to avoid precision issues.
				f32 s = minIntersect - 0.001f;
				// 'dw' is used to further scale the sliding movement based on how acute the angle between walls is.
				// at the limit of 90 degrees or less, the slide is nullified.
				f32 dw = secMinOverlapWall >= 0 ? DXL2_Math::dot(&minWallNrm, &secWallNrm) : 1.0f;
				// If 'dw' is less than zero, then this is not a corner, so remove any scaling.
				if (dw < 0.0f) { dw = 1.0f; }

				// we have moved up to the wall and should finish off movement by sliding.
				Vec3f movedPos = { startPos.x + move.x*s, startPos.y + move.y*s, startPos.z + move.z*s };
				Vec2f slideNrm = { movedPos.x - minWallPt.x, movedPos.z - minWallPt.z };
				slideNrm = DXL2_Math::normalize(&slideNrm);

				// Verify that moving up to the wall is valid.
				if (!tryMoveSecList(&movedPos, radius))
				{
					// If not, we may be able to salvage the situation by pushin away from the wall a little bit
					// and trying again. If this fails, then abort.
					const f32 push = 0.01f;
					const Vec3f pushedPos = { movedPos.x + nrm.x*push, movedPos.y, movedPos.z + nrm.z*push };
					if (tryMoveSecList(&pushedPos, radius))
					{
						movedPos = pushedPos;
					}
					else
					{
						endPos = startPos;
						break;
					}
				}
				startPos = movedPos;

				// scale based on the amount of move left over.
				// remove a small amount of the movement so we constantly lose velocity each iteration.
				const f32 moveScale = std::max(0.0f, (1.0f - s) - 0.0001f) * 0.99f * dw;
				move.x *= moveScale;
				move.z *= moveScale;
				// project the movement onto the wall normal to get the tangental movement vector.
				const f32 proj = move.x*slideNrm.x + move.z*slideNrm.z;
				move.x = move.x - proj * slideNrm.x;
				move.z = move.z - proj * slideNrm.z;

				// If the amount of movement is really small then we're done.
				if (fabsf(move.x) + fabsf(move.z) < 0.00001f)
				{
					endPos = startPos;
					break;
				}

				// Move the new end position based on the sliding movement.
				// Collision from startPos -> endPos will continue next iteration.
				endPos.x = startPos.x + move.x;
				endPos.z = startPos.z + move.z;

				if (iter == MOVE_ITER_END)
				{
					// This is the last iteration, try moving to the end and give up if we can't.
					if (!tryMoveSecList(&endPos, radius))
					{
						endPos = startPos;
					}
					break;
				}

				iter++;
				continue;
			}
			else if (!tryMoveSecList(&endPos, radius))
			{
				// There was no collision but the end position isn't valid, so we have no choice but to abort.
				endPos = startPos;
			}
			break;
		}

		*actualMove = { endPos.x - startPoint->x, endPos.y - startPoint->y, endPos.z - startPoint->z };
		*newSectorId = getCurrentSectorId(&endPos);

		// Finally check to see if the player has crossed any walls (adjoins).
		if (sendInfMsg && (startPoint->x != endPos.x || startPoint->z != endPos.z || baseSectorId != *newSectorId))
		{
			checkWallCrossing(startPoint, &endPos, radius, *newSectorId);
		}

		return (endPos.x != startPoint->x || endPos.z != startPoint->z);
	}

	s32 findClosestWallInSector(const Sector* sector, const Vec2f* pos, f32 maxDistSq, f32* minDistToWallSq)
	{
		const SectorWall* walls = s_level->walls.data() + sector->wallOffset;
		const Vec2f* vertices = s_level->vertices.data() + sector->vtxOffset;
		const u32 count = sector->wallCount;
		f32 minDistSq = FLT_MAX;
		s32 closestId = -1;
		for (u32 w = 0; w < count; w++)
		{
			const Vec2f* v0 = &vertices[walls[w].i0];
			const Vec2f* v1 = &vertices[walls[w].i1];

			Vec2f pointOnSeg;
			Geometry::closestPointOnLineSegment(*v0, *v1, *pos, &pointOnSeg);
			const Vec2f diff = { pointOnSeg.x - pos->x, pointOnSeg.z - pos->z };
			const f32 distSq = diff.x*diff.x + diff.z*diff.z;

			if (distSq < maxDistSq && distSq < minDistSq && (!minDistToWallSq || distSq < *minDistToWallSq))
			{
				minDistSq = distSq;
				closestId = s32(w);
			}
		}
		if (minDistToWallSq)
		{
			*minDistToWallSq = std::min(*minDistToWallSq, minDistSq);
		}
		return closestId;
	}

	// Find the closest wall to 'pos' that lies within sector 'sectorId'
	// Only accept walls within 'maxDist' and in sectors on layer 'layer'
	// Pass -1 to sectorId to choose any sector on the layer, sectorId will be set to the sector.
	s32 findClosestWall(s32* sectorId, s32 layer, const Vec2f* pos, f32 maxDist)
	{
		assert(sectorId && pos);
		const f32 maxDistSq = maxDist * maxDist;
		if (*sectorId >= 0)
		{
			return findClosestWallInSector(&s_level->sectors[*sectorId], pos, maxDistSq, nullptr);
		}

		const s32 sectorCount = (s32)s_level->sectors.size();
		const Sector* sectors = s_level->sectors.data();

		f32 minDistSq = FLT_MAX;
		s32 closestId = -1;
		for (s32 i = 0; i < sectorCount; i++)
		{
			if (sectors[i].layer != layer) { continue; }
			const s32 id = findClosestWallInSector(&sectors[i], pos, maxDistSq, &minDistSq);
			if (id >= 0) { *sectorId = i; closestId = id; }
		}

		return closestId;
	}
		
	s32 findSector(s32 layer, const Vec2f* pos)
	{
		if (!s_level) { return -1; }
		const s32 sectorCount = (s32)s_level->sectors.size();
		const Sector* sectors = s_level->sectors.data();
		const SectorWall* walls = s_level->walls.data();
		const Vec2f* vertices = s_level->vertices.data();

		for (s32 i = 0; i < sectorCount; i++)
		{
			if (sectors[i].layer != layer) { continue; }
			if (Geometry::pointInSector(pos, sectors[i].vtxCount, vertices + sectors[i].vtxOffset, sectors[i].wallCount, walls + sectors[i].wallOffset))
			{
				return i;
			}
		}
		return -1;
	}
		
	s32 findSector(const Vec3f* pos)
	{
		const s32 sectorCount = (s32)s_level->sectors.size();
		const Sector* sectors = s_level->sectors.data();
		const SectorWall* walls = s_level->walls.data();
		const Vec2f* vertices = s_level->vertices.data();

		const Vec2f mapPos = { pos->x, pos->z };
		s32 insideCount = 0;
		s32 insideIndices[256];

		// sometimes objects can be in multiple valid sectors, so pick the best one.
		for (s32 i = 0; i < sectorCount; i++)
		{
			if (Geometry::pointInSector(&mapPos, sectors[i].vtxCount, vertices + sectors[i].vtxOffset, sectors[i].wallCount, walls + sectors[i].wallOffset))
			{
				assert(insideCount < 256);
				insideIndices[insideCount++] = i;
			}
		}
		// if the object isn't inside a sector than return.
		if (!insideCount) { return -1; }

		// First see if its actually inside any sectors based on height.
		// If so, pick the sector where it is closest to the floor.
		f32 minHeightDiff = FLT_MAX;
		s32 closestFit = -1;
		for (s32 i = 0; i < insideCount; i++)
		{
			const Sector* sector = &sectors[insideIndices[i]];
			if (pos->y >= sector->ceilAlt - HEIGHT_EPS && pos->y <= sector->floorAlt + HEIGHT_EPS)
			{
				f32 diff = fabsf(pos->y - sector->floorAlt);
				if (diff < minHeightDiff)
				{
					minHeightDiff = diff;
					closestFit = insideIndices[i];
				}
			}
		}
		if (closestFit >= 0)
		{
			return closestFit;
		}

		// If no valid height range is found, just pick whatever is closest and hope for the best.
		for (s32 i = 0; i < insideCount; i++)
		{
			const Sector* sector = &sectors[insideIndices[i]];
			const f32 diff = fabsf(pos->y - sector->floorAlt);
			if (diff < minHeightDiff)
			{
				minHeightDiff = diff;
				closestFit = insideIndices[i];
			}
		}

		//assert(0);
		return closestFit;
	}
		
	#define MAX_SPRITES_COLLIDE 256
	s32 s_spritesToCheck[MAX_SPRITES_COLLIDE];
	u32 s_spriteCount = 0;

	// O = origin, D = direction, C = circle center, t[] = intersections
	bool intersectLineCircle(const Vec2f& origin, const Vec2f& dir, const Vec2f& circleCenter, f32 radius, f32* t, Vec2f* point)
	{
		Vec2f d = { origin.x - circleCenter.x, origin.z - circleCenter.z };
		f32 a = dir.x*dir.x + dir.z*dir.z;
		f32 b = d.x*dir.x + d.z*dir.z;
		f32 c = d.x*d.x + d.z*d.z - radius * radius;
		f32 disc = b * b - a * c;
		if (disc < 0.0f) { return false; }

		f32 sqrtDisc = sqrtf(disc);
		f32 invA = 1.0f / a;

		t[0] = (-b - sqrtDisc) * invA;
		t[1] = (-b + sqrtDisc) * invA;
		if (t[0] < 0.0f && t[1] < 0.0f) { return false; }

		f32 invRadius = 1.0f / radius;
		point[0] = { origin.x + t[0] * dir.x, origin.z + t[0] * dir.z };
		point[1] = { origin.x + t[1] * dir.x, origin.z + t[1] * dir.z };
		return true;
	}

	void addSpriteToCheck(s32 id)
	{
		if (s_spriteCount >= MAX_SPRITES_COLLIDE) { return; }

		// see if it is already in the list.
		for (u32 i = 0; i < s_spriteCount; i++)
		{
			if (s_spritesToCheck[i] == id) { return; }
		}
		// add it.
		s_spritesToCheck[s_spriteCount++] = id;
	}

	void addSector(MultiRayHitInfo* hitInfo, u32 sectorId)
	{
		if (hitInfo->sectorCount >= 16) { return; }

		for (u32 s = 0; s < hitInfo->sectorCount; s++)
		{
			if (hitInfo->sectors[s] == sectorId) { return; }
		}
		hitInfo->sectors[hitInfo->sectorCount++] = sectorId;
	}

	// Gather multiple hits until the ray hits a solid surface.
	bool traceRayMulti(const Ray* ray, MultiRayHitInfo* hitInfo)
	{
		assert(ray && hitInfo);
		hitInfo->hitCount = 0;
		hitInfo->sectorCount = 1;
		hitInfo->sectors[0] = ray->originSectorId;

		f32 maxDist  = ray->maxDist;
		Vec3f origin = ray->origin;
		Vec2f p0xz = { origin.x, origin.z };
		Vec2f p1xz = { p0xz.x + ray->dir.x * maxDist, p0xz.z + ray->dir.z * maxDist };

		const s32 sectorCount   = (s32)s_level->sectors.size();
		const Sector* sectors   = s_level->sectors.data();
		const SectorWall* walls = s_level->walls.data();
		const Vec2f* vertices   = s_level->vertices.data();

		s32 nextSector = ray->originSectorId;
		s32 windowId = -1, prevSector = -1;
		bool hitFound = false;
		while (nextSector >= 0 && hitInfo->hitCount < 16)
		{
			const s32 sectorId = nextSector;
			const Sector* curSector = &sectors[sectorId];
			const SectorWall* curWalls = walls + curSector->wallOffset;
			const Vec2f* curVtx = vertices + curSector->vtxOffset;
			const s32 wallCount = (s32)curSector->wallCount;
			nextSector = -1;

			// Check the ray against all of the walls and take the closest hit, if any.
			f32 closestHit = FLT_MAX;
			s32 closestWallId = -1;
			for (s32 w = 0; w < wallCount; w++)
			{
				// skip if this is the portal we just went through.
				// TODO: Figure out why this doesn't always work.
				if (curWalls[w].adjoin >= 0 && curWalls[w].mirror == windowId && curWalls[w].adjoin == prevSector)
				{
					continue;
				}

				const s32 i0 = curWalls[w].i0;
				const s32 i1 = curWalls[w].i1;
				const Vec2f* v0 = &curVtx[i0];
				const Vec2f* v1 = &curVtx[i1];

				f32 s, t;
				if (Geometry::lineSegmentIntersect(&p0xz, &p1xz, v0, v1, &s, &t))
				{
					if (s < closestHit)
					{
						closestHit = s;
						closestWallId = w;
					}
				}
			}
			if (closestWallId < 0)
			{
				break;
			}
			closestHit *= maxDist;

			// Check the floor.
			Vec3f p0 = origin;
			Vec3f p1 = { p0.x + ray->dir.x*closestHit, p0.y + ray->dir.y*closestHit, p0.z + ray->dir.z*closestHit };
			Vec3f hitPoint;
			bool planeHit = false;
			if (Geometry::lineYPlaneIntersect(&p0, &p1, curSector->floorAlt, &hitPoint))
			{
				p1 = hitPoint;
				planeHit = true;
			}
			else if (Geometry::lineYPlaneIntersect(&p0, &p1, curSector->ceilAlt, &hitPoint))
			{
				p1 = hitPoint;
				planeHit = true;
			}
						
			addSector(hitInfo, sectorId);
			if (planeHit)
			{
				break;
			}
			else
			{
				hitInfo->hitSectorId[hitInfo->hitCount] = sectorId;
				hitInfo->wallHitId[hitInfo->hitCount] = closestWallId;
				hitInfo->hitPoint[hitInfo->hitCount] = p1;
				hitInfo->hitCount++;

				if (curWalls[closestWallId].adjoin < 0)
				{
					break;
				}
				else
				{
					nextSector = curWalls[closestWallId].adjoin;

					// Always add the next sector even if it is not traversable in order to handle things like doors and elevators that can be toggled
					// from the outside.
					addSector(hitInfo, nextSector);
					
					const Sector* next = &sectors[nextSector];
					if (p1.y >= next->floorAlt || p1.y <= next->ceilAlt)
					{
						// If the ray hits the top or bottom section than stop traversing here.
						break;
					}

					// Otherwise we'll continue to add more hits.
					origin = p1;
					p0xz = { origin.x, origin.z };
					maxDist = ray->maxDist - DXL2_Math::distance(&ray->origin, &origin);
					windowId = closestWallId;
					prevSector = sectorId;
				}
			}
		}

		return hitInfo->hitCount > 0 || hitInfo->sectorCount;
	}

	// Traces a ray through the level.
	// Returns true if the ray hit something.
	bool traceRay(const Ray* ray, RayHitInfo* hitInfo)
	{
		assert(ray && hitInfo);
		hitInfo->obj = nullptr;

		f32 maxDist = ray->maxDist;
		Vec3f origin = ray->origin;
		Vec2f p0xz = { origin.x, origin.z };
		Vec2f p1xz = { p0xz.x + ray->dir.x * maxDist, p0xz.z + ray->dir.z * maxDist };
		
		const s32 sectorCount = (s32)s_level->sectors.size();
		const Sector* sectors = s_level->sectors.data();
		const SectorWall* walls = s_level->walls.data();
		const Vec2f* vertices = s_level->vertices.data();

		const GameObject* objects = LevelGameObjects::getGameObjectList()->data();
		const SectorObjectList* sectorObjects = LevelGameObjects::getSectorObjectList();

		s_spriteCount = 0;

		s32 nextSector = ray->originSectorId;
		s32 windowId = -1, prevSector = -1;
		bool hitFound = false;
		while (nextSector >= 0)
		{
			const s32 sectorId = nextSector;
			const Sector* curSector = &sectors[sectorId];
			const SectorWall* curWalls = walls + curSector->wallOffset;
			const Vec2f* curVtx = vertices + curSector->vtxOffset;
			const s32 wallCount = (s32)curSector->wallCount;
			nextSector = -1;

			// Add Sprites to check.
			if (ray->objSelect)
			{
				const size_t count = (*sectorObjects)[sectorId].list.size();
				const u32* objectIds = (*sectorObjects)[sectorId].list.data();
				for (size_t i = 0; i < count; i++)
				{
					// Skip sprites with no collision.
					if (objects[objectIds[i]].collisionRadius <= 0.0f || objects[objectIds[i]].collisionHeight <= 0.0f)
					{
						continue;
					}
					addSpriteToCheck(objectIds[i]);
				}
			}

			hitInfo->hitSectorId = sectorId;

			// Check the ray against all of the walls and take the closest hit, if any.
			f32 closestHit = FLT_MAX;
			s32 closestWallId = -1;
			for (s32 w = 0; w < wallCount; w++)
			{
				// skip if this is the portal we just went through.
				// TODO: Figure out why this doesn't always work.
				if (curWalls[w].adjoin >= 0 && curWalls[w].mirror == windowId && curWalls[w].adjoin == prevSector)
				{
					continue;
				}

				const s32 i0 = curWalls[w].i0;
				const s32 i1 = curWalls[w].i1;
				const Vec2f* v0 = &curVtx[i0];
				const Vec2f* v1 = &curVtx[i1];

				f32 s, t;
				if (Geometry::lineSegmentIntersect(&p0xz, &p1xz, v0, v1, &s, &t))
				{
					if (s < closestHit)
					{
						closestHit = s;
						closestWallId = w;
					}
				}
			}
			if (closestWallId < 0)
			{
				break;
			}
			closestHit *= maxDist;

			// Check the floor.
			Vec3f p0 = origin;
			Vec3f p1 = { p0.x + ray->dir.x*closestHit, p0.y + ray->dir.y*closestHit, p0.z + ray->dir.z*closestHit };
			Vec3f hitPoint;
			bool planeHit = false;
			if (Geometry::lineYPlaneIntersect(&p0, &p1, curSector->floorAlt, &hitPoint))
			{
				p1 = hitPoint;
				planeHit = true;
			}

			if (Geometry::lineYPlaneIntersect(&p0, &p1, curSector->ceilAlt, &hitPoint))
			{
				p1 = hitPoint;
				planeHit = true;
			}

			if (planeHit)
			{
				hitInfo->hitPoint = p1;
				hitInfo->hitSectorId = sectorId;
				hitInfo->hitWallId = -1;
				hitFound = true;
				break;
			}
			else if (curWalls[closestWallId].adjoin >= 0)
			{
				// given the hit point, is it below the next floor or above the next ceiling?
				const Sector* next = &sectors[curWalls[closestWallId].adjoin];
				if (p1.y >= next->floorAlt || p1.y <= next->ceilAlt)
				{
					hitInfo->hitPoint = p1;
					hitInfo->hitSectorId = sectorId;
					hitFound = true;
					break;
				}

				origin = p1;
				p0xz = { origin.x, origin.z };
				maxDist = ray->maxDist - DXL2_Math::distance(&ray->origin, &origin);
				nextSector = curWalls[closestWallId].adjoin;
				windowId = closestWallId;
				prevSector = sectorId;
			}
			else
			{
				hitInfo->hitPoint.x = origin.x + ray->dir.x * closestHit;
				hitInfo->hitPoint.y = origin.y + ray->dir.y * closestHit;
				hitInfo->hitPoint.z = origin.z + ray->dir.z * closestHit;
				hitInfo->hitSectorId = sectorId;
				hitInfo->hitWallId = closestWallId;
				hitFound = true;
				break;
			}
		};

		if (!ray->objSelect) { return hitFound; }

		// Now check for the closest sprite hit...
		Vec3f p0 = ray->origin;
		Vec3f p1;
		if (hitFound)
		{
			p1 = hitInfo->hitPoint;
		}
		else
		{
			p1 = { p0.x + ray->dir.x * ray->maxDist, p0.y + ray->dir.y * ray->maxDist, p0.z + ray->dir.z * ray->maxDist };
		}

		p0xz = { p0.x, p0.z };
		p1xz = { p1.x, p1.z };
		const Vec2f dirxz = { ray->dir.x, ray->dir.z };
		f32 minDist = FLT_MAX;
		s32 closestSprite = -1;
		s32 spriteSectorId = -1;
		Vec3f spriteHitPoint;
		for (u32 i = 0; i < s_spriteCount; i++)
		{
			const GameObject* obj = &objects[s_spritesToCheck[i]];
			const Vec2f objPosxz = { obj->pos.x, obj->pos.z };
			f32 t[2];
			Vec2f points[2];
			if (intersectLineCircle(p0xz, dirxz, objPosxz, obj->collisionRadius, t, points))
			{
				// minimum intersection.
				s32 minPt = 1;
				if (t[0] > 0.0f && t[0] <= t[1])
				{
					minPt = 0;
				}

				if (t[minPt] < minDist)
				{
					// check if the height is in range.
					f32 intersectHeight = p0.y + t[minPt] * ray->dir.y;
					if (intersectHeight <= obj->pos.y && intersectHeight >= obj->pos.y - obj->collisionHeight)
					{
						spriteHitPoint = { points[minPt].x, intersectHeight, points[minPt].z };
						minDist = t[minPt];
						closestSprite = s32(obj->id);
						spriteSectorId = obj->sectorId;
					}
				}
			}
		}

		// Now take the closest point as the final intersection.
		if (closestSprite >= 0)
		{
			if ((hitFound && DXL2_Math::distance(&ray->origin, &spriteHitPoint) < DXL2_Math::distance(&ray->origin, &hitInfo->hitPoint)) || !hitFound)
			{
				hitFound = true;
				hitInfo->hitPoint = spriteHitPoint;
				hitInfo->hitSectorId = spriteSectorId;
				hitInfo->hitWallId = -1;
				hitInfo->obj = &objects[closestSprite];
			}
		}

		return hitFound;
	}
}
