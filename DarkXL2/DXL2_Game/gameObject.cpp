#pragma once
//////////////////////////////////////////////////////////////////////
// DarkXL 2 Player
// This is the player object, someday there may be more than one. :)
//////////////////////////////////////////////////////////////////////
#include "gameObject.h"

namespace LevelGameObjects
{
	static GameObjectList s_objects;
	static SectorObjectList s_sectorObjects;

	GameObjectList* getGameObjectList()
	{
		return &s_objects;
	}

	SectorObjectList* getSectorObjectList()
	{
		return &s_sectorObjects;
	}
}
