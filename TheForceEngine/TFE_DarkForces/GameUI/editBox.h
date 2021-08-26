#pragma once
//////////////////////////////////////////////////////////////////////
// A simple edit box.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_Jedi/Level/rfont.h>

struct EditBox
{
	char* inputField = nullptr;
	s32   cursor = 0;
	s32   maxLen = 0;
};

namespace TFE_DarkForces
{
	void updateEditBox(EditBox* editBox);
	void drawEditBox(EditBox* editBox, Font* font, s32 offset, s32 x0, s32 y0, s32 x1, s32 y1);
}
