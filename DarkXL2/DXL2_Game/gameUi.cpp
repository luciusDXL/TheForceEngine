#include "gameLoop.h"
#include "gameUi.h"
#include "gameHud.h"
#include "editBox.h"
#include "player.h"
#include <DXL2_System/system.h>
#include <DXL2_Renderer/renderer.h>
#include <DXL2_FileSystem/paths.h>
#include <DXL2_Asset/textureAsset.h>
#include <DXL2_Asset/fontAsset.h>
#include <DXL2_Asset/paletteAsset.h>
#include <DXL2_Asset/colormapAsset.h>
#include <DXL2_Asset/levelList.h>
#include <DXL2_Input/input.h>
#include <DXL2_RenderBackend/renderBackend.h>
#include <assert.h>
#include <vector>
#include <algorithm>

namespace DXL2_GameUi
{
	///////////////////////////////////////
	// UI Definitions
	// TODO: Move these to game data.
	///////////////////////////////////////
	enum EscapeButtons
	{
		ESC_ABORT,
		ESC_CONFIG,
		ESC_QUIT,
		ESC_RETURN,
		ESC_COUNT
	};
	static const Vec2i c_escButtons[ESC_COUNT] =
	{
		{64, 35},	// ESC_ABORT
		{64, 55},	// ESC_CONFIG
		{64, 75},	// ESC_QUIT
		{64, 99},	// ESC_RETURN
	};
	static const Vec2i c_escButtonDim = { 96, 16 };

	enum AgentMenuButtons
	{
		AGENT_NEW,
		AGENT_REMOVE,
		AGENT_EXIT,
		AGENT_BEGIN,
		AGENT_COUNT
	};
	static const Vec2i c_agentButtons[AGENT_COUNT] =
	{
		{21, 165},	// AGENT_NEW
		{90, 165},	// AGENT_REMOVE
		{164, 165},	// AGENT_EXIT
		{239, 165},	// AGENT_BEGIN
	};
	static const Vec2i c_agentButtonDim = { 60, 25 };

	enum NewAgentDlg
	{
		NEW_AGENT_NO,
		NEW_AGENT_YES,
		NEW_AGENT_COUNT
	};
	static const Vec2i c_newAgentButtons[NEW_AGENT_COUNT] =
	{
		{167, 107},	// NEW_AGENT_NO
		{206, 107},	// NEW_AGENT_YES
	};
	static const Vec2i c_newAgentButtonDim = { 28, 15 };

	struct GameMenus
	{
		Palette256* pal  = nullptr;
		ColorMap* cmap   = nullptr;
		Texture* escMenu = nullptr;
		Texture* cursor  = nullptr;
		// state

		bool load()
		{
			pal     = DXL2_Palette::get256("SECBASE.PAL");
			cmap    = DXL2_ColorMap::get("SECBASE.CMP");
			escMenu = DXL2_Texture::getFromAnim("escmenu.anim", "LFD/MENU.LFD");
			cursor  = DXL2_Texture::getFromDelt("cursor.delt", "LFD/MENU.LFD");
			return (pal && cmap && escMenu && cursor);
		}

		void unload()
		{
			DXL2_Texture::free(escMenu);
			DXL2_Texture::free(cursor);
			escMenu = nullptr;
			cursor = nullptr;
		}
	};

	struct AgentMenu
	{
		Palette256* pal;
		Texture* agentMenu;
		Texture* agentDlg;
		Texture* cursor;
		Font* sysFont;
		// state

		bool load()
		{
			pal       = DXL2_Palette::getPalFromPltt("agentmnu.pltt", "LFD/AGENTMNU.LFD");
			agentMenu = DXL2_Texture::getFromAnim("agentmnu.anim", "LFD/AGENTMNU.LFD");
			agentDlg  = DXL2_Texture::getFromAnim("agentdlg.anim", "LFD/AGENTMNU.LFD");
			cursor    = DXL2_Texture::getFromDelt("cursor.delt", "LFD/AGENTMNU.LFD");
			sysFont   = DXL2_Font::createSystemFont6x8();
			return (pal && agentMenu && agentDlg && cursor);
		}

		void unload()
		{
			DXL2_Texture::free(agentMenu);
			DXL2_Texture::free(agentDlg);
			DXL2_Texture::free(cursor);
			agentMenu = nullptr;
			agentDlg = nullptr;
			cursor = nullptr;
		}
	};

	static s32 s_buttonPressed = -1;
	static bool s_buttonHover = false;
	static bool s_drawGame = true;

	static DXL2_Renderer* s_renderer;
	static s32 s_scaleX = 1, s_scaleY = 1;
	static s32 s_uiScale = 256;
	static Palette256 s_prevPal;
	static const ColorMap* s_prevCMap;
	// Visual Cursor position
	static Vec2i s_cursorPos = { 0 };
	// Ui cursor position (in the original resolution)
	static Vec2i s_virtualCursorPos = { 0 };
	// Accumlation, so the movement speed can be consistent across resolutions.
	static Vec2i s_cursorPosAccum = { 0 };

	static GameMenus s_gameMenus = {};
	static AgentMenu s_agentMenu = {};

	static bool s_escMenuOpen = false;
	static bool s_agentMenuOpen = false;
	static bool s_nextMission = false;

	static s32 s_selectedLevel = 0;
	static s32 s_selectedDifficulty = 1;

	void drawEscapeMenu();
	void drawAgentMenu();
	GameUiResult updateEscapeMenu();
	GameUiResult updateAgentMenu();
	void loadAgents();

	void init(DXL2_Renderer* renderer)
	{
		s_renderer = renderer;
		u32 width, height;
		s_renderer->getResolution(&width, &height);

		s_scaleX = 256 * s_uiScale * width  / (320 * 256);
		s_scaleY = 256 * s_uiScale * height / (200 * 256);

		// Preload everything - the original games would only load what was needed at the moment to save memory.
		// But its pretty safe to just load it all at once for now.
		s_gameMenus.load();
		s_agentMenu.load();

		DXL2_LevelList::load();
		loadAgents();
	}

	void openAgentMenu()
	{
		s_drawGame = false;
		s_escMenuOpen = false;
		s_agentMenuOpen = true;
		s_renderer->setPalette(s_agentMenu.pal);

		s_buttonPressed = -1;
		s_buttonHover = false;
	}

	void closeAgentMenu()
	{
		s_agentMenuOpen = false;
	}

	void openEscMenu()
	{
		s_escMenuOpen = true;
		s_prevPal = s_renderer->getPalette();
		s_prevCMap = s_renderer->getColorMap();
		s_renderer->setPalette(s_gameMenus.pal);
		s_renderer->setColorMap(s_gameMenus.cmap);
		DXL2_RenderCommon::enableGrayScale(true);

		s_buttonPressed = -1;
		s_buttonHover = false;
	}

	bool isEscMenuOpen()
	{
		return s_escMenuOpen;
	}

	void toggleNextMission(bool enable)
	{
		s_nextMission = enable;
	}

	void closeEscMenu()
	{
		s_escMenuOpen = false;
		DXL2_RenderCommon::enableGrayScale(false);
		s_renderer->setPalette(&s_prevPal);
		s_renderer->setColorMap(s_prevCMap);
	}
				
	GameUiResult update(Player* player)
	{
		GameUiResult result = GAME_CONTINUE;
		s_drawGame = !s_agentMenuOpen;
		// Mouse Handling
		DisplayInfo displayInfo;
		DXL2_RenderBackend::getDisplayInfo(&displayInfo);

		u32 width, height;
		s_renderer->getResolution(&width, &height);

		// Only update the mouse when a menu or dialog is open.
		if (s_escMenuOpen || s_agentMenuOpen)
		{
			s32 dx, dy;
			DXL2_Input::getMouseMove(&dx, &dy);

			s_cursorPosAccum.x += dx;
			s_cursorPosAccum.z += dy;
			s_cursorPos.x = std::max(0, std::min(s_cursorPosAccum.x * (s32)height / (s32)displayInfo.height, (s32)width - 1));
			s_cursorPos.z = std::max(0, std::min(s_cursorPosAccum.z * (s32)height / (s32)displayInfo.height, (s32)height - 1));

			// Calculate the virtual (UI) cursor position.
			s_virtualCursorPos.x = s_cursorPos.x * 256 / s_scaleX;
			s_virtualCursorPos.z = s_cursorPos.z * 256 / s_scaleY;
		}

		// Menus
		if (DXL2_Input::keyPressed(KEY_ESCAPE) && s_escMenuOpen)
		{
			closeEscMenu();
		}

		if (s_agentMenuOpen)
		{
			result = updateAgentMenu();
		}
		else if (s_escMenuOpen)
		{
			result = updateEscapeMenu();
		}

		return result;
	}

	// Returns true if the game view should be drawn.
	// This will be false for fullscreen UI.
	bool shouldDrawGame()
	{
		return s_drawGame;
	}

	// Returns true if the game view should be updated.
	// This will be false for modal UI.
	bool shouldUpdateGame()
	{
		return !s_escMenuOpen && !s_agentMenuOpen;
	}
		
	// Get the level selected in the UI.
	s32 getSelectedLevel(s32* difficulty)
	{
		*difficulty = s_selectedDifficulty;
		return s_selectedLevel;
	}

	void draw(Player* player)
	{
		if (s_agentMenuOpen)
		{
			drawAgentMenu();
		}
		else if (s_escMenuOpen)
		{
			drawEscapeMenu();
		}
	}
		
	//////////////////////////////////////////////////////////////////
	// Internal
	//////////////////////////////////////////////////////////////////
	void drawEscapeMenu()
	{
		s_renderer->applyColorEffect();
		// TODO: Figure out the real formula for this - this works for now though.
		const s32 x0 = ((s_gameMenus.escMenu->frames[0].width >> 2) - s_gameMenus.escMenu->frames[0].offsetX) * s_scaleX >> 8;
		const s32 y0 = ((s_gameMenus.escMenu->frames[0].height >> 2) - s_gameMenus.escMenu->frames[0].offsetY - 8) * s_scaleY >> 8;

		// Draw the menu
		s_renderer->blitImage(&s_gameMenus.escMenu->frames[0], x0, y0, s_scaleX, s_scaleY, 31, TEX_LAYOUT_HORZ);

		// Draw the next mission button if the mission has been completed.
		if (s_nextMission)
		{
			if (s_buttonPressed == ESC_ABORT && s_buttonHover)
			{
				s_renderer->blitImage(&s_gameMenus.escMenu->frames[3], x0, y0, s_scaleX, s_scaleY, 31, TEX_LAYOUT_HORZ);
			}
			else
			{
				s_renderer->blitImage(&s_gameMenus.escMenu->frames[4], x0, y0, s_scaleX, s_scaleY, 31, TEX_LAYOUT_HORZ);
			}
		}
		if ((s_buttonPressed > ESC_ABORT || (s_buttonPressed == ESC_ABORT && !s_nextMission)) && s_buttonHover)
		{
			// Draw the highlight button
			const s32 highlightIndices[] = { 1, 7, 9, 5 };
			s_renderer->blitImage(&s_gameMenus.escMenu->frames[highlightIndices[s_buttonPressed]], x0, y0, s_scaleX, s_scaleY, 31, TEX_LAYOUT_HORZ);
		}

		// draw the mouse cursor.
		s_renderer->blitImage(&s_gameMenus.cursor->frames[0], s_cursorPos.x, s_cursorPos.z, s_scaleX, s_scaleY, 31, TEX_LAYOUT_HORZ);
	}

	GameUiResult updateEscapeMenu()
	{
		GameUiResult result = GAME_CONTINUE;
		
		if (DXL2_Input::mousePressed(MBUTTON_LEFT))
		{
			const s32 x = s_virtualCursorPos.x;
			const s32 z = s_virtualCursorPos.z;
			s_buttonPressed = -1;
			for (u32 i = 0; i < ESC_COUNT; i++)
			{
				if (x >= c_escButtons[i].x && x < c_escButtons[i].x + c_escButtonDim.x &&
					z >= c_escButtons[i].z && z < c_escButtons[i].z + c_escButtonDim.z)
				{
					s_buttonPressed = s32(i);
					s_buttonHover = true;
					break;
				}
			}
		}
		else if (DXL2_Input::mouseDown(MBUTTON_LEFT) && s_buttonPressed >= 0)
		{
			// Verify that the mouse is still over the button.
			const s32 x = s_virtualCursorPos.x;
			const s32 z = s_virtualCursorPos.z;
			if (x >= c_escButtons[s_buttonPressed].x && x < c_escButtons[s_buttonPressed].x + c_escButtonDim.x &&
				z >= c_escButtons[s_buttonPressed].z && z < c_escButtons[s_buttonPressed].z + c_escButtonDim.z)
			{
				s_buttonHover = true;
			}
			else
			{
				s_buttonHover = false;
			}
		}
		else
		{
			// Activate button 's_escButtonPressed'
			if (s_buttonPressed >= ESC_ABORT && s_buttonHover)
			{
				switch (s_buttonPressed)
				{
				case ESC_ABORT:
					result = s_nextMission ? GAME_NEXT_LEVEL : GAME_ABORT;
					closeEscMenu();
					break;
				case ESC_CONFIG:
					// TODO
					break;
				case ESC_QUIT:
					result = GAME_QUIT;
					closeEscMenu();
					break;
				case ESC_RETURN:
					closeEscMenu();
					break;
				};
			}

			// Reset.
			s_buttonPressed = -1;
			s_buttonHover = false;
		}
		return result;
	}

	struct Agent
	{
		char name[32];
		u8 completeDifficulty[14];
		u8 nextMission;	//14 = game complete.
		u8 selectedMission;
	};

	static Agent s_agent[14];
	static u32 s_agentCount;
	static char s_newAgentName[32];

	void loadAgents()
	{
		// Hardcode test agents.
		s_agentCount = 5;

		strcpy(s_agent[0].name, "DoomedXL");
		for (u32 i = 0; i < 3; i++) { s_agent[0].completeDifficulty[i] = 0; }
		s_agent[0].nextMission = 3;
		s_agent[0].selectedMission = 2;

		strcpy(s_agent[1].name, "Jeremy");
		for (u32 i = 0; i < 3; i++) { s_agent[1].completeDifficulty[i] = 0; }
		for (u32 i = 3; i < 7; i++) { s_agent[1].completeDifficulty[i] = 1; }
		for (u32 i = 8; i < 14; i++) { s_agent[1].completeDifficulty[i] = 2; }
		s_agent[1].nextMission = 13;
		s_agent[1].selectedMission = 3;
		
		strcpy(s_agent[2].name, "Kyle");
		s_agent[2].nextMission = 0;
		s_agent[2].selectedMission = 0;

		strcpy(s_agent[3].name, "Playthrough");
		for (u32 i = 0; i < 5; i++) { s_agent[3].completeDifficulty[i] = 1; }
		s_agent[3].nextMission = 5;
		s_agent[3].selectedMission = 5;

		strcpy(s_agent[4].name, "Test");
		for (u32 i = 0; i < 8; i++) { s_agent[4].completeDifficulty[i] = 1; }
		s_agent[4].completeDifficulty[8] = 0;
		s_agent[4].nextMission = 9;
		s_agent[4].selectedMission = 8;
	}

	static s32 s_selectedAgent = 0;
	static s32 s_selectedMission = 0;
	static s32 s_editCursorFlicker = 0;
	static bool s_lastSelectedAgent = true;
	static bool s_newAgentDlg = false;
	static bool s_removeAgentDlg = false;
	static bool s_quitConfirmDlg = false;

	static EditBox s_editBox = {};

	void drawYesNoButtons(u32 indexNo, u32 indexYes)
	{
		if (s_buttonPressed == 0 && s_buttonHover)
		{
			s_renderer->blitImage(&s_agentMenu.agentDlg->frames[indexNo], 0, 0, s_scaleX, s_scaleY, 31, TEX_LAYOUT_HORZ);
		}
		else if (s_buttonPressed == 1 && s_buttonHover)
		{
			s_renderer->blitImage(&s_agentMenu.agentDlg->frames[indexYes], 0, 0, s_scaleX, s_scaleY, 31, TEX_LAYOUT_HORZ);
		}
	}

	void drawYesNoDlg(u32 baseFrame)
	{
		s_renderer->blitImage(&s_agentMenu.agentDlg->frames[baseFrame], 0, 0, s_scaleX, s_scaleY, 31, TEX_LAYOUT_HORZ);
		drawYesNoButtons(4, 6);
	}

	void drawNewAgentDlg()
	{
		// Draw the dialog
		s_renderer->blitImage(&s_agentMenu.agentDlg->frames[2], 0, 0, s_scaleX, s_scaleY, 31, TEX_LAYOUT_HORZ);
		drawEditBox(&s_editBox, s_agentMenu.sysFont, 106, 87, 216, 100, s_scaleX, s_scaleY, s_renderer);
		drawYesNoButtons(8, 10);
	}

	void drawAgentMenu()
	{
		// Clear part of the background.
		// TODO: Fade out the game view when existing, so the screen is black when drawing the menu.
		s_renderer->drawColoredQuad(0, 0, 320, 200, 0);

		// Draw the menu
		s_renderer->blitImage(&s_agentMenu.agentMenu->frames[0], 0, 0, s_scaleX, s_scaleY, 31, TEX_LAYOUT_HORZ);

		// Draw the buttons
		for (s32 i = 0; i < 4; i++)
		{
			s32 index;
			if (s_newAgentDlg)
			{
				if (i == 0) { index = i * 2 + 1; }
				else { index = i * 2 + 2; }
			}
			else if (s_removeAgentDlg)
			{
				if (i == 1) { index = i * 2 + 1; }
				else { index = i * 2 + 2; }
			}
			else if (s_quitConfirmDlg)
			{
				if (i == 2) { index = i * 2 + 1; }
				else { index = i * 2 + 2; }
			}
			else
			{
				if (s_buttonPressed == i && s_buttonHover) { index = i * 2 + 1; }
				else { index = i * 2 + 2; }
			}
			s_renderer->blitImage(&s_agentMenu.agentMenu->frames[index], 0, 0, s_scaleX, s_scaleY, 31, TEX_LAYOUT_HORZ);
		}
				
		// 11 = easy, 12 = medium, 13 = hard
		s32 yOffset = -20;
		s32 textX = 174;
		s32 textY = 36;
		u8 color = 47;
		if (s_selectedAgent >= 0 && s_agentCount > 0)
		{
			for (u32 i = 0; i < s_agent[s_selectedAgent].nextMission; i++)
			{
				color = 47;
				if (s_selectedMission == i)
				{
					color = 32;
					s_renderer->drawColoredQuad(textX - 3, textY - 1, 118, 9, s_lastSelectedAgent ? 13 : 12);
				}

				s_renderer->print(DXL2_LevelList::getLevelName(i), s_agentMenu.sysFont, textX, textY, s_scaleX, s_scaleY, color);
				s32 diff = s_agent[s_selectedAgent].completeDifficulty[i] + 11;
				s_renderer->blitImage(&s_agentMenu.agentMenu->frames[diff], 0, yOffset, s_scaleX, s_scaleY, 31, TEX_LAYOUT_HORZ);
				yOffset += 8;
				textY += 8;
			}
			color = 47;
			if (s_selectedMission == s_agent[s_selectedAgent].nextMission)
			{
				color = 32;
				s_renderer->drawColoredQuad(textX - 3, textY - 1, 118, 9, s_lastSelectedAgent ? 13 : 12);
			}
			s_renderer->print(DXL2_LevelList::getLevelName(s_agent[s_selectedAgent].nextMission), s_agentMenu.sysFont, textX, textY, s_scaleX, s_scaleY, color);
		}

		// Draw agents.
		textX = 28;
		textY = 36;
		for (u32 i = 0; i < s_agentCount; i++)
		{
			color = 47;
			if (s_selectedAgent == i)
			{
				color = 32;
				s_renderer->drawColoredQuad(textX - 3, textY - 1, 118, 9, s_lastSelectedAgent ? 12 : 13);
			}
			s_renderer->print(s_agent[i].name, s_agentMenu.sysFont, textX, textY, s_scaleX, s_scaleY, color);
			textY += 8;
		}

		if (s_newAgentDlg)
		{
			drawNewAgentDlg();
		}
		else if (s_removeAgentDlg)
		{
			drawYesNoDlg(0);
		}
		else if (s_quitConfirmDlg)
		{
			drawYesNoDlg(1);
		}

		// draw the mouse cursor.
		s_renderer->blitImage(&s_agentMenu.cursor->frames[0], s_cursorPos.x, s_cursorPos.z, s_scaleX, s_scaleY, 31, TEX_LAYOUT_HORZ);

		char mousePos[256];
		sprintf(mousePos, "%03d.%03d", s_virtualCursorPos.x, s_virtualCursorPos.z);
		DXL2_GameHud::setMessage(mousePos);
		s_renderer->print(mousePos, s_agentMenu.sysFont, 26, 190, s_scaleX, s_scaleY);
	}

	s32 alphabeticalAgentCmp(const void* a, const void* b)
	{
		const Agent* agent0 = (Agent*)a;
		const Agent* agent1 = (Agent*)b;
		return strcasecmp(agent0->name, agent1->name);
	}

	void createNewAgent()
	{
		// Create the new agent.
		Agent* agent = &s_agent[s_agentCount];
		strcpy(agent->name, s_newAgentName);
		agent->nextMission = 0;
		agent->selectedMission = 0;
		s_selectedMission = 0;
		s_agentCount++;

		// Sort agents alphabetically.
		std::qsort(s_agent, s_agentCount, sizeof(Agent), alphabeticalAgentCmp);

		// Then select the new agent.
		for (u32 i = 0; i < s_agentCount; i++)
		{
			if (strcasecmp(s_newAgentName, s_agent[i].name) == 0)
			{
				s_selectedAgent = i;
				break;
			}
		}
	}

	void removeAgent(s32 index)
	{
		for (s32 i = index; i < (s32)s_agentCount; i++)
		{
			s_agent[i] = s_agent[i + 1];
		}
		s_agentCount--;

		s_selectedAgent = s_agentCount > 0 ? 0 : -1;
		s_selectedMission = s_agentCount > 0 ? s_agent[0].selectedMission : -1;
	}

	void updateNewAgentDlg()
	{
		updateEditBox(&s_editBox);

		if (DXL2_Input::keyPressed(KEY_RETURN) || DXL2_Input::keyPressed(KEY_KP_ENTER))
		{
			createNewAgent();
			s_newAgentDlg = false;
			return;
		}
		else if (DXL2_Input::keyPressed(KEY_ESCAPE))
		{
			s_newAgentDlg = false;
			return;
		}

		const s32 x = s_virtualCursorPos.x;
		const s32 z = s_virtualCursorPos.z;

		if (DXL2_Input::mousePressed(MBUTTON_LEFT))
		{
			s_buttonPressed = -1;
			for (u32 i = 0; i < NEW_AGENT_COUNT; i++)
			{
				if (x >= c_newAgentButtons[i].x && x < c_newAgentButtons[i].x + c_newAgentButtonDim.x &&
					z >= c_newAgentButtons[i].z && z < c_newAgentButtons[i].z + c_newAgentButtonDim.z)
				{
					s_buttonPressed = s32(i);
					s_buttonHover = true;
					break;
				}
			}
		}
		else if (DXL2_Input::mouseDown(MBUTTON_LEFT) && s_buttonPressed >= 0)
		{
			// Verify that the mouse is still over the button.
			if (x >= c_newAgentButtons[s_buttonPressed].x && x < c_newAgentButtons[s_buttonPressed].x + c_newAgentButtonDim.x &&
				z >= c_newAgentButtons[s_buttonPressed].z && z < c_newAgentButtons[s_buttonPressed].z + c_newAgentButtonDim.z)
			{
				s_buttonHover = true;
			}
			else
			{
				s_buttonHover = false;
			}
		}
		else
		{
			// Activate button 's_escButtonPressed'
			if (s_buttonPressed >= NEW_AGENT_NO && s_buttonHover)
			{
				switch (s_buttonPressed)
				{
				case NEW_AGENT_YES:
				{
					createNewAgent();
					s_newAgentDlg = false;
					return;
				} break;
				case NEW_AGENT_NO:
					s_newAgentDlg = false;
					break;
				};
			}
			// Reset.
			s_buttonPressed = -1;
			s_buttonHover = false;
		}
	}

	void updateRemoveAgentDlg()
	{
		if (DXL2_Input::keyPressed(KEY_RETURN) || DXL2_Input::keyPressed(KEY_KP_ENTER) || DXL2_Input::keyPressed(KEY_ESCAPE))
		{
			s_removeAgentDlg = false;
			return;
		}

		const s32 x = s_virtualCursorPos.x;
		const s32 z = s_virtualCursorPos.z;

		if (DXL2_Input::mousePressed(MBUTTON_LEFT))
		{
			s_buttonPressed = -1;
			for (u32 i = 0; i < NEW_AGENT_COUNT; i++)
			{
				if (x >= c_newAgentButtons[i].x && x < c_newAgentButtons[i].x + c_newAgentButtonDim.x &&
					z >= c_newAgentButtons[i].z && z < c_newAgentButtons[i].z + c_newAgentButtonDim.z)
				{
					s_buttonPressed = s32(i);
					s_buttonHover = true;
					break;
				}
			}
		}
		else if (DXL2_Input::mouseDown(MBUTTON_LEFT) && s_buttonPressed >= 0)
		{
			// Verify that the mouse is still over the button.
			if (x >= c_newAgentButtons[s_buttonPressed].x && x < c_newAgentButtons[s_buttonPressed].x + c_newAgentButtonDim.x &&
				z >= c_newAgentButtons[s_buttonPressed].z && z < c_newAgentButtons[s_buttonPressed].z + c_newAgentButtonDim.z)
			{
				s_buttonHover = true;
			}
			else
			{
				s_buttonHover = false;
			}
		}
		else
		{
			if (DXL2_Input::keyPressed(KEY_Y))
			{
				removeAgent(s_selectedAgent);
				s_removeAgentDlg = false;
			}
			else if (DXL2_Input::keyPressed(KEY_N))
			{
				s_removeAgentDlg = false;
			}
			else if (s_buttonPressed >= NEW_AGENT_NO && s_buttonHover)
			{
				switch (s_buttonPressed)
				{
					case NEW_AGENT_YES:
					{
						removeAgent(s_selectedAgent);
						s_removeAgentDlg = false;
					} break;
					case NEW_AGENT_NO:
					{
						s_removeAgentDlg = false;
					} break;
				};
			}
			// Reset.
			s_buttonPressed = -1;
			s_buttonHover = false;
		}
	}

	bool updateQuitConfirmDlg()
	{
		bool quit = false;

		if (DXL2_Input::keyPressed(KEY_RETURN) || DXL2_Input::keyPressed(KEY_KP_ENTER) || DXL2_Input::keyPressed(KEY_ESCAPE))
		{
			s_quitConfirmDlg = false;
			return quit;
		}

		const s32 x = s_virtualCursorPos.x;
		const s32 z = s_virtualCursorPos.z;

		if (DXL2_Input::mousePressed(MBUTTON_LEFT))
		{
			s_buttonPressed = -1;
			for (u32 i = 0; i < NEW_AGENT_COUNT; i++)
			{
				if (x >= c_newAgentButtons[i].x && x < c_newAgentButtons[i].x + c_newAgentButtonDim.x &&
					z >= c_newAgentButtons[i].z && z < c_newAgentButtons[i].z + c_newAgentButtonDim.z)
				{
					s_buttonPressed = s32(i);
					s_buttonHover = true;
					break;
				}
			}
		}
		else if (DXL2_Input::mouseDown(MBUTTON_LEFT) && s_buttonPressed >= 0)
		{
			// Verify that the mouse is still over the button.
			if (x >= c_newAgentButtons[s_buttonPressed].x && x < c_newAgentButtons[s_buttonPressed].x + c_newAgentButtonDim.x &&
				z >= c_newAgentButtons[s_buttonPressed].z && z < c_newAgentButtons[s_buttonPressed].z + c_newAgentButtonDim.z)
			{
				s_buttonHover = true;
			}
			else
			{
				s_buttonHover = false;
			}
		}
		else
		{
			// Activate button 's_escButtonPressed'
			if (DXL2_Input::keyPressed(KEY_Y))
			{
				quit = true;
				s_quitConfirmDlg = false;
			}
			else if (DXL2_Input::keyPressed(KEY_N))
			{
				s_quitConfirmDlg = false;
			}
			else if (s_buttonPressed >= NEW_AGENT_NO && s_buttonHover)
			{
				switch (s_buttonPressed)
				{
					case NEW_AGENT_YES:
					{
						quit = true;
						s_quitConfirmDlg = false;
					} break;
					case NEW_AGENT_NO:
					{
						s_quitConfirmDlg = false;
					} break;
				};
			}
			// Reset.
			s_buttonPressed = -1;
			s_buttonHover = false;
		}

		return quit;
	}

	GameUiResult updateAgentMenu()
	{
		GameUiResult result = GAME_CONTINUE;

		if (s_newAgentDlg)
		{
			updateNewAgentDlg();
			return result;
		}
		else if (s_removeAgentDlg)
		{
			updateRemoveAgentDlg();
			return result;
		}
		else if (s_quitConfirmDlg)
		{
			if (updateQuitConfirmDlg())
			{
				closeAgentMenu();
				result = GAME_QUIT;
			}
			return result;
		}

		if (DXL2_Input::mousePressed(MBUTTON_LEFT))
		{
			const s32 x = s_virtualCursorPos.x;
			const s32 z = s_virtualCursorPos.z;
			s_buttonPressed = -1;
			for (u32 i = 0; i < AGENT_COUNT; i++)
			{
				if (x >= c_agentButtons[i].x && x < c_agentButtons[i].x + c_agentButtonDim.x &&
					z >= c_agentButtons[i].z && z < c_agentButtons[i].z + c_agentButtonDim.z)
				{
					s_buttonPressed = s32(i);
					s_buttonHover = true;
					break;
				}
			}

			if (s_buttonPressed < 0)
			{
				if (x >= 25 && x < 122 && z >= 35 && z < 147 && s_agentCount > 0)
				{
					s_selectedAgent = std::max(0, std::min((z - 35) / 8, (s32)s_agentCount-1));
					s_selectedMission = s_agent[s_selectedAgent].selectedMission;
					s_lastSelectedAgent = true;
				}
				else if (x >= 171 && x < 290 && z >= 35 && z < 147 && s_agentCount > 0)
				{
					s_selectedMission = std::max(0, std::min((z - 35) / 8, (s32)s_agent[s_selectedAgent].nextMission));
					s_agent[s_selectedAgent].selectedMission = s_selectedMission;
					s_lastSelectedAgent = false;
				}
			}
		}
		else if (DXL2_Input::mouseDown(MBUTTON_LEFT) && s_buttonPressed >= 0)
		{
			// Verify that the mouse is still over the button.
			const s32 x = s_virtualCursorPos.x;
			const s32 z = s_virtualCursorPos.z;
			if (x >= c_agentButtons[s_buttonPressed].x && x < c_agentButtons[s_buttonPressed].x + c_agentButtonDim.x &&
				z >= c_agentButtons[s_buttonPressed].z && z < c_agentButtons[s_buttonPressed].z + c_agentButtonDim.z)
			{
				s_buttonHover = true;
			}
			else
			{
				s_buttonHover = false;
			}
		}
		else
		{
			if (DXL2_Input::keyPressed(KEY_N))
			{
				s_buttonPressed = AGENT_NEW;
				s_buttonHover = true;
			}
			else if (DXL2_Input::keyPressed(KEY_R))
			{
				s_buttonPressed = AGENT_REMOVE;
				s_buttonHover = true;
			}
			else if (DXL2_Input::keyPressed(KEY_D))
			{
				s_buttonPressed = AGENT_EXIT;
				s_buttonHover = true;
			}
			else if (DXL2_Input::keyPressed(KEY_B) || DXL2_Input::keyPressed(KEY_RETURN) || DXL2_Input::keyPressed(KEY_KP_ENTER))
			{
				s_buttonPressed = AGENT_BEGIN;
				s_buttonHover = true;
			}

			// Arrow keys to change the selected agent or mission.
			if (DXL2_Input::bufferedKeyDown(KEY_DOWN))
			{
				if (s_lastSelectedAgent && s_agentCount > 0)
				{
					s_selectedAgent++;
					if (s_selectedAgent >= s_agentCount) { s_selectedAgent = 0; }
					if (s_selectedAgent >= 0 && s_agentCount > 0) { s_selectedMission = s_agent[s_selectedAgent].selectedMission; }
				}
				else if (!s_lastSelectedAgent && s_selectedAgent >= 0 && s_agent[s_selectedAgent].nextMission > 0)
				{
					s_selectedMission++;
					if (s_selectedMission > s_agent[s_selectedAgent].nextMission) { s_selectedMission = 0; }
				}
			}
			else if (DXL2_Input::bufferedKeyDown(KEY_UP))
			{
				if (s_lastSelectedAgent && s_agentCount > 0)
				{
					s_selectedAgent--;
					if (s_selectedAgent < 0) { s_selectedAgent = s_agentCount - 1; }
					if (s_selectedAgent >= 0 && s_agentCount > 0) { s_selectedMission = s_agent[s_selectedAgent].selectedMission; }
				}
				else if (!s_lastSelectedAgent && s_selectedAgent >= 0 && s_agent[s_selectedAgent].nextMission > 0)
				{
					s_selectedMission--;
					if (s_selectedMission < 0) { s_selectedMission = s_agent[s_selectedAgent].nextMission; }
				}
			}
			else if (DXL2_Input::bufferedKeyDown(KEY_LEFT) || DXL2_Input::bufferedKeyDown(KEY_RIGHT))
			{
				s_lastSelectedAgent = !s_lastSelectedAgent;
			}

			// Activate button 's_escButtonPressed'
			if (s_buttonPressed >= AGENT_NEW && s_buttonHover)
			{
				switch (s_buttonPressed)
				{
				case AGENT_NEW:
					s_newAgentDlg = true;
					memset(s_newAgentName, 0, 32);
					s_editBox.cursor = 0;
					s_editBox.inputField = s_newAgentName;
					s_editBox.maxLen = 16;
					break;
				case AGENT_REMOVE:
					s_removeAgentDlg = true;
					break;
				case AGENT_EXIT:
					s_quitConfirmDlg = true;
					break;
				case AGENT_BEGIN:
					if (s_agentCount > 0 && s_selectedMission >= 0)
					{
						result = GAME_SELECT_LEVEL;
						s_selectedLevel = s_selectedMission;
						closeAgentMenu();
					}
					break;
				};
			}

			// Reset.
			s_buttonPressed = -1;
			s_buttonHover = false;
		}
		return result;
	}
}
