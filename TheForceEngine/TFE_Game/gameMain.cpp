#include "gameMain.h"
#include "gameLoop.h"
#include "player.h"
#include <TFE_Game/GameUI/gameUi.h>
#include <TFE_System/system.h>
#include <TFE_Renderer/renderer.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Asset/textureAsset.h>
#include <TFE_Asset/fontAsset.h>
#include <TFE_Asset/paletteAsset.h>
#include <TFE_Asset/colormapAsset.h>
#include <TFE_Asset/gameMessages.h>
#include <TFE_Asset/levelAsset.h>
#include <TFE_Asset/levelList.h>
#include <TFE_Asset/gmidAsset.h>
#include <TFE_Audio/audioSystem.h>
#include <TFE_Audio/midiPlayer.h>
#include <TFE_Settings/settings.h>
#include <assert.h>
#include <vector>
#include <algorithm>

namespace TFE_GameMain
{
	static s32 s_curLevel;
	static s32 s_levelCount;
	static TFE_Renderer* s_renderer;

	static GameState   s_gameState   = GAME_TITLE;
	static GameOverlay s_gameOverlay = OVERLAY_NONE;

	static const GMidiAsset* s_levelStalk;
	static const GMidiAsset* s_levelFight;
	static const GMidiAsset* s_levelBoss;

	void startLevel();

	void init(TFE_Renderer* renderer)
	{
		s_curLevel = 0;
		s_renderer = renderer;

		TFE_GameMessages::load();
		TFE_LevelList::load();

		s_levelCount = TFE_LevelList::getLevelCount();

		TFE_GameUi::openAgentMenu();
	}

	void transitionToAgentMenu()
	{
		TFE_Audio::stopAllSounds();
		TFE_MidiPlayer::stop();
		TFE_GameUi::reset();

		TFE_GameUi::openAgentMenu();

		s_gameState = GAME_AGENT_MENU;
		s_gameOverlay = OVERLAY_NONE;
	}

	void transitionToLevel(bool incrLevel)
	{
		TFE_Audio::stopAllSounds();
		TFE_MidiPlayer::stop();

		// Start the selected level.
		s32 difficulty = 1;
		if (incrLevel)
		{
			s_curLevel++;
		}
		else
		{
			s_curLevel = TFE_GameUi::getSelectedLevel(&difficulty);
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

	GameTransition loop(bool consoleOpen)
	{
		GameTransition trans = TFE_GameLoop::update(consoleOpen, s_gameState, s_gameOverlay);
		TFE_GameLoop::draw();

		// Update the current state.
		trans = applyTransition(trans);

		return trans;
	}

	//////////////////////////////
	void startLevel()
	{
		const TFE_Settings_Graphics* config = TFE_Settings::getGraphicsSettings();

		StartLocation start = {};
		start.overrideStart = false;

		char levelPath[256];
		sprintf(levelPath, "%s.LEV", TFE_LevelList::getLevelFileName(s_curLevel));

		TFE_LevelAsset::load(levelPath);
		LevelData* level = TFE_LevelAsset::getLevelData();
		TFE_GameLoop::startLevel(level, start, s_renderer, config->gameResolution.x, config->gameResolution.z, true);

		char stalkTrackName[64];
		char fightTrackName[64];
		char bossTrackName[64];
		sprintf(stalkTrackName, "STALK-%02d.GMD", s_curLevel + 1);
		sprintf(fightTrackName, "FIGHT-%02d.GMD", s_curLevel + 1);
		sprintf(bossTrackName,  "BOSS-%02d.GMD",  s_curLevel + 1);

		s_levelStalk = TFE_GmidAsset::get(stalkTrackName);
		s_levelFight = TFE_GmidAsset::get(fightTrackName);
		s_levelBoss  = TFE_GmidAsset::get(bossTrackName);
		TFE_MidiPlayer::playSong(s_levelStalk, true);
	}
}
