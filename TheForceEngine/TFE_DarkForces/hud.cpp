#include "hud.h"
#include "time.h"
#include "automap.h"
#include "gameMessage.h"
#include "mission.h"
#include "util.h"
#include "player.h"
#include "weapon.h"
#include <TFE_FileSystem/paths.h>
#include <TFE_System/system.h>
#include <TFE_Jedi/Renderer/jediRenderer.h>
#include <TFE_Jedi/Renderer/RClassic_Fixed/screenDraw.h>
#include <TFE_Jedi/Level/rfont.h>
#include <TFE_Jedi/Level/rtexture.h>
#include <TFE_Jedi/Level/roffscreenBuffer.h>

namespace TFE_DarkForces
{
	///////////////////////////////////////////
	// Constants
	///////////////////////////////////////////
	enum HudConstants
	{
		HUD_COLORS_START    = 24,
		HUD_COLORS_COUNT    = 8,
		HUD_MAX_PRIORITY    = 0,
		HUD_HIGH_PRIORITY   = 1,
		HUD_LOWEST_PRIORITY = 9,

		HUD_MSG_LONG_DUR  = 1165,	// 8 seconds
		HUD_MSG_SHORT_DUR =  436,	// 3 seconds
	};

	static const s32 c_hudVertAnimTable[] =
	{
		160, 170, 185, 200,
	};

	///////////////////////////////////////////
	// Internal State
	///////////////////////////////////////////
	static GameMessages s_hudMessages;
	static Font* s_hudFont;
	static char s_hudMessage[80];
	static s32 s_hudCurrentMsgId = 0;
	static s32 s_hudMsgPriority = HUD_LOWEST_PRIORITY;
	static Tick s_hudMsgExpireTick;

	static JBool s_screenDirtyLeft[4]  = { 0 };
	static JBool s_screenDirtyRight[4] = { 0 };
	static JBool s_basePaletteCopyMode = JFALSE;
	static JBool s_updateHudColors = JFALSE;

	static u8 s_tempBuffer[256 * 3];
	static u8 s_hudPalette[HUD_COLORS_COUNT * 3];
	static u8 s_hudWorkPalette[HUD_COLORS_COUNT * 3];
	static s32 s_hudColorCount = 8;
	static s32 s_hudPaletteIndex = 24;
	
	static TextureData* s_hudStatusL  = nullptr;
	static TextureData* s_hudStatusR  = nullptr;
	static TextureData* s_hudLightOn  = nullptr;
	static TextureData* s_hudLightOff = nullptr;
	static Font* s_hudMainAmmoFont    = nullptr;
	static Font* s_hudSuperAmmoFont   = nullptr;
	static Font* s_hudShieldFont      = nullptr;
	static Font* s_hudHealthFont      = nullptr;
	static OffScreenBuffer* s_cachedHudLeft = nullptr;
	static OffScreenBuffer* s_cachedHudRight = nullptr;

	static s32 s_rightHudVertTarget;
	static s32 s_rightHudVertAnim;
	static s32 s_rightHudShow;
	static s32 s_rightHudMove;
	static s32 s_leftHudVertTarget;
	static s32 s_leftHudVertAnim;
	static s32 s_leftHudShow;
	static s32 s_leftHudMove;
	static JBool s_forceHudPaletteUpdate = JFALSE;

	static s32 s_prevEnergy = 0;
	static s32 s_prevLifeCount = 0;
	static s32 s_prevShields = 0;
	static s32 s_prevHealth = 0;
	static s32 s_prevPrimaryAmmo = 0;
	static s32 s_prevSecondaryAmmo = 0;
	static JBool s_prevSuperchageHud = JFALSE;
	static JBool s_prevHeadlampActive = JFALSE;

	static DrawRect s_hudTextDrawRect = { 0, 0, 319, 199 };
	static ScreenRect s_hudTextScreenRect = { 0, 0, 319, 199 };

	///////////////////////////////////////////
	// Shared State
	///////////////////////////////////////////
	fixed16_16 s_flashEffect = 0;
	fixed16_16 s_healthDamageFx = 0;
	fixed16_16 s_shieldDamageFx = 0;

	s32 s_secretsFound = 0;
	s32 s_secretsPercent = 0;
	JBool s_showData = JFALSE;

	///////////////////////////////////////////
	// Forward Declarations
	///////////////////////////////////////////
	GameMessage* hud_getMessage(GameMessages* messages, s32 msgId);
	TextureData* hud_loadTexture(const char* texFile);
	Font* hud_loadFont(const char* fontFile);
	void copyIntoPalette(u8* dst, u8* src, s32 count, s32 mode);
	void getCameraXZ(fixed16_16* x, fixed16_16* z);
	void displayHudMessage(Font* font, DrawRect* rect, s32 x, s32 y, char* msg, u8* framebuffer);
	void hud_drawString(OffScreenBuffer* elem, Font* font, s32 x0, s32 y0, const char* str);

	///////////////////////////////////////////
	// API Implementation
	///////////////////////////////////////////
	void hud_sendTextMessage(s32 msgId)
	{
		GameMessage* msg = hud_getMessage(&s_hudMessages, msgId);
		// Only display the message if it is the same or lower priority than the current message.
		if (!msg || msg->priority > s_hudMsgPriority)
		{
			return;
		}

		const char* msgText = msg->text;
		if (!msgText[0]) { return; }
		strCopyAndZero(s_hudMessage, msgText, 80);

		s_hudMsgExpireTick = s_curTick + ((msg->priority <= HUD_HIGH_PRIORITY) ? HUD_MSG_LONG_DUR : HUD_MSG_SHORT_DUR);
		s_hudCurrentMsgId  = msgId;
		s_hudMsgPriority   = msg->priority;
	}

	void hud_clearMessage()
	{
		s_hudMessage[0] = 0;
		s_hudCurrentMsgId = 0;
		s_hudMsgPriority = HUD_LOWEST_PRIORITY;
		for (s32 i = 0; i < 4; i++)
		{
			s_screenDirtyLeft[i]  = JFALSE;
			s_screenDirtyRight[i] = JFALSE;
		}
	}

	void hud_loadGameMessages()
	{
		s_hudMessages.count = 0;
		s_hudMessages.msgList = nullptr;

		FilePath filePath;
		if (TFE_Paths::getFilePath("text.msg", &filePath))
		{
			parseMessageFile(&s_hudMessages, &filePath, 1);
		}
		s_hudMessage[0] = 0;
		if (TFE_Paths::getFilePath("glowing.fnt", &filePath))
		{
			s_hudFont = font_load(&filePath);
		}
	}
		
	void hud_loadGraphics()
	{
		s_hudStatusL       = hud_loadTexture("StatusLf.bm");
		s_hudStatusR       = hud_loadTexture("StatusRt.bm");
		s_hudLightOn       = hud_loadTexture("lighton.bm");
		s_hudLightOff      = hud_loadTexture("lightoff.bm");
		s_hudMainAmmoFont  = hud_loadFont("AmoNum.fnt");
		s_hudSuperAmmoFont = hud_loadFont("SuperWep.fnt");
		s_hudShieldFont    = hud_loadFont("ArmNum.fnt");
		s_hudHealthFont    = hud_loadFont("HelNum.fnt");

		// Create offscreen buffers for the HUD elements.
		s_cachedHudLeft  = createOffScreenBuffer(s_hudStatusL->width, s_hudStatusL->height, OBF_TRANS);
		s_cachedHudRight = createOffScreenBuffer(s_hudStatusR->width, s_hudStatusR->height, OBF_TRANS);
		// Clear the buffer images to transparent (0).
		offscreenBuffer_clearImage(s_cachedHudLeft,  0/*clear_color*/);
		offscreenBuffer_clearImage(s_cachedHudRight, 0/*clear_color*/);

		if (s_hudStatusL)
		{
			offscreenBuffer_drawTexture(s_cachedHudLeft, s_hudStatusL, 0, 0);
		}
		if (s_hudStatusR)
		{
			offscreenBuffer_drawTexture(s_cachedHudRight, s_hudStatusR, 0, 0);
		}
	}
		
	void hud_updateBasePalette(u8* srcBuffer, s32 offset, s32 count)
	{
		u8* dstBuffer = s_tempBuffer + offset * 3;
		memcpy(dstBuffer, srcBuffer, count * 3);
		copyIntoPalette(s_basePalette + (offset * 3), srcBuffer, count, s_basePaletteCopyMode);

		s_hudPaletteIndex = offset;
		s_hudColorCount = count;
		s_updateHudColors = JTRUE;
	}
		
	void hud_initAnimation()
	{
		s_rightHudMove = 4;
		s_leftHudMove = 4;
		s_forceHudPaletteUpdate = JTRUE;
		s_rightHudShow = 4;
		s_leftHudShow = 4;
	}
		
	void hud_setupToggleAnim1(JBool enable)
	{
		if (enable)
		{
			s_leftHudVertTarget = 0;
			s_rightHudVertTarget = 0;

			s_leftHudVertAnim = 0;
			s_rightHudVertAnim = 0;

			s_leftHudShow = 4;
			s_rightHudShow = 4;

			s_leftHudMove = 4;
			s_rightHudMove = 4;
		}
		else
		{
			s_leftHudVertTarget = s_leftHudVertAnim == 0 ? 3 : 0;
			s_rightHudVertTarget = s_rightHudVertAnim == 0 ? 3 : 0;

			s_leftHudShow = 4;
			s_rightHudShow = 4;

			s_leftHudMove = 4;
			s_rightHudMove = 4;
		}
	}

	void hud_toggleDataDisplay()
	{
		s_showData = ~s_showData;
	}

	void hud_startup()
	{
		// Reset cached values.
		s_prevEnergy = 0;
		s_prevLifeCount = 0;
		s_prevShields = 0;
		s_prevHealth = 0;
		s_prevPrimaryAmmo = 0;
		s_prevSecondaryAmmo  = 0;
		s_prevSuperchageHud  = JFALSE;
		s_prevHeadlampActive = JFALSE;

		s_secretsFound = 0;
		s_secretsPercent = 0;

		hud_initAnimation();
		hud_setupToggleAnim1(JTRUE);
		offscreenBuffer_drawTexture(s_cachedHudRight, s_hudLightOff, 19, 0);
	}
		
	void hud_drawMessage(u8* framebuffer)
	{
		if (s_missionMode == MISSION_MODE_MAIN && s_hudMessage[0])
		{
			displayHudMessage(s_hudFont, &s_hudTextDrawRect, 4, 10, s_hudMessage, framebuffer);
			if (s_curTick > s_hudMsgExpireTick)
			{
				s_hudMessage[0]   = 0;
				s_hudCurrentMsgId = 0;
				s_hudMsgPriority  = HUD_LOWEST_PRIORITY;
			}
			// s_screenDirtyLeft[s_curFrameBufferIdx] = JTRUE;
		}

		if (s_showData && s_playerEye)
		{
			fixed16_16 x, z;
			getCameraXZ(&x, &z);

			char dataStr[64];
			sprintf(dataStr, "X:%04d Y:%.1f Z:%04d H:%.1f S:%d%%", floor16(x), -fixed16ToFloat(s_playerEye->posWS.y), floor16(z), fixed16ToFloat(s_playerEye->worldHeight), s_secretsPercent);
			displayHudMessage(s_hudFont, &s_hudTextDrawRect, 164, 10, dataStr, framebuffer);
			// s_screenDirtyRight[s_curFrameBufferIdx] = JTRUE;
		}
	}
		
	void hud_drawAndUpdate(u8* framebuffer)
	{
		// Clear the 3D view while the HUD positions are being animated.
		if (s_rightHudMove || s_leftHudMove)
		{
			if (s_rightHudMove)
			{
				s_rightHudMove--;
			}
			if (s_leftHudMove)
			{
				s_leftHudMove--;
			}
			clear3DView(framebuffer);
		}

		// Go ahead and just update the HUD every frame, there are already checks to see if each item changed.
		//if (s_leftHudShow || s_rightHudShow)
		{
			fixed16_16 energy = TFE_Jedi::abs(s_energy * 100);
			s32 percent = round16(energy >> 1);
			if (s_prevEnergy != percent)  // True when energy ticks down.
			{
				s_prevEnergy = percent;
				const s32 offset = 3 * 3 + 1;	// offset 3 color indices + 1 because we are only changing the green color.
				// I think this should be < 5, since there are 5 pips.
				for (s32 clrIndex = 0; clrIndex <= 5; clrIndex++)
				{
					if (percent > 20)
					{
						s_hudWorkPalette[3 * clrIndex + offset] = 58;
						percent -= 20;
					}
					else
					{
						// Color value between [8, 58], where 8 @ 0% and 58 @ 20%
						s_hudWorkPalette[3 * clrIndex + offset] = 8 + ((percent * 5) >> 1);
						percent = 0;
					}
				}
			}
			s32 lifeCount = player_getLifeCount() % 10;
			if (s_prevLifeCount != lifeCount)
			{
				s_prevLifeCount = lifeCount;

				char lifeCountStr[8];
				sprintf(lifeCountStr, "%1d", lifeCount);
				hud_drawString(s_cachedHudLeft, s_hudHealthFont, 52, 26, lifeCountStr);

				s_rightHudShow = 4;
			}
			if (s_prevHeadlampActive != s_headlampActive)
			{
				s_prevHeadlampActive = s_headlampActive;
				if (s_headlampActive)
				{
					offscreenBuffer_drawTexture(s_cachedHudRight, s_hudLightOn, 19, 0);
				}
				else
				{
					offscreenBuffer_drawTexture(s_cachedHudRight, s_hudLightOff, 19, 0);
				}
				s_rightHudShow = 4;
			}
			if (s_playerInfo.shields != s_prevShields)
			{
				char shieldStr[8];

				if (s_playerInfo.shields == -1)
				{
					s_hudWorkPalette[0] = 55;
					s_hudWorkPalette[1] = 55;
					s_hudWorkPalette[3] = 55;
					s_hudWorkPalette[4] = 55;

					strcpy(shieldStr, "200");
				}
				else
				{
					s_hudWorkPalette[0] = 0;
					s32 upperShields;
					if (s_playerInfo.shields > 100)
					{
						upperShields = s_playerInfo.shields;
					}
					else
					{
						upperShields = 100;
					}
					upperShields -= 100;
					u8 upperColor = 8 + (upperShields >> 1);
					s_hudWorkPalette[1] = upperColor;

					s_hudWorkPalette[3] = 0;
					s32 lowerShields;
					if (s_playerInfo.shields < 100)
					{
						lowerShields = s_playerInfo.shields;
					}
					else
					{
						lowerShields = 100;
					}
					s_hudWorkPalette[4] = 8 + (lowerShields >> 1);
					sprintf(shieldStr, "%03d", s_playerInfo.shields);
				}
				s_prevShields = s_playerInfo.shields;
				hud_drawString(s_cachedHudLeft, s_hudShieldFont, 15, 26, shieldStr);
			}
			if (s_playerInfo.health != s_prevHealth)
			{
				s32 health = min(100, s_playerInfo.health);
				// 6: color 2, red channel, 7 = green channel
				s_hudWorkPalette[6] = 8 + (health >> 1);
				s_hudWorkPalette[7] = health * 63 / 400;
				s_prevHealth = s_playerInfo.health;

				char healthStr[32];
				sprintf(healthStr, "%03d", s_playerInfo.health);
				hud_drawString(s_cachedHudLeft, s_hudHealthFont, 33, 26, healthStr);

				s_leftHudShow = 4;
			}

			if (s_forceHudPaletteUpdate || memcmp(s_hudPalette, s_hudWorkPalette, HUD_COLORS_COUNT * 3) != 0)
			{
				// Copy work palette into hud palette.
				memcpy(s_hudPalette, s_hudWorkPalette, HUD_COLORS_COUNT * 3);
				// Copy hud palette into the base palette, this will update next time render_setPalette() is called.
				hud_updateBasePalette(s_hudPalette, HUD_COLORS_START, HUD_COLORS_COUNT);
				// Copy the final 8 HUD colors to the loaded palette.
				memcpy(s_levelPalette + HUD_COLORS_START * 3, s_hudPalette, HUD_COLORS_COUNT * 3);

				s_forceHudPaletteUpdate = JFALSE;
			}

			s32 ammo0 = -1;
			s32 ammo1 = -1;
			PlayerWeapon* weapon = s_curPlayerWeapon;
			if (weapon)
			{
				if (weapon->ammo)
				{
					ammo0 = *weapon->ammo;
				}
				if (weapon->secondaryAmmo)
				{
					ammo1 = *weapon->secondaryAmmo;
				}
			}
						
			if (ammo0 != s_prevPrimaryAmmo || ammo1 != s_prevSecondaryAmmo || s_superChargeHud != s_prevSuperchageHud)
			{
				char str[32];
				if (ammo0 < 0)
				{
					strcpy(str, ":::");
				}
				else
				{
					sprintf(str, "%03d", ammo0);
				}
				hud_drawString(s_cachedHudRight, s_superChargeHud ? s_hudSuperAmmoFont : s_hudMainAmmoFont, 12, 21, str);

				if (ammo1 < 0)
				{
					strcpy(str, "::");
				}
				else
				{
					sprintf(str, "%02d", ammo1);
				}
				hud_drawString(s_cachedHudRight, s_hudShieldFont, 25, 12, str);

				s_rightHudShow = 4;
				s_prevPrimaryAmmo = ammo0;
				s_prevSecondaryAmmo = ammo1;
				s_prevSuperchageHud = s_superChargeHud;
			}

			if (s_rightHudShow || s_screenRect.bot >= 160)
			{
				if (s_rightHudVertAnim > s_rightHudVertTarget)
				{
					s_rightHudVertAnim--;
				}
				else if (s_rightHudVertAnim < s_rightHudVertTarget)
				{
					s_rightHudVertAnim++;
					s_rightHudMove = 4;
				}
				else if (s_rightHudShow)
				{
					s_rightHudShow--;
				}
			}
			if (s_leftHudShow || s_screenRect.bot >= 160)
			{
				if (s_leftHudVertAnim > s_leftHudVertTarget)
				{
					s_leftHudVertAnim--;
					s_leftHudMove = 4;
				}
				else if (s_leftHudVertAnim < s_leftHudVertTarget)
				{
					s_leftHudVertAnim++;
					s_leftHudMove = 4;
				}
				else if (s_leftHudShow)
				{
					s_leftHudShow--;
				}
			}
		}
		assert(s_rightHudVertAnim >= 0 && s_rightHudVertAnim < 4);
		hud_drawElementToScreen(s_cachedHudRight, &s_hudTextScreenRect, 260, c_hudVertAnimTable[s_rightHudVertAnim], framebuffer);
		hud_drawElementToScreen(s_cachedHudLeft,  &s_hudTextScreenRect,   0, c_hudVertAnimTable[s_leftHudVertAnim],  framebuffer);
	}

	///////////////////////////////////////////
	// Internal Implementation
	///////////////////////////////////////////
	TextureData* hud_loadTexture(const char* texFile)
	{
		FilePath filePath;
		if (TFE_Paths::getFilePath(texFile, &filePath))
		{
			return bitmap_load(&filePath, 0);
		}
		TFE_System::logWrite(LOG_ERROR, "HUD", "Cannot load texture '%s'", texFile);
		return nullptr;
	}

	Font* hud_loadFont(const char* fontFile)
	{
		FilePath filePath;
		if (TFE_Paths::getFilePath(fontFile, &filePath))
		{
			return font_load(&filePath);
		}
		TFE_System::logWrite(LOG_ERROR, "HUD", "Cannot load font '%s'", fontFile);
		return nullptr;
	}

	GameMessage* hud_getMessage(GameMessages* messages, s32 msgId)
	{
		s32 count = messages->count;
		GameMessage* msg = messages->msgList;

		for (s32 i = 0; i < count; i++, msg++)
		{
			if (msgId == msg->id)
			{
				return msg;
			}
		}
		return nullptr;
	}
		
	void copyIntoPalette(u8* dst, u8* src, s32 count, s32 mode)
	{
		memcpy(dst, src, count * 3);
	}

	void getCameraXZ(fixed16_16* x, fixed16_16* z)
	{
		if (s_drawAutomap)
		{
			*x = s_mapX0;
			*z = s_mapZ0;
		}
		else
		{
			*x = s_eyePos.x;
			*z = s_eyePos.z;
		}
	}

	void displayHudMessage(Font* font, DrawRect* rect, s32 x, s32 y, char* msg, u8* framebuffer)
	{
		if (!font || !rect || !framebuffer) { return; }

		s32 xi = x;
		s32 x0 = x;
		for (char c = *msg; c != 0;)
		{
			if (c == '\n')
			{
				xi = x0;
				y += font->height + font->vertSpacing;
			}
			else if (c == ' ')
			{
				xi += font->width;
			}
			else if (c >= font->minChar && c <= font->maxChar)
			{
				s32 charIndex = c - font->minChar;
				TextureData* glyph = &font->glyphs[charIndex];
				blitTextureToScreen(glyph, rect, xi, y, framebuffer);

				xi += font->horzSpacing + glyph->width;
			}
			msg++;
			c = *msg;
		}
	}

	void hud_drawString(OffScreenBuffer* elem, Font* font, s32 x0, s32 y0, const char* str)
	{
		if (!font) { return; }

		s32 x = x0;
		s32 y = y0;
		for (char c = *str; c != 0; )
		{
			if (c == '\n')
			{
				y += font->height + font->vertSpacing;
				x = x0;
			}
			else
			{
				if (c == ' ')
				{
					x += font->width;
				}
				else if (c >= font->minChar && c <= font->maxChar)
				{
					s32 charIndex = c - font->minChar;
					s32 offset = charIndex * 28;

					TextureData* glyph = &font->glyphs[charIndex];
					offscreenBuffer_drawTexture(elem, glyph, x, y);
					x += glyph->width + font->horzSpacing;
				}
			}
			str++;
			c = *str;
		}
	}

	void hud_drawElementToScreen(OffScreenBuffer* elem, ScreenRect* rect, s32 x0, s32 y0, u8* framebuffer)
	{
		s32 x1 = x0 + elem->width - 1;
		u8* image = elem->image;
		s32 y1 = y0 + elem->height - 1;
		if (x0 > rect->right || x1 < rect->left || y0 > rect->bot || y1 < rect->top)
		{
			return;
		}
		if (y0 < rect->top)
		{
			image += (rect->top - y0) * elem->width;
			y0 = rect->top;
		}
		if (y1 > rect->bot)
		{
			y1 = rect->bot;
		}
		if (x0 < rect->left)
		{
			image += rect->left - x0;
			x0 = rect->left;
		}
		if (x1 > rect->right)
		{
			x1 = rect->right;
		}
		s32 yOffset = y0 * 320;
		if (elem->flags & OBF_TRANS)
		{
			for (s32 x = x0; x <= x1; x++)
			{
				u8* output = framebuffer + x + yOffset;
				u8* imageSrc = image + x - x0;
				for (s32 y = y0; y <= y1; y++, output += 320, imageSrc += elem->width)
				{
					u8 color = *imageSrc;
					if (color)
					{
						*output = color;
					}
				}
			}
		}
		else
		{
			for (s32 x = x0; x <= x1; x++)
			{
				u8* output = framebuffer + x + yOffset;
				u8* imageSrc = image + x - x0;
				for (s32 y = y0; y <= y1; y++, output += 320, imageSrc += elem->width)
				{
					*output = *imageSrc;
				}
			}
		}
	}
}  // TFE_DarkForces