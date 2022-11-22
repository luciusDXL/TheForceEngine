#include "saveSystem.h"
#include <TFE_Input/inputMapping.h>
#include <TFE_System/system.h>
#include <TFE_Settings/gameSourceData.h>
#include <TFE_FileSystem/fileutil.h>

#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Asset/imageAsset.h>
#include <assert.h>

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
	static char s_gameSavePath[TFE_MAX_PATH];
	static IGame* s_game = nullptr;

	static u32* s_imageBuffer[3] = { nullptr, nullptr, nullptr };
	static size_t s_imageBufferSize[3] = { 0 };

	void saveHeader(Stream* stream, const char* saveName)
	{
		// Generate a screenshot.
		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);
		size_t size = displayInfo.width * displayInfo.height * 4;
		if (size > s_imageBufferSize[0])
		{
			s_imageBuffer[0] = (u32*)realloc(s_imageBuffer[0], size);
			s_imageBufferSize[0] = size;
		}
		TFE_RenderBackend::captureScreenToMemory(s_imageBuffer[0]);

		// Scale and crop the image to fit inside 426 x 240 (widescreen).
		s64 scale = SAVE_IMAGE_HEIGHT * 65536 / displayInfo.height;
		s64 invScale = displayInfo.height * 65536 / SAVE_IMAGE_HEIGHT;
		s32 scaledWidth = (displayInfo.width * scale) >> 16ll;
		s32 newWidth = SAVE_IMAGE_WIDTH, newHeight = SAVE_IMAGE_HEIGHT;

		s32 dstOffset = 0;
		s32 srcOffset = 0;
		if (scaledWidth < newWidth)
		{
			dstOffset = (newWidth - scaledWidth) / 2;
		}
		else if (scaledWidth > newWidth)
		{
			srcOffset = (scaledWidth - newWidth) / 2;
			srcOffset = srcOffset * invScale;
		}

		size_t newSize = newWidth * newHeight * 4;
		if (newSize > s_imageBufferSize[1])
		{
			s_imageBuffer[1] = (u32*)realloc(s_imageBuffer[1], newSize);
			s_imageBufferSize[1] = newSize;
		}

		const u32 *src;
		u32* dst = s_imageBuffer[1];
		memset(dst, 0, newWidth * newHeight * 4);

		s64 u  = srcOffset, v = 0;
		s64 du = invScale, dv = invScale;
		for (s32 y = 0; y < newHeight; y++, v += dv, dst += newWidth)
		{
			u = srcOffset;
			src = &s_imageBuffer[0][(v >> 16ll) * displayInfo.width];
			for (s32 x = dstOffset; x < newWidth - dstOffset; x++, u += du)
			{
				dst[x] = src[u >> 16ll];
			}
		}

		// Save to memory.
		u8* png;
		u32 pngSize = (u32)TFE_Image::writeImageToMemory(png, newWidth, newHeight, s_imageBuffer[1]);

		// Master version.
		u32 version = SVER_CUR;
		stream->write(&version);

		// Save Name.
		u8 len = (u8)strlen(saveName);
		stream->write(&len);
		stream->writeBuffer(saveName, len);

		// Time and Date of Save.
		char timeDate[256];
		TFE_System::getDateTimeString(timeDate);
		len = (u8)strlen(timeDate);
		stream->write(&len);
		stream->writeBuffer(timeDate, len);

		// Image.
		stream->write(&pngSize);
		stream->writeBuffer(png, pngSize);
	}

	void loadHeader(Stream* stream, SaveHeader* header)
	{
		// Master version.
		u32 version;
		stream->read(&version);

		// Save Name.
		u8 len;
		stream->read(&len);
		stream->readBuffer(header->saveName, len);
		header->saveName[len] = 0;

		// Time and Date of Save.
		stream->read(&len);
		stream->readBuffer(header->dateTime, len);
		header->dateTime[len] = 0;

		// Image.
		u32 pngSize;
		stream->read(&pngSize);
		if (pngSize > s_imageBufferSize[2])
		{
			s_imageBuffer[2] = (u32*)realloc(s_imageBuffer[2], pngSize);
			s_imageBufferSize[2] = pngSize;
		}
		stream->readBuffer(s_imageBuffer[2], pngSize);

		Image image = { 0 };
		image.data = header->imageData;
		TFE_Image::readImageFromMemory(&image, pngSize, s_imageBuffer[2]);
		assert(image.width == SAVE_IMAGE_WIDTH && image.height == SAVE_IMAGE_HEIGHT);
	}

	void init()
	{
	}

	void destroy()
	{
		for (s32 i = 0; i < 3; i++)
		{
			free(s_imageBuffer[i]);
			s_imageBufferSize[i] = 0;
		}
	}

	bool saveGame(const char* filename, const char* saveName)
	{
		char filePath[TFE_MAX_PATH];
		sprintf(filePath, "%s%s", s_gameSavePath, filename);

		bool ret = false;
		FileStream stream;
		if (stream.open(filePath, Stream::MODE_WRITE))
		{
			saveHeader(&stream, saveName);
			ret = s_game->serializeGameState(&stream, filename, true);
			stream.close();
		}
		return ret;
	}

	bool loadGame(const char* filename)
	{
		char filePath[TFE_MAX_PATH];
		sprintf(filePath, "%s%s", s_gameSavePath, filename);

		bool ret = false;
		FileStream stream;
		if (stream.open(filePath, Stream::MODE_READ))
		{
			SaveHeader header;
			loadHeader(&stream, &header);
			ret = s_game->serializeGameState(&stream, filename, false);
			stream.close();
		}
		return ret;
	}

	bool loadGameHeader(const char* filename, SaveHeader* header)
	{
		char filePath[TFE_MAX_PATH];
		sprintf(filePath, "%s%s", s_gameSavePath, filename);

		bool ret = false;
		FileStream stream;
		if (stream.open(filePath, Stream::MODE_READ))
		{
			loadHeader(&stream, header);
			stream.close();
			ret = true;
		}
		return ret;
	}
		
	void postLoadRequest(const char* filename)
	{
		s_req = SF_REQ_LOAD;
		strcpy(s_reqFilename, filename);
	}

	void postSaveRequest(const char* filename, const char* saveName)
	{
		s_req = SF_REQ_SAVE;
		strcpy(s_reqFilename, filename);
		strcpy(s_reqSavename, saveName);
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
		if (s_req == SF_REQ_SAVE)
		{
			s_req = SF_REQ_NONE;
			return s_reqFilename;
		}
		return nullptr;
	}
		
	void setCurrentGame(IGame* game)
	{
		s_game = game;
		char relativePath[TFE_MAX_PATH];
		sprintf(relativePath, "Saves/%s/", TFE_Settings::c_gameName[s_game->id]);

		TFE_Paths::appendPath(PATH_USER_DOCUMENTS, relativePath, s_gameSavePath);
		if (!FileUtil::directoryExits(s_gameSavePath))
		{
			FileUtil::makeDirectory(s_gameSavePath);
		}
	}

	void update()
	{
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