#include "modLoader.h"
#include "frontEndUi.h"
#include "console.h"
#include "uiTexture.h"
#include "profilerView.h"
#include <TFE_DarkForces/config.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/system.h>
#include <TFE_System/parser.h>
#include <TFE_System/cJSON.h>
#include <TFE_FileSystem/download.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_Archive/archive.h>
#include <TFE_Settings/settings.h>
#include <TFE_Asset/imageAsset.h>
#include <TFE_Archive/zipArchive.h>
#include <TFE_Archive/gobMemoryArchive.h>
#include <TFE_Input/inputMapping.h>
#include <TFE_Asset/imageAsset.h>
#include <TFE_Ui/ui.h>
#include <TFE_Ui/markdown.h>
// Game
#include <TFE_DarkForces/mission.h>
#include <TFE_Jedi/Renderer/jediRenderer.h>
#include <map>
#include <algorithm>
#include <cstdio>
#include <iostream>
#include <fstream>

#ifdef _WIN32
	#include <windows.h>
#else
	#include <cstdlib>
#endif

using namespace TFE_Input;

namespace TFE_FrontEndUI
{
	enum QueuedReadType
	{
		QREAD_DIR = 0,
		QREAD_ZIP,
		QREAD_COUNT,
		QREAD_WEB
	};
	const u32 c_itemsPerFrame = 1;

	struct QueuedRead
	{
		QueuedReadType type;
		std::string path;
		std::string fileName;
	};

	struct UiImage
	{
		void* image;
		u32 width;
		u32 height;
	};

	struct ModData
	{
		std::vector<std::string> gobFiles;
		std::string textFile;
		std::string imageFile;
		UiTexture image;

		std::string name;
		std::string relativePath;
		std::string text;

		bool invertImage = true;

		std::string author;
		std::string levelName;
		std::string fileName;
		std::string filePath;
		std::string cover;
		std::string description;
		std::string walkthrough;
		std::string review;
		std::string createDate;
		std::string updateDate;
		int rating;	
	};


	static std::vector<ModData> s_mods;
	static std::vector<ModData> s_webMods;
	static std::vector<ModData*> s_filteredMods;

	static std::vector<char> s_fileBuffer;
	static std::vector<u8> s_readBuffer[2];
	static s32 s_selectedMod;

	static std::vector<QueuedRead> s_readQueue;
	static std::vector<QueuedRead> s_webReadQueue;
	static std::vector<u8> s_imageBuffer;
	static std::vector<u8> s_WebimageBuffer;
	static size_t s_readIndex = 0;
	static size_t s_webReadIndex = 0;

	static ViewMode s_viewMode = VIEW_IMAGES;


	static char s_modFilter[256] = { 0 };
	static char s_prevModFilter[256] = { 0 };
	static size_t s_filterLen = 0;
	static bool s_filterUpdated = false;
	static bool s_modsRead = false;
	char programDirModDir[TFE_MAX_PATH];
	char programDirModCacheDir[TFE_MAX_PATH];
	static bool s_useWebModUI = false;
	const char* ratings[] = {"5", "4", "3", "2", "1", "0"};
	int ratingIndex = 0;
	static bool s_loadedWebJson = false; 
	bool downloadPopUp = false;
	int waitFrames = 3;
	int frameCounter = 0;

	void fixupName(char* name);
	void readFromQueue(size_t itemsPerFrame);
	bool parseNameFromText(const char* textFileName, const char* path, char* name, std::string* fullText);
	void extractPosterFromImage(const char* baseDir, const char* zipFile, const char* imageFileName, UiTexture* poster);
	bool extractPosterFromMod(const char* baseDir, const char* archiveFileName, ModData * poster);
	void filterMods(bool filterByName, bool sort = true);

	const char* dfLevelJson = "https://df-21.net/downloads/conf_files/df_level_list.json";
	const char* modCachePath = "";

	bool sortQueueByName(QueuedRead& a, QueuedRead& b)
	{
		return strcasecmp(a.fileName.c_str(), b.fileName.c_str()) < 0;
	}

	// Handle Web Mods - pull from DF-21.net in JSON format
	void modLoaderWeb_read()
	{
		int levelCount = 0;
		s_webMods.clear();

		// Create Directory if you plan to download mods.
		sprintf(programDirModDir, "%sMods/", TFE_Paths::getPath(PATH_PROGRAM));
		TFE_Paths::fixupPathAsDirectory(programDirModDir);
		if (!FileUtil::directoryExists(programDirModDir))
		{
			FileUtil::makeDirectory(programDirModDir);
		}

		sprintf(programDirModCacheDir, "%sModCache/", TFE_Paths::getPath(PATH_PROGRAM));
		TFE_Paths::fixupPathAsDirectory(programDirModCacheDir);

		if (!FileUtil::directoryExists(programDirModCacheDir))
		{
			if (!FileUtil::makeDirectory(programDirModCacheDir))
			{
				TFE_System::logWrite(LOG_WARNING, "WEB_MODS", "Unable to create mod cache folder %s ", programDirModCacheDir);
				return;
			}
		}

		std::vector<std::string> stringModList;
		string curlResult = Download::curlWeb(dfLevelJson);
		const char* responseCharPtr = curlResult.c_str();
		cJSON* root = cJSON_Parse(responseCharPtr);
		if (root)
		{
			const cJSON* curElem = root->child;

			// There should be a total number of levels and the level list
			for (; curElem; curElem = curElem->next)
			{
				if (!curElem->string) { continue; }
						
				// Handle Level Count
				if (strcasecmp(curElem->string, "total") == 0)
				{
					levelCount = TFE_Settings::parseJSonIntToOverride(curElem);
				}

				else if (strcasecmp(curElem->string, "levels") == 0)
				{
					const cJSON* modsIter = curElem->child;
							
					if (!modsIter)
					{
						TFE_System::logWrite(LOG_WARNING, "WEB_MODS", "WEB Mod Json Array '%s' is empty!", modsIter->string);
					}

					// Level List
					for (; modsIter; modsIter = modsIter->next)
					{
						const cJSON* modIter = modsIter->child;
						ModData webMod;
						if (!modIter)
						{
							TFE_System::logWrite(LOG_WARNING, "WEB_MODS", "WEB Mod Json '%s' is empty!", modIter->string);
						}

						// Populate the ModDate struct
						for (; modIter; modIter = modIter->next)
						{
							if (strcasecmp(modIter->string, "name") == 0)
							{
								webMod.name = modIter->valuestring;
							}
							if (strcasecmp(modIter->string, "author") == 0)
							{
								webMod.author = modIter->valuestring;
							}
							else if (strcasecmp(modIter->string, "filename") == 0)
							{
								webMod.fileName = modIter->valuestring;
							}
							else if (strcasecmp(modIter->string, "filepath") == 0)
							{
								webMod.filePath = modIter->valuestring;
							}
							else if (strcasecmp(modIter->string, "levelname") == 0)
							{
								webMod.levelName = modIter->valuestring;
							}
							else if (strcasecmp(modIter->string, "cover") == 0)
							{
								webMod.cover = modIter->valuestring;
							}
							else if (strcasecmp(modIter->string, "description") == 0)
							{
								webMod.description = modIter->valuestring;
							}
							else if (strcasecmp(modIter->string, "walkthrough") == 0)
							{
								webMod.walkthrough = modIter->valuestring;
							}
							else if (strcasecmp(modIter->string, "review") == 0)
							{
								webMod.review = modIter->valuestring;
							}
							else if (strcasecmp(modIter->string, "create_date") == 0)
							{
								webMod.createDate = modIter->valuestring;
							}
							else if (strcasecmp(modIter->string, "last_mod_date") == 0)
							{
								webMod.updateDate = modIter->valuestring;
							}
							else if (strcasecmp(modIter->string, "rating") == 0)
							{
								webMod.rating = TFE_Settings::parseJSonIntToOverride(modIter);
							}

						}
						webMod.relativePath = webMod.fileName;
						webMod.gobFiles.push_back(webMod.fileName);

						/// Create Cache Directory
						char modCachePath[TFE_MAX_PATH];
						sprintf(modCachePath, "%s%s", programDirModCacheDir, webMod.levelName.c_str());
						TFE_Paths::fixupPathAsDirectory(modCachePath);

						if (!FileUtil::directoryExists(modCachePath))
						{
							if (!FileUtil::makeDirectory(modCachePath))
							{
								TFE_System::logWrite(LOG_WARNING, "WEB_MODS", "Unable to create mod cache level folder %s ", modCachePath);
								return;
							}
						}

						// Save the meta from the JSON to the cache
						char cachedModMeta[TFE_MAX_PATH];
						sprintf(cachedModMeta, "%smod.json", modCachePath);

						FILE* modCacheMeta = fopen(cachedModMeta, "w");
						if (modCacheMeta == NULL)
						{
							TFE_System::logWrite(LOG_ERROR, "ModLoader", "Cannot write downloaded meta for path: '%s'", cachedModMeta);
							continue;
						}
						char* jsonString = cJSON_Print(modsIter);
						fputs(jsonString, modCacheMeta);
						fclose(modCacheMeta);
						free(jsonString);

						s_webMods.push_back(webMod);
					}
				}
			}
		}

		std::reverse(s_webMods.begin(), s_webMods.end());
		s_filteredMods.clear();
		s_selectedMod = -1;
		clearSelectedMod();

		s_webReadQueue.clear();
		s_webReadIndex = 0;

		// Read in web paths
		for (s32 i = 0; i < s_webMods.size(); i++)
		{
			s_webReadQueue.push_back({ QREAD_WEB, s_webMods[i].filePath, "" });
		}

		std::sort(s_webReadQueue.begin(), s_webReadQueue.end(), sortQueueByName);
	}

	void modLoader_read()
	{
		// Reuse the cached mods unless no mods have been read yet.
		if (s_modsRead && s_mods.size() > 0) { return; }
		s_modsRead = true;

		s_mods.clear();
		s_filteredMods.clear();
		s_selectedMod = -1;
		clearSelectedMod();

		s_readQueue.clear();
		s_readIndex = 0;

		// There are 3 possible local mod directory locations:
		// In the TFE directory,
		// In the original source data.
		// In ProgramData/
		char sourceDataModDir[TFE_MAX_PATH];
		snprintf(sourceDataModDir, TFE_MAX_PATH, "%sMods/", TFE_Paths::getPath(PATH_SOURCE_DATA));
		TFE_Paths::fixupPathAsDirectory(sourceDataModDir);

		// Add Mods/ paths to the program data directory and local executable directory.
		// Note only directories that exist are actually added.
		const char* programData = TFE_Paths::getPath(PATH_PROGRAM_DATA);
		const char* programDir  = TFE_Paths::getPath(PATH_PROGRAM);

		char programDataModDir[TFE_MAX_PATH];
		sprintf(programDataModDir, "%sMods/", programData);
		TFE_Paths::fixupPathAsDirectory(programDataModDir);

		sprintf(programDirModDir, "%sMods/", programDir);
		TFE_Paths::fixupPathAsDirectory(programDirModDir);

		s32 modPathCount = 0;
		char modPaths[3][TFE_MAX_PATH];
		if (FileUtil::directoryExists(sourceDataModDir))
		{
			strcpy(modPaths[modPathCount], sourceDataModDir);
			modPathCount++;
		}
		if (FileUtil::directoryExists(programDataModDir))
		{
			strcpy(modPaths[modPathCount], programDataModDir);
			modPathCount++;
		}
		if (FileUtil::directoryExists(programDirModDir))
		{
			strcpy(modPaths[modPathCount], programDirModDir);
			modPathCount++;
		}

		if (!modPathCount)
		{
			s_modsRead = false;
			return;
		}

		FileList dirList, zipList;
		for (s32 i = 0; i < modPathCount; i++)
		{
			dirList.clear();
			FileUtil::readSubdirectories(modPaths[i], dirList);
			if (dirList.empty()) { continue; }

			const size_t count = dirList.size();
			const std::string* dir = dirList.data();
			for (size_t d = 0; d < count; d++)
			{
				s_readQueue.push_back({ QREAD_DIR, dir[d], "" });
			}
		}
		// Read Zip Files.
		for (s32 i = 0; i < modPathCount; i++)
		{
			zipList.clear();
			FileUtil::readDirectory(modPaths[i], "zip", zipList);
			size_t count = zipList.size();
			for (size_t z = 0; z < count; z++)
			{
				s_readQueue.push_back({ QREAD_ZIP, modPaths[i], zipList[z] });
			}
		}

		// De-dup the read queue since mods can come from different directories.
		std::map<std::string, s32> nameMap;
		std::vector<QueuedRead>::iterator iEntry = s_readQueue.begin();
		for (; iEntry != s_readQueue.end();)
		{
			std::string modDirOrZip;
			// If this is a zip file, we already have the file name.
			if (iEntry->type == QREAD_ZIP)
			{
				modDirOrZip = iEntry->fileName;
			}
			// If this is a directory, then extract after the last slash.
			else if (iEntry->path.length())  // QREAD_DIR
			{
				modDirOrZip = iEntry->path;

				size_t len = modDirOrZip.length();
				const char* str = modDirOrZip.data();
				s32 slashIndex = -1;
				for (size_t i = 0; i < len - 1; i++)
				{
					if ((str[i] == '/' || str[i] == '\\') && (str[i+1] != '/' && str[i+1] != '\\'))
					{
						slashIndex = s32(i);
					}
				}
				if (slashIndex >= 0)
				{
					modDirOrZip = &iEntry->path[slashIndex + 1];
				}
			}

			std::map<std::string, s32>::iterator iExistingEntry = nameMap.find(modDirOrZip);
			if (iExistingEntry != nameMap.end())
			{
				// This already exists and should be removed.
				iEntry = s_readQueue.erase(iEntry);
			}
			else
			{
				// Add it to the map.
				nameMap[modDirOrZip] = 1;
				++iEntry;
			}
		}

		std::sort(s_readQueue.begin(), s_readQueue.end(), sortQueueByName);
	}

	void modLoader_cleanupResources()
	{
		
		for (size_t i = 0; i < s_mods.size(); i++)
		{
			if (s_mods[i].image.texture)
			{
				TFE_RenderBackend::freeTexture(s_mods[i].image.texture);
			}
		}

		for (size_t i = 0; i < s_webMods.size(); i++)
		{
			if (s_webMods[i].image.texture)
			{
				TFE_RenderBackend::freeTexture(s_webMods[i].image.texture);
			}
		}
		s_mods.clear();
		s_filteredMods.clear();
		s_webMods.clear();
	}
		
	ViewMode modLoader_getViewMode()
	{
		return s_viewMode;
	}

	void modLoader_imageListUI(f32 uiScale)
	{
		DisplayInfo dispInfo;
		TFE_RenderBackend::getDisplayInfo(&dispInfo);
		s32 columns = max(1, (s32)((dispInfo.width - s32(16*uiScale)) / s32(268*uiScale)));

		f32 y = ImGui::GetCursorPosY();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		for (size_t i = 0; i < s_filteredMods.size();)
		{
			for (s32 x = 0; x < columns && i < s_filteredMods.size(); x++, i++)
			{
				char label[32];
				sprintf(label, "###%zd", i);
				ImGui::SetCursorPos(ImVec2((f32(x) * 268.0f + 16.0f)*uiScale, y));
				ImGui::InvisibleButton(label, ImVec2(256*uiScale, 192*uiScale));
				if (ImGui::IsItemClicked() && s_selectedMod < 0)
				{
					s_selectedMod = s32(i);
					TFE_System::logWrite(LOG_MSG, "Mods", "Selected Mod = %d", i);
				}

				f32 yScrolled = y - ImGui::GetScrollY();

				if (ImGui::IsItemHovered() || ImGui::IsItemActive())
				{
					drawList->AddImageRounded(TFE_RenderBackend::getGpuPtr(s_filteredMods[i]->image.texture), ImVec2((f32(x) * 268 + 16 - 2)*uiScale, yScrolled - 2*uiScale),
						ImVec2((f32(x) * 268 + 16 + 256 + 2)*uiScale, yScrolled + (192 + 2)*uiScale), ImVec2(0.0f, s_filteredMods[i]->invertImage ? 1.0f : 0.0f),
						ImVec2(1.0f, s_filteredMods[i]->invertImage ? 0.0f : 1.0f), 0xffffffff, 8.0f, ImDrawFlags_RoundCornersAll);
					drawList->AddImageRounded(getGradientTexture(), ImVec2((f32(x) * 268 + 16 - 2)*uiScale, yScrolled - 2*uiScale),
						ImVec2((f32(x) * 268 + 16 + 256 + 2)*uiScale, yScrolled + (192 + 2)*uiScale), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
						0x40ffb080, 8.0f, ImDrawFlags_RoundCornersAll);
				}
				else
				{
					drawList->AddImageRounded(TFE_RenderBackend::getGpuPtr(s_filteredMods[i]->image.texture), ImVec2((f32(x) * 268 + 16)*uiScale, yScrolled),
						ImVec2((f32(x) * 268 + 16 + 256)*uiScale, yScrolled + 192*uiScale), ImVec2(0.0f, s_filteredMods[i]->invertImage ? 1.0f : 0.0f),
						ImVec2(1.0f, s_filteredMods[i]->invertImage ? 0.0f : 1.0f), 0xffffffff, 8.0f, ImDrawFlags_RoundCornersAll);
				}

				ImGui::SetCursorPos(ImVec2((f32(x) * 268 + 20)*uiScale, y + 192*uiScale));

				// Limit the name to 36 characters to avoid going into the next cell.
				if (s_filteredMods[i]->name.length() <= 36)
				{
					ImGui::LabelText("###Label", "%s", s_filteredMods[i]->name.c_str());
				}
				else
				{
					char name[TFE_MAX_PATH];
					strcpy(name, s_filteredMods[i]->name.c_str());
					name[33] = '.';
					name[34] = '.';
					name[35] = '.';
					name[36] = 0;
					ImGui::LabelText("###Label", "%s", name);
				}
			}
			y += 232*uiScale;
		}
	}

	void modLoader_NameListUI(f32 uiScale)
	{
		DisplayInfo dispInfo;
		TFE_RenderBackend::getDisplayInfo(&dispInfo);
		f32 topPos = ImGui::GetCursorPosY();
		s32 rowCount = (dispInfo.height - s32(topPos + 24 * uiScale)) / s32(28*uiScale) - 1;

		char buttonLabel[32];
		ImGui::PushFont(getDialogFont());
		size_t i = 0;
		for (s32 x = 0; i < s_filteredMods.size(); x++)
		{
			for (s32 y = 0; y < rowCount && i < s_filteredMods.size(); y++, i++)
			{
				sprintf(buttonLabel, "###mod%zd", i);
				ImVec2 cursor((8.0f + x * 410)*uiScale, (topPos + y * 28)*uiScale);
				ImGui::SetCursorPos(cursor);
				if (ImGui::Button(buttonLabel, ImVec2(400*uiScale, 24*uiScale)) && s_selectedMod < 0)
				{
					s_selectedMod = s32(i);
					TFE_System::logWrite(LOG_MSG, "Mods", "Selected Mod = %d", i);
				}
				
				ImGui::SetCursorPos(ImVec2(cursor.x + 8.0f*uiScale, cursor.y - 2.0f*uiScale));
				char name[TFE_MAX_PATH];
				strcpy(name, s_filteredMods[i]->name.c_str());
				size_t len = strlen(name);
				if (len > 36)
				{
					name[33] = '.';
					name[34] = '.';
					name[35] = '.';
					name[36] = 0;
				}

				ImGui::LabelText("###", "%s", name);
			}
		}
		ImGui::PopFont();
	}

	void modLoader_FileListUI(f32 uiScale)
	{
		DisplayInfo dispInfo;
		TFE_RenderBackend::getDisplayInfo(&dispInfo);
		f32 topPos = ImGui::GetCursorPosY();
		s32 rowCount = (dispInfo.height - s32(topPos + 24 * uiScale)) / s32(28*uiScale) - 1;

		char buttonLabel[32];
		ImGui::PushFont(getDialogFont());
		size_t i = 0;
		for (s32 x = 0; i < s_filteredMods.size(); x++)
		{
			for (s32 y = 0; y < rowCount && i < s_filteredMods.size(); y++, i++)
			{
				sprintf(buttonLabel, "###mod%zd", i);
				ImVec2 cursor((8.0f + x * 410)*uiScale, (topPos + y * 28)*uiScale);
				ImGui::SetCursorPos(cursor);
				if (ImGui::Button(buttonLabel, ImVec2(400*uiScale, 24*uiScale)) && s_selectedMod < 0)
				{
					s_selectedMod = s32(i);
					TFE_System::logWrite(LOG_MSG, "Mods", "Selected Mod = %d", i);
				}

				ImGui::SetCursorPos(ImVec2(cursor.x + 8.0f*uiScale, cursor.y - 2.0f*uiScale));
				char name[TFE_MAX_PATH];
				strcpy(name, s_filteredMods[i]->gobFiles[0].c_str());
				size_t len = strlen(name);
				if (len > 36)
				{
					name[33] = '.';
					name[34] = '.';
					name[35] = '.';
					name[36] = 0;
				}

				ImGui::LabelText("###", "%s", name);
			}
		}
		ImGui::PopFont();
	}

	void modLoader_WebListUI(f32 uiScale)
	{
		DisplayInfo dispInfo;
		TFE_RenderBackend::getDisplayInfo(&dispInfo);
		f32 topPos = ImGui::GetCursorPosY();
		s32 rowCount = (dispInfo.height - s32(topPos + 24 * uiScale)) / s32(28 * uiScale) - 1;

		char buttonLabel[32];
		ImGui::PushFont(getDialogFont());
		size_t i = 0;
		for (s32 x = 0; i < s_filteredMods.size(); x++)
		{
			for (s32 y = 0; y < rowCount && i < s_filteredMods.size(); y++, i++)
			{
				sprintf(buttonLabel, "###mod%zd", i);
				ImVec2 cursor((8.0f + x * 410) * uiScale, (topPos + y * 28) * uiScale);
				ImGui::SetCursorPos(cursor);				
				
				if (ImGui::Button(buttonLabel, ImVec2(400 * uiScale, 24 * uiScale)) && s_selectedMod < 0)
				{
					s_selectedMod = s32(i);
					TFE_System::logWrite(LOG_MSG, "Mods", "Selected Mod = %d", i);
				}

				float hoverAlpha = ImGui::IsItemHovered() ? 1.0f : 0.7f;				

				ImGui::SetCursorPos(ImVec2(cursor.x + 8.0f * uiScale, cursor.y - 2.0f * uiScale));
				char name[TFE_MAX_PATH];
				strcpy(name, s_filteredMods[i]->name.c_str());
				size_t len = strlen(name);
				if (len > 36)
				{
					name[33] = '.';
					name[34] = '.';
					name[35] = '.';
					name[36] = 0;
				}
				char modPath[TFE_MAX_PATH];
				sprintf(modPath, "%s%s", programDirModDir, s_filteredMods[i]->fileName.c_str());
				

				if (FileUtil::exists(modPath))
				{
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, hoverAlpha));
				}
				else
				{
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.6f, 0.0f, hoverAlpha));
				}
				
				ImGui::LabelText("###", "%s", name);

				ImGui::PopStyleColor();
			}
		}
		ImGui::PopFont();
	}

	void modLoader_preLoad()
	{
		readFromQueue(c_itemsPerFrame);
	}

	void handleWebQueueItem(ModData* mod)
	{		

		char coverPath[TFE_MAX_PATH];
		sprintf(coverPath, "%scover.png", modCachePath);
		if (!FileUtil::exists(coverPath))
		{
			if (!Download::download(mod->cover.c_str(), coverPath))
			{
				return;
			}
		}

		mod->textFile = mod->description;
		mod->text = "Author: " + mod->author + "\n\n" +
			"Released: " + mod->createDate + "\n\n" +
			"Updated: " + mod->updateDate + "\n\n" +
			"Rating: " + std::to_string(mod->rating) + "\n\n" +
			mod->description;
		extractPosterFromImage("", nullptr, coverPath, &mod->image);

		mod->invertImage = false;
	}	

	bool modLoader_selectionUI()
	{
		bool stayOpen = true;
		f32 uiScale = (f32)TFE_Ui::getUiScale() * 0.01f;

		// Load in the mod data a few at a time so to limit waiting for loading.
		readFromQueue(c_itemsPerFrame);
		clearSelectedMod();

		ImGui::Separator();
		ImGui::PushFont(getDialogFont());
		ImVec2 sideBarButtonSize(144 * uiScale, 24 * uiScale);
		
		if (!s_useWebModUI)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
		}

		if (ImGui::Button("Local Mods", sideBarButtonSize))
		{
			s_useWebModUI = false;
			s_viewMode = VIEW_IMAGES;
			filterMods(true);
		}
		ImGui::SameLine();

		if (s_useWebModUI)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
		}
		
		if (ImGui::Button("Download Mods", sideBarButtonSize))
		{
			s_useWebModUI = true;

			if (!s_loadedWebJson)
			{
				// Mod Loader Web Check
				modLoaderWeb_read();
				s_loadedWebJson = true;
			}
		}
		ImGui::PopStyleColor(2);
		ImGui::PopFont();

		bool viewImages = s_viewMode == VIEW_IMAGES;
		bool viewNameList = s_viewMode == VIEW_NAME_LIST;
		bool viewFileList = s_viewMode == VIEW_FILE_LIST;
		bool viewWebList = s_viewMode == VIEW_WEB_LIST;

		if (!s_useWebModUI)
		{
			ImGui::PushFont(getDialogFont());
			ImGui::Separator();
			ImGui::LabelText("###", "VIEW");
			ImGui::SameLine(128.0f * uiScale);

			if (ImGui::Checkbox("Images", &viewImages))
			{
				if (viewImages)
				{
					s_viewMode = VIEW_IMAGES;
				}
				else
				{
					s_viewMode = VIEW_NAME_LIST;
				}
				filterMods(s_viewMode != VIEW_FILE_LIST);
			}
			ImGui::SameLine(236.0f * uiScale);
			if (ImGui::Checkbox("Name List", &viewNameList))
			{
				if (viewNameList)
				{
					s_viewMode = VIEW_NAME_LIST;
				}
				else
				{
					s_viewMode = VIEW_FILE_LIST;
				}
				filterMods(s_viewMode != VIEW_FILE_LIST);
			}
			ImGui::SameLine(380.0f * uiScale);
			if (ImGui::Checkbox("File List", &viewFileList))
			{
				if (viewFileList)
				{
					s_viewMode = VIEW_FILE_LIST;
				}
				else
				{
					s_viewMode = VIEW_IMAGES;
				}
				filterMods(s_viewMode != VIEW_FILE_LIST);
			}

			ImGui::SameLine(510.0f * uiScale);
			if (ImGui::Button("Refresh Mod Listing"))
			{
				s_modsRead = false;
				modLoader_read();
			}


			ImGui::SameLine(730.0f * uiScale);
			if (ImGui::Button("Open Mod Folder"))
			{				
				#ifdef _WIN32
					ShellExecute(NULL, "open", programDirModDir, NULL, NULL, SW_SHOWNORMAL);
				#else
				    string xdg_string = "xdg-open " + std::string(programDirModDir);
					system(xdg_string.c_str());
				#endif
			}
			

			ImGui::Separator();
			ImGui::PopFont();
		}
		else
		{
			s_viewMode = VIEW_WEB_LIST;
			filterMods(true);
		}

		if (s_mods.empty() && !s_useWebModUI) { return stayOpen; }
		ImGui::PushFont(getDialogFont());

		// Filter
		ImGui::Text("Filter");
		ImGui::SameLine(80.0f * uiScale);

		ImGui::PopFont();
		ImGui::SetNextItemWidth(300.0f * uiScale);
		ImGui::InputText("##FilterText", s_modFilter, 64);
		ImGui::SameLine(400.0f * uiScale);
		if (ImGui::Button("CLEAR"))
		{
			s_modFilter[0] = 0;
			s_filterLen = 0;
		}
		else
		{
			s_filterLen = strlen(s_modFilter);
		}
		
		if (s_useWebModUI)
		{
			ImGui::SameLine(460.0f * uiScale);
			ImGui::SetNextItemWidth(70.0f);
			if (ImGui::BeginCombo("Rating", ratings[ratingIndex]))
			{
				for (int n = 0; n < IM_ARRAYSIZE(ratings); n++)
				{
					const bool is_selected = (ratingIndex == n);
					if (ImGui::Selectable(ratings[n], is_selected))
						ratingIndex = n;

					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}

		if (s_prevModFilter[0] != s_modFilter[0] || strlen(s_prevModFilter) != s_filterLen || strcasecmp(s_prevModFilter, s_modFilter))
		{
			s_prevModFilter[0] = 0;
			strcpy(s_prevModFilter, s_modFilter);
			filterMods(s_viewMode != VIEW_FILE_LIST);
		}

		ImGui::Separator();			

		if (s_viewMode == VIEW_IMAGES)
		{
			modLoader_imageListUI(uiScale);
		}
		else if (s_viewMode == VIEW_NAME_LIST)
		{
			modLoader_NameListUI(uiScale);
		}
		else if (s_viewMode == VIEW_FILE_LIST)
		{
			modLoader_FileListUI(uiScale);			
		}
		else if (s_viewMode == VIEW_WEB_LIST)
		{
			modLoader_WebListUI(uiScale);
		}			

		if (s_selectedMod >= 0)
		{
			DisplayInfo dispInfo;
			TFE_RenderBackend::getDisplayInfo(&dispInfo);

			bool open = true;
			bool retFromLoader = false;
			s32 infoWidth = dispInfo.width - s32(120 * uiScale);
			s32 infoHeight = dispInfo.height - s32(120 * uiScale);

			const u32 window_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
			ImGui::SetCursorPos(ImVec2(10 * uiScale, 10 * uiScale));
			ImGui::Begin("Mod Info", &open, /*ImVec2(f32(infoWidth), f32(infoHeight)), 1.0f,*/ window_flags);
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 cursor = ImGui::GetCursorPos();
			
			if (s_useWebModUI)
			{
				handleWebQueueItem(s_filteredMods[s_selectedMod]);
			}

			drawList->AddImageRounded(TFE_RenderBackend::getGpuPtr(s_filteredMods[s_selectedMod]->image.texture),
				ImVec2(cursor.x + 64, cursor.y + 64),
				ImVec2(cursor.x + 64 + 320 * uiScale, cursor.y + 64 + 200 * uiScale),
				ImVec2(0.0f, s_filteredMods[s_selectedMod]->invertImage ? 1.0f : 0.0f),
				ImVec2(1.0f, s_filteredMods[s_selectedMod]->invertImage ? 0.0f : 1.0f),
				0xffffffff, 8.0f, ImDrawFlags_RoundCornersAll);
			ImGui::PushFont(getDialogFont());
			ImGui::SetCursorPosX(cursor.x + (320 + 70) * uiScale);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.9f, 1.0f, 1.0f));
			ImGui::LabelText("###", "%s", s_filteredMods[s_selectedMod]->name.c_str());
			ImGui::PopStyleColor();

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.75f));
			ImGui::SetCursorPos(ImVec2(cursor.x + 10 * uiScale, cursor.y + 220 * uiScale));
			ImGui::Text("Game: Dark Forces");
			ImGui::SetCursorPosX(cursor.x + 10 * uiScale);
			ImGui::Text("Type: Vanilla Compatible");
			ImGui::SetCursorPosX(cursor.x + 10 * uiScale);
			if (s_useWebModUI)
			{
				ImGui::Text("File: %s", s_filteredMods[s_selectedMod]->fileName.c_str());
			}
			else
			{
				ImGui::Text("File: %s", s_filteredMods[s_selectedMod]->gobFiles[0].c_str());
			}
			ImGui::PopStyleColor();

			ImGui::SetCursorPos(ImVec2(cursor.x + 90 * uiScale, cursor.y + 320 * uiScale));

			char modPath[TFE_MAX_PATH];
			sprintf(modPath, "%s%s", programDirModDir, s_filteredMods[s_selectedMod]->fileName.c_str());

			bool alreadyDownloaded = FileUtil::exists(modPath);

			if (alreadyDownloaded)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.5f, 0.0f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.8f, 0.0f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.0f, 0.0f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.0f, 0.0f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
			}

			if (s_useWebModUI && !alreadyDownloaded)
			{			
				bool downloadSuccessful = true; 

				if (downloadPopUp)
				{
					ImVec2 windowSize = ImGui::GetWindowSize();

					// Calculate the center position
					ImVec2 popupSize = ImVec2(200, 100); 
					ImVec2 center = ImVec2((windowSize.x - popupSize.x) * 0.5f, (windowSize.y - popupSize.y) * 0.5f);

					ImGui::SetNextWindowPos(center, ImGuiCond_Always);
					ImGui::OpenPopup("##download");
				}

				if (ImGui::BeginPopupModal("##download", NULL, ImGuiWindowFlags_AlwaysAutoResize))
				{
					ImGui::Text("Downloading, please wait...");

					// Render the popup first, then trigger the download in the next frame 
					// Otherwise the thing won't be seen and user will see a freeze. 
					if (downloadPopUp)
					{
						frameCounter++;
						if (frameCounter >= waitFrames)
						{							
							downloadSuccessful = Download::download(s_filteredMods[s_selectedMod]->filePath.c_str(), modPath);

							s_mods.push_back(*s_filteredMods[s_selectedMod]);
					
							downloadPopUp = false;
							ImGui::CloseCurrentPopup();
							frameCounter = 0;
						}
					}
					ImGui::EndPopup();
				}
				if (!downloadSuccessful) ImGui::OpenPopup("##download_error");

				if (ImGui::BeginPopupModal("##download_error", NULL, ImGuiWindowFlags_AlwaysAutoResize))
				{
					ImGui::Text("Error Downloading Mod!");

					ImVec2 availableSpace = ImGui::GetContentRegionAvail();
					float buttonWidth = ImGui::CalcTextSize("OK").x + ImGui::GetStyle().FramePadding.x * 2.0f;

					ImGui::SetCursorPosX((availableSpace.x - buttonWidth) * 0.5f);

					if (ImGui::Button("OK"))
					{						
						ImGui::CloseCurrentPopup();
					}

					ImGui::EndPopup();
				}

				if (ImGui::Button("Download", ImVec2(128 * uiScale, 32 * uiScale)))
				{
					downloadPopUp = true;  // Show the popup
					frameCounter = 0;
				}				
			}
			else
			{
				if (ImGui::Button("PLAY", ImVec2(128 * uiScale, 32 * uiScale)))
				{

					char selectedModCmd[TFE_MAX_PATH]; 
					sprintf(selectedModCmd, "-u%s%s", s_filteredMods[s_selectedMod]->relativePath.c_str(), s_filteredMods[s_selectedMod]->gobFiles[0].c_str());
					setSelectedMod(selectedModCmd);

					setState(APP_STATE_GAME);
					clearMenuState();
					open = false;
					retFromLoader = true;
				}
			}
			ImGui::PopStyleColor(3);

			ImGui::SetCursorPos(ImVec2(cursor.x + 90 * uiScale, cursor.y + 360 * uiScale));
			if (ImGui::Button("CANCEL", ImVec2(128 * uiScale, 32 * uiScale)) || TFE_Input::keyPressed(KEY_ESCAPE))
			{
				open = false;
			}

			if (!s_filteredMods[s_selectedMod]->filePath.empty())
			{
				ImGui::SetCursorPos(ImVec2(cursor.x + 90 * uiScale, cursor.y + 400 * uiScale));
				char levelUrl[TFE_MAX_PATH];
				FileUtil::getFilePath(s_filteredMods[s_selectedMod]->filePath.c_str(), levelUrl);

				if (ImGui::Button("OPEN SITE", ImVec2(128 * uiScale, 32 * uiScale)) || TFE_Input::keyPressed(KEY_ESCAPE))
				{
				#ifdef _WIN32
					ShellExecute(0, 0, levelUrl, 0, 0, SW_SHOW);
				#else
					string xdg_string = "xdg-open " + std::string(levelUrl);
					system(xdg_string.c_str());
				#endif
				}
			}

			int buttonOffset = 40;
			if (!s_filteredMods[s_selectedMod]->review.empty())
			{
				// Keep track of button offset in case there are no reviews.
				string reviewURL = s_filteredMods[s_selectedMod]->review;
				if (reviewURL != "")
				{
					ImGui::SetCursorPos(ImVec2(cursor.x + 90 * uiScale, cursor.y + 440 * uiScale));
					if (ImGui::Button("REVIEW", ImVec2(128 * uiScale, 32 * uiScale)) || TFE_Input::keyPressed(KEY_ESCAPE))
					{

					#ifdef _WIN32
						ShellExecute(0, 0, reviewURL.c_str(), 0, 0, SW_SHOW);
					#else
						string xdg_string = "xdg-open " + reviewURL;
						system(xdg_string.c_str());
					#endif
					}
				}
				else
				{
					buttonOffset = 0;
				}
			}

			if (!s_filteredMods[s_selectedMod]->walkthrough.empty())
			{
				string walkthroughURL = s_filteredMods[s_selectedMod]->walkthrough;
				if (walkthroughURL != "")
				{
					ImGui::SetCursorPos(ImVec2(cursor.x + 90 * uiScale, cursor.y + (440 + buttonOffset) * uiScale));
					if (ImGui::Button("WALKTHROUGH", ImVec2(128 * uiScale, 32 * uiScale)) || TFE_Input::keyPressed(KEY_ESCAPE))
					{

					#ifdef _WIN32
						ShellExecute(0, 0, walkthroughURL.c_str(), 0, 0, SW_SHOW);
					#else
						string xdg_string = "xdg-open " + walkthroughURL;
						system(xdg_string.c_str());					
					#endif
					}
				}
			}
			


			ImGui::PopFont();

			ImGui::SetCursorPos(ImVec2(cursor.x + 328 * uiScale, cursor.y + 30 * uiScale));
			ImGui::BeginChild("###Mod Info Text", ImVec2(f32(infoWidth - 344 * uiScale), f32(infoHeight - 68 * uiScale)), true, ImGuiWindowFlags_NoBringToFrontOnFocus);

			// TODO: Modify imGUI to make the internal "temp" text buffer at least as large as the input text.
			
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 0.75f));
			
			ImGui::TextWrapped("%s", s_filteredMods[s_selectedMod]->text.c_str());
			ImGui::PopStyleColor();
			ImGui::EndChild();

			if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows | ImGuiFocusedFlags_RootWindow))
			{
				open = false;
			}
			ImGui::End();

			if (!open)
			{
				s_selectedMod = -1;
			}
		}
		else if (TFE_Input::keyPressed(KEY_ESCAPE))
		{
			stayOpen = false;
		}
	
		return stayOpen;
	}

	void fixupName(char* name)
	{
		size_t len = strlen(name);
		name[0] = toupper(name[0]);
		for (size_t i = 1; i < len; i++)
		{
			name[i] = tolower(name[i]);
		}
	}

	bool parseNameFromText(const char* textFileName, const char* path, char* name, std::string* fullText)
	{
		if (!textFileName || textFileName[0] == 0) { return false; }

		const size_t len = strlen(textFileName);
		const char* ext = &textFileName[len - 3];
		size_t textLen = 0;
		if (strcasecmp(ext, "zip") == 0)
		{
			ZipArchive zipArchive;
			char zipPath[TFE_MAX_PATH];
			sprintf(zipPath, "%s%s", path, textFileName);
			if (zipArchive.open(zipPath))
			{
				s32 txtIndex = -1;
				const u32 count = zipArchive.getFileCount();
				for (u32 i = 0; i < count; i++)
				{
					const char* name = zipArchive.getFileName(i);
					const size_t nameLen = strlen(name);
					const char* zext = &name[nameLen - 3];
					if (strcasecmp(zext, "txt") == 0)
					{
						txtIndex = i;
						break;
					}
				}

				if (txtIndex >= 0 && zipArchive.openFile(txtIndex))
				{
					textLen = zipArchive.getFileLength();
					s_fileBuffer.resize(textLen + 1);
					s_fileBuffer[0] = 0;
					zipArchive.readFile(s_fileBuffer.data(), textLen);
					zipArchive.closeFile();
				}
			}
		}
		else
		{
			char fullPath[TFE_MAX_PATH];
			sprintf(fullPath, "%s%s", path, textFileName);

			FileStream textFile;
			if (!textFile.open(fullPath, Stream::MODE_READ))
			{
				return false;
			}
			textLen = textFile.getSize();
			s_fileBuffer.resize(textLen + 1);
			s_fileBuffer[0] = 0;
			textFile.readBuffer(s_fileBuffer.data(), (u32)textLen);
			textFile.close();
		}
		if (!textLen || s_fileBuffer[0] == 0)
		{
			return false;
		}

		// Some files start with garbage at the beginning...
		// So try a small probe first to see if such fixup is reqiured.
		bool needsFixup = false;
		for (size_t i = 0; i < 10 && i < s_fileBuffer.size(); i++)
		{
			if (s_fileBuffer[i] == 0)
			{
				needsFixup = true;
				break;
			}
		}
		// If it is, try to find a valid start.
		size_t lastZero = 0;
		if (needsFixup)
		{
			size_t len = s_fileBuffer.size();
			const char* text = s_fileBuffer.data();
			for (size_t i = 0; i < len - 1 && i < 128; i++)
			{
				if (text[i] == 0)
				{
					lastZero = i;
				}
			}
			if (lastZero) { lastZero++; }
		}
		*fullText = std::string(s_fileBuffer.data() + lastZero, s_fileBuffer.data() + s_fileBuffer.size());

		TFE_Parser parser;
		parser.init(fullText->c_str(), fullText->length());
		// First pass - look for "Title" followed by ':'
		size_t bufferPos = 0;
		size_t titleLen = strlen("Title");
		bool foundTitle = false;
		while (!foundTitle)
		{
			const char* line = parser.readLine(bufferPos, true);
			if (!line)
			{
				break;
			}

			if (strncasecmp("Title", line, titleLen) == 0)
			{
				size_t lineLen = strlen(line);
				for (size_t c = titleLen + 1; c < lineLen; c++)
				{
					if (line[c] == ':' && line[c + 1] == ' ')
					{
						// Found it.
						strcpy(name, &line[c + 2]);
						foundTitle = true;
						break;
					}
				}
			}
		}

		// Second pass - look for "name": - JSon syntax.
		const char* jsonName = "\"name\":";
		bufferPos = 0;
		size_t nameLen = strlen(jsonName);
		while (!foundTitle)
		{
			const char* line = parser.readLine(bufferPos, true);
			if (!line)
			{
				break;
			}
			if (strncasecmp(jsonName, line, nameLen) == 0)
			{
				size_t lineLen = strlen(line);
				for (size_t c = nameLen + 1; c < lineLen && !foundTitle; c++)
				{
					if (line[c] == '\"')
					{
						for (size_t c2 = c + 1; c2 < lineLen; c2++)
						{
							if (line[c2] == '\"')
							{
								// Found it.
								memcpy(name, &line[c + 1], c2 - c - 1);
								name[c2 - c - 1] = 0;
								foundTitle = true;
								break;
							}
						}
					}
				}
			}
		}

		if (!foundTitle)
		{
			// Looking for "Title" failed, try reading the first 'valid' line.
			bufferPos = 0;
			while (!foundTitle)
			{
				const char* line = parser.readLine(bufferPos, true);
				if (!line)
				{
					break;
				}
				if (line[0] == '<' || line[0] == '>' || line[0] == '=' || line[0] == '|')
				{
					continue;
				}
				if (strncasecmp(line, "PRESENTING", strlen("PRESENTING")) == 0)
				{
					continue;
				}

				strcpy(name, line);
				foundTitle = true;
				break;
			}
		}

		if (foundTitle)
		{
			size_t titleLen = strlen(name);
			size_t lastValid = 0;
			for (size_t c = 0; c < titleLen; c++)
			{
				if (name[c] != ' ' && name[c] != '-')
				{
					lastValid = c;
				}
				if (name[c] == '-')
				{
					break;
				}
			}
			if (lastValid > 0)
			{
				name[lastValid + 1] = 0;
			}
			if (lastValid && name[0] != '/')
			{
				return true;
			}
		}
		return false;
	}

	// Return true if 'str' matches the 'filter', taking into account special symbols.
	bool stringFilter(const char* str, const char* filter, size_t filterLength)
	{
		for (size_t i = 0; i < filterLength; i++)
		{
			if (filter[i] == '?') { continue; }
			if (tolower(str[i]) != tolower(filter[i])) { return false; }
		}
		return true;
	}

	bool filter(const std::string& name)
	{
		// No filter applied.
		if (s_modFilter[0] == 0 || s_filterLen == 0) { return true; }

		// Make sure the name is at least as long as the filter.
		const size_t nameLength = name.length();
		if (nameLength < s_filterLen) { return false; }

		// See if there is a match...
		const char* nameStr = name.c_str();
		const size_t validLength = nameLength - s_filterLen + 1;
		for (size_t i = 0; i < validLength; i++)
		{
			if (stringFilter(&nameStr[i], s_modFilter, s_filterLen))
			{
				return true;
			}
		}
		return false;
	}

	bool sortFilteredByName(ModData*& a, ModData*& b)
	{
		return strcasecmp(a->name.c_str(), b->name.c_str()) < 0;
	}

	bool sortFilteredByFile(ModData*& a, ModData*& b)
	{
		if (s_useWebModUI)
		{
			return strcasecmp(a->levelName.c_str(), b->levelName.c_str()) < 0;
		}
		else
		{
			return strcasecmp(a->gobFiles[0].c_str(), b->gobFiles[0].c_str()) < 0;
		}
	}

	void filterMods(bool filterByName, bool sort)
	{
		const size_t count = s_useWebModUI ? s_webMods.size() : s_mods.size();
		s_filteredMods.clear();
		s_filteredMods.reserve(count);

		// Filter by Mod name or by filename.
		// This is done by looping through all of the mods in the list and then adding them to
		// filtered mods if the filter passes.

		static std::vector<ModData> filterModsList;
		if (s_useWebModUI)
		{
			filterModsList = s_webMods;			
		}
		else
		{
			filterModsList = s_mods;
		} 
		std::string filterFileType;

		for (size_t i = 0; i < count; i++)
		{	

			// Filter out ratings for web mods.
			if (s_useWebModUI && filterModsList[i].rating != std::atoi(ratings[ratingIndex]))
			{
				continue;
			}
			
			filterFileType = s_useWebModUI ? filterModsList[i].levelName : filterModsList[i].gobFiles[0];
			

			if (filter(filterByName ? filterModsList[i].name : filterFileType))
			{
				s_filteredMods.push_back(&filterModsList[i]);
			}
		}

		// Then apply an alphabetical sorting to the filtered result.
		// Note - other methods of sorting can be implemented if needed in the future by changing the sort functions.
		if (sort)
		{
			std::sort(s_filteredMods.begin(), s_filteredMods.end(), filterByName ? sortFilteredByName : sortFilteredByFile);
		}
	}

	bool getCachedModDir(const char* modName, char* modCachedPath)
	{
		char modCachePathTemp[TFE_MAX_PATH];
		// Check if any of the maps are of the df21 format split by underscores
		// Ex: convert aons_modern.zip to aons to find it in the ModCache
		const char* underscorePos = std::strchr(modName, '_');
		if (underscorePos != nullptr)
		{
			// Copy characters from input up to the underscore
			std::strncpy(modCachedPath, modName, underscorePos - modName);
			modCachedPath[underscorePos - modName] = '\0';
		}
		else
		{
			std::strcpy(modCachedPath, modName);
		}

		sprintf(modCachePathTemp, "%sModCache/%s", TFE_Paths::getPath(PATH_PROGRAM), modCachedPath);
		TFE_Paths::fixupPathAsDirectory(modCachePathTemp);

		if (!FileUtil::directoryExists(modCachePathTemp))
		{
			return false;
		}
		strcpy(modCachedPath, modCachePathTemp);
		return true;
	}

	bool loadCoverPathFromMod(const char* modGobName, UiTexture* poster)
	{
		char coverCachedPath[TFE_MAX_PATH];

		if (getCachedModDir(modGobName, coverCachedPath))
		{
			char coverPath[TFE_MAX_PATH];
			sprintf(coverPath, "%scover.png", coverCachedPath);
			FileUtil::fixupPath(coverPath);

			if (FileUtil::exists(coverPath))
			{
				extractPosterFromImage("", nullptr, coverPath, poster);
				return true;
			}
		}

		return false;
	}

	void updateModFromCacheMeta(ModData* mod)
	{

		// Check if we already downloaded this mod and update s_mods
		// For now just use the filename and later 
		// we may want to use libcrypto's MDP hash checking?
		char modCachePath[TFE_MAX_PATH];
		if (getCachedModDir(mod->gobFiles[0].c_str(), modCachePath))
		{

			// Load the meta from the JSON from the cache
			char cachedModMeta[TFE_MAX_PATH];
			sprintf(cachedModMeta, "%s%mod.json", modCachePath);

			FileStream modFile;
			if (!FileUtil::exists(cachedModMeta))
			{
				return;
			}

			if (!modFile.open(cachedModMeta, FileStream::MODE_READ))
			{
				return;
			}

			const size_t size = modFile.getSize();
			char* data = (char*)malloc(size + 1);
			if (!data || size == 0)
			{
				TFE_System::logWrite(LOG_ERROR, "ModLoader", "Mod Meta is empty %s", modCachePath);
				return;
			}
			modFile.readBuffer(data, (u32)size);
			data[size] = 0;
			modFile.close();

			cJSON* root = cJSON_Parse(data);

			for (; root; root = root->next)
			{
				if (root->type == cJSON_Object)
				{
					cJSON* walkthrough = cJSON_GetObjectItem(root, "walkthrough");
					if (walkthrough)
					{
						mod->walkthrough = walkthrough->valuestring;
					}

					cJSON* review = cJSON_GetObjectItem(root, "review");
					if (review)
					{
						mod->review = review->valuestring;
					}

					cJSON* filePath = cJSON_GetObjectItem(root, "filePath");
					if (filePath)
					{
						mod->filePath = filePath->valuestring;
					}

					cJSON* name = cJSON_GetObjectItem(root, "name");
					if (name)
					{
						mod->name = name->valuestring;
					}

					// Use this later in a future commit to pretty print this data
					/*
					cJSON* text = cJSON_GetObjectItem(root, "text");
					if (text)
					{
						mod->text = text->valuestring;
					}
					*/

				}
			}

			cJSON_Delete(root);
		}
	}

	void readFromQueue(size_t itemsPerFrame)
	{
		FileList gobFiles, txtFiles, imgFiles;
		int readIndex = s_useWebModUI ? s_webReadIndex : s_readIndex;
		const size_t readEnd = s_useWebModUI ? min(s_webReadIndex + itemsPerFrame, s_webReadQueue.size()) : min(s_readIndex + itemsPerFrame, s_readQueue.size());
		const QueuedRead* reads = s_useWebModUI ? s_webReadQueue.data() : s_readQueue.data();

		bool updateFilter = false;
		for (size_t i = readIndex; i < readEnd; i++, readIndex++)
		{
			updateFilter = true;
			if (reads[i].type == QREAD_DIR)
			{
				// Clear doesn't deallocate in most implementations, so doing it this way should reduce memory allocations.
				gobFiles.clear();
				txtFiles.clear();
				imgFiles.clear();

				const char* subDir = reads[i].path.c_str();
				FileUtil::readDirectory(subDir, "gob", gobFiles);
				FileUtil::readDirectory(subDir, "txt", txtFiles);
				FileUtil::readDirectory(subDir, "jpg", imgFiles);

				// No gob files = no mod.
				if (gobFiles.size() != 1)
				{
					continue;
				}
				
				s_mods.push_back({});
				ModData& mod =s_mods.back();  
			
				mod.gobFiles = gobFiles;
				mod.textFile = txtFiles.empty() ? "" : txtFiles[0];
				mod.imageFile = imgFiles.empty() ? "" : imgFiles[0];				
				mod.text = "";				

				size_t fullDirLen = strlen(subDir);
				for (size_t i = 0; i < fullDirLen; i++)
				{
					if (strncasecmp("Mods", &subDir[i], 4) == 0)
					{
						mod.relativePath = &subDir[i + 5];
						break;
					}
				}

				if (mod.imageFile.empty())
				{
					if (!extractPosterFromMod(subDir, mod.gobFiles[0].c_str(), &mod))
					{
						s_mods.pop_back();
						continue;
					}					
				}
				else
				{
					extractPosterFromImage(subDir, nullptr, mod.imageFile.c_str(), &mod.image);
					mod.invertImage = false;
				}

				char name[TFE_MAX_PATH];
				if (!parseNameFromText(mod.textFile.c_str(), subDir, name, &mod.text))
				{
					const char* gobFileName = mod.gobFiles[0].c_str();
					memcpy(name, gobFileName, strlen(gobFileName) - 4);
					name[strlen(gobFileName) - 4] = 0;
					fixupName(name);
				}

				mod.name = name;
				updateModFromCacheMeta(&mod);
			}
			else if (reads[i].type == QREAD_WEB)
			{
				// no op - maybe we should add some kind of thottling but 
				// you really just load a simple json which is a few kilobytes. 
			}
			else
			{
				const char* modPath = reads[i].path.c_str();
				const char* zipName = reads[i].fileName.c_str();
				ZipArchive zipArchive;

				char zipPath[TFE_MAX_PATH];
				sprintf(zipPath, "%s%s", modPath, zipName);
				if (!zipArchive.open(zipPath)) { return; }

				s32 gobFileIndex = -1;
				s32 txtFileIndex = -1;
				s32 jpgFileIndex = -1;

				// Look for the following:
				// 1. Gob File.
				// 2. Text File.
				// 3. JPG
				for (u32 f = 0; f < zipArchive.getFileCount(); f++)
				{
					const char* fileName = zipArchive.getFileName(f);
					size_t len = strlen(fileName);
					if (len <= 4)
					{
						continue;
					}
					const char* ext = &fileName[len - 3];
					if (strcasecmp(ext, "gob") == 0)
					{
						gobFileIndex = s32(f);
					}
					else if (strcasecmp(ext, "txt") == 0)
					{
						txtFileIndex = s32(f);
					}
					else if (strcasecmp(ext, "jpg") == 0)
					{
						jpgFileIndex = s32(f);
					}
				}
				if (gobFileIndex >= 0)
				{
					s_mods.push_back({});
					ModData& mod = s_mods.back();
					mod.gobFiles.push_back(zipName);
					mod.text = "";

					char name[TFE_MAX_PATH];
					if (!parseNameFromText(mod.gobFiles[0].c_str(), modPath, name, &mod.text))
					{
						const char* gobFileName = mod.gobFiles[0].c_str();
						memcpy(name, gobFileName, strlen(gobFileName) - 4);
						name[strlen(gobFileName) - 4] = 0;
						fixupName(name);
					}
					mod.name = name;

					if (jpgFileIndex < 0)
					{												
					    if (!extractPosterFromMod(modPath, mod.gobFiles[0].c_str(), &mod))
						{
							s_mods.pop_back();
						}
					}
					else
					{
						extractPosterFromImage(modPath, mod.gobFiles[0].c_str(), zipArchive.getFileName(jpgFileIndex), &mod.image);
						mod.invertImage = false;
					}

					// Override any settings from the cache
					updateModFromCacheMeta(&mod);
				}

				zipArchive.close();
			}
		}

		// Update the filtered list.
		if (updateFilter)
		{
			// Only sort once the full list is loaded, otherwise the entries constantly suffle around since the name sorting doesn't
			// match file name sorting very well.
			if (s_useWebModUI)
			{
				s_webReadIndex = readIndex;				
				filterMods(s_viewMode != VIEW_FILE_LIST, /*sort*/s_webReadIndex == s_webReadQueue.size());
			}
			else
			{
				s_readIndex = readIndex;
				filterMods(s_viewMode != VIEW_FILE_LIST, /*sort*/s_readIndex == s_readQueue.size() || s_viewMode == VIEW_FILE_LIST || s_viewMode == VIEW_WEB_LIST);				
			}
		}
	}
				

	void extractPosterFromImage(const char* baseDir, const char* zipFile, const char* imageFileName, UiTexture* poster)
	{
		if (zipFile && zipFile[0])
		{
			char zipPath[TFE_MAX_PATH];
			sprintf(zipPath, "%s%s", baseDir, zipFile);

			ZipArchive zipArchive;
			if (!zipArchive.open(zipPath)) { return; }
			if (zipArchive.openFile(imageFileName))
			{
				size_t imageSize = zipArchive.getFileLength();
				s_imageBuffer.resize(imageSize);
				zipArchive.readFile(s_imageBuffer.data(), imageSize);
				zipArchive.closeFile();

				SDL_Surface* image = TFE_Image::loadFromMemory((u8*)s_imageBuffer.data(), imageSize);
				if (image)
				{
					TextureGpu* gpuImage = TFE_RenderBackend::createTexture(image->w, image->h, (u32*)image->pixels, MAG_FILTER_LINEAR);
					poster->texture = gpuImage;
					poster->width = image->w;
					poster->height = image->h;

					TFE_Image::free(image);
				}
			}
			zipArchive.close();
		}
		else
		{
			char imagePath[TFE_MAX_PATH];
			sprintf(imagePath, "%s%s", baseDir, imageFileName);

			SDL_Surface* image = TFE_Image::get(imagePath);
			if (image)
			{
				TextureGpu* gpuImage = TFE_RenderBackend::createTexture(image->w, image->h, (u32*)image->pixels, MAG_FILTER_LINEAR);
				poster->texture = gpuImage;
				poster->width = image->w;
				poster->height = image->h;
			}
		}
	}

	bool extractPosterFromMod(const char* baseDir, const char* archiveFileName, ModData * mod)
	{		
		// Extract a "poster", if possible, from the GOB file.
		// And then save it as a JPG in /ProgramData/TheForceEngine/ModPosters/NAME.jpg
		char modPath[TFE_MAX_PATH], srcPath[TFE_MAX_PATH], srcPathTex[TFE_MAX_PATH];
		sprintf(modPath, "%s%s", baseDir, archiveFileName);
		sprintf(srcPath, "%s%s", TFE_Paths::getPath(PATH_SOURCE_DATA), "DARK.GOB");
		sprintf(srcPathTex, "%s%s", TFE_Paths::getPath(PATH_SOURCE_DATA), "TEXTURES.GOB");

		GobMemoryArchive gobMemArchive;
		const size_t len = strlen(archiveFileName);
		const char* archiveExt = &archiveFileName[len - 3];
		Archive* archiveMod = nullptr;
		bool archiveIsGob = true;
		bool validGob = false;
		if (strcasecmp(archiveExt, "zip") == 0)
		{
			archiveIsGob = false;

			ZipArchive zipArchive;
			if (zipArchive.open(modPath))
			{
				// Find the gob...
				s32 gobIndex = -1;
				const u32 count = zipArchive.getFileCount();
				for (u32 i = 0; i < count; i++)
				{
					const char* name = zipArchive.getFileName(i);
					const size_t nameLen = strlen(name);
					const char* zext = &name[nameLen - 3];
					if (strcasecmp(zext, "gob") == 0)
					{
						gobIndex = i;
						break;
					}
				}

				if (gobIndex >= 0)
				{
					size_t bufferLen = zipArchive.getFileLength(gobIndex);
					u8* buffer = (u8*)malloc(bufferLen);
					zipArchive.openFile(gobIndex);
					const size_t lengthRead = zipArchive.readFile(buffer, bufferLen);
					zipArchive.closeFile();

					bool archiveRead = false;
					if (lengthRead > 0)
					{
						archiveRead = gobMemArchive.open(buffer, bufferLen);
						if (archiveRead)
						{
							archiveMod = &gobMemArchive;
							validGob = true;
						}
					}
					
					if (!archiveRead)
					{
						TFE_System::logWrite(LOG_ERROR, "ModLoader", "Cannot open zip: '%s'", modPath);
					}
				}

				zipArchive.close();
			}
		}
		else
		{
			archiveMod = Archive::getArchive(ARCHIVE_GOB, archiveFileName, modPath);
			validGob = archiveMod != nullptr;
		}
		Archive* archiveTex = Archive::getArchive(ARCHIVE_GOB, "TEXTURES.GOB", srcPathTex);
		Archive* archiveBase = Archive::getArchive(ARCHIVE_GOB, "DARK.GOB", srcPath);

		s_readBuffer[0].clear();
		s_readBuffer[1].clear();		
		if (archiveMod || archiveBase)
		{						
			// Check if there is a custom loading screen
			if (archiveMod && archiveMod->fileExists("wait.bm") && archiveMod->openFile("wait.bm"))
			{
				s_readBuffer[0].resize(archiveMod->getFileLength());
				archiveMod->readFile(s_readBuffer[0].data(), archiveMod->getFileLength());
				archiveMod->closeFile();
			}

			// Load the cached version
			else if (loadCoverPathFromMod(archiveFileName, &mod->image))
			{
				if (archiveMod && archiveIsGob)
				{
					Archive::freeArchive(archiveMod);
				}
				mod->invertImage = false;
				return true;
			}
				
			// Fall back to the base game loading screen
			else if (archiveTex->openFile("wait.bm"))
			{
				s_readBuffer[0].resize(archiveTex->getFileLength());
				archiveTex->readFile(s_readBuffer[0].data(), archiveTex->getFileLength());
				archiveTex->closeFile();
			}

			if (archiveMod && archiveMod->fileExists("wait.pal") && archiveMod->openFile("wait.pal"))
			{
				s_readBuffer[1].resize(archiveMod->getFileLength());
				archiveMod->readFile(s_readBuffer[1].data(), archiveMod->getFileLength());
				archiveMod->closeFile();
			}
			else if (archiveBase->openFile("wait.pal"))
			{
				s_readBuffer[1].resize(archiveBase->getFileLength());
				archiveBase->readFile(s_readBuffer[1].data(), archiveBase->getFileLength());
				archiveBase->closeFile();
			}
		}

		if (!s_readBuffer[0].empty() && !s_readBuffer[1].empty())
		{
			TextureData* imageData = bitmap_loadFromMemory(s_readBuffer[0].data(), s_readBuffer[0].size(), 1);
			u32 palette[256];
			convertPalette(s_readBuffer[1].data(), palette);
			createTexture(imageData, palette, &mod->image, MAG_FILTER_LINEAR);
		}

		if (archiveMod && archiveIsGob)
		{
			Archive::freeArchive(archiveMod);
		}
		mod->invertImage = validGob;
		return validGob;
	}
}