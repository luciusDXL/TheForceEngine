#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Player
// This is the player object, someday there may be more than one. :)
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Asset/levelObjectsAsset.h>
#include <vector>

struct Sprite;
struct Frame;
struct Model;
struct SoundBuffer;
struct SoundSource;

enum CollisionFlags
{
	COLLIDE_NONE = 0,
	COLLIDE_TRIGGER = (1 << 0),		// Collision is not solid but triggers a pickup or event message.
	COLLIDE_PLAYER = (1 << 1),		// Collides with the player.
	COLLIDE_ENEMY = (1 << 2),		// Only collides with enemies.
};

enum PhysicsFlags
{
	PHYSICS_NONE    = 0,
	PHYSICS_GRAVITY = (1 << 0),		// The object will fall due to gravity.
	PHYSICS_BOUNCE  = (1 << 1),		// The object will bounce when it hits a surface.
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
		SoundBuffer* buffer;
	};
	bool fullbright;
	bool show;

	// General
	u32 id;
	s32 sectorId;
	f32 verticalVel;
	f32 zbias;
	Vec3f pos;
	Vec3f angles;
	Vec3f scale;
	
	// Animation
	s16 animId;
	s16 frameIndex;

	// Logic
	u32 flags;
	u32 messageMask;
	s32 hp;
	s32 state;
	f32 time;			// generic time field, useful for animations and other effects requiring timing.

	// Common Variables.
	u32 comFlags = 0;		// common flags - see LogicCommonFlags.
	f32 radius = 0.0f;
	f32 height = 0.0f;

	// Collision
	u32 collisionFlags;
	f32 collisionRadius;
	f32 collisionHeight;

	// Physics
	u32 physicsFlags;

	// Vue Transform.
	const Mat3* vueTransform;

	// Sound Source.
	SoundSource* source;
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
