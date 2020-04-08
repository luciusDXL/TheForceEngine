#pragma once
// Game states, such as cutscenes, agent/save menu, mission briefings, etc.
enum GameState
{
	// Out of game menus
	GAME_TITLE = 0,				// Title cutscenes.
	GAME_AGENT_MENU,			// Agent Menu.
	GAME_PRE_MISSION_CUTSCENE,	// Current level "pre mission" cutscene (if there is one).
	GAME_MISSION_BRIEFING,		// Current level "mission briefing"
	GAME_POST_MISSION_CUTSCENE,	// Current level "post mission" cutscene (if there is one).
	// In-Game level
	GAME_LEVEL,
	GAME_COUNT
};

// Overlayed game states that occur during the "GAME_LEVEL" state - 
// such as the escape menu and in-game PDA.
enum GameOverlay
{
	OVERLAY_NONE = 0,
	OVERLAY_MENU_ESCAPE,
	OVERLAY_PDA,
	OVERLAY_COUNT
};

enum GameTransition
{
	TRANS_NONE = 0,
	TRANS_START_GAME,
	TRANS_TO_AGENT_MENU,
	TRANS_TO_PREMISSION_CUTSCENE,
	TRANS_TO_MISSION_BRIEFING,
	TRANS_START_LEVEL,
	TRANS_TO_POSTMISSION_CUTSCENE,
	TRANS_NEXT_LEVEL,
	TRANS_QUIT,
	TRANS_RETURN_TO_FRONTEND,
	TRANS_COUNT
};
