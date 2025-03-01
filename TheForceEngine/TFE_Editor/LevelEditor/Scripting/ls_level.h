#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/system.h>
#include <TFE_System/types.h>
#include <TFE_ForceScript/scriptInterface.h>
#include <string>

namespace LevelEditor
{
	class LS_Level : public ScriptAPIClass
	{
	public:
		void findSector(std::string& name);
		void findSectorById(s32 id);
		// System
		bool scriptRegister(ScriptAPI api) override;

	public:
		// Variables
		s32 m_index;
	};
}
