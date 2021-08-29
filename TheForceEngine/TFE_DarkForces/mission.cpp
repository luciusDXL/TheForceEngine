#include "mission.h"
#include "actor.h"
#include "agent.h"
#include "animLogic.h"
#include "automap.h"
#include "hud.h"
#include "updateLogic.h"
#include "pickup.h"
#include "player.h"
#include "projectile.h"
#include "weapon.h"
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/InfSystem/infSystem.h>
#include <TFE_RenderBackend/renderBackend.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	#define CONV_6bitTo8bit(x) (((x)<<2) | ((x)>>4))

	struct DrawRect
	{
		s32 x0;
		s32 y0;
		s32 x1;
		s32 y1;
	};
	static DrawRect s_videoDrawRect =
	{
		0, 0,
		319, 199
	};

	enum GameMissionMode
	{
		MISSION_MODE_LOADING    = 0,	// causes the loading screen to be displayed.
		MISSION_MODE_MAIN       = 1,	// the main in-game experience.
		MISSION_MODE_UNKNOWN    = 2,	// unknown - not set (as far as I can tell).
		MISSION_MODE_LOAD_START = 3,	// set right as loading starts.
	};

	/////////////////////////////////////////////
	// Shared State
	/////////////////////////////////////////////
	TextureData* s_loadScreen = nullptr;
	u8 s_loadingScreenPal[768];

	/////////////////////////////////////////////
	// Internal State
	/////////////////////////////////////////////
	static u8 s_framebuffer[320 * 200];
	static JBool s_exitLevel = JFALSE;
	static GameMissionMode s_missionMode = MISSION_MODE_MAIN;
	static Task* s_levelEndTask = nullptr;
	static char s_cheatString[32] = { 0 };
	static s32  s_cheatCharIndex = 0;
	static s32  s_cheatInputCount = 0;

	/////////////////////////////////////////////
	// Forward Declarations
	/////////////////////////////////////////////
	void mission_mainTaskFunc(s32 id);
	void setPalette(u8* pal);
	void textureBlitColumnOpaque(u8* image, u8* outBuffer, s32 yPixelCount);
	void textureBlitColumnTrans(u8* image, u8* outBuffer, s32 yPixelCount);
	void blitTextureToScreen(TextureData* texture, DrawRect* rect, s32 x0, s32 y0, u8* output);
	void blitLoadingScreen();
	void displayLoadingScreen();
	void mission_setupTasks();
	
	/////////////////////////////////////////////
	// API Implementation
	/////////////////////////////////////////////
	void mission_startTaskFunc(s32 id)
	{
		task_begin;
		{
			s_invalidLevelIndex = JFALSE;
			s_levelComplete = JFALSE;
			s_exitLevel = JFALSE;
			pushTask(mission_mainTaskFunc);

			s_missionMode = MISSION_MODE_LOAD_START;
			mission_setupTasks();
			displayLoadingScreen();

			const char* levelName = agent_getLevelName();
			if (level_load(levelName, s_agentData[s_agentId].difficulty + 1))
			{
			}
		}
		// Sleep until we are done with the main task.
		task_yield(TASK_SLEEP);

		// Cleanup - shut down all tasks.
		task_freeAll();

		// End the task.
		task_end;
	}

	void mission_mainTaskFunc(s32 id)
	{
		task_begin;
		while (1)
		{
			// This means it is time to abort, we are done with this level.
			if (s_curTick >= 0 && (s_exitLevel || id < 0))
			{
				break;
			}
			// Handle delta time.
			s_deltaTime = div16(intToFixed16(s_curTick - s_prevTick), FIXED(145));
			s_prevTick = s_curTick;
			s_playerTick = s_curTick;

			// setupCamera();

			switch (s_missionMode)
			{
				case MISSION_MODE_LOADING:
				{
					blitLoadingScreen();
				} break;
				case MISSION_MODE_MAIN:
				{
					// updateScreensize();
					// drawWorld();
				} break;
				// These modes never seem to get called.
				case MISSION_MODE_UNKNOWN:
				{
				} break;
				case MISSION_MODE_LOAD_START:
				{
					// vgaClearPalette();
				} break;
			}

			setPalette(s_loadingScreenPal);
			TFE_RenderBackend::updateVirtualDisplay(s_framebuffer, 320 * 200);

			task_yield(TASK_NO_DELAY);
		}
		task_end;
	}

	/////////////////////////////////////////////
	// Internal Implementation
	/////////////////////////////////////////////
	// Convert the palette to 32 bit color and then send to the render backend.
	// This is functionally similar to loading the palette into VGA registers.
	void setPalette(u8* pal)
	{
		// Update the palette.
		u32 palette[256];
		u32* outColor = palette;
		u8* srcColor = pal;
		for (s32 i = 0; i < 256; i++, outColor++, srcColor += 3)
		{
			*outColor = CONV_6bitTo8bit(srcColor[0]) | (CONV_6bitTo8bit(srcColor[1]) << 8u) | (CONV_6bitTo8bit(srcColor[2]) << 16u) | (0xffu << 24u);
		}
		TFE_RenderBackend::setPalette(palette);
	}

	void textureBlitColumnOpaque(u8* image, u8* outBuffer, s32 yPixelCount)
	{
		s32 end = yPixelCount - 1;
		s32 offset = 0;
		for (s32 i = end; i >= 0; i--, offset += 320)
		{
			outBuffer[offset] = image[i];
		}
	}

	void textureBlitColumnTrans(u8* image, u8* outBuffer, s32 yPixelCount)
	{
		s32 end = yPixelCount - 1;
		s32 offset = 0;
		for (s32 i = end; i >= 0; i--, offset += 320)
		{
			if (image[i]) { outBuffer[offset] = image[i]; }
		}
	}

	void blitTextureToScreen(TextureData* texture, DrawRect* rect, s32 x0, s32 y0, u8* output)
	{
		s32 x1 = x0 + texture->width - 1;
		s32 y1 = y0 + texture->height - 1;

		// Cull if outside of the draw rect.
		if (x1 < rect->x0 || y1 < rect->y0 || x0 > rect->x1 || y0 > rect->y1) { return; }

		s32 srcX = 0, srcY = 0;
		if (y0 < rect->y0)
		{
			y0 = rect->y0;
		}
		if (y1 > rect->y1)
		{
			srcY = y1 - rect->y1;
			y1 = rect->y1;
		}

		if (x0 < rect->x0)
		{
			x0 = rect->x0;
		}
		if (x1 > rect->x1)
		{
			srcX = x1 - rect->x1;
			x1 = rect->x1;
		}

		s32 yPixelCount = y1 - y0 + 1;
		if (yPixelCount <= 0) { return; }

		u8* buffer = texture->image + texture->height*srcX + srcY;
		if (texture->flags & OPACITY_TRANS)
		{
			for (s32 col = x0; col <= x1; col++, buffer += texture->height)
			{
				textureBlitColumnTrans(buffer, output + y0*320 + col, yPixelCount);
			}
		}
		else
		{
			for (s32 col = x0; col <= x1; col++, buffer += texture->height)
			{
				textureBlitColumnOpaque(buffer, output + y0*320 + col, yPixelCount);
			}
		}
	}

	void blitLoadingScreen()
	{
		if (!s_loadScreen) { return; }
		blitTextureToScreen(s_loadScreen, &s_videoDrawRect, 0/*x0*/, 0/*y0*/, s_framebuffer);
	}

	void displayLoadingScreen()
	{
		blitLoadingScreen();
		setPalette(s_loadingScreenPal);
		TFE_RenderBackend::updateVirtualDisplay(s_framebuffer, 320 * 200);
	}

	void mission_setupTasks()
	{
		setSpriteAnimation(nullptr, nullptr);
		bitmap_setupAnimationTask();
		// resetFrameData();		// TODO
		hud_startup();
		hud_clearMessage();
		automap_computeScreenBounds();
		weapon_clearFireRate();
		weapon_createPlayerWeaponTask();
		projectile_createTask();
		player_createController();
		inf_createElevatorTask();
		player_clearEyeObject();
		pickup_createTask();
		inf_createTeleportTask();
		inf_createTriggerTask();
		actor_createTask();
		hitEffect_createTask();
		// createIMuseTask();  <- this will wait until a later release.
		level_clearData();
		updateLogic_clearTask();
		s_drawAutomap = JFALSE;
		s_levelEndTask = nullptr;
		s_cheatString[0] = 0;
		s_cheatCharIndex = 0;
		s_cheatInputCount = 0;

		for (s32 i = 9; i >= 0; i--)
		{
			s_goals[i] = JFALSE;
		}
	}

}  // TFE_DarkForces