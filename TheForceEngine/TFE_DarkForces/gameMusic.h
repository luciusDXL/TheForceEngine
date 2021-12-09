#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Game Music
// Handles music states and iMuse callbacks.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_FileSystem/paths.h>

enum MusicState
{
	MUS_STATE_NULLSTATE = 0,
	MUS_STATE_BOSS,
	MUS_STATE_FIGHT,
	MUS_STATE_ENGAGE,
	MUS_STATE_STALK,
	MUS_STATE_EXPLORE,
	MUS_STATE_UNDEFINED
};

namespace TFE_DarkForces
{
	void gameMusic_start(s32 level);
	void gameMusic_stop();

	void gameMusic_setState(MusicState state);
	void gameMusic_startFight();
	void gameMusic_sustainFight();

	MusicState gameMusic_getState();
}  // TFE_DarkForces