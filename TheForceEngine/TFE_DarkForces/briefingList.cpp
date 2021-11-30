#include "briefingList.h"
#include "util.h"
#include <TFE_Game/igame.h>
#include <TFE_System/system.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_System/parser.h>

namespace TFE_DarkForces
{
	static char* s_buffer = nullptr;

	void briefingList_freeBuffer()
	{
		s_buffer = nullptr;
	}

	// FileStream and TFE_Parser are used to read the file and parse out lines.
	// However all other processing matches the original DOS version.
	s32 parseBriefingList(BriefingList* briefingList, const char* filename)
	{
		FilePath path;
		if (!TFE_Paths::getFilePath(filename, &path)) { return 0; }

		FileStream file;
		if (!file.open(&path, FileStream::MODE_READ)) { return 0; }

		size_t size = file.getSize();
		s_buffer = (char*)game_realloc(s_buffer, size);
		if (!s_buffer) { return 0; }

		file.readBuffer(s_buffer, (u32)size);
		file.close();

		TFE_Parser parser;
		parser.addCommentString("#");
		parser.init(s_buffer, size);

		size_t bufferPos = 0;
		const char* fileData = parser.readLine(bufferPos);

		s32 v0 = 0, v1 = 0;
		s32 count = sscanf(fileData, "BRF %d.%d", &v0, &v1);
		if (count != 2 || v0 != 1 || v1 != 0)
		{
			TFE_System::logWrite(LOG_ERROR, "Briefing List", "Invalid version: %d.%d. Only version 1.0 is supported.", v0, v1);
			return 0;
		}

		fileData = parser.readLine(bufferPos);
		s32 msgCount;
		count = sscanf(fileData, "BRIEFS %d", &msgCount);
		if (count != 1)
		{
			TFE_System::logWrite(LOG_ERROR, "Briefing List", "Cannot parse briefing count.");
			return 0;
		}

		briefingList->count = msgCount;
		briefingList->briefing = (BriefingInfo*)game_alloc(msgCount * sizeof(BriefingInfo));
		BriefingInfo* info = briefingList->briefing;
		for (s32 i = 0; i < msgCount; i++, info++)
		{
			fileData = parser.readLine(bufferPos);
			if (!fileData)
			{
				TFE_System::logWrite(LOG_ERROR, "Game Message", "The message count is incorrect!");
				return 0;
			}

			char mission[32];
			char archive[32];
			char bgAnim[32];
			char palette[32];
			if (sscanf(fileData, "LEV: %s LFD: %s ANI: %s PAL: %s", mission, archive, bgAnim, palette) != 4)
			{
				TFE_System::logWrite(LOG_ERROR, "Briefing List", "Error reading briefing file %s on line: >>> %s", filename, fileData);
				return 0;
			}
			else
			{
				_strlwr(mission);
				_strlwr(archive);
				_strlwr(bgAnim);
				_strlwr(palette);

				strcpy(info->mission, mission);
				strcpy(info->archive, archive);
				strcpy(info->bgAnim,  bgAnim);
				strcpy(info->palette, palette);
			}
		}  // for (s32 i = 0; i < msgCount; i++, info++)
		return 1;
	}
}  // TFE_DarkForces