#pragma once
//////////////////////////////////////////////////////////////////////
// DarkXL 2 Player
// This is the player object, someday there may be more than one. :)
//////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>
#include <DXL2_Asset/levelObjectsAsset.h>
#include <vector>

struct Sprite;
struct Frame;
struct Model;

enum CollisionFlags
{
	COLLIDE_NONE = 0,
	COLLIDE_TRIGGER = (1 << 0),		// Collision is not solid but triggers a pickup or event message.
	COLLIDE_PLAYER = (1 << 1),		// Collides with the player.
	COLLIDE_ENEMY = (1 << 2),		// Only collides with enemies.
};

struct GameObject
{
	ObjectClass oclass;

	// Render
	union
	{
		Sprite* sprite;
		Frame* frame;
		Model* model;
	};
	bool fullbright;
	bool show;

	// General
	u32 id;
	s32 sectorId;
	Vec3f pos;
	Vec3f angles;
	
	// Animation
	s16 animId;
	s16 frameIndex;

	// Logic
	u32 flags;
	u32 messageMask;
	s32 hp;
	s32 state;
	f32 time;			// generic time field, useful for animations and other effects requiring timing.

	// Collision
	u32 collisionFlags;
	f32 collisionRadius;
	f32 collisionHeight;
};

struct SectorObjects
{
	std::vector<u32> list;
};

typedef std::vector<GameObject> GameObjectList;
typedef std::vector<SectorObjects> SectorObjectList;

namespace LevelGameObjects
{
	GameObjectList* getGameObjectList();
	SectorObjectList* getSectorObjectList();
}
