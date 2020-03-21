#include "gameMain.h"
#include "gameLoop.h"
#include "player.h"
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
#include <DXL2_Settings/settings.h>
#include <assert.h>
#include <vector>
#include <algorithm>

namespace DXL2_GameMain
{
	static s32 s_curLevel;
	static s32 s_levelCount;
	static DXL2_Renderer* s_renderer;

	void startLevel();

	void init(DXL2_Renderer* renderer)
	{
		s_curLevel = 0;
		s_renderer = renderer;

		DXL2_GameMessages::load();
		DXL2_LevelList::load();

		s_levelCount = DXL2_LevelList::getLevelCount();

		// For now just start the first level...
		startLevel();
	}

	GameUpdateState loop()
	{
		GameUpdateState state = DXL2_GameLoop::update();
		DXL2_GameLoop::draw();

		if (state == GSTATE_NEXT && s_curLevel + 1 < s_levelCount)
		{
			// Go to the next level in the list.
			s_curLevel++;
			startLevel();
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
	}
}
