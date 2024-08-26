#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/system.h>
#ifdef ENABLE_FORCE_SCRIPT
#include <TFE_System/types.h>
#include <TFE_ForceScript/float2.h>
#include "ls_api.h"
#include <string>

namespace LevelEditor
{
	class LS_Level : public LS_API
	{
	public:
		// Properties
		// Functions
		std::string getName();
		std::string getSlot();
		std::string getPalette();
		s32 getSectorCount();
		s32 getEntityCount();
		s32 getLevelNoteCount();
		s32 getGuidelineCount();
		s32 getMinLayer();
		s32 getMaxLayer();
		TFE_ForceScript::float2 getParallax();

		void setName(std::string& name);
		void setPalette(std::string& name);
		void findSector(std::string& name);
		void findSectorById(s32 id);
		// System
		bool scriptRegister(asIScriptEngine* engine) override;
	};
}
#endif