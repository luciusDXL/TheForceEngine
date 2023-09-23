#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces Config
// Replacement for the Jedi Config for TFE.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Input/input.h>

namespace TFE_DarkForces
{
	struct GameConfig
	{
		JBool wpnAutoMount;
		JBool mouseTurnEnabled;
		JBool mouseLookEnabled;
		JBool superShield;
		JBool showUI;
	};

	void config_startup();
	void config_shutdown();

	extern GameConfig s_config;
}  // TFE_DarkForces