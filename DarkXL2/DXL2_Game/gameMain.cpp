#include "gameMain.h"
#include "gameLoop.h"
#include "player.h"
#include <DXL2_Game/GameUI/gameUi.h>
#include <DXL2_System/system.h>
#include <DXL2_Renderer/renderer.h>
#include <DXL2_FileSystem/paths.h>
#include <DXL2_Asset/textureAsset.h>
#include <DXL2_Asset/fontAsset.h>
#include <DXL2_Asset/paletteAsset.h>
#include <DXL2_Asset/colormapAsset.h>
#include <DXL2_Asset/gameMessages.h>
#include <DXL2_Asset/levelAsset.h>
#include <DXL2_Asset/levelList.h>
#include <DXL2_Asset/gmidAsset.h>
#include <DXL2_Audio/audioSystem.h>
#include <DXL2_Audio/midiPlayer.h>
#include <DXL2_Settings/settings.h>
#include <assert.h>
#include <vector>
#include <algorithm>

namespace DXL2_GameMain
{
	static s32 s_curLevel;
	static s32 s_levelCount;
	static DXL2_Renderer* s_renderer;

	static const GMidiAsset* s_levelStalk;
	static const GMidiAsset* s_levelFight;
	static const GMidiAsset* s_levelBoss;

	void startLevel();

	void init(DXL2_Renderer* renderer)
	{
		s_curLevel = 0;
		s_renderer = renderer;

		DXL2_GameMessages::load();
		DXL2_LevelList::load();

		s_levelCount = DXL2_LevelList::getLevelCount();

		DXL2_GameUi::openAgentMenu();
	}

	GameUpdateState loop()
	{
		GameUpdateState state = DXL2_GameLoop::update();
		DXL2_GameLoop::draw();

		if (state == GSTATE_NEXT && s_curLevel + 1 < s_levelCount)
		{
			// Stop the sounds.
			DXL2_Audio::stopAllSounds();
			DXL2_MidiPlayer::stop();

			// Go to the next level in the list.
			s_curLevel++;
			startLevel();
		}
		else if (state == GSTATE_SELECT_LEVEL)
		{
			// Stop the sounds.
			DXL2_Audio::stopAllSounds();
			DXL2_MidiPlayer::stop();

			// Start the selected level.
			s32 difficulty = 1;
			s_curLevel = DXL2_GameUi::getSelectedLevel(&difficulty);
			startLevel();
		}
		else if (state == GSTATE_ABORT)
		{
			DXL2_Audio::stopAllSounds();
			DXL2_MidiPlayer::stop();
			DXL2_GameUi::reset();

			DXL2_GameUi::openAgentMenu();
			state = GSTATE_CONTINUE;
		}

		return state;
	}

	//////////////////////////////
	void startLevel()
	{
		const DXL2_Settings_Graphics* config = DXL2_Settings::getGraphicsSettings();

		StartLocation start = {};
		start.overrideStart = false;

		char levelPath[256];
		sprintf(levelPath, "%s.LEV", DXL2_LevelList::getLevelFileName(s_curLevel));

		DXL2_LevelAsset::load(levelPath);
		LevelData* level = DXL2_LevelAsset::getLevelData();
		DXL2_GameLoop::startLevel(level, start, s_renderer, config->gameResolution.x, config->gameResolution.z, true);

		char stalkTrackName[64];
		char fightTrackName[64];
		char bossTrackName[64];
		sprintf(stalkTrackName, "STALK-%02d.GMD", s_curLevel + 1);
		sprintf(fightTrackName, "FIGHT-%02d.GMD", s_curLevel + 1);
		sprintf(bossTrackName,  "BOSS-%02d.GMD",  s_curLevel + 1);

		s_levelStalk = DXL2_GmidAsset::get(stalkTrackName);
		s_levelFight = DXL2_GmidAsset::get(fightTrackName);
		s_levelBoss  = DXL2_GmidAsset::get(bossTrackName);
		DXL2_MidiPlayer::playSong(s_levelStalk, true);
	}
}
