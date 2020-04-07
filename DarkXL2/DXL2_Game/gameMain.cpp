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

	static GameState   s_gameState   = GAME_TITLE;
	static GameOverlay s_gameOverlay = OVERLAY_NONE;

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

	void transitionToAgentMenu()
	{
		DXL2_Audio::stopAllSounds();
		DXL2_MidiPlayer::stop();
		DXL2_GameUi::reset();

		DXL2_GameUi::openAgentMenu();

		s_gameState = GAME_AGENT_MENU;
		s_gameOverlay = OVERLAY_NONE;
	}

	void transitionToLevel(bool incrLevel)
	{
		DXL2_Audio::stopAllSounds();
		DXL2_MidiPlayer::stop();

		// Start the selected level.
		s32 difficulty = 1;
		if (incrLevel)
		{
			s_curLevel++;
		}
		else
		{
			s_curLevel = DXL2_GameUi::getSelectedLevel(&difficulty);
		}
		startLevel();

		s_gameState = GAME_LEVEL;
		s_gameOverlay = OVERLAY_NONE;
	}

	GameTransition applyTransition(GameTransition trans)
	{
		// We get back the next transition.
		switch (trans)
		{
			case TRANS_NONE:
			{
				// No change.
			} break;
			case TRANS_START_GAME:
			{
				// TODO: Cutscenes.
				// startCutscene(Title);

				// Cutscenes are not implemented yet, so just go straight to the Agent Menu.
				transitionToAgentMenu();
				trans = TRANS_NONE;
			} break;
			case TRANS_TO_AGENT_MENU:
			{
				transitionToAgentMenu();
				trans = TRANS_NONE;
			} break;
			case TRANS_TO_PREMISSION_CUTSCENE:
			{
				// TODO: Cutscenes.
				// startCutscene(Title);

				// 
			} break;
			case TRANS_TO_MISSION_BRIEFING:
			{
				// TODO: Mission Briefing
			} break;
			case TRANS_START_LEVEL:
			{
				transitionToLevel(false);
				trans = TRANS_NONE;
			} break;
			case TRANS_TO_POSTMISSION_CUTSCENE:
			{
				// TODO: Cutscenes
			} break;
			case TRANS_NEXT_LEVEL:
			{
				transitionToLevel(true);
				trans = TRANS_NONE;
			} break;
			case TRANS_QUIT:
			{
				return TRANS_QUIT;
			} break;
			case TRANS_RETURN_TO_FRONTEND:
			{
				return TRANS_RETURN_TO_FRONTEND;
			} break;
			default:
			{
				assert(0);
			}
		};

		return trans;
	}

	GameTransition loop()
	{
		GameTransition trans = DXL2_GameLoop::update(s_gameState, s_gameOverlay);
		DXL2_GameLoop::draw();

		// Update the current state.
		trans = applyTransition(trans);

		return trans;
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
