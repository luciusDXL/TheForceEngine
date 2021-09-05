#include "hud.h"
#include "time.h"
#include "automap.h"
#include "gameMessage.h"
#include "mission.h"
#include "util.h"
#include "player.h"
#include <TFE_FileSystem/paths.h>
#include <TFE_System/system.h>
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

	static u8 s_basePalette[256 * 3];
	static u8 s_tempBuffer[256 * 3];
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

	static DrawRect s_hudTextDrawRect = { 0, 0, 319, 199 };

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
			s_leftHudVertTarget = 0;
			s_rightHudVertTarget = 0;

			s_leftHudShow = 4;
			s_rightHudShow = 4;

			s_leftHudMove = 4;
			s_rightHudMove = 4;
		}
	}

	void hud_startup()
	{
		hud_initAnimation();
		hud_setupToggleAnim1(JTRUE);
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

		if (s_showData)
		{
			fixed16_16 x, z;
			getCameraXZ(&x, &z);

			char dataStr[32];
			sprintf(dataStr, "X:%04d Y:%.1f Z:%04d H:%.1f S:%d%%", floor16(x), -fixed16ToFloat(s_playerEye->posWS.y), floor16(z), fixed16ToFloat(s_playerEye->worldHeight), s_secretsPercent);
			displayHudMessage(s_hudFont, &s_hudTextDrawRect, 164, 10, dataStr, framebuffer);
			// s_screenDirtyRight[s_curFrameBufferIdx] = JTRUE;
		}
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
}  // TFE_DarkForces