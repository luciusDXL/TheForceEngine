#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Player
// This is the player object, someday there may be more than one. :)
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_Asset/infAsset.h>

#if 0  // Support for multiple games
// Game-specific player data.
struct GameData
{
	u32 gameId;
	u32 sizeInBytes;
	u32 flags;			// Game specific flags.
	// Game specific
};

struct DarkForcesData
{
	s32 m_shields;
	s32 m_lives;
	s32 m_dtCount;
	//...
};

struct OutlawsData
{
	//...
};
#endif

class Player
{
public:
	Player();
	~Player();

public:
	Vec3f pos;
	Vec3f vel;
	f32 m_yaw;
	f32 m_pitch;
	s32 m_sectorId;

	s32 m_shields;
	s32 m_health;
	s32 m_lives;
	s32 m_ammo;
	s32 m_dtCount;	// -1 = no DT.

	u32 m_keys;

	bool m_headlampOn;		// TODO: Convert to stateFlags - General (shooting, onScrollFloor) and Game Specific (headlampOn, nightVisionOn).
	bool m_nightVisionOn;
	bool m_shooting;
	bool m_onScrollFloor;
};
