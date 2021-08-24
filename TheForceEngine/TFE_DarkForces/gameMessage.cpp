#include "gameMessage.h"
#include <TFE_System/system.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_System/parser.h>

namespace TFE_DarkForces
{
	static char* s_buffer = nullptr;

	void gameMessage_freeBuffer()
	{
		free(s_buffer);
		s_buffer = nullptr;
	}

	char* copyAndAllocateString(const char* start, const char* end)
	{
		s32 count = s32(end - start);
		char* outString = (char*)malloc(count + 1);
		memcpy(outString, start, count);
		outString[count] = 0;

		return outString;
	}

	// FileStream and TFE_Parser are used to read the file and parse out lines.
	// However all other processing matches the original DOS version.
	s32 parseMessageFile(GameMessages* messages, const FilePath* path, s32 mode)
	{
		u32 size = FileStream::readContents(path, (void**)&s_buffer);
		if (!size || !s_buffer)
		{
			return 0;
		}

		TFE_Parser parser;
		parser.addCommentString("#");
		parser.init(s_buffer, size);

		size_t bufferPos = 0;
		const char* fileData = parser.readLine(bufferPos);

		s32 v0 = 0, v1 = 0;
		s32 count = sscanf(fileData, "MSG %d.%d", &v0, &v1);
		if (count != 2 || v0 != 1 || v1 != 0)
		{
			TFE_System::logWrite(LOG_ERROR, "Game Message", "Invalid version: %d.%d. Only version 1.0 is supported.", v0, v1);
			return 0;
		}

		fileData = parser.readLine(bufferPos);
		s32 msgCount;
		count = sscanf(fileData, "MSGS %d", &msgCount);
		if (count != 1)
		{
			TFE_System::logWrite(LOG_ERROR, "Game Message", "Cannot parse message count");
			return 0;
		}

		messages->count = msgCount;
		messages->msgList = (GameMessage*)malloc(msgCount * sizeof(GameMessage));
		GameMessage* msg = messages->msgList;
		for (s32 i = 0; i < msgCount; i++, msg++)
		{
			fileData = parser.readLine(bufferPos);
			if (!fileData)
			{
				TFE_System::logWrite(LOG_ERROR, "Game Message", "The message count is incorrect!");
				return 0;
			}
			if (sscanf(fileData, "%d %d", &msg->id, &msg->priority) != 2)
			{
				TFE_System::logWrite(LOG_ERROR, "Game Message", "Cannot parse message ID and Priority.");
				return 0;
			}

			const char* line = fileData;
			char c = *line;
			while (c)
			{
				if (c == '"')
				{
					break;
				}
				else if (!c)
				{
					line = nullptr;
					break;
				}
				line++;
				c = *line;
			}

			if (!line)
			{
				TFE_System::logWrite(LOG_ERROR, "Game Message", "Failed to find the start of the message body.");
				return 0;
			}
			line++;
			const char* strStart = line;

			c = *line;
			while (c)
			{
				if (c == '"')
				{
					break;
				}
				else if (!c)
				{
					line = nullptr;
					break;
				}
				line++;
				c = *line;
			}
			if (!line)
			{
				TFE_System::logWrite(LOG_ERROR, "Game Message", "Failed to find the end of the message body.");
				return 0;
			}
			msg->text = copyAndAllocateString(strStart, line);
		}  // for (s32 i = 0; i < msgCount; i++, msg++)
		return 1;
	}
}  // TFE_DarkForces