#include "player.h"

Player::Player()
{
	m_shields = 100;
	m_health = 100;
	m_lives = 3;
	m_ammo = 100;
	m_dtCount = -1;	// -1 = no DT.
	m_shooting = false;
	m_onScrollFloor = false;
}

Player::~Player()
{
}
