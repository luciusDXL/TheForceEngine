#include "cutscene.h"
#include "cutscene_player.h"
#include "lsystem.h"
#include "lcanvas.h"
#include <TFE_Game/igame.h>
#include <TFE_System/system.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Renderer/virtualFramebuffer.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_System/parser.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	static JBool s_playing = JFALSE;

	CutsceneState* s_playSeq = nullptr;
	s32 s_soundVolume = 0;
	s32 s_musicVolume = 0;
	s32 s_enabled = 1;

	void cutscene_init(CutsceneState* cutsceneList)
	{
		s_playSeq = cutsceneList;
		s_playing = JFALSE;
	}

	JBool cutscene_play(s32 sceneId)
	{
		if (!s_enabled || !s_playSeq) { return JFALSE; }

		// Search for the requested scene.
		s32 found = 0;
		for (s32 i = 0; !found && s_playSeq[i].id != SCENE_EXIT; i++)
		{
			if (s_playSeq[i].id == sceneId)
			{
				found = 1;
				break;
			}
		}
		if (!found) return JFALSE;
		// Re-initialize the canvas, so cutscenes run at the correct resolution even if it was changed for gameplay
		// (i.e. high resolution support).
		lcanvas_init(320, 200);
		
		// The original code then starts the cutscene loop here, and then returns when done.
		// Instead we set a bool and then the calling code will call 'update' until it returns false.
		s_playing = JTRUE;
		cutscenePlayer_start(sceneId);
		return JTRUE;
	}

	JBool cutscene_update()
	{
		if (!s_playing) { return JFALSE; }

		s_playing = cutscenePlayer_update();
		return s_playing;
	}

	void cutscene_enable(s32 enable)
	{
		s_enabled = enable;
	}

	s32 cutscene_isEnabled()
	{
		return s_enabled;
	}

	void cutscene_setSoundVolume(s32 volume)
	{
		s_soundVolume = clamp(volume, 0, 127);
	}

	void cutscene_setMusicVolume(s32 volume)
	{
		s_musicVolume = clamp(volume, 0, 127);
	}

	s32 cutscene_getSoundVolume()
	{
		return s_soundVolume;
	}

	s32 cutscene_getMusicVolume()
	{
		return s_musicVolume;
	}
}  // TFE_DarkForces