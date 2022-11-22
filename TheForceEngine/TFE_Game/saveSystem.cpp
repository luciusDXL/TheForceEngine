#include "saveSystem.h"
#include <TFE_Input/inputMapping.h>
#include <TFE_System/system.h>

#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Asset/imageAsset.h>

using namespace TFE_Input;

namespace TFE_SaveSystem
{
	enum SaveRequest
	{
		SF_REQ_NONE = 0,
		SF_REQ_SAVE,
		SF_REQ_LOAD,
	};
	static SaveRequest s_req = SF_REQ_NONE;
	static char s_reqFilename[TFE_MAX_PATH];

	bool saveGame(IGame* game, const char* filename)
	{
		bool ret = game->serializeGameState(filename, true);

		return ret;
	}

	bool loadGame(IGame* game, const char* filename)
	{
		bool ret = game->serializeGameState(filename, false);

		return ret;
	}
		
	void postLoadRequest(const char* filename)
	{
		s_req = SF_REQ_LOAD;
		strcpy(s_reqFilename, filename);
	}

	void postSaveRequest(const char* filename)
	{
		s_req = SF_REQ_SAVE;
		strcpy(s_reqFilename, filename);
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

	void update(IGame* game)
	{
		static s32 lastState = 0;
		const char* saveFilename = saveRequestFilename();

		bool canSave = !lastState && game->canSave();
		if (saveFilename && canSave)
		{
			saveGame(game, saveFilename);
			lastState = 1;
		}
		else if (inputMapping_getActionState(IAS_QUICK_SAVE) == STATE_PRESSED && canSave)
		{
			saveGame(game, c_quickSaveName);
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

	void handleSaveScreenshot()
	{
#if 0
		// Test...
		// Hacktastic.
		static u32 mem[1920 * 1080];
		s32 w = 1920, h = 1080;
		TFE_RenderBackend::captureScreenToMemory(mem);

		// Test.
		// Downsample.
		s32 scale = h / 256;
		s32 newWidth = w / scale, newHeight = h / scale;

		static u32 output[1920 * 1080];
		const u32 *src = mem;
		u32* dst = output;
		for (s32 y = 0, rY = 0; y < newHeight; y++, rY += scale)
		{
			for (s32 x = 0, rX = 0; x < newWidth; x++, rX += scale)
			{
				dst[x] = src[rX];
			}
			dst += newWidth;
			src += w * scale;
		}

		TFE_Image::writeImage("TestCapture.png", newWidth, newHeight, output);
#endif
	}
}