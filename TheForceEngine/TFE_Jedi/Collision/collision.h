#pragma once
//////////////////////////////////////////////////////////////////////
// Collision System
// Classic Dark Forces (DOS) Jedi derived Collision system. This is
// the core collision system used by Dark Forces.
//
// Copyright note:
// While the project as a whole is licensed under GPL-2.0, some of the
// code under TFE_Collision/ was derived from reverse-engineered
// code from "Dark Forces" (DOS) which is copyrighted by LucasArts.
//
// I consider the reverse-engineering to be "Fair Use" - a means of 
// supporting the games on other platforms and to improve support on
// existing platforms without claiming ownership of the games
// themselves or their IPs.
//
// That said using this code in a commercial project is risky without
// permission of the original copyright holders (LucasArts).
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/Math/core_math.h>

struct RSector;
struct SecObject;
struct RWall;

struct CollisionInterval
{
	fixed16_16 x0;
	fixed16_16 x1;
	fixed16_16 y0;
	fixed16_16 y1;
	fixed16_16 z0;
	fixed16_16 z1;
	fixed16_16 move;
	fixed16_16 dirX;
	fixed16_16 dirZ;
};

struct ColObject
{
	union
	{
		SecObject* obj;
		RSector* sector;
		RWall* wall;
		void* ptr;
	};
};

typedef void(*CollisionEffectFunc)(SecObject*);

namespace TFE_Jedi
{
	void collision_getHitPoint(fixed16_16* x, fixed16_16* z);
	RSector* collision_tryMove(RSector* sector, fixed16_16 x0, fixed16_16 z0, fixed16_16 x1, fixed16_16 z1);
	RSector* collision_moveObj(SecObject* obj, fixed16_16 dx, fixed16_16 dz);
	RWall* collision_pathWallCollision(RSector* sector);
	RWall* collision_wallCollisionFromPath(RSector* sector, fixed16_16 srcX, fixed16_16 srcZ, fixed16_16 dstX, fixed16_16 dstZ);

	SecObject* collision_getObjectCollision(RSector* sector, CollisionInterval* interval, SecObject* prevObj);
	JBool collision_isAnyObjectInRange(RSector* sector, fixed16_16 radius, vec3_fixed origin, SecObject* skipObj, u32 entityFlags);

	void collision_effectObjectsInRange3D(RSector* startSector, fixed16_16 range, vec3_fixed origin, CollisionEffectFunc effectFunc, SecObject* excludeObj, u32 entityFlags);
	void collision_effectObjectsInRangeXZ(RSector* startSector, fixed16_16 range, vec3_fixed origin, CollisionEffectFunc effectFunc, SecObject* excludeObj, u32 entityFlags);

	// Variables
	extern fixed16_16 s_colObjOverlap;
	extern s32 s_collisionFrameWall;
}
