#pragma once
//////////////////////////////////////////////////////////////////////
// DarkXL 2 Game Level
// Manages the game while playing levels in first person.
// This holds one or more players, handles input updates, etc.
//////////////////////////////////////////////////////////////////////

#include <DXL2_System/types.h>

struct EditBox
{
	char* inputField = nullptr;
	s32   cursor = 0;
	s32   maxLen = 0;
};

namespace DXL2_GameUi
{
	void updateEditBox(EditBox* editBox);
}
