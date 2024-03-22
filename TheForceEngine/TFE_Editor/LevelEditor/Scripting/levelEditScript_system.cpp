#include "levelEditScript_system.h"
#include <TFE_System/system.h>
#include <TFE_Editor/LevelEditor/infoPanel.h>

namespace LevelEditor
{
	void system_print(std::string& msg)
	{
		infoPanelAddMsg(LE_MSG_INFO, msg.c_str());
	}

	void system_printError(std::string& msg)
	{
		infoPanelAddMsg(LE_MSG_ERROR, msg.c_str());
	}

	void system_printWarning(std::string& msg)
	{
		infoPanelAddMsg(LE_MSG_WARNING, msg.c_str());
	}
}