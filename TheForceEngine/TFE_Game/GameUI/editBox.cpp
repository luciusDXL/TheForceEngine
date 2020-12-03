#include "editBox.h"
#include <TFE_System/system.h>
#include <TFE_Input/input.h>
#include <TFE_Asset/fontAsset.h>
#include <TFE_Renderer/renderer.h>
#include <string.h>

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

namespace TFE_GameUi
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
				
	void drawEditBox(EditBox* editBox, Font* font, s32 offset, s32 x0, s32 y0, s32 x1, s32 y1, s32 scaleX, s32 scaleY, TFE_Renderer* renderer)
	{
		x0 = (((x0)* scaleX) >> 8);
		x1 = (((x1)* scaleX) >> 8);
		y0 = (((y0)* scaleY) >> 8);
		y1 = (((y1)* scaleY) >> 8);
		x0 += offset;
		x1 += offset;

		renderer->drawHorizontalLine(x0, x1, y0, c_editBoxColor);
		renderer->drawHorizontalLine(x0, x1, y1, c_editBoxHighlight);
		renderer->drawVerticalLine(y0 + 1, y1 - 1, x0, c_editBoxColor);
		renderer->drawVerticalLine(y0 + 1, y1 - 1, x1, c_editBoxColor);
		renderer->drawColoredQuad(x0 + 1, y0 + 1, x1 - x0 - 1, y1 - y0 - 1, c_editBoxBackground);
		renderer->print(editBox->inputField, font, x0 + 4, y0 + 4, scaleX, scaleY, c_editBoxText);
		if ((s_editCursorFlicker >> 4) & 1)
		{
			char textToCursor[64];
			strcpy(textToCursor, editBox->inputField);
			textToCursor[editBox->cursor] = 0;
			s32 drawPos = renderer->getStringPixelLength(textToCursor, font);
			renderer->drawHorizontalLine(x0 + 4 + drawPos, x0 + 8 + drawPos, y1 - 2, c_cursorColor);
		}
		s_editCursorFlicker++;
	}
}