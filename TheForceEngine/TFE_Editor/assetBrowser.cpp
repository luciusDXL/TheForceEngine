#include "assetBrowser.h"
#include "editorTexture.h"
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
	enum AssetType : s32
	{
		TYPE_TEXTURE = 0,
		TYPE_SPRITE,
		TYPE_FRAME,
		TYPE_3DOBJ,
		TYPE_LEVEL,
		TYPE_PALETTE,
		TYPE_COLORMAP,
		TYPE_COUNT,
		TYPE_NOT_SET = TYPE_COUNT
	};

	const char* c_games[] =
	{
		"Dark Forces",
		"Outlaws",
	};
	const char* c_assetType[] =
	{
		"Texture",    // TYPE_TEXTURE
		"Sprite",     // TYPE_SPRITE
		"Frame",      // TYPE_FRAME
		"3D Object",  // TYPE_3DOBJ
		"Level",      // TYPE_LEVEL
		"Palette",    // TYPE_PALETTE
	};

	struct Asset
	{
		AssetType type;
		GameID gameId;
		std::string name;
		std::string archiveName;
		std::string filePath;

		EditorTexture texture;
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

	struct LevelTextures
	{
		std::string levelName;
		std::string paletteName;
		std::vector<std::string> textures;
	};
	std::vector<LevelTextures> s_levelTextures;

	static bool s_reloadProjectAssets = true;
	static bool s_assetsNeedProcess = true;
	static std::vector<Palette> s_palettes;
	static std::map<std::string, s32> s_texPalette;
	static s32 s_defaultPal = 0;

	static s32 s_hovered = -1;
	static s32 s_selected = -1;
	   	
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
	void reloadTexture(Asset* asset, s32 palId, s32 lightLevel = 32);

	void init()
	{
		updateAssetList();
	}

	void destroy()
	{
		s_reloadProjectAssets = true;
		s_assetsNeedProcess = true;
		s_palettes.clear();
		s_texPalette.clear();;
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
		ImGui::SetNextItemWidth(256.0f);
		ImGui::LabelText("##Label", labelText); ImGui::SameLine(96.0f);

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
			LevelTextures* textures = (LevelTextures*)data;
			*out_text = textures[idx - 1].levelName.c_str();
		}
		return true;
	}

	void listSelectionPalette(const char* labelText, std::vector<Palette>& listValues, s32* index)
	{
		ImGui::SetNextItemWidth(256.0f);
		ImGui::LabelText("##Label", labelText); ImGui::SameLine(96.0f);

		char comboId[256];
		sprintf(comboId, "##%s", labelText);
		ImGui::Combo(comboId, index, paletteItemGetter, listValues.data(), (s32)listValues.size());
	}

	void listSelectionLevel(const char* labelText, std::vector<LevelTextures>& listValues, s32* index)
	{
		ImGui::SetNextItemWidth(256.0f);
		ImGui::LabelText("##Label", labelText); ImGui::SameLine(96.0f);

		char comboId[256];
		sprintf(comboId, "##%s", labelText);
		s32 lvlIndex = (*index) + 1;
		if (ImGui::Combo(comboId, &lvlIndex, levelTextureGetter, listValues.data(), (s32)listValues.size()))
		{
			*index = lvlIndex - 1;
		}
	}
		
	void infoPanel()
	{
		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		u32 w = std::min(512u, displayInfo.width);

		ImGui::SetWindowPos("Asset Browser##Settings", { 0.0f, 20.0f });
		ImGui::SetWindowSize("Asset Browser##Settings", { f32(w), 196.0f });
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;

		bool active = true;
		s32 levelSource = s_viewInfo.levelSource;
		ImGui::Begin("Asset Browser##Settings", &active, window_flags);
		LIST_SELECT("Game", c_games, s_viewInfo.game);
		LIST_SELECT("Asset Type", c_assetType, s_viewInfo.type);
		listSelectionLevel("Level", s_levelTextures, &levelSource);
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
		label("Filter");
		ImGui::SetNextItemWidth(256.0f);
		ImGui::InputText("##FilterText", s_viewInfo.assetFilter, 64);
		ImGui::SameLine(272.0f);
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

	void drawInfoPanel(Asset* asset)
	{
		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		u32 w = std::min(512u, displayInfo.width);
		u32 h = std::max(256u, displayInfo.height - 216);

		ImGui::SetWindowPos("Asset Info", { 0.0f, 216.0f });
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
				EditorTexture* tex = &asset->texture;

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
					reloadTexture(asset, newPaletteIndex, newLightLevel);
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

				ImGui::Image(TFE_RenderBackend::getGpuPtr(asset->texture.texGpu[tex->curFrame]),
					ImVec2((f32)texW, (f32)texH), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
			}
			else if (asset->type == TYPE_PALETTE)
			{
				EditorTexture* tex = &asset->texture;
				ImGui::Image(TFE_RenderBackend::getGpuPtr(asset->texture.texGpu[0]),
					ImVec2(128.0f, 128.0f), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

				// Show the colormap if it exists.
				if (tex->frameCount == 2)
				{
					ImGui::Separator();
					ImGui::Image(TFE_RenderBackend::getGpuPtr(asset->texture.texGpu[1]),
						ImVec2(500.0f, 64.0f), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
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

	void listPanel()
	{
		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);
		if (displayInfo.width < 512) { return; }

		u32 w = displayInfo.width - 512u;
		u32 h = displayInfo.height - 20u;

		ImGui::SetWindowPos("Asset List", { 512.0f, 20.0f });
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

			char buttonLabel[32];
			const s32 itemWidth = 130;
			const s32 itemHeight = 84;

			s32 columnCount = max(1, s32(w - 16) / (itemWidth + 10));
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
			for (s32 y = 0; a < count; y++)
			{
				for (s32 x = 0; x < columnCount && a < count; x++, a++)
				{
					sprintf(buttonLabel, "###asset%d", a);
					ImVec2 cursor((8.0f + x * (itemWidth + 10)), topPos + y * itemHeight);
					ImGui::SetCursorPos(cursor);

					ImGui::PushStyleColor(ImGuiCol_Border, getBorderColor(a));
					if (ImGui::BeginChild(buttonLabel, ImVec2(itemWidth, itemHeight), true, ImGuiWindowFlags_NoDecoration))
					{
						EditorTexture* tex = &s_viewAssetList[a].texture;
						s32 offsetX = 0, offsetY = 0;
						s32 width = 64, height = 64;
						// Preserve the image aspect ratio.
						if (tex->width >= tex->height)
						{
							height = tex->height * 64 / tex->width;
							offsetY = (width - height) / 2;
						}
						else
						{
							width = tex->width * 64 / tex->height;
							offsetX = (height - width) / 2;
						}
						// Center image.
						offsetX += (itemWidth - 64) / 2;

						// Draw the image.
						ImGui::SetCursorPos(ImVec2((f32)offsetX, (f32)offsetY));
						ImGui::Image(TFE_RenderBackend::getGpuPtr(s_viewAssetList[a].texture.texGpu[0]),
							ImVec2((f32)width, (f32)height), ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));

						// Draw the label.
						ImGui::SetCursorPos(ImVec2(8.0f, 64.0f));
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
			drawInfoPanel(selectedAsset);
		}

		ImGui::End();
	}

	void update()
	{
		infoPanel();
		listPanel();
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

	void setTexturePalette(const char* name, s32 paletteId)
	{
		std::map<std::string, s32>::iterator iTexPal = s_texPalette.find(name);
		if (iTexPal == s_texPalette.end())
		{
			s_texPalette[name] = paletteId;
		}
	}

	s32 getTexturePalette(const char* name)
	{
		std::map<std::string, s32>::iterator iTexPal = s_texPalette.find(name);
		if (iTexPal != s_texPalette.end())
		{
			return iTexPal->second;
		}
		// Not sure how to automatically figure this out yet.
		if (strcasecmp(name, "WAIT.BM") == 0)
		{
			return 0;
		}
		return s_defaultPal;
	}
		
	void addToLevelTextures(std::vector<std::string>& textures, const char* textureName)
	{
		const size_t count = textures.size();
		for (size_t i = 0; i < count; i++)
		{
			if (textures[i] == textureName)
			{
				return;
			}
		}
		textures.push_back(textureName);
	}

	void preprocessAssets()
	{
		if (!s_assetsNeedProcess) { return; }
		s_assetsNeedProcess = false;
		s_levelTextures.clear();

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
				LevelTextures levelTextures;
				levelTextures.levelName = projAsset->name;

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
				s32 paletteId = getPaletteId(paletteName);
				if (paletteId < 0) { paletteId = 0; }

				levelTextures.paletteName = paletteName;

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
						setTexturePalette(textureName, paletteId);
						addToLevelTextures(levelTextures.textures, textureName);
					}
				}
				// Sometimes there are extra textures, just add them - they will be compacted later.
				bool readNext = true;
				while (sscanf(line, " TEXTURE: %s ", textureName) == 1)
				{
					setTexturePalette(textureName, paletteId);
					addToLevelTextures(levelTextures.textures, textureName);
					line = parser.readLine(bufferPos);
					readNext = false;
				}
				s_levelTextures.push_back(levelTextures);
			}
		}
	}

	void reloadTexture(Asset* asset, s32 palId, s32 lightLevel)
	{
		Archive* archive = getArchive(asset->archiveName.c_str(), asset->gameId);
		if (archive)
		{
			freeTexture(asset->name.c_str());
			if (lightLevel == 32 || !s_palettes[palId].hasColormap)
			{
				loadEditorTexture(TEX_BM, archive, asset->name.c_str(), s_palettes[palId].data, palId, &asset->texture);
			}
			else
			{
				loadEditorTextureLit(TEX_BM, archive, asset->name.c_str(), s_palettes[palId].data, palId, s_palettes[palId].colormap, lightLevel, &asset->texture);
			}
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

		const size_t count = s_levelTextures[s_viewInfo.levelSource].textures.size();
		const std::string* texName = s_levelTextures[s_viewInfo.levelSource].textures.data();
		for (size_t i = 0; i < count; i++, texName++)
		{
			if (strcasecmp(name, texName->c_str()) == 0)
			{
				return true;
			}
		}
		return false;
	}

	bool isLevelPalette(const char* name)
	{
		if (s_viewInfo.levelSource < 0) { return true; }
		return strcasecmp(name, s_levelTextures[s_viewInfo.levelSource].paletteName.c_str()) == 0;
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
				if (!isLevelTexture(projAsset->name.c_str()))
				{
					continue;
				}

				Archive* archive = getArchive(projAsset->archiveName.c_str(), projAsset->gameId);
				Asset asset;
				asset.type = projAsset->type;
				asset.name = projAsset->name;
				asset.gameId = projAsset->gameId;
				asset.archiveName = projAsset->archiveName;
				asset.filePath = projAsset->filePath;

				memset(&asset.texture, 0, sizeof(EditorTexture));
				s32 palId = getTexturePalette(projAsset->name.c_str());

				loadEditorTexture(TEX_BM, archive, projAsset->name.c_str(), s_palettes[palId].data, palId, &asset.texture);
				s_viewAssetList.push_back(asset);
			}
		}
		else if (s_viewInfo.type == TYPE_PALETTE)
		{
			const u32 count = (u32)s_projectAssetList[TYPE_PALETTE].size();
			const Asset* projAsset = s_projectAssetList[TYPE_PALETTE].data();
			for (u32 i = 0; i < count; i++, projAsset++)
			{
				if (!isLevelPalette(projAsset->name.c_str()))
				{
					continue;
				}

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
							cmapArchive->closeFile();
							hasColormap = true;
							break;
						}
					}
				}

				memset(&asset.texture, 0, sizeof(EditorTexture));
				loadPaletteAsTexture(archive, projAsset->name.c_str(), hasColormap ? colormapData : nullptr, &asset.texture);
				s_viewAssetList.push_back(asset);
			}
		}
	}
}