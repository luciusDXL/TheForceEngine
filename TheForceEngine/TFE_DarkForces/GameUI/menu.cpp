#include <cstring>

#include "menu.h"
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
#include <TFE_Jedi/Renderer/virtualFramebuffer.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	///////////////////////////////////////////
	// Internal State
	///////////////////////////////////////////
	static JBool s_loaded = JFALSE;

	Vec2i s_cursorPosAccum;
	Vec2i s_cursorPos;
	s32 s_buttonPressed = -1;
	JBool s_buttonHover = JFALSE;
		
	///////////////////////////////////////////
	// API Implementation
	///////////////////////////////////////////
	void menu_init()
	{
		if (s_loaded) { return; }

		FilePath filePath;
		if (!TFE_Paths::getFilePath("AGENTMNU.LFD", &filePath)) { return; }
		Archive* archive = Archive::getArchive(ARCHIVE_LFD, "AGENTMNU", filePath.path);
		TFE_Paths::addLocalArchive(archive);
		getFrameFromDelt("cursor.delt", &s_cursor);
		TFE_Paths::removeLastArchive();

		s_loaded = JTRUE;
	}

	void menu_destroy()
	{
		menu_resetState();
	}

	void menu_resetState()
	{
		s_loaded = JFALSE;
		delt_resetState();
	}
	
	void menu_handleMousePosition()
	{
		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		u32 width, height;
		vfb_getResolution(&width, &height);

		s32 dx, dy;
		TFE_Input::getAccumulatedMouseMove(&dx, &dy);

		s_cursorPosAccum.x = clamp(s_cursorPosAccum.x + dx, 0, displayInfo.width);
		s_cursorPosAccum.z = clamp(s_cursorPosAccum.z + dy, 0, displayInfo.height);
		if (displayInfo.width >= displayInfo.height)
		{
			s_cursorPos.x = clamp(s_cursorPosAccum.x * (s32)height / (s32)displayInfo.height, 0, (s32)width - 3);
			s_cursorPos.z = clamp(s_cursorPosAccum.z * (s32)height / (s32)displayInfo.height, 0, (s32)height - 3);
		}
		else
		{
			s_cursorPos.x = clamp(s_cursorPosAccum.x * (s32)width / (s32)displayInfo.width, 0, (s32)width - 3);
			s_cursorPos.z = clamp(s_cursorPosAccum.z * (s32)width / (s32)displayInfo.width, 0, (s32)height - 3);
		}
	}

	void menu_resetCursor()
	{
		// Reset the cursor.
		u32 width, height;
		vfb_getResolution(&width, &height);

		DisplayInfo displayInfo;
		TFE_RenderBackend::getDisplayInfo(&displayInfo);

		s_cursorPosAccum = { (s32)displayInfo.width >> 1, (s32)displayInfo.height >> 1 };
		s_cursorPos.x = clamp(s_cursorPosAccum.x * (s32)height / (s32)displayInfo.height, 0, (s32)width - 3);
		s_cursorPos.z = clamp(s_cursorPosAccum.z * (s32)height / (s32)displayInfo.height, 0, (s32)height - 3);
	}

	u8* menu_startupDisplay()
	{
		vfb_setResolution(320, 200);
		menu_resetCursor();
		return vfb_getCpuBuffer();
	}

	void menu_blitCursor(s32 x, s32 y, u8* framebuffer)
	{
		blitDeltaFrame(&s_cursor, x, y, framebuffer);
	}
}