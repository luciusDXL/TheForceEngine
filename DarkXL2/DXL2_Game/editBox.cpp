#include "editBox.h"
#include <DXL2_System/system.h>
#include <DXL2_Input/input.h>
#include <string.h>

namespace
{
	void insertChar(char* output, s32* cursor, char input, s32 maxLen)
	{
		if ((*cursor) >= maxLen)
		{
			return;
		}

		size_t len = strlen(output);
		if (*cursor == len)
		{
			output[len] = input;
			output[len + 1] = 0;
		}
		else
		{
			for (s32 i = (s32)len - 1; i > *cursor; i--)
			{
				output[i + 1] = output[i];
			}
			output[*cursor] = input;
			output[len + 1] = 0;
		}
		(*cursor)++;
	}
}

namespace DXL2_GameUi
{
	void updateEditBox(EditBox* editBox)
	{
		const char* input = DXL2_Input::getBufferedText();
		const s32 len = (s32)strlen(input);
		s32 start = editBox->cursor;
		for (s32 c = 0; c < len; c++)
		{
			insertChar(editBox->inputField, &editBox->cursor, input[c], editBox->maxLen);
		}

		s32 nameLen = (s32)strlen(editBox->inputField);
		if (DXL2_Input::bufferedKeyDown(KEY_BACKSPACE))
		{
			if (editBox->cursor > 0)
			{
				for (s32 c = editBox->cursor - 1; c < nameLen - 1; c++)
				{
					editBox->inputField[c] = editBox->inputField[c + 1];
				}
				editBox->inputField[nameLen - 1] = 0;
				editBox->cursor--;
			}
		}
		else if (DXL2_Input::bufferedKeyDown(KEY_DELETE))
		{
			if (editBox->cursor < nameLen)
			{
				for (s32 c = editBox->cursor; c < nameLen - 1; c++)
				{
					editBox->inputField[c] = editBox->inputField[c + 1];
				}
				editBox->inputField[nameLen - 1] = 0;
			}
		}

		if (DXL2_Input::bufferedKeyDown(KEY_LEFT) && editBox->cursor > 0)
		{
			editBox->cursor--;
		}
		else if (DXL2_Input::bufferedKeyDown(KEY_RIGHT) && editBox->cursor < nameLen)
		{
			editBox->cursor++;
		}
		if (DXL2_Input::bufferedKeyDown(KEY_HOME))
		{
			editBox->cursor = 0;
		}
		else if (DXL2_Input::bufferedKeyDown(KEY_END))
		{
			editBox->cursor = nameLen;
		}
	}
}