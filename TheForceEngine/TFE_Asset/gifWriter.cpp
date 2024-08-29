#include "gifWriter.h"
#include <TFE_System/system.h>
#include <TFE_FileSystem/physfswrapper.h>
#include <assert.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

#define MSF_GIF_IMPL
#include "msf_gif.h"

namespace TFE_GIF
{
	static MsfGifState s_gifState;
	static s32 s_centisecondsPerFrame;
	static s32 s_width;
	static s32 s_height;
	static char s_path[TFE_MAX_PATH];

	static std::vector<u8> s_tempBuffer;

	bool startGif(const char* path, u32 width, u32 height, u32 fps)
	{
		memset(&s_gifState, 0, sizeof(MsfGifState));
		msf_gif_begin(&s_gifState, width, height);

		s_width = width;
		s_height = height;

		s_centisecondsPerFrame = s32(100.0f/f32(fps) + 0.5f);
		strcpy(s_path, path);
		
		s_tempBuffer.resize(width * height * 4);
		
		return true;
	}

	void addFrame(const u8* imageData)
	{
		// We have to flip the frame vertically.
		u8* output = s_tempBuffer.data();
		for (s32 y = 0; y < s_height; y++, output += s_width*4)
		{
			memcpy(output, &imageData[(s_height - y - 1) * s_width * 4], s_width * 4);
		}

		msf_gif_frame(&s_gifState, s_tempBuffer.data(), s_centisecondsPerFrame, 16, s_width * 4);
	}

	bool write()
	{
		MsfGifResult result = msf_gif_end(&s_gifState);
		
		vpFile file;
		file.openwrite(s_path);
		if (!file) {
			msf_gif_free(result);
			return false;
		}
		file.write(result.data, result.dataSize);
		file.close();

		msf_gif_free(result);
		return true;
	}
}