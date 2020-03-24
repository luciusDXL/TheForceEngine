#include "editBox.h"
#include <DXL2_System/system.h>
#include <DXL2_Input/input.h>
#include <DXL2_Asset/fontAsset.h>
#include <DXL2_Renderer/renderer.h>
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
	static u32 s_editCursorFlicker = 0;

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
		
	void drawEditBox(EditBox* editBox, Font* font, s32 x0, s32 y0, s32 x1, s32 y1, s32 scaleX, s32 scaleY, DXL2_Renderer* renderer)
	{
		renderer->drawHorizontalLine(x0, x1, y0, 47);
		renderer->drawHorizontalLine(x0, x1, y1, 32);
		renderer->drawVerticalLine(y0 + 1, y1 - 1, x0, 47);
		renderer->drawVerticalLine(y0 + 1, y1 - 1, x1, 47);
		renderer->drawColoredQuad(x0 + 1, y0 + 1, x1 - x0 - 1, y1 - y0 - 1, 0);
		renderer->print(editBox->inputField, font, x0 + 4, y0 + 4, scaleX, scaleY, 32);
		if ((s_editCursorFlicker >> 4) & 1)
		{
			char textToCursor[64];
			strcpy(textToCursor, editBox->inputField);
			textToCursor[editBox->cursor] = 0;
			s32 drawPos = renderer->getStringPixelLength(textToCursor, font);
			renderer->drawHorizontalLine(x0 + 4 + drawPos, x0 + 8 + drawPos, y1 - 2, 36);
		}
		s_editCursorFlicker++;
	}
}