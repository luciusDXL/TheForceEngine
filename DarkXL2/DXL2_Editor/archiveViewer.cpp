#include "archiveViewer.h"
#include <DXL2_System/system.h>
#include <DXL2_FileSystem/fileutil.h>
#include <DXL2_FileSystem/paths.h>
#include <DXL2_Archive/archive.h>
#include <DXL2_Input/input.h>
#include <DXL2_Asset/assetSystem.h>
#include <DXL2_Asset/levelAsset.h>
#include <DXL2_Asset/paletteAsset.h>
#include <DXL2_Asset/colormapAsset.h>
#include <DXL2_Asset/textureAsset.h>
#include <DXL2_Asset/spriteAsset.h>
#include <DXL2_Asset/fontAsset.h>
#include <DXL2_Asset/modelAsset.h>
#include <DXL2_Asset/gameMessages.h>
#include <DXL2_Asset/levelList.h>
#include <DXL2_Asset/vocAsset.h>
#include <DXL2_Asset/gmidAsset.h>
#include <DXL2_Settings/settings.h>
#include <DXL2_Ui/ui.h>
#include <DXL2_RenderBackend/renderBackend.h>
#include <DXL2_Audio/audioSystem.h>
#include <DXL2_Audio/midiPlayer.h>

#include <DXL2_Ui/imGUI/imgui_file_browser.h>
#include <DXL2_Ui/imGUI/imgui.h>

// Game
#include <DXL2_Game/gameLoop.h>

#include <algorithm>

// TODO: Add features to the file browser:
// 1. Filter does not exclude directories.
// 2. Ability to auto-set filter.
// 3. Ability to show only directories and select them.

namespace ArchiveViewer
{
	enum FileType
	{
		TYPE_TEXT = 0,
		TYPE_LEV,
		TYPE_PAL,
		TYPE_TEX,
		TYPE_FNT,
		TYPE_FRAME,
		TYPE_SPRITE,
		TYPE_3D,
		TYPE_VOC,
		TYPE_GMID,
		TYPE_BIN,
		TYPE_COUNT
	};

	static DXL2_Renderer* s_renderer = nullptr;

	static imgui_addons::ImGuiFileBrowser s_fileDialog;
	static char s_curArchiveFile[DXL2_MAX_PATH] = "";
	static char s_curArchiveName[DXL2_MAX_PATH] = "";
	static Archive* s_curArchive = nullptr;
	static std::string s_text;
	static bool s_showAsText = false;
	static s32 s_layer = 1;
	static bool s_runLevel = false;
	static bool s_showUi = true;

	static s32 s_currentFile = 0;
	static std::vector<const char*> s_items;
	static bool s_showFile = false;
	static FileType s_fileType;
	static Palette256* s_curPal = nullptr;
	static ColorMap* s_colorMap = nullptr;
	static Texture* s_curTexture = nullptr;
	static Frame* s_curFrame = nullptr;
	static Sprite* s_curSprite = nullptr;
	static Font* s_curFont = nullptr;
	static Model* s_curModel = nullptr;
	
	static f32 s_mapZoom = 1.0f;
	static f32 s_renderCenter[2] = { 0.0f, 0.0f };

	static s32 s_anim = 0;
	static s32 s_angle = 0;
	static Vec2f s_mapPos;

	static s32 s_curLevel = 0;
	static u32 s_levelCount = 0;
	static char* s_levelNames[14] = { 0 };
	static char* s_levelFiles[14] = { 0 };

	static s32 s_uiScale;
	static char s_levelFile[DXL2_MAX_PATH];
	static const ViewStats* s_viewStats = nullptr;
		
	f32 scaleUi(s32 x)
	{
		return f32(x * s_uiScale / 100);
	}

	void init(DXL2_Renderer* renderer)
	{
		s_renderer = renderer;

		// Allocate space for level names.
		for (u32 i = 0; i < 14; i++)
		{
			s_levelNames[i] = new char[256];
			s_levelFiles[i] = new char[256];
		}

		s_uiScale = DXL2_Ui::getUiScale();

		bool show = false;
		ImGui::Begin("ArchiveViewer", &show);
		ImVec2 windowPos = ImVec2(0.0f, scaleUi(32));
		ImGui::SetWindowPos("ArchiveViewer", windowPos);
		ImGui::SetWindowSize("ArchiveViewer", ImVec2(scaleUi(250), scaleUi(650)));
		ImGui::End();

		ImGui::Begin("LevelList", &show);
		ImVec2 windowPosLvl = ImVec2(scaleUi(1000), scaleUi(100));
		ImGui::SetWindowPos("LevelList", windowPosLvl);
		ImGui::SetWindowSize("LevelList", ImVec2(scaleUi(300), scaleUi(650)));
		ImGui::End();

		ImGui::Begin("TextViewer", &show);
		ImVec2 windowPosText = ImVec2(scaleUi(300), scaleUi(10));
		ImGui::SetWindowPos("TextViewer", windowPosText);
		ImGui::SetWindowSize("TextViewer", ImVec2(scaleUi(625), scaleUi(650)));
		ImGui::End();

		ImGui::Begin("ObjectViewer", &show);
		windowPosText = ImVec2(scaleUi(300), scaleUi(10));
		ImGui::SetWindowPos("ObjectViewer", windowPosText);
		ImGui::SetWindowSize("ObjectViewer", ImVec2(scaleUi(665), scaleUi(515)));
		ImGui::End();

		ImGui::Begin("SpriteViewer", &show);
		windowPosText = ImVec2(scaleUi(300), scaleUi(10));
		ImGui::SetWindowPos("SpriteViewer", windowPosText);
		ImGui::SetWindowSize("SpriteViewer", ImVec2(scaleUi(665), scaleUi(560)));
		ImGui::End();

		ImGui::Begin("LevelViewer", &show);
		windowPosText = ImVec2(scaleUi(300), scaleUi(10));
		ImGui::SetWindowPos("LevelViewer", windowPosText);
		ImGui::SetWindowSize("LevelViewer", ImVec2(scaleUi(665), scaleUi(600)));
		ImGui::End();

		s_fileDialog.setCurrentPath(DXL2_Paths::getPath(PATH_SOURCE_DATA));

		s_curArchiveFile[0] = 0;
		s_curArchiveName[0] = 0;
		s_anim = 0;
		s_angle = 0;
		s_showAsText = false;
		s_layer = 1;

		s_curPal = DXL2_Palette::get256("SECBASE.PAL");
		s_renderer->enableScreenClear(true);
	}
		
	bool render3dView()
	{
		const DXL2_Settings_Graphics* config = DXL2_Settings::getGraphicsSettings();

		if (s_runLevel)
		{
			if (DXL2_Input::keyPressed(KEY_F10))
			{
				s_showUi = !s_showUi;
				DXL2_Input::enableRelativeMode(!s_showUi);
			}

			GameTransition trans = DXL2_GameLoop::update();
			DXL2_GameLoop::draw();
			s_viewStats = DXL2_GameLoop::getViewStats();

			if (trans == TRANS_NEXT_LEVEL && s_curLevel >= 0 && s_curLevel + 1 < (s32)s_levelCount)
			{
				// Go to the next level in the list.
				s_curLevel++;

				StartLocation start = {};
				start.overrideStart = false;

				strcpy(s_levelFile, s_levelFiles[s_curLevel]);
				DXL2_LevelAsset::load(s_levelFiles[s_curLevel]);
				LevelData* level = DXL2_LevelAsset::getLevelData();
				DXL2_GameLoop::startLevel(level, start, s_renderer, config->gameResolution.x, config->gameResolution.z, true);
			}
			else if (DXL2_Input::keyPressed(KEY_BACKSPACE) || trans == TRANS_QUIT || trans == TRANS_TO_AGENT_MENU || trans == TRANS_NEXT_LEVEL)
			{
				s_renderer->changeResolution(640, 480);
				s_runLevel = false;
				s_showUi = true;
				DXL2_Input::enableRelativeMode(false);
				s_renderer->enableScreenClear(true);
				DXL2_Audio::stopAllSounds();

				// Reload the level
				DXL2_LevelAsset::load(s_levelFile);
			}
			else
			{
				const LevelData* level = DXL2_LevelAsset::getLevelData();
				s_layer = level->sectors[s_viewStats->sectorId].layer;
				s_mapPos = { s_viewStats->pos.x, s_viewStats->pos.z };

				s_renderer->clearMapMarkers();
				s_renderer->addMapMarker(s_layer, s_viewStats->pos.x, s_viewStats->pos.z, 19);
			}

			return true;
		}

		s_renderer->setPalette(s_curPal);
		if (s_fileType == TYPE_LEV)
		{
			s_renderer->drawMapLines(s_layer, s_renderCenter[0] * s_mapZoom + 320.0f, s_renderCenter[1] * s_mapZoom + 240.0f, s_mapZoom);
		}
		else if (s_fileType == TYPE_PAL)
		{
			s_renderer->drawPalette();
		}
		else if (s_fileType == TYPE_TEX && s_curTexture)
		{
			s32 x = s_curTexture->frames[0].width  < 640 ? 320 - (s_curTexture->frames[0].width >>1) : 0;
			s32 y = s_curTexture->frames[0].height < 480 ? 240 - (s_curTexture->frames[0].height>>1) : 0;
			if (s_curTexture->layout == TEX_LAYOUT_VERT)
			{
				s_renderer->drawTexture(s_curTexture, x, y);
			}
			else
			{
				s32 x = s_curTexture->frames[0].width <= 320 ? 160 : 0;
				s32 y = s_curTexture->frames[0].width <= 240 ? 140 : 0;
				s_renderer->drawTextureHorizontal(s_curTexture, x, y);
			}
		}
		else if (s_fileType == TYPE_FRAME)
		{
			s32 x = 320 - ((s_curFrame->rect[0] + s_curFrame->rect[2]) >> 1);
			s32 y = 240 - ((s_curFrame->rect[1] + s_curFrame->rect[3]) >> 1);
			s_renderer->drawFrame(s_curFrame, x, y);
		}
		else if (s_fileType == TYPE_SPRITE)
		{
			s32 x = 320 - ((s_curSprite->rect[0] + s_curSprite->rect[2]) >> 1);
			s32 y = 240 - ((s_curSprite->rect[1] + s_curSprite->rect[3]) >> 1);
			s_renderer->drawSprite(s_curSprite, x, y, s_anim, s_angle);
		}
		else if (s_fileType == TYPE_FNT)
		{
			s_renderer->drawFont(s_curFont);
		}
		else if (s_fileType == TYPE_3D)
		{
			static Vec3f orientation = { 0, 0, 0 };
			orientation.y -= 45.0f*(f32)DXL2_System::getDeltaTime();
			DXL2_GameLoop::drawModel(s_curModel, &orientation);
		}

		return false;
	}

	void selectPointOnMap(f32 wx, f32 wy)
	{
		s32 mx, my;
		DXL2_Input::getMousePos(&mx, &my);

		const f32 offsetX = s_renderCenter[0] * s_mapZoom + 320.0f;
		const f32 offsetY = s_renderCenter[1] * s_mapZoom + 240.0f;
		const f32 scale = s_mapZoom;

		const f32 rx = f32(mx - wx);
		// Y up versus Y down.
		const f32 ry = f32(479 - my + wy);

		const f32 mapX = (rx - offsetX) / scale;
		const f32 mapY = (ry - offsetY) / scale;
		s_mapPos = { mapX, mapY };

		s_renderer->clearMapMarkers();
		s_renderer->addMapMarker(s_layer, mapX, mapY, 19);
	}
	
	void unloadAssets()
	{
		DXL2_Audio::stopAllSounds();

		// Free all assets
		DXL2_Palette::freeAll();
		DXL2_ColorMap::freeAll();
		DXL2_Font::freeAll();
		DXL2_Sprite::freeAll();
		DXL2_Texture::freeAll();
		DXL2_Model::freeAll();
		DXL2_GameMessages::unload();
		DXL2_LevelList::unload();

		// Re-load and set the default palette.
		s_curPal = DXL2_Palette::get256("SECBASE.PAL");
		s_colorMap = DXL2_ColorMap::get("SECBASE.CMP");
		s_renderer->setPalette(s_curPal);
		s_renderer->setColorMap(s_colorMap);
		DXL2_GameMessages::load();
		DXL2_LevelList::load();

		s_levelCount = DXL2_LevelList::getLevelCount();
		for (u32 i = 0; i < s_levelCount; i++)
		{
			sprintf(s_levelNames[i], "%s", DXL2_LevelList::getLevelName(i));
			sprintf(s_levelFiles[i], "%s.LEV", DXL2_LevelList::getLevelFileName(i));
		}
	}

	bool isFullscreen()
	{
		return s_runLevel;
	}
		
	void draw(bool* isActive)
	{
		const DXL2_Settings_Graphics* config = DXL2_Settings::getGraphicsSettings();

		// Do not show the editor while running the level.
		if (s_runLevel)
		{
			if (s_viewStats && s_showUi)
			{
				LevelData* level = DXL2_LevelAsset::getLevelData();

				static bool viewStatsActive = true;
				ImGui::Begin("ViewStats", &viewStatsActive);
					ImGui::Text("Level Name \"%s\"", s_items[s_currentFile]);
					ImGui::Text("SectorId %d", s_viewStats->sectorId);
					if (s_viewStats->sectorId >= 0 && level->sectors[s_viewStats->sectorId].name[0])
					{
						ImGui::Text("Sector \"%s\"", level->sectors[s_viewStats->sectorId].name);
					}
					ImGui::Text("Pos (%2.2f, %2.2f)", s_viewStats->pos.x, s_viewStats->pos.z);
					ImGui::Text("Height %2.2f", s_viewStats->pos.y);
					ImGui::Text("Angles (%2.2f, %2.2f)pi", s_viewStats->yaw/PI, s_viewStats->pitch/PI);
					ImGui::Separator();
					ImGui::Text("Iteration Count %d", s_viewStats->iterCount);
					ImGui::Text("Traversal Depth %d", s_viewStats->maxTraversalDepth);
					ImGui::Text("Segs");
					ImGui::Text("  Full  %d", s_viewStats->segWallRendered);
					ImGui::Text("  Lower %d", s_viewStats->segLowerRendered);
					ImGui::Text("  Upper %d", s_viewStats->segUpperRendered);
					ImGui::Text("Flats");
					ImGui::Text("  Floor   %d", s_viewStats->floorPolyRendered);
					ImGui::Text("  Ceiling %d", s_viewStats->ceilPolyRendered);
				ImGui::End();
			}
			return;
		}

		if (s_levelCount)
		{
			ImGui::Begin("LevelList", isActive);
			ImGui::Text("Level Count: %u", s_levelCount);
			
			if (ImGui::ListBox("", &s_curLevel, s_levelNames, (s32)s_levelCount, 14))
			{
				s_fileType = TYPE_LEV;
				strcpy(s_levelFile, s_levelFiles[s_curLevel]);
				DXL2_LevelAsset::load(s_levelFiles[s_curLevel]);
				LevelData* level = DXL2_LevelAsset::getLevelData();

				// get the map center.
				s_renderCenter[0] = 0.0f;
				s_renderCenter[1] = 0.0f;

				f32 aabbMin[2] = { FLT_MAX,  FLT_MAX };
				f32 aabbMax[2] = { -FLT_MAX, -FLT_MAX };
				const u32 vertexCount = (u32)level->vertices.size();
				const Vec2f* vertices = level->vertices.data();
				for (u32 v = 0; v < vertexCount; v++)
				{
					if (fabsf(vertices[v].x) > 4096.0f || fabsf(vertices[v].z) > 4096.0f) { continue; }

					aabbMin[0] = std::min(aabbMin[0], vertices[v].x);
					aabbMin[1] = std::min(aabbMin[1], vertices[v].z);
					aabbMax[0] = std::max(aabbMax[0], vertices[v].x);
					aabbMax[1] = std::max(aabbMax[1], vertices[v].z);
				}

				s_renderCenter[0] = -(aabbMin[0] + aabbMax[0]) * 0.5f;
				s_renderCenter[1] = -(aabbMin[1] + aabbMax[1]) * 0.5f;
				s_mapZoom = 1.0f;
				s_renderer->clearMapMarkers();

				StartLocation start = {};
				start.overrideStart = false;

				if (DXL2_GameLoop::startLevel(level, start, s_renderer, config->gameResolution.x, config->gameResolution.z, true))
				{
					s_renderer->changeResolution(config->gameResolution.x, config->gameResolution.z);
					s_runLevel = true;
					s_showUi = false;
					DXL2_Input::enableRelativeMode(true);
					s_renderer->enableScreenClear(false);
				}
			}

			ImGui::End();
		}
		
		ImGui::Begin("ArchiveViewer", isActive);
		if (ImGui::Button("Open Archive"))
		{
			ImGui::OpenPopup("Open File");
		}
		if (s_curArchiveName[0])
		{
			ImGui::Text("Current Archive: %s", s_curArchiveName);
			ImGui::Text("File Count: %u", s_curArchive->getFileCount());

			if (ImGui::ListBox("", &s_currentFile, s_items.data(), (s32)s_items.size(), 25))
			{
				size_t len = (size_t)s_curArchive->getFileLength(s_currentFile);
				s_text.resize(len);

				s_curArchive->openFile(s_currentFile);
				s_curArchive->readFile((void*)s_text.data(), len);
				s_curArchive->closeFile();

				DXL2_MidiPlayer::stop();

				char extension[16];
				FileUtil::getFileExtension(s_items[s_currentFile], extension);
				s_fileType = TYPE_BIN;

				if (strcasecmp(extension, "TXT") == 0 || strcasecmp(extension, "MSG") == 0 || strcasecmp(extension, "LST") == 0 ||
					strcasecmp(extension, "LVL") == 0 || strcasecmp(extension, "INF") == 0 || strcasecmp(extension, "O") == 0 ||
					strcasecmp(extension, "VUE") == 0 || strcasecmp(extension, "GOL") == 0)
				{
					s_fileType = TYPE_TEXT;
				}
				else if (strcasecmp(extension, "LEV") == 0)
				{
					s_fileType = TYPE_LEV;
					strcpy(s_levelFile, s_items[s_currentFile]);
					DXL2_LevelAsset::load(s_items[s_currentFile]);
					LevelData* level = DXL2_LevelAsset::getLevelData();
					
					// get the map center.
					s_renderCenter[0] = 0.0f;
					s_renderCenter[1] = 0.0f;

					f32 aabbMin[2] = {  FLT_MAX,  FLT_MAX };
					f32 aabbMax[2] = { -FLT_MAX, -FLT_MAX };
					const u32 vertexCount = (u32)level->vertices.size();
					const Vec2f* vertices = level->vertices.data();
					for (u32 v = 0; v < vertexCount; v++)
					{
						if (fabsf(vertices[v].x) > 4096.0f || fabsf(vertices[v].z) > 4096.0f) { continue; }

						aabbMin[0] = std::min(aabbMin[0], vertices[v].x);
						aabbMin[1] = std::min(aabbMin[1], vertices[v].z);
						aabbMax[0] = std::max(aabbMax[0], vertices[v].x);
						aabbMax[1] = std::max(aabbMax[1], vertices[v].z);
					}

					s_renderCenter[0] = -(aabbMin[0] + aabbMax[0]) * 0.5f;
					s_renderCenter[1] = -(aabbMin[1] + aabbMax[1]) * 0.5f;
					s_mapZoom = 1.0f;
					s_renderer->clearMapMarkers();
				}
				else if (strcasecmp(extension, "PAL") == 0)
				{
					s_fileType = TYPE_PAL;
					s_curPal = DXL2_Palette::get256(s_items[s_currentFile]);
				}
				else if (strcasecmp(extension, "BM") == 0)
				{
					s_fileType = TYPE_TEX;
					s_curTexture = DXL2_Texture::get(s_items[s_currentFile]);
				}
				else if (strcasecmp(extension, "PCX") == 0)
				{
					s_fileType = TYPE_TEX;
					s_curTexture = DXL2_Texture::getFromPCX(s_items[s_currentFile], s_curArchiveFile);
					s_curPal = DXL2_Texture::getPreviousPalette();
				}
				else if (strcasecmp(extension, "FNT") == 0)
				{
					s_fileType = TYPE_FNT;
					s_curFont = DXL2_Font::get(s_items[s_currentFile]);
				}
				else if (strcasecmp(extension, "FME") == 0)
				{
					s_fileType = TYPE_FRAME;
					s_curFrame = DXL2_Sprite::getFrame(s_items[s_currentFile]);
				}
				else if (strcasecmp(extension, "WAX") == 0)
				{
					s_fileType = TYPE_SPRITE;
					s_curSprite = DXL2_Sprite::getSprite(s_items[s_currentFile]);
					s_anim = 0;
					s_angle = 0;
				}
				else if (strcasecmp(extension, "3DO") == 0)
				{
					s_fileType = TYPE_3D;
					s_curModel = DXL2_Model::get(s_items[s_currentFile]);
					DXL2_GameLoop::startRenderer(s_renderer, 640, 480);
				}
				else if (strcasecmp(extension, "VOC") == 0 || strcasecmp(extension, "VOIC") == 0)
				{
					s_fileType = TYPE_VOC;
					const SoundBuffer* sound = DXL2_VocAsset::get(s_items[s_currentFile]);

					DXL2_Audio::playOneShot(SOUND_2D, 1.0f, MONO_SEPERATION, sound, false);
				}
				else if (strcasecmp(extension, "GMD") == 0 || strcasecmp(extension, "GMID") == 0)
				{
					s_fileType = TYPE_GMID;
					const GMidiAsset* song = DXL2_GmidAsset::get(s_items[s_currentFile]);
					DXL2_MidiPlayer::playSong(song, false);
				}
				else if (strcasecmp(extension, "PLTT") == 0)
				{
					s_fileType = TYPE_PAL;
					s_curPal = DXL2_Palette::getPalFromPltt(s_items[s_currentFile], s_curArchiveFile);
				}
				else if (strcasecmp(extension, "DELT") == 0)
				{
					s_fileType = TYPE_TEX;
					s_curTexture = DXL2_Texture::getFromDelt(s_items[s_currentFile], s_curArchiveFile);
				}
				else if (strcasecmp(extension, "ANIM") == 0)
				{
					s_fileType = TYPE_TEX;
					s_curTexture = DXL2_Texture::getFromAnim(s_items[s_currentFile], s_curArchiveFile);
				}
				else if (strcasecmp(extension, "FONT") == 0)
				{
					s_fileType = TYPE_FNT;
					s_curFont = DXL2_Font::getFromFont(s_items[s_currentFile], s_curArchiveFile);
				}

				for (size_t i = 0; i < len; i++)
				{
					if (s_text[i] < ' ' && s_text[i] != '\t' && s_text[i] != '\r' && s_text[i] != '\n') { s_text[i] = ' '; }
				}

				s_showFile = true;
			}
		}

		if (s_fileDialog.showOpenFileDialog("Open File", ImVec2(scaleUi(600), scaleUi(300)), ".gob,.GOB,.lfd,.LFD,.lab,.LAB"))
		{
			strcpy(s_curArchiveFile, s_fileDialog.selected_fn.c_str());
			FileUtil::getFileNameFromPath(s_curArchiveFile, s_curArchiveName, true);

			char extension[DXL2_MAX_PATH];
			FileUtil::getFileExtension(s_curArchiveName, extension);

			ArchiveType type = ARCHIVE_COUNT;
			if (strcasecmp(extension, "gob") == 0) { type = ARCHIVE_GOB; }
			else if (strcasecmp(extension, "lfd") == 0) { type = ARCHIVE_LFD; }
			else if (strcasecmp(extension, "lab") == 0) { type = ARCHIVE_LAB; }
			else { assert(0); }

			s_curArchive = Archive::getArchive(type, s_curArchiveName, s_curArchiveFile);
			s_currentFile = 0;
			s_showFile = false;
			s_fileType = TYPE_COUNT;

			DXL2_AssetSystem::setCustomArchive(s_curArchive);
			// All assets need to be unloaded when setting up a custom archive since they may have the same names
			// as the vanilla assets.
			unloadAssets();
			
			const u32 fileCount = s_curArchive->getFileCount();
			s_items.resize(fileCount);
			for (u32 i = 0; i < fileCount; i++)
			{
				s_items[i] = s_curArchive->getFileName(i);
			}
		}

		ImGui::Checkbox("Always Display As Text", &s_showAsText);
		ImGui::End();

		if (s_curArchiveName[0] && s_showFile && (s_fileType == TYPE_TEXT || s_showAsText))
		{
			ImGui::Begin("TextViewer", &s_showFile);
			ImGui::InputTextMultiline("", (char*)s_text.data(), s_text.size(), ImVec2(scaleUi(600), scaleUi(600)), ImGuiInputTextFlags_ReadOnly);
			ImGui::End();
		}
		else if (s_curArchiveName[0] && s_showFile && s_fileType == TYPE_LEV)
		{
			LevelData* level = DXL2_LevelAsset::getLevelData();
			if (s_layer < level->layerMin) { s_layer = level->layerMin; }
			else if (s_layer > level->layerMax) { s_layer = level->layerMax; }

			ImGui::Begin("LevelViewer", &s_showFile);
			if (ImGui::ImageButton(DXL2_RenderBackend::getVirtualDisplayGpuPtr(), ImVec2(scaleUi(640), scaleUi(480)), ImVec2(0, 1), ImVec2(1, 0), 0))
			{
				ImVec2 corner = ImGui::GetWindowContentRegionMin();
				ImVec2 winPos = ImGui::GetWindowPos();
				selectPointOnMap(corner.x + winPos.x, corner.y + winPos.y);
			}
			ImGui::Text("Layer Range: [%d, %d]", level->layerMin, level->layerMax);
			ImGui::SliderInt("Layer", &s_layer, level->layerMin, level->layerMax);
			
			if (ImGui::Button("Play from Marker"))
			{
				if (s_renderer->getMapMarkerCount() > 0)
				{
					StartLocation start;
					start.overrideStart = true;
					start.pos = s_mapPos;
					start.layer = s_layer;

					if (DXL2_GameLoop::startLevel(level, start, s_renderer, config->gameResolution.x, config->gameResolution.z, true))
					{
						s_renderer->changeResolution(config->gameResolution.x, config->gameResolution.z);
						s_runLevel = true;
						s_showUi = false;
						DXL2_Input::enableRelativeMode(true);
						s_renderer->enableScreenClear(false);
					}
				}
			}
			if (ImGui::Button("Play from Start"))
			{
				StartLocation start = {};
				start.overrideStart = false;

				if (DXL2_GameLoop::startLevel(level, start, s_renderer, config->gameResolution.x, config->gameResolution.z, true))
				{
					s_renderer->changeResolution(config->gameResolution.x, config->gameResolution.z);
					s_runLevel = true;
					s_showUi = false;
					DXL2_Input::enableRelativeMode(true);
				}
			}
			ImGui::End();

			// Handle right-button scrolling.
			if (DXL2_Input::mouseDown(MBUTTON_RIGHT))
			{
				s32 x, y;
				DXL2_Input::getMouseMove(&x, &y);
				if (x || y)
				{
					s_renderCenter[0] += f32(x) / s_mapZoom;
					s_renderCenter[1] -= f32(y) / s_mapZoom;
				}

				s32 dx, dy;
				DXL2_Input::getMouseWheel(&dx, &dy);
				if (dy)
				{
					s_mapZoom += f32(dy) * f32(DXL2_System::getDeltaTime() * 3.0);
				}
			}
		}
		else if (s_curArchiveName[0] && s_showFile && s_fileType == TYPE_SPRITE)
		{
			ImGui::Begin("SpriteViewer", &s_showFile);
			ImGui::Image(DXL2_RenderBackend::getVirtualDisplayGpuPtr(), ImVec2(scaleUi(640), scaleUi(480)), ImVec2(0, 1), ImVec2(1, 0));

			ImGui::SliderInt("Animation", &s_anim, 0, s_curSprite->animationCount - 1);
			ImGui::SliderInt("Angle", &s_angle, 0, s_curSprite->anim[s_anim].angleCount - 1);
			
			ImGui::End();
		}
		else if (s_curArchiveName[0] && s_showFile)
		{
			ImGui::Begin("ObjectViewer", &s_showFile);
			ImGui::Image(DXL2_RenderBackend::getVirtualDisplayGpuPtr(), ImVec2(scaleUi(640), scaleUi(480)), ImVec2(0, 1), ImVec2(1, 0));
			ImGui::End();
		}
	}
}
