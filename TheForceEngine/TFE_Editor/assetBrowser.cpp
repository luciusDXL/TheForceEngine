#include "assetBrowser.h"
#include "editorAsset.h"
#include "editorTexture.h"
#include "editorFrame.h"
#include "editorSprite.h"
#include "editorConfig.h"
#include "editor.h"
#include <TFE_DarkForces/mission.h>
#include <TFE_FrontEndUI/frontEndUi.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/system.h>
#include <TFE_Settings/settings.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Archive/archive.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/parser.h>
#include <TFE_Ui/ui.h>

#include <TFE_Ui/imGUI/imgui.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

using namespace TFE_Editor;

namespace AssetBrowser
{
	enum UIConst : u32
	{
		INFO_PANEL_WIDTH = 524u,
	};
	const char* c_games[] =
	{
		"Dark Forces",
		"Outlaws",
	};

	struct Asset
	{
		AssetType type;
		GameID gameId;
		std::string name;
		std::string archiveName;
		std::string filePath;

		AssetHandle handle;
	};
	typedef std::vector<Asset> AssetList;

	struct Palette
	{
		std::string name;
		u32 data[256];
		// Colormap
		bool hasColormap;
		u8 colormap[256 * 32];
	};

	struct LevelAssets
	{
		std::string levelName;
		std::string paletteName;
		std::vector<std::string> textures;
		std::vector<std::string> frames;
		std::vector<std::string> sprites;
		std::vector<std::string> pods;
	};
	std::vector<LevelAssets> s_levelAssets;

	static bool s_reloadProjectAssets = true;
	static bool s_assetsNeedProcess = true;
	static std::vector<Palette> s_palettes;
	static std::map<std::string, s32> s_assetPalette;
	static s32 s_defaultPal = 0;

	static s32 s_hovered = -1;
	static s32 s_selected = -1;
	static s32 s_menuHeight = 20;
	   	
	struct ViewerInfo
	{
		std::string exportPath;

		GameID game = Game_Dark_Forces;
		GameID prevGame = Game_Count;
		AssetType type = TYPE_TEXTURE;
		AssetType prevType = TYPE_NOT_SET;

		char assetFilter[64] = "";
		char prevAssetFilter[64] = "";
		s32 levelSource = -1;
		u32 filterLen;
	};
	static ViewerInfo s_viewInfo = {};
	static AssetList s_viewAssetList;
	static AssetList s_projectAssetList[TYPE_COUNT];

	// Forward Declarations
	void updateAssetList();
	void reloadAsset(Asset* asset, s32 palId, s32 lightLevel = 32);

	void init()
	{
		updateAssetList();
	}

	void destroy()
	{
		s_reloadProjectAssets = true;
		s_assetsNeedProcess = true;
		s_palettes.clear();
		s_assetPalette.clear();
		s_defaultPal = 0;

		s_viewInfo = {};
		s_viewAssetList.clear();
		for (s32 i = 0; i < TYPE_COUNT; i++)
		{
			s_projectAssetList[i].clear();
		}
		freeCachedTextures();
	}

	void label(const char* label)
	{
		ImGui::PushFont(TFE_FrontEndUI::getDialogFont());
		ImGui::LabelText("##Label", label);
		ImGui::PopFont();
	}

#define LIST_SELECT(label, arr, index) listSelection(label, arr, IM_ARRAYSIZE(arr), (s32*)&index)

	void listSelection(const char* labelText, const char** listValues, size_t listLen, s32* index)
	{
		ImGui::SetNextItemWidth(UI_SCALE(256));
		ImGui::LabelText("##Label", labelText); ImGui::SameLine(UI_SCALE(96));

		char comboId[256];
		sprintf(comboId, "##%s", labelText);
		ImGui::Combo(comboId, index, listValues, (s32)listLen);
	}

	bool paletteItemGetter(void* data, int idx, const char** out_text)
	{
		Palette* palette = (Palette*)data;
		*out_text = palette[idx].name.c_str();
		return true;
	}

	bool levelTextureGetter(void* data, int idx, const char** out_text)
	{
		static const char* allLevels = "Any";
		if (idx == 0)
		{
			*out_text = allLevels;
		}
		else
		{
			LevelAssets* textures = (LevelAssets*)data;
			*out_text = textures[idx - 1].levelName.c_str();
		}
		return true;
	}

	void listSelectionPalette(const char* labelText, std::vector<Palette>& listValues, s32* index)
	{
		ImGui::SetNextItemWidth(UI_SCALE(256));
		ImGui::LabelText("##Label", labelText); ImGui::SameLine(UI_SCALE(96));

		char comboId[256];
		sprintf(comboId, "##%s", labelText);
		ImGui::Combo(comboId, index, paletteItemGetter, listValues.data(), (s32)listValues.size());
	}

	void listSelectionLevel(const char* labelText, std::vector<LevelAssets>& listValues, s32* index)
	{
		ImGui::SetNextItemWidth(UI_SCALE(256));
		ImGui::LabelText("##Label", labelText); ImGui::SameLine(UI_SCALE(96));

		char comboId[256];
		sprintf(comboId, "##%s", labelText);
		s32 lvlIndex = (*index) + 1;
		if (ImGui::Combo(comboId, &lvlIndex, levelTextureGetter, listValues.data(), (s32)listValues.size()))
		{
			*index = lvlIndex - 1;
		}
	}
		
	void infoPanel(u32 infoWidth, u32 infoHeight)
	{
		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);
		u32 w = infoWidth;
		u32 h = infoHeight;

		ImGui::SetWindowPos("Asset Browser##Settings", { 0.0f, f32(s_menuHeight) });
		ImGui::SetWindowSize("Asset Browser##Settings", { f32(w), f32(h) });
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

		bool active = true;
		s32 levelSource = s_viewInfo.levelSource;
		ImGui::Begin("Asset Browser##Settings", &active, window_flags);
		LIST_SELECT("Game", c_games, s_viewInfo.game);
		LIST_SELECT("Asset Type", c_assetType, s_viewInfo.type);
		listSelectionLevel("Level", s_levelAssets, &levelSource);
		ImGui::Separator();

		bool listChanged = false;
		if (s_viewInfo.game != s_viewInfo.prevGame || s_viewInfo.type != s_viewInfo.prevType || levelSource != s_viewInfo.levelSource)
		{ 
			listChanged = true;
			s_viewInfo.levelSource = levelSource;
			s_selected = -1;
			s_hovered = -1;
		}
		s_viewInfo.prevGame = s_viewInfo.game;
		s_viewInfo.prevType = s_viewInfo.type;

		// Filter
		ImGui::LabelText("##Label", "Filter"); ImGui::SameLine(UI_SCALE(96));
		ImGui::SetNextItemWidth((f32)UI_SCALE(256));
		ImGui::InputText("##FilterText", s_viewInfo.assetFilter, 64);
		ImGui::SameLine(UI_SCALE(358));
		if (ImGui::Button("CLEAR"))
		{
			s_viewInfo.assetFilter[0] = 0;
			s_viewInfo.filterLen = 0;
		}
		else
		{
			s_viewInfo.filterLen = (u32)strlen(s_viewInfo.assetFilter);
		}
		if (s_viewInfo.prevAssetFilter[0] != s_viewInfo.assetFilter[0] || strlen(s_viewInfo.prevAssetFilter) != s_viewInfo.filterLen ||
			strcasecmp(s_viewInfo.prevAssetFilter, s_viewInfo.assetFilter))
		{
			strcpy(s_viewInfo.prevAssetFilter, s_viewInfo.assetFilter);
			listChanged = true;
		}

		ImGui::End();

		if (listChanged)
		{
			updateAssetList();
		}
	}

	void drawInfoPanel(Asset* asset, u32 infoWidth, u32 infoHeight)
	{
		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		u32 w = std::min(infoWidth,  displayInfo.width);
		u32 h = std::max(256u, displayInfo.height - (infoHeight + s_menuHeight));

		ImGui::SetWindowPos("Asset Info", { 0.0f, f32(infoHeight + s_menuHeight) });
		ImGui::SetWindowSize("Asset Info", { f32(w), f32(h) });
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

		bool active = true;
		ImGui::Begin("Asset Info", &active, window_flags);
		if (asset)
		{
			ImGui::LabelText("##Name", "Name: %s", asset->name.c_str());
			ImGui::LabelText("##Type", "Type: %s", c_assetType[asset->type]);

			if (asset->type == TYPE_TEXTURE)
			{
				EditorTexture* tex = (EditorTexture*)getAssetData(asset->handle);

				ImGui::LabelText("##Size", "Size: %d x %d", tex->width, tex->height);

				bool reloadRequired = false;
				s32 newPaletteIndex = tex->paletteIndex;
				listSelectionPalette("Palette", s_palettes, &newPaletteIndex);
				if (newPaletteIndex != tex->paletteIndex)
				{
					reloadRequired = true;
				}

				s32 newLightLevel = tex->lightLevel;
				if (s_palettes[newPaletteIndex].hasColormap)
				{
					ImGui::SliderInt("Light Level", &newLightLevel, 0, 32);
				}
				else
				{
					newLightLevel = 32;
				}
				if (newLightLevel != tex->lightLevel)
				{
					reloadRequired = true;
				}

				if (reloadRequired)
				{
					// Make sure the frame isn't reset when reloading the texture.
					s32 curFrame = tex->curFrame;
					reloadAsset(asset, newPaletteIndex, newLightLevel);
					tex = (EditorTexture*)getAssetData(asset->handle);
					tex->curFrame = curFrame;
				}

				ImGui::LabelText("##Frames", "Frame Count: %d", tex->frameCount);
				if (tex->frameCount > 1)
				{
					ImGui::SliderInt("Frame", &tex->curFrame, 0, tex->frameCount - 1);
				}
				else
				{
					tex->curFrame = 0;
				}

				s32 texW = tex->width;
				s32 texH = tex->height;
				s32 scale = max(1, min(4, s32(w) / texW));
				texW *= scale;
				texH *= scale;

				ImGui::Image(TFE_RenderBackend::getGpuPtr(tex->texGpu[tex->curFrame]),
					ImVec2((f32)texW, (f32)texH), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
			}
			else if (asset->type == TYPE_FRAME)
			{
				EditorFrame* frame = (EditorFrame*)getAssetData(asset->handle);

				ImGui::LabelText("##Size", "Size: %d x %d", frame->width, frame->height);
				ImGui::LabelText("##Dim",  "World Dim: %0.3f x %0.3f", frame->worldWidth, frame->worldHeight);

				bool reloadRequired = false;
				s32 newPaletteIndex = frame->paletteIndex;
				listSelectionPalette("Palette", s_palettes, &newPaletteIndex);
				if (newPaletteIndex != frame->paletteIndex)
				{
					reloadRequired = true;
				}

				s32 newLightLevel = frame->lightLevel;
				if (s_palettes[newPaletteIndex].hasColormap)
				{
					ImGui::SliderInt("Light Level", &newLightLevel, 0, 32);
				}
				else
				{
					newLightLevel = 32;
				}
				if (newLightLevel != frame->lightLevel)
				{
					reloadRequired = true;
				}

				if (reloadRequired)
				{
					reloadAsset(asset, newPaletteIndex, newLightLevel);
					frame = (EditorFrame*)getAssetData(asset->handle);
				}

				s32 texW = frame->width;
				s32 texH = frame->height;
				s32 scale = max(1, min(4, s32(w) / texW));
				texW *= scale;
				texH *= scale;

				ImGui::Image(TFE_RenderBackend::getGpuPtr(frame->texGpu),
					ImVec2((f32)texW, (f32)texH), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
			}
			else if (asset->type == TYPE_PALETTE)
			{
				EditorTexture* tex = (EditorTexture*)getAssetData(asset->handle);
				ImGui::Image(TFE_RenderBackend::getGpuPtr(tex->texGpu[0]),
					ImVec2(128.0f, 128.0f), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

				// Show the colormap if it exists.
				if (tex->frameCount == 2)
				{
					ImGui::Separator();
					ImGui::Image(TFE_RenderBackend::getGpuPtr(tex->texGpu[1]),
						ImVec2(512.0f, 64.0f), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
				}
			}
		}
		ImGui::End();
	}
		
	ImVec4 getBorderColor(s32 index)
	{
		if (index == s_selected)
		{
			return ImVec4(1.0f, 1.0f, 0.5f, 1.0f);
		}
		else if (index == s_hovered)
		{
			return ImVec4(0.5f, 1.0f, 1.0f, 1.0f);
		}
		return ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
	}

	void listPanel(u32 infoWidth, u32 infoHeight)
	{
		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);
		if (displayInfo.width < infoWidth) { return; }

		s32 w = displayInfo.width - (s32)infoWidth;
		s32 h = displayInfo.height - s_menuHeight;
		if (w <= 0 || h <= 0) { return; }

		ImGui::SetWindowPos("Asset List", { f32(infoWidth), f32(s_menuHeight) });
		ImGui::SetWindowSize("Asset List", { f32(w), f32(h) });
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

		bool active = true;
		ImGui::Begin("Asset List", &active, window_flags);
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		// Draw items
		if (!s_viewAssetList.empty())
		{
			const s32 count = (s32)s_viewAssetList.size();
			const Asset* asset = s_viewAssetList.data();

			s32 textWidth = (s32)ImGui::CalcTextSize("12345678.123").x;
			s32 fontSize  = (s32)ImGui::GetFontSize();

			char buttonLabel[32];
			s32 itemWidth  = 16 + max(textWidth, s_editorConfig.thumbnailSize);
			s32 itemHeight =  4 + fontSize + s_editorConfig.thumbnailSize;

			s32 columnCount = max(1, s32(w - 16) / itemWidth);
			f32 topPos = ImGui::GetCursorPosY();
						
			bool mouseClicked = false;
			if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
			{
				if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows))
				{
					mouseClicked = true;
					s_selected = -1;
				}
			}
			else
			{
				s_hovered = -1;
			}

			s32 a = 0;
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0.0f, 0.0f });
			for (s32 y = 0; a < count; y++)
			{
				for (s32 x = 0; x < columnCount && a < count; x++, a++)
				{
					sprintf(buttonLabel, "###asset%d", a);
					ImVec2 cursor((8.0f + x * itemWidth), topPos + y * itemHeight);
					ImGui::SetCursorPos(cursor);

					ImGui::PushStyleColor(ImGuiCol_Border, getBorderColor(a));
					if (ImGui::BeginChild(buttonLabel, ImVec2(f32(itemWidth), f32(itemHeight)), true, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollWithMouse))
					{
						s32 offsetX = 0, offsetY = 0;
						s32 width = s_editorConfig.thumbnailSize, height = s_editorConfig.thumbnailSize;

						TextureGpu* textureGpu = nullptr;
						f32 u0 = 0.0f, v0 = 1.0f;
						f32 u1 = 1.0f, v1 = 0.0f;
						if (s_viewAssetList[a].type == TYPE_TEXTURE || s_viewAssetList[a].type == TYPE_PALETTE)
						{
							EditorTexture* tex = (EditorTexture*)getAssetData(s_viewAssetList[a].handle);
							textureGpu = tex->texGpu[0];
							// Preserve the image aspect ratio.
							if (tex->width >= tex->height)
							{
								height = tex->height * s_editorConfig.thumbnailSize / tex->width;
								offsetY = (width - height) / 2;
							}
							else
							{
								width = tex->width * s_editorConfig.thumbnailSize / tex->height;
								offsetX = (height - width) / 2;
							}
						}
						else if (s_viewAssetList[a].type == TYPE_FRAME)
						{
							EditorFrame* frame = (EditorFrame*)getAssetData(s_viewAssetList[a].handle);
							textureGpu = frame->texGpu;
							// Preserve the image aspect ratio.
							if (frame->width >= frame->height)
							{
								height = frame->height * s_editorConfig.thumbnailSize / frame->width;
								offsetY = (width - height) / 2;
							}
							else
							{
								width = frame->width * s_editorConfig.thumbnailSize / frame->height;
								offsetX = (height - width) / 2;
							}
						}
						else if (s_viewAssetList[a].type == TYPE_SPRITE)
						{
							EditorSprite* sprite = (EditorSprite*)getAssetData(s_viewAssetList[a].handle);
							textureGpu = sprite->texGpu;
							SpriteCell* cell = &sprite->cell[0];
							// Preserve the image aspect ratio.
							if (cell->w >= cell->h)
							{
								height = cell->h * s_editorConfig.thumbnailSize / cell->w;
								offsetY = (width - height) / 2;
							}
							else
							{
								width = cell->w * s_editorConfig.thumbnailSize / cell->h;
								offsetX = (height - width) / 2;
							}

							u0 = f32(cell->u) / (f32)textureGpu->getWidth();
							v1 = f32(cell->v) / (f32)textureGpu->getHeight();

							u1 = f32(cell->u + cell->w) / (f32)textureGpu->getWidth();
							v0 = f32(cell->v + cell->h) / (f32)textureGpu->getHeight();
						}
						// Center image.
						offsetX += (itemWidth - s_editorConfig.thumbnailSize) / 2;

						// Draw the image.
						ImGui::SetCursorPos(ImVec2((f32)offsetX, (f32)offsetY));
						ImGui::Image(TFE_RenderBackend::getGpuPtr(textureGpu),
							ImVec2((f32)width, (f32)height), ImVec2(u0, v0), ImVec2(u1, v1));

						// Draw the label.
						ImGui::SetCursorPos(ImVec2(8.0f, (f32)s_editorConfig.thumbnailSize));
						ImGui::SetNextItemWidth(f32(itemWidth - 16));
						ImGui::LabelText("###", "%s", asset[a].name.c_str());

						if (ImGui::IsWindowHovered())
						{
							s_hovered = a;
							if (mouseClicked)
							{
								s_selected = a;
							}
						}
					}
					ImGui::EndChild();
					ImGui::PopStyleColor();
				}
			}
			ImGui::PopStyleVar();

			// Info Panel
			Asset* selectedAsset = nullptr;
			if (s_selected >= 0 && s_selected < s_viewAssetList.size())
			{
				selectedAsset = &s_viewAssetList[s_selected];
			}
			else if (s_hovered >= 0 && s_hovered < s_viewAssetList.size())
			{
				selectedAsset = &s_viewAssetList[s_hovered];
			}
			drawInfoPanel(selectedAsset, infoWidth, infoHeight);
		}

		ImGui::End();
	}

	void update()
	{
		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		// Figure out how to scale the info column and panel based on the font scale.
		s32 wScale = 100 + max(0, s_editorConfig.fontScale - 125);
		s32 hScale = 100 + (s_editorConfig.fontScale - 100) / 2;

		u32 infoWidth = std::min((u32)(INFO_PANEL_WIDTH * wScale / 100), displayInfo.width);
		u32 infoHeight = (u32)(150 * hScale / 100);
				
		// Push the "small" font and draw the panels.
		pushFont(TFE_Editor::FONT_SMALL);
		s_menuHeight = 6 + (s32)ImGui::GetFontSize();

		infoPanel(infoWidth, infoHeight);
		listPanel(infoWidth, infoHeight);

		popFont();
	}

	void render()
	{
	}

	////////////////////////////////////////////////
	// Internal
	////////////////////////////////////////////////
	Archive* getArchive(const char* name, GameID gameId)
	{
		TFE_Settings_Game* game = TFE_Settings::getGameSettings();
		char filePath[TFE_MAX_PATH];
		sprintf(filePath, "%s%s", game->header[gameId].sourcePath, name);

		return Archive::getArchive(ARCHIVE_GOB, name, filePath);
	}
		
	void addPalette(const char* name, Archive* archive)
	{
		const size_t count = s_palettes.size();
		for (size_t i = 0; i < count; i++)
		{
			if (strcasecmp(s_palettes[i].name.c_str(), name) == 0)
			{
				return;
			}
		}

		if (!archive->openFile(name)) { return; }
		WorkBuffer& buffer = getWorkBuffer();
		const size_t len = archive->getFileLength();
		buffer.resize(len);
		archive->readFile(buffer.data(), len);
		archive->closeFile();

		const u8* pal666 = buffer.data();
		Palette dstPalette;
		dstPalette.name = name;
		dstPalette.hasColormap = false;
		for (s32 i = 0; i < 256; i++)
		{
			const u8* rgb6 = &pal666[i * 3];
			dstPalette.data[i] = 0xff000000 | CONV_6bitTo8bit(rgb6[0]) | (CONV_6bitTo8bit(rgb6[1]) << 8) | (CONV_6bitTo8bit(rgb6[2]) << 16);
		}

		// Does it have a colormap?
		// For now we assume the names match.
		char colorMapPath[256];
		FileUtil::replaceExtension(name, "CMP", colorMapPath);

		const size_t cmapCount = s_projectAssetList[TYPE_COLORMAP].size();
		const Asset* projAsset = s_projectAssetList[TYPE_COLORMAP].data();
		for (size_t i = 0; i < cmapCount; i++, projAsset++)
		{
			if (strcasecmp(projAsset->name.c_str(), colorMapPath) == 0)
			{
				Archive* cmapArchive = getArchive(projAsset->archiveName.c_str(), projAsset->gameId);
				if (cmapArchive && cmapArchive->openFile(colorMapPath))
				{
					WorkBuffer& buffer = getWorkBuffer();
					const size_t len = cmapArchive->getFileLength();
					buffer.resize(len);
					cmapArchive->readFile(buffer.data(), len);
					cmapArchive->closeFile();

					memcpy(dstPalette.colormap, buffer.data(), 8192);
					dstPalette.hasColormap = true;

					cmapArchive->closeFile();
					break;
				}
			}
		}
		s_palettes.push_back(dstPalette);

		if (strcasecmp(name, "SECBASE.PAL") == 0)
		{
			s_defaultPal = (s32)s_palettes.size() - 1;
		}
	}

	s32 getPaletteId(const char* name)
	{
		const size_t count = s_palettes.size();
		for (size_t i = 0; i < count; i++)
		{
			if (strcasecmp(s_palettes[i].name.c_str(), name) == 0)
			{
				return s32(i);
			}
		}
		return -1;
	}

	void setAssetPalette(const char* name, s32 paletteId)
	{
		std::map<std::string, s32>::iterator iAssetPal = s_assetPalette.find(name);
		if (iAssetPal == s_assetPalette.end())
		{
			s_assetPalette[name] = paletteId;
		}
	}

	s32 getAssetPalette(const char* name)
	{
		std::map<std::string, s32>::iterator iAssetPal = s_assetPalette.find(name);
		if (iAssetPal != s_assetPalette.end())
		{
			return iAssetPal->second;
		}
		// Not sure how to automatically figure this out yet.
		if (strcasecmp(name, "WAIT.BM") == 0)
		{
			return 0;
		}
		return s_defaultPal;
	}
		
	void addToLevelAssets(std::vector<std::string>& assetList, const char* assetName)
	{
		const size_t count = assetList.size();
		for (size_t i = 0; i < count; i++)
		{
			if (assetList[i] == assetName)
			{
				return;
			}
		}
		assetList.push_back(assetName);
	}

	void preprocessAssets()
	{
		if (!s_assetsNeedProcess) { return; }
		s_assetsNeedProcess = false;
		s_levelAssets.clear();

		WorkBuffer& buffer = getWorkBuffer();
		const u32 count = (u32)s_projectAssetList[TYPE_PALETTE].size();

		// First search for palettes.
		const Asset* projAsset = s_projectAssetList[TYPE_PALETTE].data();
		for (u32 f = 0; f < count; f++, projAsset++)
		{
			addPalette(projAsset->name.c_str(), getArchive(projAsset->archiveName.c_str(), projAsset->gameId));
		}

		// Then for levels.
		const u32 levelCount = (u32)s_projectAssetList[TYPE_LEVEL].size();
		projAsset = s_projectAssetList[TYPE_LEVEL].data();
		for (u32 f = 0; f < levelCount; f++, projAsset++)
		{
			Archive* archive = getArchive(projAsset->archiveName.c_str(), projAsset->gameId);

			// Parse the level for 1) the palette, 2) the data lists.
			if (archive->openFile(projAsset->name.c_str()))
			{
				LevelAssets levelAssets;
				levelAssets.levelName = projAsset->name;
				s32 paletteId = s_defaultPal;

				// .LEV
				{
					size_t len = archive->getFileLength();
					buffer.resize(len);
					archive->readFile(buffer.data(), len);
					archive->closeFile();

					TFE_Parser parser;
					size_t bufferPos = 0;
					parser.init((char*)buffer.data(), buffer.size());
					parser.addCommentString("#");
					parser.convertToUpperCase(true);

					// Read just enough of the file...
					const char* line;
					line = parser.readLine(bufferPos);
					s32 versionMajor, versionMinor;
					if (sscanf(line, " LEV %d.%d", &versionMajor, &versionMinor) != 2)
					{
						continue;
					}

					char readBuffer[256];
					line = parser.readLine(bufferPos);
					if (sscanf(line, " LEVELNAME %s", readBuffer) != 1)
					{
						continue;
					}

					// This gets read here just to be overwritten later... so just ignore for now.
					line = parser.readLine(bufferPos);
					if (sscanf(line, " PALETTE %s", readBuffer) != 1)
					{
						continue;
					}
					// Fixup the palette, strip any path.
					char paletteName[256];
					FileUtil::getFileNameFromPath(readBuffer, paletteName, true);
					paletteId = getPaletteId(paletteName);
					if (paletteId < 0) { paletteId = 0; }

					levelAssets.paletteName = paletteName;

					// Another value that is ignored.
					line = parser.readLine(bufferPos);
					if (sscanf(line, " MUSIC %s", readBuffer) == 1)
					{
						line = parser.readLine(bufferPos);
					}

					// Sky Parallax - this option until version 1.9, so handle its absence.
					f32 x, z;
					if (sscanf(line, " PARALLAX %f %f", &x, &z) == 2)
					{
						line = parser.readLine(bufferPos);
					}

					// Number of textures used by the level.
					s32 textureCount = 0;
					if (sscanf(line, " TEXTURES %d", &textureCount) != 1)
					{
						continue;
					}

					// Read texture names.
					char textureName[256];
					for (s32 i = 0; i < textureCount; i++)
					{
						line = parser.readLine(bufferPos);
						if (sscanf(line, " TEXTURE: %s ", textureName) == 1)
						{
							setAssetPalette(textureName, paletteId);
							addToLevelAssets(levelAssets.textures, textureName);
						}
					}
					// Sometimes there are extra textures, just add them - they will be compacted later.
					bool readNext = true;
					while (sscanf(line, " TEXTURE: %s ", textureName) == 1)
					{
						setAssetPalette(textureName, paletteId);
						addToLevelAssets(levelAssets.textures, textureName);
						line = parser.readLine(bufferPos);
						readNext = false;
					}
				}

				// .O File
				char objFile[TFE_MAX_PATH];
				FileUtil::replaceExtension(projAsset->name.c_str(), "O", objFile);
				if (archive->openFile(objFile))
				{
					size_t len = archive->getFileLength();
					buffer.resize(len);
					archive->readFile(buffer.data(), len);
					archive->closeFile();

					TFE_Parser parser;
					size_t bufferPos = 0;
					parser.init((char*)buffer.data(), buffer.size());
					parser.addCommentString("#");
					parser.convertToUpperCase(true);

					enum ProcessFlags
					{
						PFLAG_POD = (1 << 0),
						PFLAG_SPRITE = (1 << 1),
						PFLAG_FRAME = (1 << 2),
						PFLAG_PROCESS_DONE = PFLAG_POD | PFLAG_SPRITE | PFLAG_FRAME
					};
					u32 processFlags = 0;
					const char* line;
					while ((line = parser.readLine(bufferPos)) && (processFlags != PFLAG_PROCESS_DONE))
					{
						// Search for "PODS"
						s32 count = 0;
						char assetName[32];
						if (sscanf(line, "PODS %d", &count) == 1)
						{
							for (s32 p = 0; p < count; p++)
							{
								line = parser.readLine(bufferPos);
								if (line)
								{
									if (sscanf(line, " POD: %s", assetName) == 1)
									{
										setAssetPalette(assetName, paletteId);
										addToLevelAssets(levelAssets.pods, assetName);
									}
								}
							}
							processFlags |= PFLAG_POD;
						}
						else if (sscanf(line, "SPRS %d", &count) == 1)
						{
							for (s32 p = 0; p < count; p++)
							{
								line = parser.readLine(bufferPos);
								if (line)
								{
									if (sscanf(line, " SPR: %s", assetName) == 1)
									{
										setAssetPalette(assetName, paletteId);
										addToLevelAssets(levelAssets.sprites, assetName);
									}
								}
							}
							processFlags |= PFLAG_SPRITE;
						}
						else if (sscanf(line, "FMES %d", &count) == 1)
						{
							for (s32 p = 0; p < count; p++)
							{
								line = parser.readLine(bufferPos);
								if (line)
								{
									if (sscanf(line, " FME: %s", assetName) == 1)
									{
										setAssetPalette(assetName, paletteId);
										addToLevelAssets(levelAssets.frames, assetName);
									}
								}
							}
							processFlags |= PFLAG_FRAME;
						}
					}
				}

				s_levelAssets.push_back(levelAssets);
			}
		}
	}

	void reloadAsset(Asset* asset, s32 palId, s32 lightLevel)
	{
		Archive* archive = getArchive(asset->archiveName.c_str(), asset->gameId);
		if (archive)
		{
			AssetColorData colorData = { s_palettes[palId].data, s_palettes[palId].colormap, palId, lightLevel };
			reloadAssetData(asset->handle, archive, &colorData);
		}
	}

	// For now only handle textures and palettes.
	AssetType getAssetType(const char* ext)
	{
		if (strcasecmp(ext, "BM") == 0)
		{
			return TYPE_TEXTURE;
		}
		else if (strcasecmp(ext, "LEV") == 0)
		{
			return TYPE_LEVEL;
		}
		else if (strcasecmp(ext, "PAL") == 0)
		{
			return TYPE_PALETTE;
		}
		else if (strcasecmp(ext, "CMP") == 0)
		{
			return TYPE_COLORMAP;
		}
		else if (strcasecmp(ext, "FME") == 0)
		{
			return TYPE_FRAME;
		}
		else if (strcasecmp(ext, "WAX") == 0)
		{
			return TYPE_SPRITE;
		}
		return TYPE_COUNT;
	}

	void buildProjectAssetList(GameID gameId)
	{
		if (!s_reloadProjectAssets) { return; }
		s_reloadProjectAssets = false;

		if (gameId == Game_Dark_Forces)
		{
			const char* darkForcesArchives[] =
			{
				"DARK.GOB",
				"SOUNDS.GOB",
				"TEXTURES.GOB",
				"SPRITES.GOB",
			};

			for (u32 i = 0; i < TYPE_COUNT; i++)
			{
				s_projectAssetList[i].clear();
			}

			const size_t count = TFE_ARRAYSIZE(darkForcesArchives);
			for (size_t i = 0; i < count; i++)
			{
				Archive* archive = getArchive(darkForcesArchives[i], gameId);
				if (!archive) { continue; }

				u32 fileCount = archive->getFileCount();
				for (u32 f = 0; f < fileCount; f++)
				{
					const char* fileName = archive->getFileName(f);
					char ext[16];
					FileUtil::getFileExtension(fileName, ext);

					AssetType type = getAssetType(ext);
					if (type < TYPE_COUNT)
					{
						Asset newAsset =
						{
							type,
							gameId,
							fileName,
							darkForcesArchives[i],
							fileName,
							{}
						};
						s_projectAssetList[type].push_back(newAsset);
					}
				}
			}
		}
	}

	bool isLevelTexture(const char* name)
	{
		if (s_viewInfo.levelSource < 0) { return true; }

		const size_t count = s_levelAssets[s_viewInfo.levelSource].textures.size();
		const std::string* texName = s_levelAssets[s_viewInfo.levelSource].textures.data();
		for (size_t i = 0; i < count; i++, texName++)
		{
			if (strcasecmp(name, texName->c_str()) == 0)
			{
				return true;
			}
		}
		return false;
	}

	bool isLevelFrame(const char* name)
	{
		if (s_viewInfo.levelSource < 0) { return true; }

		const size_t count = s_levelAssets[s_viewInfo.levelSource].frames.size();
		const std::string* frameName = s_levelAssets[s_viewInfo.levelSource].frames.data();
		for (size_t i = 0; i < count; i++, frameName++)
		{
			if (strcasecmp(name, frameName->c_str()) == 0)
			{
				return true;
			}
		}
		return false;
	}

	bool isLevelSprite(const char* name)
	{
		if (s_viewInfo.levelSource < 0) { return true; }

		const size_t count = s_levelAssets[s_viewInfo.levelSource].sprites.size();
		const std::string* spriteName = s_levelAssets[s_viewInfo.levelSource].sprites.data();
		for (size_t i = 0; i < count; i++, spriteName++)
		{
			if (strcasecmp(name, spriteName->c_str()) == 0)
			{
				return true;
			}
		}
		return false;
	}

	bool isLevelPalette(const char* name)
	{
		if (s_viewInfo.levelSource < 0) { return true; }
		return strcasecmp(name, s_levelAssets[s_viewInfo.levelSource].paletteName.c_str()) == 0;
	}

	void loadAsset(const Asset* projAsset)
	{
		Archive* archive = getArchive(projAsset->archiveName.c_str(), projAsset->gameId);
		Asset asset;
		asset.type = projAsset->type;
		asset.name = projAsset->name;
		asset.gameId = projAsset->gameId;
		asset.archiveName = projAsset->archiveName;
		asset.filePath = projAsset->filePath;
		s32 palId = getAssetPalette(projAsset->name.c_str());

		AssetColorData colorData = { s_palettes[palId].data, nullptr, palId, 32 };
		asset.handle = loadAssetData(projAsset->type, archive, &colorData, projAsset->name.c_str());
		s_viewAssetList.push_back(asset);
	}

	void updateAssetList()
	{
		// Only Dark Forces for now.
		// TODO
		if (s_viewInfo.game != Game_Dark_Forces)
		{
			return;
		}
		buildProjectAssetList(s_viewInfo.game);

		preprocessAssets();
		s_viewAssetList.clear();
		if (s_viewInfo.type == TYPE_TEXTURE)
		{
			const u32 count = (u32)s_projectAssetList[TYPE_TEXTURE].size();
			const Asset* projAsset = s_projectAssetList[TYPE_TEXTURE].data();
			for (u32 i = 0; i < count; i++, projAsset++)
			{
				if (!isLevelTexture(projAsset->name.c_str())) { continue; }
				loadAsset(projAsset);
			}
		}
		else if (s_viewInfo.type == TYPE_FRAME)
		{
			const u32 count = (u32)s_projectAssetList[TYPE_FRAME].size();
			const Asset* projAsset = s_projectAssetList[TYPE_FRAME].data();
			for (u32 i = 0; i < count; i++, projAsset++)
			{
				if (!isLevelFrame(projAsset->name.c_str())) { continue; }
				loadAsset(projAsset);
			}
		}
		else if (s_viewInfo.type == TYPE_SPRITE)
		{
			const u32 count = (u32)s_projectAssetList[TYPE_SPRITE].size();
			const Asset* projAsset = s_projectAssetList[TYPE_SPRITE].data();
			for (u32 i = 0; i < count; i++, projAsset++)
			{
				if (!isLevelSprite(projAsset->name.c_str())) { continue; }
				loadAsset(projAsset);
			}
		}
		else if (s_viewInfo.type == TYPE_PALETTE)
		{
			const u32 count = (u32)s_projectAssetList[TYPE_PALETTE].size();
			const Asset* projAsset = s_projectAssetList[TYPE_PALETTE].data();
			for (u32 i = 0; i < count; i++, projAsset++)
			{
				if (!isLevelPalette(projAsset->name.c_str())) { continue; }

				Archive* archive = getArchive(projAsset->archiveName.c_str(), projAsset->gameId);
				Asset asset;
				asset.type = projAsset->type;
				asset.name = projAsset->name;
				asset.gameId = projAsset->gameId;
				asset.archiveName = projAsset->archiveName;
				asset.filePath = projAsset->filePath;

				// Does it have a colormap too?
				// For now we assume the names match.
				char colorMapPath[256];
				FileUtil::replaceExtension(projAsset->name.c_str(), "CMP", colorMapPath);

				const size_t cmapCount = s_projectAssetList[TYPE_COLORMAP].size();
				const Asset* projAssetCM = s_projectAssetList[TYPE_COLORMAP].data();
				u8 colormapData[256 * 32];
				bool hasColormap = false;
				for (size_t i = 0; i < cmapCount; i++, projAssetCM++)
				{
					if (strcasecmp(projAssetCM->name.c_str(), colorMapPath) == 0)
					{
						Archive* cmapArchive = getArchive(projAssetCM->archiveName.c_str(), projAssetCM->gameId);
						if (cmapArchive && cmapArchive->openFile(colorMapPath))
						{
							const size_t len = cmapArchive->getFileLength();
							cmapArchive->readFile(colormapData, min(8192u, (u32)len));
							cmapArchive->closeFile();
							hasColormap = true;
							break;
						}
					}
				}

				AssetColorData colorData = { nullptr, colormapData, 0, 32 };
				asset.handle = loadAssetData(TYPE_PALETTE, archive, &colorData, projAsset->name.c_str());
				s_viewAssetList.push_back(asset);
			}
		}
	}
}