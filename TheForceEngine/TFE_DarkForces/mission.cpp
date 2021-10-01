#include "mission.h"
#include "agent.h"
#include "animLogic.h"
#include "automap.h"
#include "cheats.h"
#include "config.h"
#include "hud.h"
#include "updateLogic.h"
#include "pickup.h"
#include "player.h"
#include "projectile.h"
#include "weapon.h"
#include <TFE_DarkForces/Actor/actor.h>
#include <TFE_Game/igame.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_Jedi/Level/level.h>
#include <TFE_Jedi/InfSystem/infSystem.h>
#include <TFE_Jedi/Renderer/rlimits.h>
#include <TFE_Jedi/Renderer/jediRenderer.h>
#include <TFE_Jedi/Renderer/rcommon.h>
#include <TFE_Jedi/Renderer/RClassic_Fixed/screenDraw.h>
#include <TFE_Jedi/Renderer/RClassic_Fixed/rclassicFixed.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/system.h>
#include <TFE_Input/input.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	#define CONV_6bitTo8bit(x) (((x)<<2) | ((x)>>4))
	// Show the loading screen for at least 1 second.
	#define MIN_LOAD_TIME 145

	static DrawRect s_videoDrawRect =
	{
		0, 0,
		319, 199
	};
	
	/////////////////////////////////////////////
	// Shared State
	/////////////////////////////////////////////
	JBool s_gamePaused = JTRUE;
	JBool s_canTeleport = JTRUE;
	GameMissionMode s_missionMode = MISSION_MODE_MAIN;

	TextureData* s_loadScreen = nullptr;
	u8 s_loadingScreenPal[768];
	u8 s_levelPalette[768];
	u8 s_basePalette[768];
	u8 s_framePalette[768];

	// Move these to color?
	JBool s_palModified = JTRUE;
	JBool s_canChangePal = JTRUE;
	JBool s_screenFxEnabled = JTRUE;
	JBool s_screenBrightnessEnabled = JTRUE;
	JBool s_luminanceMask[3] = { JFALSE, JFALSE, JFALSE };
	JBool s_updateHudColors = JFALSE;

	JBool s_screenBrightnessChanged = JFALSE;
	JBool s_screenFxChanged = JFALSE;
	JBool s_lumMaskChanged = JFALSE;
	
	s32 s_flashFxLevel = 0;
	s32 s_healthFxLevel = 0;
	s32 s_shieldFxLevel = 0;
	s32 s_screenBrightness = ONE_16;

	u8* s_colormap;
	u8* s_lightSourceRamp;

	// Loading
	u8* s_levelColorMap = nullptr;
	u8* s_levelColorMapBasePtr = nullptr;
	u8 s_levelLightRamp[LIGHT_SOURCE_LEVELS];
	
	/////////////////////////////////////////////
	// Internal State
	/////////////////////////////////////////////
	static u8 s_framebuffer[320 * 200];
	static JBool s_exitLevel = JFALSE;
	static Task* s_levelEndTask = nullptr;
	static Task* s_mainTask = nullptr;
	static Task* s_missionLoadTask = nullptr;
	
	static s32 s_visionFxCountdown = 0;
	static s32 s_visionFxEndCountdown = 0;

	/////////////////////////////////////////////
	// Forward Declarations
	/////////////////////////////////////////////
	void mission_mainTaskFunc(MessageType msg);
	void setPalette(u8* pal);
	void blitLoadingScreen();
	void displayLoadingScreen();
	void mission_setupTasks();
	u8*  color_loadMap(FilePath* path, u8* lightRamp, u8** basePtr);

	void setScreenBrightness(fixed16_16 brightness);
	void setScreenFxLevels(s32 healthFx, s32 shieldFx, s32 flashFx);
	void setLuminanceMask(JBool r, JBool g, JBool b);
	void setCurrentColorMap(u8* colorMap, u8* lightRamp);
	void mainTask_handleCall(MessageType msg);
	void handleGeneralInput();

	void updateScreensize();

	// Palette Filters and Effects
	void handlePaletteFx();
	void applyLuminanceFilter(u8* pal, JBool lumRedMask, JBool lumGreenMask, JBool lumBlueMask);
	void applyScreenFxToPalette(u8* pal, s32 healthFxLevel, s32 shieldFxLevel, s32 flashFxLevel);
	void applyScreenBrightness(u8* pal, s32 brightness);
			
	/////////////////////////////////////////////
	// API Implementation
	/////////////////////////////////////////////
	static Tick s_loadingScreenStart;
	static Tick s_loadingScreenDelta;

	void mission_startTaskFunc(MessageType msg)
	{
		task_begin;
		{
			// Make sure the loading screen is displayed for at least 1 second.
			displayLoadingScreen();
			task_yield(MIN_LOAD_TIME);

			s_prevTick = s_curTick;
			s_playerTick = s_curTick;
			s_mainTask = createTask("main task", mission_mainTaskFunc);

			s_invalidLevelIndex = JFALSE;
			s_levelComplete = JFALSE;
			s_exitLevel = JFALSE;
									
			s_missionMode = MISSION_MODE_LOAD_START;
			mission_setupTasks();
			displayLoadingScreen();

			// Add a yield here, so the loading screen is shown immediately.
			task_yield(TASK_NO_DELAY);
			s_loadingScreenStart = s_curTick;
			{
				const char* levelName = agent_getLevelName();
				// For now always load medium difficulty since it cannot be selected.
				if (level_load(levelName, /*s_agentData[s_agentId].difficulty + 1*/3))
				{
					setScreenBrightness(ONE_16);
					setScreenFxLevels(0, 0, 0);
					setLuminanceMask(0, 0, 0);

					char palName[TFE_MAX_PATH];
					strcpy(palName, levelName);
					strcat(palName, ".PAL");
					FilePath filePath;
					if (TFE_Paths::getFilePath(palName, &filePath))
					{
						FileStream::readContents(&filePath, s_levelPalette, 768);
						// The "base palette" is adjusted by the hud colors, which is why it is a copy.
						memcpy(s_basePalette, s_levelPalette, 768);
						s_palModified = JTRUE;
					}

					char colorMapName[TFE_MAX_PATH];
					strcpy(colorMapName, levelName);
					strcat(colorMapName, ".CMP");
					s_levelColorMap = nullptr;

					if (TFE_Paths::getFilePath(colorMapName, &filePath))
					{
						s_levelColorMap = color_loadMap(&filePath, s_levelLightRamp, &s_levelColorMapBasePtr);
					}
					else if (TFE_Paths::getFilePath("DEFAULT.CMP", &filePath))
					{
						TFE_System::logWrite(LOG_WARNING, "mission_startTaskFunc", "USING DEFAULT.CMP");
						s_levelColorMap = color_loadMap(&filePath, s_levelLightRamp, &s_levelColorMapBasePtr);
					}

					setCurrentColorMap(s_levelColorMap, s_levelLightRamp);
					automap_updateMapData(MAP_CENTER_PLAYER);
					setSkyParallax(s_parallax0, s_parallax1);
					// initSoundEffects();  <- TODO: Handle later
					s_missionMode = MISSION_MODE_MAIN;
					s_gamePaused = JFALSE;
					hud_startup();
				}
			}
			TFE_Input::clearAccumulatedMouseMove();
		}
		// Sleep until we are done with the main task.
		task_yield(TASK_SLEEP);

		// Cleanup - shut down all tasks.
		task_freeAll();

		// End the task.
		task_end;
	}

	void mission_setLoadMissionTask(Task* task)
	{
		s_missionLoadTask = task;
	}
			
	// In DOS, this was part of drawWorld() - 
	// for TFE I split it out to limit the amount of game code in the renderer.
	void handleVisionFx()
	{
		// Countdown to enable the night vision effect.
		if (s_visionFxCountdown != 0)
		{
			s_visionFxCountdown--;
			if (s_visionFxCountdown == 0)
			{
				setLuminanceMask(JFALSE, JTRUE, JFALSE);
			}
		}

		// Countdown to disable the night vision effect.
		if (s_visionFxEndCountdown != 0)
		{
			s_visionFxEndCountdown--;
			if (s_visionFxEndCountdown == 0)
			{
				setLuminanceMask(JFALSE, JFALSE, JFALSE);
			}
		}
	}

	void mission_mainTaskFunc(MessageType msg)
	{
		task_begin;
		while (msg != MSG_FREE_TASK)
		{
			// This means it is time to abort, we are done with this level.
			if (s_curTick >= 0 && (s_exitLevel || msg < MSG_RUN_TASK))
			{
				break;
			}
			// Handle delta time.
			s_deltaTime = div16(intToFixed16(s_curTick - s_prevTick), FIXED(TICKS_PER_SECOND));
			s_deltaTime = min(s_deltaTime, MAX_DELTA_TIME);
			s_prevTick = s_curTick;
			s_playerTick = s_curTick;

			player_setupCamera();

			if (s_missionMode == MISSION_MODE_LOADING)
			{
				blitLoadingScreen();
			}
			else if (s_missionMode == MISSION_MODE_MAIN)
			{
				updateScreensize();
				drawWorld(s_framebuffer, s_playerEye->sector, s_levelColorMap, s_lightSourceRamp);
				weapon_draw(s_framebuffer, &s_videoDrawRect);
				handleVisionFx();
			}
			else if (s_missionMode == MISSION_MODE_UNKNOWN)
			{
				// STUB
			}
			else if (s_missionMode == MISSION_MODE_LOAD_START)
			{
				// vgaClearPalette();
			}

			handleGeneralInput();
			handlePaletteFx();
			if (s_drawAutomap)
			{
				automap_draw(s_framebuffer);
			}
			hud_drawAndUpdate(s_framebuffer);
			hud_drawMessage(s_framebuffer);

			// vgaSwapBuffers() in the DOS code.
			TFE_RenderBackend::updateVirtualDisplay(s_framebuffer, 320 * 200);

			// Pump tasks and look for any with a different ID.
			do
			{
				task_yield(TASK_NO_DELAY);
				if (msg != MSG_FREE_TASK && msg != MSG_RUN_TASK)
				{
					mainTask_handleCall(msg);
				}
			} while (msg != MSG_FREE_TASK && msg != MSG_RUN_TASK);
		}

		s_mainTask = nullptr;
		task_makeActive(s_missionLoadTask);
		task_end;
	}

	/////////////////////////////////////////////
	// Internal Implementation
	/////////////////////////////////////////////
	void mainTask_handleCall(MessageType msg)
	{
		if (msg == MSG_LIGHTS)
		{
			sector_changeGlobalLightLevel();
		}
	}
	
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
				
	void blitLoadingScreen()
	{
		if (!s_loadScreen) { return; }
		blitTextureToScreen(s_loadScreen, &s_videoDrawRect, 0/*x0*/, 0/*y0*/, s_framebuffer);
	}

	void displayLoadingScreen()
	{
		blitLoadingScreen();

		// Update twice to make sure the loading screen is visible.
		// The virtual display is buffered, meaning there is a frame of latency.
		// This hackery basically removes that latency so the image is properly displayed immediately.
		setPalette(s_loadingScreenPal);
		setPalette(s_loadingScreenPal);
		TFE_RenderBackend::updateVirtualDisplay(s_framebuffer, 320 * 200);
		TFE_RenderBackend::updateVirtualDisplay(s_framebuffer, 320 * 200);
	}

	void mission_setupTasks()
	{
		setSpriteAnimation(nullptr, nullptr);
		bitmap_setupAnimationTask();
		// resetFrameData();		// TODO(Core Game Loop Release)
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
		
	void setScreenBrightness(fixed16_16 brightness)
	{
		if (brightness != s_screenBrightness)
		{
			s_screenBrightness = brightness;
			s_screenBrightnessChanged = JTRUE;
		}
	}

	void setScreenFxLevels(s32 healthFx, s32 shieldFx, s32 flashFx)
	{
		if (healthFx != s_healthFxLevel || shieldFx != s_shieldFxLevel || flashFx != s_flashFxLevel)
		{
			s_healthFxLevel = healthFx;
			s_shieldFxLevel = shieldFx;
			s_flashFxLevel  = flashFx;
			s_screenFxChanged = JTRUE;
		}
	}

	void setLuminanceMask(JBool r, JBool g, JBool b)
	{
		if (r != s_luminanceMask[0] || g != s_luminanceMask[1] || b != s_luminanceMask[2])
		{
			s_luminanceMask[0] = r;
			s_luminanceMask[1] = g;
			s_luminanceMask[2] = b;
			s_lumMaskChanged = JTRUE;
		}
	}
		
	void handlePaletteFx()
	{
		JBool useFramePal   = JFALSE;
		JBool updateBasePal = JFALSE;
		JBool copiedPalette = JFALSE;

		if (!s_canChangePal) { return; }
		if (!s_lumMaskChanged && !s_screenFxChanged && !s_screenBrightnessChanged && !s_updateHudColors && !s_palModified) { return; }

		if (s_luminanceMask[0] || s_luminanceMask[1] || s_luminanceMask[2])
		{
			if (!copiedPalette)
			{
				memcpy(s_framePalette, s_basePalette, 768);
				copiedPalette = JTRUE;
			}
			applyLuminanceFilter(s_framePalette, s_luminanceMask[0], s_luminanceMask[1], s_luminanceMask[2]);
			s_palModified = JTRUE;
			useFramePal = JTRUE;
		}
		else if (s_palModified)
		{
			updateBasePal = JTRUE;
		}

		s_lumMaskChanged = JFALSE;
		if (s_screenFxEnabled)
		{
			if (s_healthFxLevel || s_shieldFxLevel || s_flashFxLevel)
			{
				if (!copiedPalette)
				{
					memcpy(s_framePalette, s_basePalette, 768);
					copiedPalette = JTRUE;
				}
				applyScreenFxToPalette(s_framePalette, s_healthFxLevel, s_shieldFxLevel, s_flashFxLevel);
				useFramePal = JTRUE;
				s_palModified = JTRUE;
			}
			else if (s_palModified)
			{
				updateBasePal = JTRUE;
			}
			s_screenFxChanged = JFALSE;
		}

		if (s_screenBrightnessEnabled)
		{
			if (s_screenBrightness < ONE_16)
			{
				if (!copiedPalette)
				{
					memcpy(s_framePalette, s_basePalette, 768);
					copiedPalette = JTRUE;
				}
				applyScreenBrightness(s_framePalette, s_screenBrightness);
				s_palModified = JTRUE;
				useFramePal = JTRUE;
			}
			else if (s_palModified)
			{
				updateBasePal = JTRUE;
			}
			s_screenBrightnessChanged = JFALSE;
		}

		if (useFramePal)
		{
			// Convert the palette to true color and send to the render backend.
			setPalette(s_framePalette);
			s_updateHudColors = JFALSE;
		}
		else
		{
			if (updateBasePal)
			{
				copiedPalette = JFALSE;
				setPalette(s_basePalette);
				s_updateHudColors = JFALSE;
			}
			else if (s_updateHudColors)
			{
				// Update the HUD shield color indices.
				// For TFE, just upload the full palette, the original DOS code
				// only updated the VGA registers for the changed HUD colors.
				setPalette(s_basePalette);
				s_updateHudColors = JFALSE;
			}
		}

		// TFE uses a dynamic multi-buffered texture for the palette. This doesn't work well when trying to set it only once.
		// For this reason, it is easier to just set the palette every frame regardless of change.
	#if 0
		if (!s_luminanceMask[0] && !s_luminanceMask[1] && !s_luminanceMask[2] && !s_healthFxLevel && !s_shieldFxLevel && !s_flashFxLevel && s_screenBrightness == ONE_16)
		{
			s_palModified = JFALSE;
		}
	#endif
	}

	void setCurrentColorMap(u8* colorMap, u8* lightRamp)
	{
		s_colormap = colorMap;
		s_lightSourceRamp = lightRamp;
	}

	// Move to the appropriate place.
	u8* color_loadMap(FilePath* path, u8* lightRamp, u8** basePtr)
	{
		FileStream file;
		if (!file.open(path, FileStream::MODE_READ))
		{
			TFE_System::logWrite(LOG_ERROR, "color_loadMap", "Error loading color map.");
			return nullptr;
		}

		// Allocate 256 colors * 32 light levels + 256, where the last 256 is so that the address can be rounded to the next 256 byte boundary.
		u8* colorMapBase = (u8*)level_alloc(8576);
		u8* colorMap = colorMapBase;
		*basePtr = colorMapBase;
		if (size_t(colorMap) & 0xffu)
		{
			// colorMap2 is rounded to the next 256 bytes.
			colorMap = colorMapBase + 256 - (size_t(colorMap) & 0xff);
		}
		// 256 colors * 32 light levels = 8192
		file.readBuffer(colorMap, 8192);
		file.readBuffer(lightRamp, LIGHT_SOURCE_LEVELS);
		file.close();

		return colorMap;
	}

	void disableHeadlamp()
	{
		hud_sendTextMessage(12);
		s_headlampActive = JFALSE;
		hud_setupToggleAnim1(JTRUE);
	}

	void enableHeadlamp()
	{
		if (s_energy)
		{
			s_headlampActive = JTRUE;
			hud_sendTextMessage(13);
			hud_setupToggleAnim1(JTRUE);
		}
	}

	void beginNightVision(s32 ambient)
	{
		s_flatAmbient = ambient;
		s_flatLighting = JTRUE;
		s_visionFxCountdown = 2;
	}

	void disableNightvisionInternal()
	{
		s_flatLighting = JFALSE;
		s_visionFxEndCountdown = 3;
	}

	void disableNightvision()
	{
		disableNightvisionInternal();
		s_nightvisionActive = JFALSE;
		hud_sendTextMessage(9);
	}

	void enableNightVision()
	{
		if (!s_playerInfo.itemGoggles) { return; }

		if (!s_energy)
		{
			hud_sendTextMessage(11);
			playSound2D(s_nightVisionDeactiveSoundSource);
			s_nightvisionActive = JFALSE;
			return;
		}

		s_nightvisionActive = JTRUE;
		beginNightVision(16);
		hud_sendTextMessage(10);
		playSound2D(s_nightVisionActiveSoundSource);
	}

	void disableCleats()
	{
		if (s_wearingCleats)
		{
			s_wearingCleats = JFALSE;
			hud_sendTextMessage(21);
		}
	}

	void enableCleats()
	{
		s_wearingCleats = JTRUE;
		hud_sendTextMessage(22);
	}

	void disableMask()
	{
		if (!s_wearingGasmask)
		{
			return;
		}

		hud_sendTextMessage(19);
		s_wearingGasmask = JFALSE;
		if (s_gasmaskTask)
		{
			task_free(s_gasmaskTask);
			s_gasmaskTask = nullptr;
		}
	}

	void enableMask()
	{
		if (!s_playerInfo.itemMask) { return; }

		if (!s_energy)
		{
			hud_sendTextMessage(11);
			s_wearingGasmask = JFALSE;
			if (s_gasmaskTask)
			{
				task_free(s_gasmaskTask);
				s_gasmaskTask = nullptr;
			}
			return;
		}

		s_wearingGasmask = JTRUE;
		hud_sendTextMessage(20);
		if (!s_gasmaskTask)
		{
			s_gasmaskTask = createSubTask("gasmask", gasmaskTaskFunc);
		}
	}
		
	void handleBufferedInput()
	{
		const char* bufferedText = TFE_Input::getBufferedText();
		const size_t bufferedLen = strlen(bufferedText);
		for (size_t i = 0; i < bufferedLen; i++)
		{
			char c = toupper(bufferedText[i]);
			if (s_cheatInputCount < 2 && c == 'L')
			{
				s_cheatInputCount = 1;
			}
			else if (s_cheatInputCount == 1)
			{
				if (c == 'A')
				{
					s_cheatInputCount = 2;
					s_cheatCharIndex = 0;
					s_cheatString[0] = 0;
				}
				else
				{
					s_cheatInputCount = 0;
				}
			}
			else if (s_cheatInputCount >= 2)
			{
				s32 index = s_cheatCharIndex;
				s_cheatCharIndex++;

				s_cheatString[index] = c;
				s_cheatString[s_cheatCharIndex] = 0;

				CheatID cheatID = cheat_getID();
				if (cheatID == CHEAT_NONE)
				{
					continue;
				}

				switch (cheatID)
				{
					case CHEAT_LACDS:
					{
						automap_updateMapData(MAP_INCR_SECTOR_MODE);
					} break;
					case CHEAT_LANTFH:
					{
						automap_updateMapData(MAP_TELEPORT);
						cheat_teleport();
					} break;
					case CHEAT_LAPOGO:
					{
						cheat_enableNoheightCheck();
					} break;
					case CHEAT_LARANDY:
					{
						pickupSupercharge();
						hud_sendTextMessage(701);
					} break;
					case CHEAT_LAIMLAME:
					{
						cheat_godMode();
					} break;
					case CHEAT_LAPOSTAL:
					{
						cheat_postal();
					} break;
					case CHEAT_LADATA:
					{
						hud_toggleDataDisplay();
					} break;
					case CHEAT_LABUG:
					{
						cheat_bugMode();
					} break;
					case CHEAT_LAREDLITE:
					{
					} break;
					case CHEAT_LASECBASE:
					{
					} break;
					case CHEAT_LATALAY:
					{
					} break;
					case CHEAT_LASEWERS:
					{
					} break;
					case CHEAT_LATESTBASE:
					{
					} break;
					case CHEAT_LAGROMAS:
					{
					} break;
					case CHEAT_LADTENTION:
					{
					} break;
					case CHEAT_LARAMSHAD:
					{
					} break;
					case CHEAT_LAROBOTICS:
					{
					} break;
					case CHEAT_LANARSHADA:
					{
					} break;
					case CHEAT_LAJABSHIP:
					{
					} break;
					case CHEAT_LAIMPCITY:
					{
					} break;
					case CHEAT_LAFUELSTAT:
					{
					} break;
					case CHEAT_LAEXECUTOR:
					{
					} break;
					case CHEAT_LAARC:
					{
					} break;
					case CHEAT_LASKIP:
					{
					} break;
					case CHEAT_LABRADY:
					{
						cheat_fullAmmo();
					} break;
					case CHEAT_LAUNLOCK:
					{
						cheat_unlock();
					} break;
					case CHEAT_LAMAXOUT:
					{
						cheat_maxout();
					} break;
				}
			}
		}
	}

	void handleGeneralInput()
	{
		// In the DOS code, the game would just loop here - checking to see if paused has been pressed and then continue.
		// Obviously that won't work for TFE, so the game paused variable is set and the game will have to handle it.
		if (getActionState(IA_PAUSE) == STATE_PRESSED)
		{
			s_gamePaused = ~s_gamePaused;
		}

		if (!s_gamePaused)
		{
			handleBufferedInput();

			// For now just deal with a few controls.
			if (getActionState(IA_PDA_TOGGLE) == STATE_PRESSED)
			{
				// STUB: Bring up PDA.
			}
			if (getActionState(IA_NIGHT_VISION_TOG) == STATE_PRESSED && s_playerInfo.itemGoggles)
			{
				if (s_nightvisionActive)
				{
					disableNightvision();
				}
				else
				{
					enableNightVision();
				}
			}
			if (getActionState(IA_CLEATS_TOGGLE) == STATE_PRESSED && s_playerInfo.itemCleats)
			{
				if (s_wearingCleats)
				{
					disableCleats();
				}
				else
				{
					enableCleats();
				}
			}
			if (getActionState(IA_GAS_MASK_TOGGLE) == STATE_PRESSED && s_playerInfo.itemMask)
			{
				if (s_wearingGasmask)
				{
					disableMask();
				}
				else
				{
					enableMask();
				}
			}
			if (getActionState(IA_HEAD_LAMP_TOGGLE) == STATE_PRESSED)
			{
				if (s_headlampActive)
				{
					disableHeadlamp();
				}
				else
				{
					enableHeadlamp();
				}
			}
			renderer_setupCameraLight(JFALSE, s_headlampActive);

			if (getActionState(IA_HEADWAVE_TOGGLE) == STATE_PRESSED)
			{
				s_config.headwave = ~s_config.headwave;
				if (s_config.headwave)
				{
					hud_sendTextMessage(14);
				}
				else
				{
					hud_sendTextMessage(15);
				}
			}

			if (getActionState(IA_HUD_TOGGLE) == STATE_PRESSED)
			{
				hud_setupToggleAnim1(JFALSE);
			}

			if (getActionState(IA_HOLSTER_WEAPON) == STATE_PRESSED)
			{
				weapon_holster();
			}

			if (getActionState(IA_AUTOMOUNT_TOGGLE) == STATE_PRESSED)
			{
				s_config.wpnAutoMount = ~s_config.wpnAutoMount;
				weapon_enableAutomount(s_config.wpnAutoMount);
				if (!s_config.wpnAutoMount)
				{
					hud_sendTextMessage(23);
				}
				else
				{
					hud_sendTextMessage(24);
				}
			}

			if (getActionState(IA_CYCLEWPN_PREV))
			{
				player_cycleWeapons(-1);
			}
			else if (getActionState(IA_CYCLEWPN_NEXT))
			{
				player_cycleWeapons(1);
			}

			if (getActionState(IA_AUTOMAP) == STATE_PRESSED)
			{
				s_drawAutomap = ~s_drawAutomap;
				if (s_drawAutomap)
				{
					automap_updateMapData(MAP_ENABLE_AUTOCENTER);
					automap_updateMapData(MAP_CENTER_PLAYER);
					s_canTeleport = JTRUE;
				}
				else
				{
					s_canTeleport = JFALSE;
				}
			}

			if (s_drawAutomap)
			{
				if (getActionState(IA_MAP_ENABLE_SCROLL))
				{
					automap_disableTeleport();
				}
				else
				{
					automap_enableTeleport();
				}

				if (getActionState(IA_MAP_ZOOM_IN))
				{
					automap_updateMapData(MAP_ZOOM_IN);
				}
				else if (getActionState(IA_MAP_ZOOM_OUT))
				{
					automap_updateMapData(MAP_ZOOM_OUT);
				}

				if (getActionState(IA_MAP_LAYER_UP) == STATE_PRESSED)
				{
					automap_updateMapData(MAP_LAYER_UP);

					s32 msgId = min(automap_getLayer() + 609, 618);
					hud_sendTextMessage(msgId);
				}
				else if (getActionState(IA_MAP_LAYER_DN) == STATE_PRESSED)
				{
					automap_updateMapData(MAP_LAYER_DOWN);

					s32 msgId = min(automap_getLayer() + 609, 618);
					hud_sendTextMessage(msgId);
				}

				if (!s_automapCanTeleport)
				{
					if (getActionState(IA_MAP_SCROLL_UP))
					{
						automap_updateMapData(MAP_MOVE1_UP);
					}
					if (getActionState(IA_MAP_SCROLL_DN))
					{
						automap_updateMapData(MAP_MOVE1_DN);
					}
					if (getActionState(IA_MAP_SCROLL_LT))
					{
						automap_updateMapData(MAP_MOVE1_LEFT);
					}
					if (getActionState(IA_MAP_SCROLL_RT))
					{
						automap_updateMapData(MAP_MOVE1_RIGHT);
					}
				}
			}
		}
	}

	void updateScreensize()
	{
		// STUB: Not needed until screensizes are supported.
	}

	////////////////////////////////////////////
	// Palette Filters and Effects
	////////////////////////////////////////////
	void applyLuminanceFilter(u8* pal, JBool lumRedMask, JBool lumGreenMask, JBool lumBlueMask)
	{
		u8* end = &pal[768];
		for (; pal < end; pal += 3)
		{
			// Compute the approximate luminance (red/4 + green/2 + blue/4)
			u32 L = (pal[0] >> 2) + (pal[1] >> 1) + (pal[2] >> 2);
			// Then assign the luminance to the requested channels.
			pal[0] = lumRedMask ? L : 0;
			pal[1] = lumGreenMask ? L : 0;
			pal[2] = lumBlueMask ? L : 0;
		}
	}

	void applyScreenFxToPalette(u8* pal, s32 healthFxLevel, s32 shieldFxLevel, s32 flashFxLevel)
	{
		s32 intensity;
		s32 filterIndex;
		s32 clrIndex0, clrIndex1;
		if (healthFxLevel)
		{
			intensity = 63 - (healthFxLevel & 63);
			filterIndex = 0;
			clrIndex0 = 1;
			clrIndex1 = 2;
		}
		else if (shieldFxLevel)
		{
			intensity = 63 - (shieldFxLevel & 63);
			filterIndex = 1;
			clrIndex0 = 0;
			clrIndex1 = 2;
		}
		else
		{
			intensity = 63 - (flashFxLevel & 63);
			filterIndex = 2;
			clrIndex0 = 0;
			clrIndex1 = 1;
		}

		u8* c0 = &pal[clrIndex0];
		u8* c1 = &pal[clrIndex1];
		u8* filterColor = &pal[filterIndex];

		for (s32 i = 0; i < 256; i++)
		{
			// Essentially: 63 - abs(63 - color) * (1 - flashLevel)
			// Note that when flashLevel = 0: 63 - 63 + color = color
			//                flashLevel = 1: 63 = pure blue.
			*filterColor = 63 - (TFE_Jedi::abs((63 - (*filterColor)) * intensity) >> 6);

			// Color channel = color * (1 - flashLevel)
			*c0 = TFE_Jedi::abs((*c0) * intensity) >> 6;
			*c1 = TFE_Jedi::abs((*c1) * intensity) >> 6;

			filterColor += 3;
			c0 += 3;
			c1 += 3;
		}
	}

	void applyScreenBrightness(u8* pal, s32 brightness)
	{
		u8* end = &pal[768];
		for (s32 i = 0; i < 256; i++, pal += 3)
		{
			fixed16_16 color = intToFixed16(pal[0]);
			color = mul16(color, brightness) + HALF_16;
			pal[0] = floor16(color);

			color = intToFixed16(pal[1]);
			color = mul16(color, brightness) + HALF_16;
			pal[1] = floor16(color);

			color = intToFixed16(pal[2]);
			color = mul16(color, brightness) + HALF_16;
			pal[2] = floor16(color);
		}
	}

}  // TFE_DarkForces