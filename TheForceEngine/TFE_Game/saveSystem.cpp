#include "saveSystem.h"
#include <TFE_Input/inputMapping.h>
#include <TFE_System/system.h>
#include <TFE_Settings/gameSourceData.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Asset/imageAsset.h>
#include <cassert>
#include <cstring>

using namespace TFE_Input;

namespace TFE_SaveSystem
{
	enum SaveRequest
	{
		SF_REQ_NONE = 0,
		SF_REQ_SAVE,
		SF_REQ_LOAD,
	};

	enum SaveMasterVersion
	{
		SVER_INIT = 1,
		SVER_CUR = SVER_INIT
	};

	static SaveRequest s_req = SF_REQ_NONE;
	static char s_reqFilename[TFE_MAX_PATH];
	static char s_reqSavename[TFE_MAX_PATH];
	static char s_gameSavePath[32];
	static IGame* s_game = nullptr;
	static s32 s_saveDelay = 0;

	static char* s_imageBuffer[2] = { nullptr, nullptr };
	static size_t s_imageBufferSize[2] = { 0 };

#define WR(x, y) if (!f.write(x)) return (y);
#define WRB(x, z, y) if (!f.write(x, z)) return (y);
	static int saveHeader(vpFile& f, const char* saveName)
	{
		u32 pngSize, version;
		char buf[256];
		u8 len;

		// Generate a screenshot.
		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);
		size_t size = displayInfo.width * displayInfo.height * 4;
		if (size > s_imageBufferSize[0])
		{
			s_imageBuffer[0] = (char *)realloc(s_imageBuffer[0], size);
			s_imageBufferSize[0] = size;
		}
		TFE_RenderBackend::captureScreenToMemory(s_imageBuffer[0]);
		// Save to memory.
		char* png = (char *)malloc(SAVE_IMAGE_WIDTH * SAVE_IMAGE_HEIGHT * 4);
		if (png)
		{
			pngSize = TFE_Image::writeImageToMemory(png, displayInfo.width, displayInfo.height,
								 SAVE_IMAGE_WIDTH, SAVE_IMAGE_HEIGHT,
								 s_imageBuffer[0]);
		}
		else
		{
			pngSize = 0;
			png = s_imageBuffer[0];
		}

		// Master version.
		version = SVER_CUR;
		WR(version, 1);

		// Save Name.
		size_t saveNameLen = strlen(saveName);
		if (saveNameLen > SAVE_MAX_NAME_LEN - 1) { saveNameLen = SAVE_MAX_NAME_LEN - 1; }
		len = (u8)saveNameLen;
		WR(len, 2);
		WRB(saveName, len, 3);

		// Time and Date of Save.
		memset(buf, 0, 256);
		TFE_System::getDateTimeString(buf);
		len = (u8)strlen(buf);
		WR(len, 4);
		WRB(buf, len, 5);

		// Level Name
		memset(buf, 0, 256);
		s_game->getLevelName(buf);
		len = (u8)strlen(buf);
		WR(len, 6);
		WRB(buf, len, 7);

		// Mod List
		memset(buf, 0, 256);
		s_game->getModList(buf);
		len = (u8)strlen(buf);
		WR(len, 8);
		WRB(buf, len, 9);

		// Image.
		WR(pngSize, 10);
		WRB((const void *)png, pngSize, 11);
		free(png);

		return 0; // success
	}
#undef WRB
#undef WR

	static int loadHeader(vpFile& f, SaveHeader* header, const char* fileName)
	{
		// Master version.
		const u32 sz = SAVE_IMAGE_WIDTH * SAVE_IMAGE_HEIGHT * sizeof(u32);
		u32 version, pngSize;
		u8 len;

		f.read(&version);
		if (version > SVER_CUR)
			return 1;

		// Save Name.
		f.read(&len);
		if (len) {
			f.read(header->saveName, len);
			header->saveName[len] = 0;
			// Fix existing invalid save names.
			header->saveName[SAVE_MAX_NAME_LEN - 1] = 0;
		} else {
			strcpy(header->saveName, "(unknown)");
		}

		// Handle the case when there is no save name.
		if (!len || header->saveName[0] == 0 || header->saveName[0] == ' ')
		{
			strncpy(header->saveName, fileName, SAVE_MAX_NAME_LEN);
		}

		// Time and Date of Save.
		f.read(&len, 1);
		if (len) {
			f.read(header->dateTime, len);
			header->dateTime[len] = 0;
		} else {
			strcpy(header->dateTime, "(unknown)");
		}

		// Level Name
		f.read(&len);
		if (len) {
			f.read(header->levelName, len);
			header->levelName[len] = 0;
		} else {
			strcpy(header->levelName, "(unknown)");
		}

		// Mod List
		f.read(&len);
		if (len) {
			f.read(header->modNames, len);
			header->modNames[len] = 0;
		} else {
			memset(header->modNames, 0, sizeof(header->modNames));
		}

		// Image, re-use buffer 0 for the PNG.
		f.read(&pngSize);

		// cap it at 2MB max in case pngSize got corrupted.
		if (pngSize > 10 && pngSize < 2048 * 1024)
		{
			if (pngSize > s_imageBufferSize[0])
			{
				s_imageBuffer[0] = (char *)realloc(s_imageBuffer[0], pngSize);
				s_imageBufferSize[0] = pngSize;
			}
			f.read(s_imageBuffer[0], pngSize);

			SDL_Surface* image;
			TFE_Image::readImageFromMemory(&image, pngSize, (void *)s_imageBuffer[0]);
			if (image)
			{
				memcpy(header->imageData, image->pixels, sz);
				TFE_Image::free(image);
			} else {
				memset(header->imageData, 0, sz);
				TFE_System::logWrite(LOG_MSG, "saveSystem", "loadHeader PNG error");
			}
		}
		return 0;
	}

	void populateSaveDirectory(std::vector<SaveHeader>& dir)
	{
		TFEExtList te = { "tfe" };
		TFEFileList tl;

		bool ok = vpGetFileList(VPATH_TFE, (const char *)s_gameSavePath, tl, te);
		if (!ok)
			return;

		dir.clear();
		size_t saveCount = tl.size();
		dir.resize(saveCount);

		const std::string* filenames = tl.data();
		SaveHeader* headers = dir.data();
		for (size_t i = 0, j = 0; i < saveCount; i++)
		{
			int ok = loadGameHeader(filenames[i].c_str(), &headers[j]);
			if (ok == 0)
			{
				++j;
			}
			else
			{
				TFE_System::logWrite(LOG_WARNING, "SaveSystem", "Save %s seems corrupt", filenames[i].c_str());
			}
		}
	}

	void init()
	{
	}

	void destroy()
	{
		for (s32 i = 0; i < 2; i++)
		{
			free(s_imageBuffer[i]);
			s_imageBufferSize[i] = 0;
			s_imageBuffer[i] = nullptr;
		}
	}

	bool saveGame(const char* filename, const char* saveName)
	{
		char filePath[32];
		bool ret = false;
		vpFile savefile;

		sprintf(filePath,"%s%s", s_gameSavePath, filename);
		if (savefile.openwrite(filePath))
		{
			ret = saveHeader(savefile, saveName);
			if (ret) {
				TFE_System::logWrite(LOG_ERROR, "SaveSystem", "saveHeader() failed: %d", ret);
				savefile.close();
				vpDeleteFile(filePath);
				return false;
			}
			ret = s_game->serializeGameState(&savefile, filename, true);
			savefile.close();
			if (!ret) {
				TFE_System::logWrite(LOG_ERROR, "SaveSystem", "serialization failed");
				vpDeleteFile(filePath);
			}
		}
		return ret;
	}

	bool loadGame(const char* filename)
	{
		char filePath[32];
		int ret = -1;

		sprintf(filePath, "%s%s", s_gameSavePath, filename);
		vpFile loadfile(VPATH_TFE, filePath);
		if (loadfile)
		{
			SaveHeader header;
			ret = loadHeader(loadfile, &header, filename);
			if (ret == 0)
				ret = s_game->serializeGameState(&loadfile, filename, false) ? 0 : 1;
			loadfile.close();
		}
		return (ret == 0);
	}

	int loadGameHeader(const char* filename, SaveHeader* header)
	{
		char filePath[32];
		int ret = -1;

		sprintf(filePath, "%s%s", s_gameSavePath, filename);
		vpFile savefile(VPATH_TFE, filePath);
		if (savefile)
		{
			ret = loadHeader(savefile, header, filename);
			if (ret == 0) {
				strcpy(header->fileName, filename);
			}
			savefile.close();
		}
		return ret;
	}
		
	void postLoadRequest(const char* filename)
	{
		s_req = SF_REQ_LOAD;
		strcpy(s_reqFilename, filename);
	}

	void postSaveRequest(const char* filename, const char* saveName, s32 delay)
	{
		s_req = SF_REQ_SAVE;
		strcpy(s_reqFilename, filename);
		strcpy(s_reqSavename, saveName);
		s_saveDelay = delay;
	}

	const char* loadRequestFilename()
	{
		if (s_req == SF_REQ_LOAD)
		{
			s_req = SF_REQ_NONE;
			return s_reqFilename;
		}
		return nullptr;
	}

	const char* saveRequestFilename()
	{
		if (s_req == SF_REQ_SAVE && s_saveDelay <= 0)
		{
			s_req = SF_REQ_NONE;
			return s_reqFilename;
		}
		if (s_saveDelay > 0) { s_saveDelay--; }
		return nullptr;
	}

	void getSaveFilenameFromIndex(s32 index, char* name)
	{
		if (index == 0)
		{
			strcpy(name, c_quickSaveName);
		}
		else
		{
			sprintf(name, "save%03d.tfe", index - 1);
		}
	}

	void setCurrentGame(GameID id)
	{
		sprintf(s_gameSavePath, "Saves/%s/", TFE_Settings::c_gameName[id]);
		vpMkdir(s_gameSavePath);
	}
		
	void setCurrentGame(IGame* game)
	{
		s_game = game;
		setCurrentGame(game->id);
	}

	void update()
	{
		if (!s_game) { return; }

		static s32 lastState = 0;
		const char* saveFilename = saveRequestFilename();

		bool canSave = !lastState && s_game->canSave();
		if (saveFilename && canSave)
		{
			saveGame(saveFilename, s_reqSavename);
			lastState = 1;
		}
		else if (inputMapping_getActionState(IAS_QUICK_SAVE) == STATE_PRESSED && canSave)
		{
			saveGame(c_quickSaveName, "Quicksave");
			lastState = 1;
		}
		else if (inputMapping_getActionState(IAS_QUICK_LOAD) == STATE_PRESSED && !lastState)
		{
			postLoadRequest(c_quickSaveName);
			lastState = 1;
		}
		else
		{
			lastState = 0;
		}
	}
}