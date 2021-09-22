#include "player.h"
#include "playerCollision.h"
#include "automap.h"
#include "pickup.h"
#include <TFE_System/system.h>
#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_Jedi/Level/rwall.h>
#include <TFE_Jedi/Level/robject.h>
#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/InfSystem/infSystem.h>
// Internal types need to be included in this case.
#include <TFE_Jedi/InfSystem/infTypesInternal.h>
#include <TFE_Jedi/Collision/collision.h>

namespace TFE_DarkForces
{
	///////////////////////////////////////////
	// Constants
	///////////////////////////////////////////
	enum Passability
	{
		PASS_ALWAYS_WALK = 0,
		PASS_DEFAULT = 0xffffffff,
	};

	///////////////////////////////////////////
	// Internal State
	///////////////////////////////////////////
	static PlayerLogic* s_curPlayerLogic;

	///////////////////////////////////////////
	// TODO: Move to collision
	///////////////////////////////////////////
	SecObject* s_collidedObj;
	ColObject s_colObject;
	ColObject s_colObj1;
	RWall* s_colWallCollided;
	fixed16_16 s_colSrcPosX;
	fixed16_16 s_colSrcPosY;
	fixed16_16 s_colSrcPosZ;
	fixed16_16 s_colHeight;
	fixed16_16 s_colDoubleRadius;
	fixed16_16 s_colHeightBase;
	fixed16_16 s_colMaxBaseHeight;
	fixed16_16 s_colMinBaseHeight;
	fixed16_16 s_colBottom;
	fixed16_16 s_colTop;
	fixed16_16 s_colY1;
	fixed16_16 s_colBaseFloorHeight;
	fixed16_16 s_colBaseCeilHeight;
	fixed16_16 s_colFloorHeight;
	fixed16_16 s_colCeilHeight;
	fixed16_16 s_colCurFloor;
	fixed16_16 s_colCurCeil;
	fixed16_16 s_colCurTop;
	angle14_32 s_colResponseAngle;
	vec2_fixed s_colResponsePos;
	vec2_fixed s_colWallV0;
	s32 s_collisionFrameSector;

	CollisionObjFunc s_objCollisionFunc;
	CollisionProxFunc s_objCollisionProxFunc;

	///////////////////////////////////////////
	// Shared State
	///////////////////////////////////////////
	fixed16_16 s_colCurLowestFloor;
	fixed16_16 s_colCurHighestCeil;
	fixed16_16 s_colCurBot;
	fixed16_16 s_colMinHeight;
	fixed16_16 s_colMaxHeight;
	
	fixed16_16 s_colWidth;
	fixed16_16 s_colDstPosX;
	fixed16_16 s_colDstPosY;
	fixed16_16 s_colDstPosZ;
	RSector* s_nextSector;
	RWall* s_colWall0;
	JBool s_colResponseStep;
	JBool s_objCollisionEnabled = JTRUE;
	vec2_fixed s_colResponseDir;

	///////////////////////////////////////////
	// Implementation
	///////////////////////////////////////////

	// Returns a collision object or nullptr if no object is hit.
	SecObject* col_findObjectCollision(RSector* sector)
	{
		fixed16_16 colWidth = s_colWidth;
		if (s_objCollisionEnabled)
		{
			s32 objCount = sector->objectCount;
			s32 objCapacity = sector->objectCapacity;
			fixed16_16 relHeight = s_colDstPosY - s_colHeightBase;

			fixed16_16 dirX, dirZ;
			fixed16_16 pathDz = s_colDstPosZ - s_colSrcPosZ;
			fixed16_16 pathDx = s_colDstPosX - s_colSrcPosX;
			computeDirAndLength(pathDx, pathDz, &dirX, &dirZ);

			for (s32 objIndex = 0, objListIndex = 0; objIndex < objCount && objListIndex < objCapacity; objListIndex++)
			{
				SecObject* obj = sector->objectList[objListIndex];
				if (obj)
				{
					objIndex++;
					if ((obj->flags & OBJ_FLAG_HAS_COLLISION) && !(obj->entityFlags & ETFLAG_PICKUP) && obj->worldWidth && (s_colSrcPosX != obj->posWS.x || s_colSrcPosZ != obj->posWS.z))
					{
						// Check the seperation of the object and destination position.
						// If they are seperated by more than their combined widths on the X or Z axis, then there is no collision.
						fixed16_16 sepX  = TFE_Jedi::abs(obj->posWS.x - s_colDstPosX);
						fixed16_16 sepZ  = TFE_Jedi::abs(obj->posWS.z - s_colDstPosZ);
						fixed16_16 width = obj->worldWidth + colWidth;
						if (sepX >= width || sepZ >= width)
						{
							continue;
						}

						// The top of the object is *below* the final position.
						fixed16_16 objTop = obj->posWS.y - obj->worldHeight;
						if (objTop >= s_colDstPosY || relHeight >= obj->posWS.y)
						{
							continue;
						}

						// Check XZ seperation again... (this second test can be skipped)
						sepX = TFE_Jedi::abs(s_colDstPosX - obj->posWS.x);
						sepZ = TFE_Jedi::abs(s_colDstPosZ - obj->posWS.z);
						if ((sepX >= obj->worldWidth + s_colWidth) || (sepZ >= obj->worldWidth + s_colWidth))
						{
							continue;
						}

						// Check to see if the path starts already colliding with the object.
						// And if it is, then skip collision (so they come apart and don't get stuck).
						fixed16_16 startSepX = TFE_Jedi::abs(s_colSrcPosX - obj->posWS.x);
						fixed16_16 startSepZ = TFE_Jedi::abs(s_colSrcPosZ - obj->posWS.z);
						if (startSepX < width && startSepZ < width)
						{
							continue;
						}
												
						fixed16_16 dx = s_colDstPosX - s_colSrcPosX;
						fixed16_16 dz = s_colDstPosZ - s_colSrcPosZ;
						s32 xSign = (dx < 0) ? -1 : 1;
						s32 zSign = (dz < 0) ? -1 : 1;

						// Compute the object AABB edges that need to be considered for the collision.
						// this is the same as: objEdgeX = obj->posWS.x - obj->worldWidth * xSign;
						fixed16_16 objEdgeX = (xSign >= 0) ? (obj->posWS.x - obj->worldWidth) : (obj->posWS.x + obj->worldWidth);
						fixed16_16 objEdgeZ = (zSign >= 0) ? (obj->posWS.z - obj->worldWidth) : (obj->posWS.z + obj->worldWidth);

						// Cross product between the vector from the destination to the nearest AABB corner to the start and
						// the path direction.
						// This is *zero* if the corner is exactly on the path, *negative* if the corner is between the start and destination,
						// and *positive* if the point is *past* the destination (i.e. unreachable).
						fixed16_16 cprod = mul16(objEdgeX - s_colDstPosX, dirZ) - mul16(objEdgeZ - s_colDstPosZ, dirX);
						s32 cSign = cprod < 0 ? -1 : 1;

						// Is the sign of the product different than the sign of either x or z.
						s32 signDiff = (cSign^xSign) ^ zSign;
						if (signDiff < 0)	// condition above is *true*
						{
							s_colResponseStep = JTRUE;
							if (zSign >= 0)
							{
								s_colResponseAngle = 4095;	// ~90 degrees
								s_colResponsePos.x = obj->posWS.x - obj->worldWidth;
								s_colResponsePos.z = obj->posWS.z - obj->worldWidth;
								s_colResponseDir.x = ONE_16;
								s_colResponseDir.z = 0;
								return obj;
							}
							else // zSign < 0
							{
								s_colResponseAngle = 12287;		// ~270 degrees
								s_colResponsePos.x = obj->posWS.x + obj->worldWidth;
								s_colResponsePos.z = obj->posWS.z + obj->worldWidth;
								s_colResponseDir.x = -ONE_16;
								s_colResponseDir.z = 0;

								return obj;
							}
						}
						else
						{
							s_colResponseStep = JTRUE;
							if (xSign >= 0)
							{
								s_colResponseAngle = 8191;	// ~180 degrees
								s_colResponsePos.x = obj->posWS.x - obj->worldWidth;
								s_colResponsePos.z = obj->posWS.z + obj->worldWidth;
								s_colResponseDir.x = 0;
								s_colResponseDir.z = -ONE_16;

								return obj;
							}
							else
							{
								s_colResponseAngle = 0;		// 0 degrees
								s_colResponsePos.x = obj->posWS.x + obj->worldWidth;
								s_colResponsePos.z = obj->posWS.z - obj->worldWidth;
								s_colResponseDir.x = 0;
								s_colResponseDir.z = ONE_16;

								return obj;
							}
						}
					}
				}
			}
		}
		return nullptr;
	}

	// Get the "feature" to collide against, such as an object or wall.
	// Returns JFALSE if collision else JTRUE
	u32 col_getCollisionFeature(RSector* sector, u32 passability)
	{
		s_colBaseFloorHeight = sector->floorHeight;
		s_colBaseCeilHeight  = sector->ceilingHeight;
		s_colFloorHeight     = sector->colFloorHeight;
		s_colCeilHeight      = sector->colCeilHeight;

		if (sector->secHeight < 0 && s_colDstPosY > sector->colSecHeight)
		{
			s_colCurFloor = s_colFloorHeight;
			s_colCurCeil = s_colBaseFloorHeight + sector->secHeight;
		}
		else
		{
			s_colCurFloor = sector->colSecHeight;
			s_colCurCeil  = sector->colMinHeight;	// why not sector->colCeilHeight?
		}

		if (s_colMaxBaseHeight >= s_colBaseFloorHeight)
		{
			s_colMaxBaseHeight = s_colBaseFloorHeight;
		}
		if (s_colMinBaseHeight <= s_colBaseCeilHeight)
		{
			s_colMinBaseHeight = s_colBaseCeilHeight;
		}
		if (s_colMaxHeight >= s_colFloorHeight)
		{
			s_colMaxHeight = s_colFloorHeight;
		}
		if (s_colMinHeight <= s_colCeilHeight)
		{
			s_colMinHeight = s_colCeilHeight;
		}
		if (s_colCurLowestFloor >= s_colCurFloor)
		{
			s_colCurLowestFloor = s_colCurFloor;
		}
		if (s_colCurHighestCeil <= s_colCurCeil)
		{
			s_colCurHighestCeil = s_colCurCeil;
		}

		if (s_colCurCeil > s_colCurTop)
		{
			s_colCurTop = s_colCurCeil;
			// s_2c865c = sector;
		}
		if (s_colCurLowestFloor < s_colCurBot)
		{
			s_colCurBot = s_colCurLowestFloor;
			s_nextSector = sector;
		}

		if (passability == PASS_ALWAYS_WALK || (s_colCurBot >= s_colBottom && s_colCurTop <= s_colTop && TFE_Jedi::abs(s_colCurBot - s_colCurTop) >= s_colHeight && s_colCurFloor <= s_colY1))
		{
			s_collidedObj = col_findObjectCollision(sector);
			if (s_collidedObj)
			{
				return JFALSE;
			}
			RWall* wall = sector->walls;
			s32 wallCount = sector->wallCount;
			sector->collisionFrame = s_collisionFrameSector;
			for (s32 i = 0; i < wallCount; i++, wall++)
			{
				s_colWallV0 = *wall->w0;
				fixed16_16 length = wall->length;

				JBool canPass = JFALSE;
				// Note WF3_ALWAYS_WALK overrides WF3_SOLID_WALL.
				if ((wall->flags3 & (WF3_ALWAYS_WALK | WF3_SOLID_WALL)) != WF3_SOLID_WALL && wall->nextSector)
				{
					canPass = JTRUE;
				}

				// Given line: (v0, v1) and position (p) compute the distance from the line:
				// dist = (p - v0).N; where N = (dz, -dx) = wall normal.
				fixed16_16 dist = mul16(s_colDstPosX - s_colWallV0.x, wall->wallDir.z) - mul16(s_colDstPosZ - s_colWallV0.z, wall->wallDir.x);
				// Convert to unsigned distance.
				if (dist < 0) { dist = -dist; }
				// Further checks can be avoided if the position is too far from the line to collide.
				if (dist < s_colWidth)
				{
					// Project s_colDstPosX/Z onto the wall, i.e.
					// u = (p-v0).D; where D = direction and v = v0 + u*D
					fixed16_16 u = mul16(s_colDstPosX - s_colWallV0.x, wall->wallDir.x) + mul16(s_colDstPosZ - s_colWallV0.z, wall->wallDir.z);
					fixed16_16 maxEdge = u + s_colWidth;
					fixed16_16 maxCol = length + s_colDoubleRadius;
					if ((u >= 0 && u <= length) || (maxEdge >= 0 && maxEdge <= maxCol))
					{
						JBool sideTest = JTRUE;
						if (canPass)
						{
							RSector* next = wall->nextSector;
							fixed16_16 srcY = s_colSrcPosY;
							fixed16_16 secHeight = next->colSecHeight;
							// If the path starts *above* the second height and then ends at or *below* it.
							if (srcY <= secHeight && s_colY1 >= secHeight && s_colTop > next->colMinHeight)
							{
								sideTest = JFALSE;
							}
						}
						// Skip the side test if dealing with a passable wall.
						if (sideTest)
						{
							// Make sure the start point is in front side of the wall.
							// side = (s_colSrcPos - s_colWallV0) x (wallDir)
							fixed16_16 side = mul16(s_colSrcPosX - s_colWallV0.x, wall->wallDir.z) - mul16(s_colSrcPosZ - s_colWallV0.z, wall->wallDir.x);
							if (side < 0)
							{
								// We hit the back side of the wall.
								continue;
							}
							fixed16_16 x1 = s_colDstPosX;
							fixed16_16 z1 = s_colDstPosZ;
							fixed16_16 z0 = s_colSrcPosZ;
							fixed16_16 x0 = s_colSrcPosX;
							fixed16_16 dz = z1 - z0;
							fixed16_16 dx = x1 - x0;
							// Check to see if the movement direction is not moving *away* from the wall.
							side = mul16(dx, wall->wallDir.z) - mul16(dz, wall->wallDir.x);
							if (side > 0)
							{
								// The collision path is moving away from the wall.
								continue;
							}
						}

						// This wall might be passable.
						if (canPass)
						{
							RSector* next = wall->nextSector;
							fixed16_16 nextFloor = (next->secHeight >= 0) ? next->colSecHeight : next->floorHeight;
							// Check to see if the object fits between the next sector floor and ceiling.
							if ((wall->flags3 & (WF3_ALWAYS_WALK | WF3_SOLID_WALL)) != WF3_ALWAYS_WALK)
							{
								if (s_colBottom > nextFloor)
								{
									s_colWallCollided = wall;
									return JFALSE;
								}
								if (s_colTop < next->colMinHeight)
								{
									s_colWallCollided = wall;
									return JFALSE;
								}
							}
							// Check to see if this sector has already been tested this "collision frame"
							if (s_collisionFrameSector != next->collisionFrame)
							{
								u32 passability = ((wall->flags3 & (WF3_ALWAYS_WALK | WF3_SOLID_WALL)) == WF3_ALWAYS_WALK) ? PASS_ALWAYS_WALK : PASS_DEFAULT;
								if (!col_getCollisionFeature(next, passability))
								{
									if (!s_colWallCollided) { s_colWallCollided = wall; }
									return JFALSE;
								}
							}
						}
						else  // !canPass
						{
							s_colWallCollided = wall;
							return JFALSE;
						}
					}
				}
			}
			return JTRUE;
		}
		s_colObj1.sector = sector;
		return JFALSE;
	}

	// Returns JFALSE if there was a collision.
	JBool col_computeCollisionResponse(RSector* sector)
	{
		fixed16_16 mx =  FIXED(9999);
		fixed16_16 mn = -FIXED(9999);
		//s_28262c = mx;
		//s_282630 = -mn;
		fixed16_16 mx2 = mx;
		fixed16_16 mn2 = mn;
		s_colMaxBaseHeight = mx;
		
		s_colMinBaseHeight = mn;
		s_colMaxHeight = mx;
		s_colMinHeight = mn;
		s_colCurLowestFloor = mx;
		s_colCurHighestCeil = mn;

		s_colCurBot = mx;
		s_colCurTop = mn;

		s_collisionFrameSector++;
		s_colWallCollided = nullptr;
		s_colDoubleRadius = s_colWidth + s_colWidth;
		s_colResponseStep = JFALSE;
		// Is there a collision?
		if (!col_getCollisionFeature(sector, PASS_ALWAYS_WALK))
		{
			s_colWall0 = s_colWallCollided;
			if (s_colWall0)
			{
				s_colResponseStep = JTRUE;
				s_colResponseDir.x = s_colWall0->wallDir.x;
				s_colResponseDir.z = s_colWall0->wallDir.z;
				s_colResponseAngle = s_colWall0->angle;

				vec2_fixed* w0 = s_colWall0->w0;
				s_colResponsePos.x = w0->x;
				s_colResponsePos.z = w0->z;
			}
			return JFALSE;
		}
		return JTRUE;
	}
		
	void handleCollisionResponse(fixed16_16 dirX, fixed16_16 dirZ, fixed16_16 posX, fixed16_16 posZ)
	{
		SecObject* obj = s_curPlayerLogic->logic.obj;
		// Project the movement vector onto the wall direction to get how much sliding movement is left.
		fixed16_16 proj = mul16(s_curPlayerLogic->move.x, dirX) + mul16(s_curPlayerLogic->move.z, dirZ);

		// Normal = (dirZ, -dirX)
		// -P.Normal
		fixed16_16 dx = obj->posWS.x - posX;
		fixed16_16 dz = obj->posWS.z - posZ;
		fixed16_16 planarDist = mul16(dx, dirZ) - mul16(dz, dirX);

		fixed16_16 r = obj->worldWidth + 65;	// worldWidth + 0.001
		// if overlap is > 0, the player cylinder is embedded in the wall.
		fixed16_16 overlap = r - planarDist;
		if (overlap >= 0)
		{
			// push at a rate of 4 units / second.
			fixed16_16 pushSpd = FIXED(4);
			// make sure the overlap adjustment is at least the size of the "push"
			overlap = max(overlap, mul16(pushSpd, s_deltaTime));
			RSector* sector = obj->sector;
			s_colCurBot = sector->floorHeight;
		}
		// Move along 'overlap' so that the cylinder is nearly touching and move along move direction based on projection.
		// This has the effect of attracting or pushing the object in order to remove the gap or overlap.
		s_curPlayerLogic->move.x =  mul16(overlap, dirZ) + mul16(proj, dirX);
		s_curPlayerLogic->move.z = -mul16(overlap, dirX) + mul16(proj, dirZ);
	}

	// Returns JTRUE in the case of a collision, otherwise returns JFALSE.
	JBool handlePlayerCollision(PlayerLogic* playerLogic)
	{
		s_curPlayerLogic = playerLogic;
		SecObject* obj = playerLogic->logic.obj;
		s_colObject.obj = obj;

		s_colSrcPosX = obj->posWS.x;
		s_colDstPosY = obj->posWS.y;
		fixed16_16 z = obj->posWS.z;
		s_colWidth = obj->worldWidth;

		s_colHeightBase = obj->worldHeight;
		s_colSrcPosY = obj->posWS.y;

		// Handles maximum stair height.
		s_colBottom = obj->posWS.y - playerLogic->stepHeight;

		s_colSrcPosZ = obj->posWS.z;
		s_colTop = obj->posWS.y - obj->worldHeight - ONE_16;

		s_colY1 = FIXED(9999);
		s_colHeight = obj->worldHeight + ONE_16;
		RSector* sector = obj->sector;

		s_colWall0 = nullptr;
		s_colObj1.ptr = nullptr;

		s_colDstPosX = obj->posWS.x + playerLogic->move.x;
		s_colDstPosZ = obj->posWS.z + playerLogic->move.z;

		// If there was no collision, return false.
		if (col_computeCollisionResponse(sector))
		{
			return JFALSE;
		}
		if (s_colResponseStep)
		{
			handleCollisionResponse(s_colResponseDir.x, s_colResponseDir.z, s_colResponsePos.x, s_colResponsePos.z);
		}
		return JTRUE;
	}

	// Attempts to move the player. Note that collision detection has already occured accounting for step height,
	// objects, collision reponse, etc.. So this is a more basic collision detection function to validate the
	// results and fire off INF_EVENT_CROSS_LINE_XXX events as needed.
	JBool playerMove()
	{
		SecObject* player = s_playerObject;
		fixed16_16 ceilHeight;
		fixed16_16 floorHeight;
		sector_getObjFloorAndCeilHeight(player->sector, player->posWS.y, &floorHeight, &ceilHeight);

		fixed16_16 finalCeilHeight = ceilHeight;
		fixed16_16 finalFloorHeight = floorHeight;

		fixed16_16 x0 = player->posWS.x;
		fixed16_16 z0 = player->posWS.z;
		fixed16_16 x1 = x0 + s_curPlayerLogic->move.x;
		fixed16_16 z1 = z0 + s_curPlayerLogic->move.z;
		RSector* nextSector = player->sector;
		RWall* wall = collision_wallCollisionFromPath(player->sector, x0, z0, x1, z1);
		while (wall)
		{
			RSector* adjoinSector = wall->nextSector;
			// If the player collides with a wall, nextSector will be set to null, which means that the movement is not applied.
			nextSector = nullptr;
			if (adjoinSector && !(wall->flags3&WF3_SOLID_WALL))
			{
				if (!s_pickupFlags)
				{
					inf_triggerWallEvent(wall, s_playerObject, INF_EVENT_CROSS_LINE_FRONT);
				}
				nextSector = adjoinSector;
				sector_getObjFloorAndCeilHeight(adjoinSector, player->posWS.y, &floorHeight, &ceilHeight);

				// Current ceiling is higher than the adjoining sector, meaning there is an upper wall part.
				finalCeilHeight = max(ceilHeight, finalCeilHeight);
				// Current floor is lower than the adjoining sector, meaning there is a lower wall part.
				finalFloorHeight = min(floorHeight, finalFloorHeight);
				wall = collision_pathWallCollision(adjoinSector);
			}
			else
			{
				wall = nullptr;
			}
		}
		// nextSector will be null if the collision failed, in which case we don't move the player and return false.
		if (nextSector)
		{
			fixed16_16 stepLimit = player->posWS.y - s_curPlayerLogic->stepHeight;
			fixed16_16 topLimit = player->posWS.y - player->worldHeight - ONE_16;
			// If the player cannot fit, then again we don't move the player and return false.
			if (finalFloorHeight >= stepLimit && finalCeilHeight <= topLimit)
			{
				// The collision was a success!
				// Move the player, change sectors if needed and adjust the map layer.
				player->posWS.x += s_curPlayerLogic->move.x;
				player->posWS.z += s_curPlayerLogic->move.z;

				RSector* curSector = player->sector;
				if (nextSector != curSector)
				{
					if (nextSector->layer != curSector->layer)
					{
						automap_setLayer(nextSector->layer);
					}
					sector_addObject(nextSector, player);
				}
				return JTRUE;
			}
		}
		return JFALSE;
	}
		
	JBool handleCollisionFunc(RSector* sector)
	{
		if (!s_objCollisionFunc || s_objCollisionFunc(sector))
		{
			sector->collisionFrame = s_collisionFrameSector;

			RWall* wall = sector->walls;
			s32 wallCount = sector->wallCount;
			for (s32 i = 0; i < wallCount; i++, wall++)
			{
				vec2_fixed* w0 = wall->w0;
				s_colWallV0.x = w0->x;
				s_colWallV0.z = w0->z;

				fixed16_16 length = wall->length;
				fixed16_16 dx = s_colDstPosZ - s_colWallV0.z;
				fixed16_16 dz = s_colDstPosX - s_colWallV0.x;

				fixed16_16 pz = mul16(dz, wall->wallDir.z);
				fixed16_16 px = mul16(dx, wall->wallDir.x);

				fixed16_16 dp = pz - px;
				fixed16_16 adp = TFE_Jedi::abs(dp);
				if (adp <= s_colWidth)
				{
					fixed16_16 paramPos = mul16(dx, wall->wallDir.z) + mul16(dz, wall->wallDir.x);
					if (paramPos > length)
					{
						if (paramPos + s_colWidth > length + s_colDoubleRadius)
						{
							continue;
						}
					}
					if (s_objCollisionProxFunc && !s_objCollisionProxFunc())
					{
						return JFALSE;
					}
					RSector* nextSector = wall->nextSector;
					if (nextSector)
					{
						if ((wall->flags3 & (WF3_ALWAYS_WALK | WF3_SOLID_WALL)) != WF3_SOLID_WALL)
						{
							if (s_collisionFrameSector != nextSector->collisionFrame)
							{
								if (!handleCollisionFunc(nextSector))
								{
									return JFALSE;
								}
							}
						}
					}
				}
			}
		}
		return JTRUE;
	}

	void playerHandleCollisionFunc(RSector* sector, CollisionObjFunc func, CollisionProxFunc proxFunc)
	{
		s_objCollisionFunc = func;
		s_colDoubleRadius = s_colWidth * 2;
		s_objCollisionProxFunc = proxFunc;
		s_collisionFrameSector++;
		handleCollisionFunc(sector);
	}
		
	JBool collision_handleCrushing(RSector* sector)
	{
		message_sendToSector(sector, s_playerObject, 0, MSG_REV_MOVE);
		return JTRUE;
	}

	JBool collision_checkPickups(RSector* sector)
	{
		fixed16_16 floorHeight = sector->floorHeight;
		fixed16_16 ceilHeight = sector->ceilingHeight;
		if (floorHeight == ceilHeight) { return JFALSE;	}

		s32 objCount = sector->objectCount;
		s32 objCapacity = sector->objectCapacity;
		for (s32 objIndex = 0, objListIndex = 0; objIndex < objCount && objListIndex < objCapacity; objListIndex++)
		{
			SecObject* obj = sector->objectList[objListIndex];
			if (obj)
			{
				objIndex++;
				if (obj->worldWidth && (obj->entityFlags & ETFLAG_PICKUP))
				{
					fixed16_16 dx = obj->posWS.x - s_colDstPosX;
					fixed16_16 dz = obj->posWS.z - s_colDstPosZ;
					fixed16_16 adx = TFE_Jedi::abs(dx);
					fixed16_16 adz = TFE_Jedi::abs(dz);
					fixed16_16 radius = obj->worldWidth + s_colWidth;
					if (adx < radius && adz < radius)
					{
						fixed16_16 objTop = obj->posWS.y - obj->worldHeight;
						fixed16_16 colliderTop = s_colDstPosY - s_colHeightBase;
						if (objTop < s_colDstPosY && colliderTop < obj->posWS.y)
						{
							s_msgEntity = s_colObject.obj;
							message_sendToObj(obj, MSG_PICKUP, nullptr);
						}
					}
				}
			}
		}
		return JTRUE;
	}
}  // TFE_DarkForces