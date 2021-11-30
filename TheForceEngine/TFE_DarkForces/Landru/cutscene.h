#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Cutscene
// This manages loading assets and playback.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "cutsceneList.h"

namespace TFE_DarkForces
{
	enum CutsceneConstants
	{
		SCENE_ENTER = -10,
		SCENE_EXIT  =   0,
	};

	void cutscene_init(CutsceneState* cutsceneList);

	JBool cutscene_play(s32 sceneId);
	JBool cutscene_update();
	void  cutscene_enable(s32 enable);
	s32   cutscene_isEnabled();

	void cutscene_setSoundVolume(s32 volume);
	void cutscene_setMusicVolume(s32 volume);

	s32  cutscene_getSoundVolume();
	s32  cutscene_getMusicVolume();
}  // TFE_DarkForces