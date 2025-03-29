#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Archive/archive.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Game/igame.h>
#include <vector>

struct ImVec4;

namespace TFE_Editor
{
	struct Asset;

	typedef std::vector<u8> WorkBuffer;
	struct RecentProject
	{
		std::string name;
		std::string path;
	};

	struct Sublist
	{
		f32 xOrigin;
		f32 tabSize;
		f32 width;
	};

	enum FontType
	{
		FONT_SMALL = 0,
		FONT_LARGE,
		FONT_COUNT,
	};

	enum EditorTextColor
	{
		TEXTCLR_NORMAL = 0,
		TEXTCLR_TITLE_ACTIVE,
		TEXTCLR_TITLE_INACTIVE,
		TEXTCLR_COUNT
	};

	enum EditorPopup
	{
		POPUP_NONE = 0,
		POPUP_MSG_BOX,
		POPUP_CONFIG,
		POPUP_RESOURCES,
		POPUP_NEW_PROJECT,
		POPUP_EDIT_PROJECT,
		POPUP_EXPORT_PROJECT,
		POPUP_NEW_LEVEL,
		POPUP_BROWSE,
		POPUP_CATEGORY,
		POPUP_EDIT_INF,
		POPUP_GROUP_NAME,
		POPUP_LIGHTING,
		POPUP_LEV_USER_PREF,
		POPUP_LEV_TEST_OPTIONS,
		POPUP_HISTORY_VIEW,
		POPUP_FIND_SECTOR,
		POPUP_SNAPSHOTS,
		POPUP_TEX_SOURCES,
		POPUP_RELOAD_CONFIRM,
		POPUP_EXIT_SAVE_CONFIRM,
		POPUP_COUNT
	};

	enum IconId
	{
		ICON_SELECT = 0,
		ICON_BOX_CENTER,
		ICON_CURVE,
		ICON_BLANK1,		// Blank spot.
		ICON_EYE,
		ICON_EYE_CLOSED,
		ICON_UNLOCKED,
		ICON_LOCKED,
		ICON_CIRCLE,
		ICON_CIRCLE_DOT,
		ICON_CIRCLE_QUESTION,
		ICON_CIRCLE_X,
		ICON_CIRCLE_BAN,
		ICON_CIRCLE_PLUS,
		ICON_STOP,
		ICON_PLAY,
		ICON_DRAW,
		ICON_VERTICES,
		ICON_EDGES,
		ICON_CUBE,
		ICON_ENTITY,
		ICON_MOVE,
		ICON_ROTATE,
		ICON_SCALE,
		ICON_EYE_DROPPER,
		ICON_IMAGE,
		ICON_MINUS,
		ICON_PLUS,
		ICON_MINUS_SMALL,
		ICON_PLUS_SMALL,
		ICON_GUIDELINES,
		ICON_NOTES,
		ICON_REMOVE_BEFORE,
		ICON_REMOVE_AFTER,
		ICON_TRASHCAN,
		ICON_COUNT
	};
	typedef void(*PopupEndCallback)(EditorPopup);

	#define LIST_SELECT(label, arr, index) listSelection(label, arr, IM_ARRAYSIZE(arr), (s32*)&index)
		
	void enable();
	bool disable();
	bool update(bool consoleOpen = false, bool minimized = false, bool exiting = false);
	bool render();
	TextureGpu* getIconAtlas();

	void pushFont(FontType type);
	void popFont();

	void showMessageBox(const char* type, const char* msg, ...);
	void openEditorPopup(EditorPopup popup, u32 userData = 0, void* userPtr = nullptr);
	void hidePopup();
	void showPopup();
	void setPopupEndCallback(PopupEndCallback callback = nullptr);
	EditorPopup getCurrentPopup();
	void listSelection(const char* labelText, const char** listValues, size_t listLen, s32* index, s32 comboOffset=96, s32 comboWidth=0);
	void setTooltip(const char* msg, ...);
	// icon: Icon from IconId, which icon to display.
	// inst: if there are multiple icons of the same type visible, which index. This is used to differentiate between multiple buttons with the same icon.
	// tooltip: optional tooltip when hovering over the button.
	// highlight: whether the button is highlighted even when not pressed or moused over.
	// tint: optional tint applied to the image.
	bool iconButton(IconId icon, const char* tooltip = nullptr, bool highlight = false, const f32* tint = nullptr);
	bool iconButtonInline(IconId icon, const char* tooltip=nullptr, const f32* tint=nullptr, bool small=false);
	void sectionHeader(const char* text);
	void optionCheckbox(const char* name, u32* flags, u32 flagValue, s32 width);
	void optionSliderEditFloat(const char* name, const char* precision, f32* value, f32 minValue, f32 maxValue, f32 step);

	// List of selectables inside of another list.
	void sublist_begin(Sublist& subList, f32 tabSize, f32 itemWidth);
	bool sublist_item(const Sublist& subList, const char* name, bool selected);
	void sublist_end(const Sublist& subList);

	void editor_clearUid();
	const char* editor_getUniqueLabel(const char* label);
	s32 editor_getUniqueId();
	bool editor_button(const char* label);

	TextureGpu* loadGpuImage(const char* path);

	void enableAssetEditor(Asset* asset);
	void disableAssetEditor();
	void disableNextItem();
	void enableNextItem();

	// Resizable temporary memory.
	WorkBuffer& getWorkBuffer();
	ArchiveType getArchiveType(const char* filename);
	Archive* getArchive(const char* name, GameID gameId);
	void getTempDirectory(char* tmpDir);
	bool editorStringFilter(const char* str, const char* filter, size_t filterLength);

	void clearRecents();
	void addToRecents(const char* path, bool saveConfig = true);
	void removeFromRecents(const char* path);
	std::vector<RecentProject>* getRecentProjects();
	
	ImVec4 getTextColor(EditorTextColor color);
	bool getMenuActive();
	bool isPopupOpen();

	Vec2i getEditorVersion();
	bool mouseInsideItem();

	bool isInAssetEditor();
	bool isProgramExiting();
}
