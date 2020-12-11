#include "gameLoop.h"
#include "player.h"
#include <TFE_System/system.h>
#include <TFE_Settings/settings.h>
#include <TFE_Renderer/renderer.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FrontEndUI/console.h>
#include <TFE_Asset/textureAsset.h>
#include <TFE_Asset/fontAsset.h>
#include <assert.h>
#include <vector>
#include <algorithm>

namespace TFE_GameHud
{
	#define MSG_SCREEN_TIME 5.0f
	#define MAX_MSG_LEN 1024
	
	enum HudElement
	{
		// Image Elements
		HUD_STATUS_LEFT = 0,
		HUD_STATUS_RIGHT,
		HUD_LIGHT_ON,
		HUD_LIGHT_OFF,
		// Text Elements
		HUD_SHIELD_NUM,
		HUD_HEALTH_NUM,
		HUD_LIVES_NUM,
		HUD_AMMO_NUM,
		HUD_DT_NUM,
		HUD_COUNT
	};

	enum HudElemType
	{
		HUD_ELEM_IMAGE = 0,
		HUD_ELEM_TEXT,
		HUD_ELEM_COUNT
	};

	enum HudAlignmentHorz
	{
		HALIGN_LEFT = 0,
		HALIGN_RIGHT,
		HALIGN_CENTER,
		HALIGN_COUNT
	};

	enum HudAlignmentVert
	{
		VALIGN_TOP = 0,
		VALIGN_BOTTOM,
		VALIGN_CENTER,
		VALIGN_COUNT
	};

	struct HudElementInfo
	{
		const char* name;
		HudElemType type;
		HudAlignmentHorz alignHorz;
		HudAlignmentVert alignVert;
		s32 x, y;
	};

	struct HudAsset
	{
		union
		{
			Texture* image;
			Font* font;
		};
	};

	static TFE_Renderer* s_renderer;
	static const HudElementInfo c_hudElements[HUD_COUNT]=
	{
		{ "STATUSLF.BM", HUD_ELEM_IMAGE, HALIGN_LEFT,  VALIGN_BOTTOM,   0, -40 },	// HUD_STATUS_LEFT
		{ "STATUSRT.BM", HUD_ELEM_IMAGE, HALIGN_RIGHT, VALIGN_BOTTOM, -60, -40 },	// HUD_STATUS_RIGHT
		{ "LIGHTON.BM",  HUD_ELEM_IMAGE, HALIGN_RIGHT, VALIGN_BOTTOM, -41, -40 },	// HUD_LIGHT_ON
		{ "LIGHTOFF.BM", HUD_ELEM_IMAGE, HALIGN_RIGHT, VALIGN_BOTTOM, -41, -40 },	// HUD_LIGHT_OFF
		{ "ARMNUM.FNT",  HUD_ELEM_TEXT,  HALIGN_LEFT,  VALIGN_BOTTOM,  15, -14 },	// HUD_SHIELD_NUM,
		{ "HELNUM.FNT",  HUD_ELEM_TEXT,  HALIGN_LEFT,  VALIGN_BOTTOM,  33, -14 },	// HUD_HEALTH_NUM,
		{ "HELNUM.FNT",  HUD_ELEM_TEXT,  HALIGN_LEFT,  VALIGN_BOTTOM,  52, -14 },	// HUD_LIVES_NUM,
		{ "AMONUM.FNT",  HUD_ELEM_TEXT,  HALIGN_RIGHT, VALIGN_BOTTOM, -48, -19 },	// HUD_AMMO_NUM,
		{ "ARMNUM.FNT",  HUD_ELEM_TEXT,  HALIGN_RIGHT, VALIGN_BOTTOM, -35, -28 },	// HUD_DT_NUM,
	};
	static Vec2i s_hudPosition[HUD_COUNT];
	static HudAsset s_hudElementAssets[HUD_COUNT];
	static s32 s_scaleX = 1, s_scaleY = 1;
	static s32 s_offsetX = 0;
	static s32 s_uiScale = 256;
	static bool s_showPos = false;

	const char* c_msgFont = "GLOWING.FNT";
	static Font* s_msgFont = nullptr;

	static char s_curMsg[MAX_MSG_LEN];
	static f32 s_msgTime = 0.0f;

	enum HudPaletteIds
	{
		HUD_PAL_SHIELD_TOP = 24,
		HUD_PAL_SHIELD_BOT = 25,
		HUD_PAL_HEALTH = 26,
		HUD_PAL_BATTERY = 27,
		HUD_PAL_BATTERY_COUNT = 5,
	};
	char s_hudText[5][32];

	void drawElement(HudElement element, s32 scaleX, s32 scaleY);
	void computeHudPositions(s32 scaleX, s32 scaleY);

	s32 getHudXPosition(s32 scaleX, s32 x, HudAlignmentHorz align, s32 width, s32 xOffset, s32 screenXOffset)
	{
		const s32 xScaled = (x * scaleX) >> 8;

		if (align == HALIGN_LEFT) { return xScaled + xOffset + screenXOffset; }
		else if (align == HALIGN_RIGHT) { return width + xScaled - xOffset + screenXOffset; }
		// HALIGN_CENTER
		return (width / 2) + xScaled + screenXOffset;
	}

	s32 getHudYPosition(s32 scaleY, s32 y, HudAlignmentVert align, s32 height, s32 yOffset)
	{
		const s32 yScaled = (y * scaleY + 255) >> 8;

		if (align == VALIGN_TOP) { return yScaled + yOffset; }
		else if (align == VALIGN_BOTTOM) { return height + yScaled - yOffset; }
		// VALIGN_CENTER
		return (height / 2) + yScaled;
	}
				
	void init(TFE_Renderer* renderer)
	{
		CVAR_BOOL(s_showPos, "r_showPos", 0, "Show current player position.");

		s_renderer = renderer;
		const u32 width  = TFE_RenderBackend::getVirtualDisplayWidth2D();
		const u32 vwidth = TFE_RenderBackend::getVirtualDisplayWidth3D();
		const u32 height = TFE_RenderBackend::getVirtualDisplayHeight();

		s_scaleX = 256 * s_uiScale * width / (320 * 256);
		s_scaleY = 256 * s_uiScale * height / (200 * 256);
		s_offsetX = TFE_RenderBackend::getVirtualDisplayOffset2D();

		// Load the HUD textures & set element positions.
		for (u32 i = 0; i < HUD_COUNT; i++)
		{
			if (c_hudElements[i].type == HUD_ELEM_IMAGE)
			{
				s_hudElementAssets[i].image = TFE_Texture::get(c_hudElements[i].name);
			}
			else
			{
				s_hudElementAssets[i].font = TFE_Font::get(c_hudElements[i].name);
			}
		}
		computeHudPositions(s_scaleX, s_scaleY);

		// Fixup the right side to remove the backed in text.
		s32 wRight = s_hudElementAssets[HUD_STATUS_RIGHT].image->frames[0].width;
		s32 hRight = s_hudElementAssets[HUD_STATUS_RIGHT].image->frames[0].height;
		u8* image = s_hudElementAssets[HUD_STATUS_RIGHT].image->frames[0].image;
		for (s32 x = 0; x < 13; x++)
		{
			u8* imageX = &image[(x + 23) * hRight + 21];
			for (s32 z = 0; z < 7; z++)
			{
				imageX[z] = 79;
			}
		}

		s_msgFont = TFE_Font::get(c_msgFont);
	}

	void computeHudPositions(s32 scaleX, s32 scaleY)
	{
		const u32 width = TFE_RenderBackend::getVirtualDisplayWidth2D();
		const u32 vwidth = TFE_RenderBackend::getVirtualDisplayWidth3D();
		const u32 height = TFE_RenderBackend::getVirtualDisplayHeight();

		const TFE_Settings_Hud* hudSettings = TFE_Settings::getHudSettings();
		const u32 hudWidth = hudSettings->hudPos == TFE_HUDPOS_EDGE ? vwidth : width;
		const s32 screenOffset = hudSettings->hudPos == TFE_HUDPOS_EDGE ? 0 : TFE_RenderBackend::getVirtualDisplayOffset2D();

		// Load the HUD textures & set element positions.
		for (u32 i = 0; i < HUD_COUNT; i++)
		{
			s_hudPosition[i].x = getHudXPosition(scaleX, c_hudElements[i].x, c_hudElements[i].alignHorz, hudWidth, hudSettings->pixelOffset[0], screenOffset);
			s_hudPosition[i].z = getHudYPosition(scaleY, c_hudElements[i].y, c_hudElements[i].alignVert, height, hudSettings->pixelOffset[1]);
		}
	}
		
	void setMessage(const char* msg)
	{
		if (!msg) { return; }
		strcpy(s_curMsg, msg);
		s_msgTime = MSG_SCREEN_TIME;
	}

	void clearMessage()
	{
		s_msgTime = 0.0f;
	}

	void update(Player* player)
	{
		sprintf(s_hudText[0], "%03d", player->m_shields);
		sprintf(s_hudText[1], "%03d", player->m_health);
		sprintf(s_hudText[2], "%d", player->m_lives);
		sprintf(s_hudText[3], "%03d", player->m_ammo);
		if (player->m_dtCount < 0)
			sprintf(s_hudText[4], "::");	// This will show up as "--" in the font.
		else
			sprintf(s_hudText[4], "%02d", player->m_dtCount);

		s32 topShield = player->m_shields >= 200 ? 58 : std::max(player->m_shields - 100, 0) / 2 + 8;
		s32 botShield = player->m_shields >= 100 ? 58 : std::min(player->m_shields, 100) / 2 + 8;
		s_renderer->setPaletteColor(HUD_PAL_SHIELD_TOP, PACK_RGB_666_888(0, topShield, 1));
		s_renderer->setPaletteColor(HUD_PAL_SHIELD_BOT, PACK_RGB_666_888(0, botShield, 1));

		s32 healthR = player->m_health >= 100 ? 58 : player->m_health / 2 + 8;
		s32 healthG = player->m_health >= 100 ? 15 : player->m_health * 15 / 100;
		s_renderer->setPaletteColor(HUD_PAL_HEALTH, PACK_RGB_666_888(healthR, healthG, 1));

		// Fill the battery indicators for now.
		for (u32 i = 0; i < HUD_PAL_BATTERY_COUNT; i++)
		{
			s_renderer->setPaletteColor(HUD_PAL_BATTERY + i, 0xff04eb00);
		}
	}

	void draw(Player* player)
	{
		const TFE_Settings_Hud* hudSettings = TFE_Settings::getHudSettings();
		s32 scaleX = s_scaleX;
		s32 scaleY = s_scaleY;
		if (hudSettings->hudScale == TFE_HUDSCALE_SCALED)
		{
			scaleX = s32(hudSettings->scale * 256.0f);
			scaleY = scaleX;
		}

		computeHudPositions(scaleX, scaleY);
				
		drawElement(HUD_STATUS_LEFT, scaleX, scaleY);
		drawElement(HUD_STATUS_RIGHT, scaleX, scaleY);

		// Headlamp light.
		if (player->m_headlampOn)
			drawElement(HUD_LIGHT_ON, scaleX, scaleY);
		else
			drawElement(HUD_LIGHT_OFF, scaleX, scaleY);

		// Text Elements
		for (u32 e = HUD_SHIELD_NUM; e < HUD_COUNT; e++)
		{
			drawElement(HudElement(e), scaleX, scaleY);
		}

		if (s_msgTime > 0.0f)
		{
			s_renderer->print(s_curMsg, s_msgFont, 6, 6, scaleX, scaleY);
			s_msgTime -= (f32)TFE_System::getDeltaTime();
		}
		else if (s_showPos)
		{
			const Vec3f* camPos = TFE_GameLoop::getCameraPos();

			char posMsg[256];
			sprintf(posMsg, "%d %0.2f %0.2f  %0.2f %0.2f", player->m_sectorId, camPos->x, camPos->z, player->m_yaw * 180.0f / PI, player->m_pitch * 180.0f / PI);
			s_renderer->print(posMsg, s_msgFont, 6, 6, scaleX, scaleY, 0, true);
		}
	}


	void drawElement(HudElement element, s32 scaleX, s32 scaleY)
	{
		if (c_hudElements[element].type == HUD_ELEM_IMAGE)
		{
			s_renderer->blitImage(&s_hudElementAssets[element].image->frames[0], s_hudPosition[element].x, s_hudPosition[element].z, scaleX, scaleY);
		}
		else
		{
			s_renderer->print(s_hudText[element-HUD_SHIELD_NUM], s_hudElementAssets[element].font, s_hudPosition[element].x, s_hudPosition[element].z, scaleX, scaleY);
		}
	}
}
