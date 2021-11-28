#include <cstring>

#include "editBox.h"
#include "uiDraw.h"
#include <TFE_System/system.h>
#include <TFE_Input/input.h>

namespace
{
	void insertChar(char* output, s32* cursor, char input, s32 maxLen)
	{
		if ((*cursor) >= maxLen)
		{
			return;
		}

		const size_t len = strlen(output);
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

namespace TFE_DarkForces
{
	const u8 c_editBoxBackground =  0u;
	const u8 c_editBoxColor      = 47u;
	const u8 c_editBoxHighlight  = 32u;
	const u8 c_editBoxText       = 32u;
	const u8 c_cursorColor       = 36u;

	static u32 s_editCursorFlicker = 0;

	void updateEditBox(EditBox* editBox)
	{
		const char* input = TFE_Input::getBufferedText();
		const s32 len = (s32)strlen(input);
		for (s32 c = 0; c < len; c++)
		{
			insertChar(editBox->inputField, &editBox->cursor, input[c], editBox->maxLen);
		}

		const s32 nameLen = (s32)strlen(editBox->inputField);
		if (TFE_Input::bufferedKeyDown(KEY_BACKSPACE))
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
		else if (TFE_Input::bufferedKeyDown(KEY_DELETE))
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

		if (TFE_Input::bufferedKeyDown(KEY_LEFT) && editBox->cursor > 0)
		{
			editBox->cursor--;
		}
		else if (TFE_Input::bufferedKeyDown(KEY_RIGHT) && editBox->cursor < nameLen)
		{
			editBox->cursor++;
		}
		if (TFE_Input::bufferedKeyDown(KEY_HOME))
		{
			editBox->cursor = 0;
		}
		else if (TFE_Input::bufferedKeyDown(KEY_END))
		{
			editBox->cursor = nameLen;
		}
	}
				
	void drawEditBox(EditBox* editBox, s32 x0, s32 y0, s32 x1, s32 y1, u8* framebuffer)
	{
		drawHorizontalLine(x0, x1, y0, c_editBoxColor, framebuffer);
		drawHorizontalLine(x0, x1, y1, c_editBoxHighlight, framebuffer);
		drawVerticalLine(y0 + 1, y1 - 1, x0, c_editBoxColor, framebuffer);
		drawVerticalLine(y0 + 1, y1 - 1, x1, c_editBoxColor, framebuffer);
		drawColoredQuad(x0 + 1, y0 + 1, x1 - x0 - 1, y1 - y0 - 1, c_editBoxBackground, framebuffer);
		print(editBox->inputField, x0 + 4, y0 + 4, c_editBoxText, framebuffer);
		if ((s_editCursorFlicker >> 4) & 1)
		{
			char textToCursor[64];
			strcpy(textToCursor, editBox->inputField);
			textToCursor[editBox->cursor] = 0;
			s32 drawPos = getStringPixelLength(textToCursor);
			drawHorizontalLine(x0 + 4 + drawPos, x0 + 8 + drawPos, y1 - 2, c_cursorColor, framebuffer);
		}
		s_editCursorFlicker++;
	}
}