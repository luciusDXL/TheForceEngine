#pragma once
//////////////////////////////////////////////////////////////////////
// DarkXL 2 Player
// This is the player object, someday there may be more than one. :)
//////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>
#include <DXL2_Asset/infAsset.h>

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

	bool m_headlampOn;
	bool m_nightVisionOn;
	bool m_shooting;
	bool m_onScrollFloor;
};
