#include "gameUi.h"
#include "editBox.h"
#include <TFE_Game/gameLoop.h>
#include <TFE_Game/gameHud.h>
#include <TFE_Game/player.h>
#include <TFE_System/system.h>
#include <TFE_Renderer/renderer.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Asset/textureAsset.h>
#include <TFE_Asset/fontAsset.h>
#include <TFE_Asset/paletteAsset.h>
#include <TFE_Asset/colormapAsset.h>
#include <TFE_Asset/levelList.h>
#include <TFE_Input/input.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <assert.h>
#include <vector>
#include <algorithm>

using namespace TFE_GameUi;

namespace TFE_AgentMenu
{
	// TODO: Improve upscaled rendering -
	// The plan is to render into a 320x200 buffer and then upscale using the GPU, which will allow all of this scaling code to go away
	// and produce cleaner looking results. Since all UI can be rendered into the same texture, this should cost very little in terms of
	// bandwidth.
	#define SX(x) (offset + (((x) * scaleX) >> 8))
	#define SY(y) (((y) * scaleY) >> 8)
	#define WX(x) (((x) * scaleX) >> 8)
	#define WY(y) (((y) * scaleY) >> 8)

	///////////////////////////////////////
	// TODO: Save System
	///////////////////////////////////////
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

	///////////////////////////////////////
	// UI Definitions
	// TODO: Move these to game data.
	///////////////////////////////////////
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

	static s32 s_selectedAgent = 0;
	static s32 s_selectedMission = 0;
	static s32 s_editCursorFlicker = 0;
	static bool s_lastSelectedAgent = true;
	static bool s_newAgentDlg = false;
	static bool s_removeAgentDlg = false;
	static bool s_quitConfirmDlg = false;
	static EditBox s_editBox = {};

	static Palette256* s_pal;
	static Texture* s_agentMenu;
	static Texture* s_agentDlg;
	static Font* s_sysFont;

	void loadAgents();
	void drawYesNoDlg(u32 baseFrame, TFE_Renderer* renderer, s32 offset, s32 scaleX, s32 scaleY, s32 buttonPressed, bool buttonHover);
	void drawNewAgentDlg(TFE_Renderer* renderer, s32 offset, s32 scaleX, s32 scaleY, s32 buttonPressed, bool buttonHover);

	void updateNewAgentDlg(s32 x, s32 z, s32* buttonPressed, bool* buttonHover);
	void updateRemoveAgentDlg(s32 x, s32 z, s32* buttonPressed, bool* buttonHover);
	bool updateQuitConfirmDlg(s32 x, s32 z, s32* buttonPressed, bool* buttonHover);
		
	bool load()
	{
		s_pal = TFE_Palette::getPalFromPltt("agentmnu.pltt", "LFD/AGENTMNU.LFD");
		s_agentMenu = TFE_Texture::getFromAnim("agentmnu.anim", "LFD/AGENTMNU.LFD");
		s_agentDlg = TFE_Texture::getFromAnim("agentdlg.anim", "LFD/AGENTMNU.LFD");
		s_sysFont = TFE_Font::createSystemFont6x8();
		loadAgents();

		if (s_agentCount > 0)
		{
			s_selectedAgent = 0;
			s_selectedMission = s_agent[s_selectedAgent].selectedMission;
		}
		else
		{
			s_selectedAgent = -1;
			s_selectedMission = 0;
		}

		return (s_pal && s_agentMenu && s_agentDlg);
	}

	void unload()
	{
		TFE_Texture::free(s_agentMenu);
		TFE_Texture::free(s_agentDlg);
		s_agentMenu = nullptr;
		s_agentDlg = nullptr;
	}

	void open(TFE_Renderer* renderer)
	{
		renderer->setPalette(s_pal);
		renderer->clearScreen();
	}

	void close()
	{
	}

	void draw(TFE_Renderer* renderer, s32 offset, s32 scaleX, s32 scaleY, s32 buttonPressed, bool buttonHover, bool nextMission)
	{
		// Clear part of the background.
		// TODO: Fade out the game view when existing, so the screen is black when drawing the menu.
		renderer->drawColoredQuad(offset, 0, WX(320), WY(200), 0);

		// Draw the menu
		renderer->blitImage(&s_agentMenu->frames[0], offset, 0, scaleX, scaleY, 31, TEX_LAYOUT_HORZ);

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
				if (buttonPressed == i && buttonHover) { index = i * 2 + 1; }
				else { index = i * 2 + 2; }
			}
			renderer->blitImage(&s_agentMenu->frames[index], offset, 0, scaleX, scaleY, 31, TEX_LAYOUT_HORZ);
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
					renderer->drawColoredQuad(SX(textX - 3), SY(textY - 1), WX(118), WY(9), s_lastSelectedAgent ? 13 : 12);
				}

				renderer->print(TFE_LevelList::getLevelName(i), s_sysFont, SX(textX), SY(textY), scaleX, scaleY, color);
				s32 diff = s_agent[s_selectedAgent].completeDifficulty[i] + 11;
				renderer->blitImage(&s_agentMenu->frames[diff], SX(0), SY(yOffset), scaleX, scaleY, 31, TEX_LAYOUT_HORZ);
				yOffset += 8;
				textY += 8;
			}
			color = 47;
			if (s_selectedMission == s_agent[s_selectedAgent].nextMission)
			{
				color = 32;
				renderer->drawColoredQuad(SX(textX - 3), SY(textY - 1), WX(118), WY(9), s_lastSelectedAgent ? 13 : 12);
			}
			renderer->print(TFE_LevelList::getLevelName(s_agent[s_selectedAgent].nextMission), s_sysFont, SX(textX), SY(textY), scaleX, scaleY, color);
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
				renderer->drawColoredQuad(SX(textX - 3), SY(textY - 1), WX(118), WY(9), s_lastSelectedAgent ? 12 : 13);
			}
			renderer->print(s_agent[i].name, s_sysFont, SX(textX), SY(textY), scaleX, scaleY, color);
			textY += 8;
		}

		if (s_newAgentDlg)
		{
			drawNewAgentDlg(renderer, offset, scaleX, scaleY, buttonPressed, buttonHover);
		}
		else if (s_removeAgentDlg)
		{
			drawYesNoDlg(0, renderer, offset, scaleX, scaleY, buttonPressed, buttonHover);
		}
		else if (s_quitConfirmDlg)
		{
			drawYesNoDlg(1, renderer, offset, scaleX, scaleY, buttonPressed, buttonHover);
		}
	}

	GameUiResult update(const Vec2i* cursor, s32* buttonPressed, bool* buttonHover, s32* selectedLevel)
	{
		GameUiResult result = GAME_UI_CONTINUE;

		const s32 x = cursor->x;
		const s32 z = cursor->z;
		if (s_newAgentDlg)
		{
			updateNewAgentDlg(x, z, buttonPressed, buttonHover);
			return result;
		}
		else if (s_removeAgentDlg)
		{
			updateRemoveAgentDlg(x, z, buttonPressed, buttonHover);
			return result;
		}
		else if (s_quitConfirmDlg)
		{
			if (updateQuitConfirmDlg(x, z, buttonPressed, buttonHover))
			{
				result = GAME_UI_QUIT;
			}
			return result;
		}

		if (TFE_Input::mousePressed(MBUTTON_LEFT))
		{
			*buttonPressed = -1;
			for (u32 i = 0; i < AGENT_COUNT; i++)
			{
				if (x >= c_agentButtons[i].x && x < c_agentButtons[i].x + c_agentButtonDim.x &&
					z >= c_agentButtons[i].z && z < c_agentButtons[i].z + c_agentButtonDim.z)
				{
					*buttonPressed = s32(i);
					*buttonHover = true;
					break;
				}
			}

			if (*buttonPressed < 0)
			{
				if (x >= 25 && x < 122 && z >= 35 && z < 147 && s_agentCount > 0)
				{
					s_selectedAgent = std::max(0, std::min((z - 35) / 8, (s32)s_agentCount - 1));
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
		else if (TFE_Input::mouseDown(MBUTTON_LEFT) && *buttonPressed >= 0)
		{
			// Verify that the mouse is still over the button.
			if (x >= c_agentButtons[*buttonPressed].x && x < c_agentButtons[*buttonPressed].x + c_agentButtonDim.x &&
				z >= c_agentButtons[*buttonPressed].z && z < c_agentButtons[*buttonPressed].z + c_agentButtonDim.z)
			{
				*buttonHover = true;
			}
			else
			{
				*buttonHover = false;
			}
		}
		else
		{
			if (TFE_Input::keyPressed(KEY_N))
			{
				*buttonPressed = AGENT_NEW;
				*buttonHover = true;
			}
			else if (TFE_Input::keyPressed(KEY_R))
			{
				*buttonPressed = AGENT_REMOVE;
				*buttonHover = true;
			}
			else if (TFE_Input::keyPressed(KEY_D))
			{
				*buttonPressed = AGENT_EXIT;
				*buttonHover = true;
			}
			else if (TFE_Input::keyPressed(KEY_B) || TFE_Input::keyPressed(KEY_RETURN) || TFE_Input::keyPressed(KEY_KP_ENTER))
			{
				*buttonPressed = AGENT_BEGIN;
				*buttonHover = true;
			}

			// Arrow keys to change the selected agent or mission.
			if (TFE_Input::bufferedKeyDown(KEY_DOWN))
			{
				if (s_lastSelectedAgent && s_agentCount > 0)
				{
					s_selectedAgent++;
					if (s_selectedAgent >= (s32)s_agentCount) { s_selectedAgent = 0; }
					if (s_selectedAgent >= 0) { s_selectedMission = s_agent[s_selectedAgent].selectedMission; }
				}
				else if (!s_lastSelectedAgent && s_selectedAgent >= 0 && s_agent[s_selectedAgent].nextMission > 0)
				{
					s_selectedMission++;
					if (s_selectedMission > s_agent[s_selectedAgent].nextMission) { s_selectedMission = 0; }
				}
			}
			else if (TFE_Input::bufferedKeyDown(KEY_UP))
			{
				if (s_lastSelectedAgent && s_agentCount > 0)
				{
					s_selectedAgent--;
					if (s_selectedAgent < 0) { s_selectedAgent = s_agentCount - 1; }
					if (s_selectedAgent >= 0) { s_selectedMission = s_agent[s_selectedAgent].selectedMission; }
				}
				else if (!s_lastSelectedAgent && s_selectedAgent >= 0 && s_agent[s_selectedAgent].nextMission > 0)
				{
					s_selectedMission--;
					if (s_selectedMission < 0) { s_selectedMission = s_agent[s_selectedAgent].nextMission; }
				}
			}
			else if (TFE_Input::bufferedKeyDown(KEY_LEFT) || TFE_Input::bufferedKeyDown(KEY_RIGHT))
			{
				s_lastSelectedAgent = !s_lastSelectedAgent;
			}

			// Activate button 's_escButtonPressed'
			if (*buttonPressed >= AGENT_NEW && *buttonHover)
			{
				switch (*buttonPressed)
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
						result = GAME_UI_SELECT_LEVEL;
						*selectedLevel = s_selectedMission;
					}
					break;
				};
			}

			// Reset.
			*buttonPressed = -1;
			*buttonHover = false;
		}
		return result;
	}
		
	//////////////////////////////////////////////////////////////////
	// Internal
	//////////////////////////////////////////////////////////////////
	void loadAgents()
	{
		// Hardcode test agents.
		s_agentCount = 5;

		strcpy(s_agent[0].name, "DoomedXL");
		for (u32 i = 0; i < 3; i++) { s_agent[0].completeDifficulty[i] = 0; }
		s_agent[0].nextMission = 3;
		s_agent[0].selectedMission = 2;

		strcpy(s_agent[1].name, "James");
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
		
	void drawYesNoButtons(u32 indexNo, u32 indexYes, TFE_Renderer* renderer, s32 offset, s32 scaleX, s32 scaleY, s32 buttonPressed, bool buttonHover)
	{
		if (buttonPressed == 0 && buttonHover)
		{
			renderer->blitImage(&s_agentDlg->frames[indexNo], offset, 0, scaleX, scaleY, 31, TEX_LAYOUT_HORZ);
		}
		else if (buttonPressed == 1 && buttonHover)
		{
			renderer->blitImage(&s_agentDlg->frames[indexYes], offset, 0, scaleX, scaleY, 31, TEX_LAYOUT_HORZ);
		}
	}

	void drawYesNoDlg(u32 baseFrame, TFE_Renderer* renderer, s32 offset, s32 scaleX, s32 scaleY, s32 buttonPressed, bool buttonHover)
	{
		renderer->blitImage(&s_agentDlg->frames[baseFrame], offset, 0, scaleX, scaleY, 31, TEX_LAYOUT_HORZ);
		drawYesNoButtons(4+offset, 6, renderer, offset, scaleX, scaleY, buttonPressed, buttonHover);
	}

	void drawNewAgentDlg(TFE_Renderer* renderer, s32 offset, s32 scaleX, s32 scaleY, s32 buttonPressed, bool buttonHover)
	{
		// Draw the dialog
		renderer->blitImage(&s_agentDlg->frames[2], offset, 0, scaleX, scaleY, 31, TEX_LAYOUT_HORZ);
		drawEditBox(&s_editBox, s_sysFont, offset, 106, 87, 216, 100, scaleX, scaleY, renderer);
		drawYesNoButtons(8+offset, 10, renderer, offset, scaleX, scaleY, buttonPressed, buttonHover);
	}

	/////////////////////////
	// Update Internal
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

	void updateNewAgentDlg(s32 x, s32 z, s32* buttonPressed, bool* buttonHover)
	{
		updateEditBox(&s_editBox);

		if (TFE_Input::keyPressed(KEY_RETURN) || TFE_Input::keyPressed(KEY_KP_ENTER))
		{
			createNewAgent();
			s_newAgentDlg = false;
			return;
		}
		else if (TFE_Input::keyPressed(KEY_ESCAPE))
		{
			s_newAgentDlg = false;
			return;
		}

		if (TFE_Input::mousePressed(MBUTTON_LEFT))
		{
			*buttonPressed = -1;
			for (u32 i = 0; i < NEW_AGENT_COUNT; i++)
			{
				if (x >= c_newAgentButtons[i].x && x < c_newAgentButtons[i].x + c_newAgentButtonDim.x &&
					z >= c_newAgentButtons[i].z && z < c_newAgentButtons[i].z + c_newAgentButtonDim.z)
				{
					*buttonPressed = s32(i);
					*buttonHover = true;
					break;
				}
			}
		}
		else if (TFE_Input::mouseDown(MBUTTON_LEFT) && *buttonPressed >= 0)
		{
			// Verify that the mouse is still over the button.
			if (x >= c_newAgentButtons[*buttonPressed].x && x < c_newAgentButtons[*buttonPressed].x + c_newAgentButtonDim.x &&
				z >= c_newAgentButtons[*buttonPressed].z && z < c_newAgentButtons[*buttonPressed].z + c_newAgentButtonDim.z)
			{
				*buttonHover = true;
			}
			else
			{
				*buttonHover = false;
			}
		}
		else
		{
			// Activate button 's_escButtonPressed'
			if (*buttonPressed >= NEW_AGENT_NO && *buttonHover)
			{
				switch (*buttonPressed)
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
			*buttonPressed = -1;
			*buttonHover = false;
		}
	}

	void updateRemoveAgentDlg(s32 x, s32 z, s32* buttonPressed, bool* buttonHover)
	{
		if (TFE_Input::keyPressed(KEY_RETURN) || TFE_Input::keyPressed(KEY_KP_ENTER) || TFE_Input::keyPressed(KEY_ESCAPE))
		{
			s_removeAgentDlg = false;
			return;
		}

		if (TFE_Input::mousePressed(MBUTTON_LEFT))
		{
			*buttonPressed = -1;
			for (u32 i = 0; i < NEW_AGENT_COUNT; i++)
			{
				if (x >= c_newAgentButtons[i].x && x < c_newAgentButtons[i].x + c_newAgentButtonDim.x &&
					z >= c_newAgentButtons[i].z && z < c_newAgentButtons[i].z + c_newAgentButtonDim.z)
				{
					*buttonPressed = s32(i);
					*buttonHover = true;
					break;
				}
			}
		}
		else if (TFE_Input::mouseDown(MBUTTON_LEFT) && *buttonPressed >= 0)
		{
			// Verify that the mouse is still over the button.
			if (x >= c_newAgentButtons[*buttonPressed].x && x < c_newAgentButtons[*buttonPressed].x + c_newAgentButtonDim.x &&
				z >= c_newAgentButtons[*buttonPressed].z && z < c_newAgentButtons[*buttonPressed].z + c_newAgentButtonDim.z)
			{
				*buttonHover = true;
			}
			else
			{
				*buttonHover = false;
			}
		}
		else
		{
			if (TFE_Input::keyPressed(KEY_Y))
			{
				removeAgent(s_selectedAgent);
				s_removeAgentDlg = false;
			}
			else if (TFE_Input::keyPressed(KEY_N))
			{
				s_removeAgentDlg = false;
			}
			else if (*buttonPressed >= NEW_AGENT_NO && *buttonHover)
			{
				switch (*buttonPressed)
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
			*buttonPressed = -1;
			*buttonHover = false;
		}
	}

	bool updateQuitConfirmDlg(s32 x, s32 z, s32* buttonPressed, bool* buttonHover)
	{
		bool quit = false;

		if (TFE_Input::keyPressed(KEY_RETURN) || TFE_Input::keyPressed(KEY_KP_ENTER) || TFE_Input::keyPressed(KEY_ESCAPE))
		{
			s_quitConfirmDlg = false;
			return quit;
		}

		if (TFE_Input::mousePressed(MBUTTON_LEFT))
		{
			*buttonPressed = -1;
			for (u32 i = 0; i < NEW_AGENT_COUNT; i++)
			{
				if (x >= c_newAgentButtons[i].x && x < c_newAgentButtons[i].x + c_newAgentButtonDim.x &&
					z >= c_newAgentButtons[i].z && z < c_newAgentButtons[i].z + c_newAgentButtonDim.z)
				{
					*buttonPressed = s32(i);
					*buttonHover = true;
					break;
				}
			}
		}
		else if (TFE_Input::mouseDown(MBUTTON_LEFT) && *buttonPressed >= 0)
		{
			// Verify that the mouse is still over the button.
			if (x >= c_newAgentButtons[*buttonPressed].x && x < c_newAgentButtons[*buttonPressed].x + c_newAgentButtonDim.x &&
				z >= c_newAgentButtons[*buttonPressed].z && z < c_newAgentButtons[*buttonPressed].z + c_newAgentButtonDim.z)
			{
				*buttonHover = true;
			}
			else
			{
				*buttonHover = false;
			}
		}
		else
		{
			// Activate button 's_escButtonPressed'
			if (TFE_Input::keyPressed(KEY_Y))
			{
				quit = true;
				s_quitConfirmDlg = false;
			}
			else if (TFE_Input::keyPressed(KEY_N))
			{
				s_quitConfirmDlg = false;
			}
			else if (*buttonPressed >= NEW_AGENT_NO && *buttonHover)
			{
				switch (*buttonPressed)
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
			*buttonPressed = -1;
			*buttonHover = false;
		}

		return quit;
	}
}
