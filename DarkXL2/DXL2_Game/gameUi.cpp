#include "gameLoop.h"
#include "gameUi.h"
#include "player.h"
#include <DXL2_System/system.h>
#include <DXL2_Renderer/renderer.h>
#include <DXL2_FileSystem/paths.h>
#include <DXL2_Asset/textureAsset.h>
#include <DXL2_Asset/fontAsset.h>
#include <DXL2_Asset/paletteAsset.h>
#include <DXL2_Input/input.h>
#include <DXL2_RenderBackend/renderBackend.h>
#include <assert.h>
#include <vector>
#include <algorithm>

namespace DXL2_GameUi
{
	static DXL2_Renderer* s_renderer;
	static s32 s_scaleX = 1, s_scaleY = 1;
	static s32 s_uiScale = 256;
	static Palette256* s_menuPal;
	static Palette256 s_prevPal;
	static Texture* s_menu;
	static Texture* s_cursor;
	static Vec2i s_cursorPos;
	static Vec2i s_cursorPosAccum;

	static bool s_escMenuOpen = false;

	void init(DXL2_Renderer* renderer)
	{
		s_renderer = renderer;
		u32 width, height;
		s_renderer->getResolution(&width, &height);

		s_scaleX = 256 * s_uiScale * width  / (320 * 256);
		s_scaleY = 256 * s_uiScale * height / (200 * 256);

		s_menuPal = DXL2_Palette::get256("SECBASE.PAL");// PalFromPltt("menu.pltt", "LFD/MENU.LFD");
		s_menu    = DXL2_Texture::getFromAnim("escmenu.anim", "LFD/MENU.LFD");
		s_cursor  = DXL2_Texture::getFromDelt("cursor.delt",  "LFD/MENU.LFD");
	}

	void openEscMenu()
	{
		s_escMenuOpen = true;
		s_prevPal = s_renderer->getPalette();
		s_renderer->setPalette(s_menuPal);
		DXL2_RenderCommon::enableGrayScale(true);
		s_cursorPos = { 0 };
		s_cursorPosAccum = { 0 };
	}

	bool isEscMenuOpen()
	{
		return s_escMenuOpen;
	}

	void update(Player* player)
	{
		if (DXL2_Input::keyPressed(KEY_ESCAPE) && s_escMenuOpen)
		{
			s_escMenuOpen = false;
			DXL2_RenderCommon::enableGrayScale(false);
			s_renderer->setPalette(&s_prevPal);
		}

		if (s_escMenuOpen)
		{
			DisplayInfo displayInfo;
			DXL2_RenderBackend::getDisplayInfo(&displayInfo);

			u32 width, height;
			s_renderer->getResolution(&width, &height);

			s32 dx, dy;
			DXL2_Input::getMouseMove(&dx, &dy);

			s_cursorPosAccum.x += dx;
			s_cursorPosAccum.z += dy;
			s_cursorPos.x = std::max(0, std::min(s_cursorPosAccum.x * (s32)height / (s32)displayInfo.height, (s32)width - 1));
			s_cursorPos.z = std::max(0, std::min(s_cursorPosAccum.z * (s32)height / (s32)displayInfo.height, (s32)height - 1));
		}
	}

	void draw(Player* player)
	{
		if (s_escMenuOpen)
		{
			s_renderer->applyColorEffect();
			// TODO: Figure out the real formula for this - this works for now though.
			const s32 x0 = ((s_menu->frames[0].width  >> 2) - s_menu->frames[0].offsetX) * s_scaleX >> 8;
			const s32 y0 = ((s_menu->frames[0].height >> 2) - s_menu->frames[0].offsetY - 8) * s_scaleY >> 8;

			s_renderer->blitImage(&s_menu->frames[0], x0, y0, s_scaleX, s_scaleY, 31, TEX_LAYOUT_HORZ);

			// draw the mouse cursor.
			s_renderer->blitImage(&s_cursor->frames[0], s_cursorPos.x, s_cursorPos.z, s_scaleX, s_scaleY, 31, TEX_LAYOUT_HORZ);
		}
	}
}
