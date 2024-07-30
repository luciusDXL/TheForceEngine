#include "guidelines.h"
#include "error.h"
#include "levelEditor.h"
#include "infoPanel.h"
#include "sharedState.h"
#include <TFE_Editor/editor.h>
#include <TFE_Editor/errorMessages.h>
#include <TFE_Editor/editorConfig.h>
#include <TFE_Editor/AssetBrowser/assetBrowser.h>
#include <TFE_Editor/EditorAsset/editorTexture.h>
#include <TFE_Editor/EditorAsset/editorFrame.h>
#include <TFE_Editor/EditorAsset/editorSprite.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_System/system.h>
#include <TFE_System/parser.h>
#include <TFE_System/math.h>
#include <TFE_Input/input.h>

#include <algorithm>
#include <vector>
#include <string>
#include <map>

using namespace TFE_Editor;

namespace LevelEditor
{
	s32 s_hoveredGuideline = -1;
	s32 s_curGuideline = -1;

	void clearGuidelineSelection()
	{
		s_hoveredGuideline = -1;
		s_curGuideline = -1;
	}
}
