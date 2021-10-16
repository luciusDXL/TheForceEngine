#include "escapeMenu.h"
#include "delt.h"
#include "uiDraw.h"
#include <TFE_DarkForces/agent.h>
#include <TFE_DarkForces/util.h>
#include <TFE_Archive/archive.h>
#include <TFE_Settings/settings.h>
#include <TFE_Input/input.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_System/system.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	static JBool s_escMenuOpen = JFALSE;

	void escapeMenu_open()
	{
		s_escMenuOpen = JTRUE;
	}

	JBool escapeMenu_isOpen()
	{
		return s_escMenuOpen;
	}

	EscapeMenuAction escapeMenu_update()
	{
		return ESC_CONTINUE;
	}
}