#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/parser.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Editor/EditorAsset/editorObj3D.h>
#include <vector>
#include <string>

namespace LevelEditor
{
	// Default = 2D + 3D + fade in 3D.
	enum LevelNoteFlags
	{
		LNF_NONE       = 0,
		LNF_2D_ONLY    = FLAG_BIT(0),
		LNF_3D_NO_FADE = FLAG_BIT(1),
		LNF_TEXT_ALWAYS_SHOW = FLAG_BIT(2),
	};

	struct LevelNote
	{
		// Group & Flags.
		u32 groupId = 0;
		u32 groupIndex = 0;
		u32 flags = LNF_NONE;

		// 3D position.
		Vec3f pos = { 0 };
		Vec2f fade = { 75.0f, 100.0f };

		u32 iconColor = 0xff80ff80;
		u32 textColor = 0xffffffff;

		// Note text in Markdown.
		std::string note;
	};

	extern const f32 c_levelNoteRadius;
	extern const u32 c_noteColors[3];
	extern s32 s_hoveredLevelNote;
	extern s32 s_curLevelNote;

	void clearLevelNoteSelection();
}
