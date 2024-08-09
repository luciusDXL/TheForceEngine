#include "ls_level.h"
#include <TFE_System/system.h>
#include <TFE_Editor/LevelEditor/infoPanel.h>
#include <TFE_Editor/LevelEditor/sharedState.h>

#ifdef ENABLE_FORCE_SCRIPT
#include <angelscript.h>
namespace LevelEditor
{
	#define Vec2f_to_float2(v) TFE_ForceScript::float2(v.x, v.z)
	#define float2_to_Vec2f(v) Vec2f{v.x, v.y};

	// Getters
	std::string LS_Level::getName()
	{
		return s_level.name;
	}
		
	std::string LS_Level::getSlot()
	{
		return s_level.slot;
	}

	std::string LS_Level::getPalette()
	{
		return s_level.palette;
	}

	s32 LS_Level::getSectorCount()
	{
		return (s32)s_level.sectors.size();
	}
		
	s32 LS_Level::getEntityCount()
	{
		return (s32)s_level.entities.size();
	}

	s32 LS_Level::getLevelNoteCount()
	{
		return (s32)s_level.notes.size();
	}

	s32 LS_Level::getGuidelineCount()
	{
		return (s32)s_level.guidelines.size();
	}

	s32 LS_Level::getMinLayer()
	{
		return s_level.layerRange[0];
	}

	s32 LS_Level::getMaxLayer()
	{
		return s_level.layerRange[1];
	}

	TFE_ForceScript::float2 LS_Level::getParallax()
	{
		return Vec2f_to_float2(s_level.parallax);
	}

	// Setters
	void LS_Level::setName(std::string& name)
	{
		s_level.name = name;
	}

	void LS_Level::setPalette(std::string& palette)
	{
		s_level.palette = palette;
	}

	// Creation
	// TODO Sector drawing.
	// TODO Guideline drawing.
	// Insert Entity
	// Insert Level Note

	// Other
	void LS_Level::findSector(std::string& name)
	{
		s32 index = findSectorByName(name.c_str());
		if (index < 0)
		{
			infoPanelAddMsg(LE_MSG_WARNING, "Cannot find sector '%s'", name.c_str());
		}
		else
		{
			selectAndScrollToSector(&s_level.sectors[index]);
		}
	}

	bool LS_Level::scriptRegister(asIScriptEngine* engine)
	{
		s32 res = 0;
		// Object Type
		res = engine->RegisterObjectType("Level", sizeof(LS_Level), asOBJ_VALUE | asOBJ_POD); assert(res >= 0);
		// Properties
		// Functions
		// -- Getters --
		res = engine->RegisterObjectMethod("Level", "string getName()", asMETHOD(LS_Level, getName), asCALL_THISCALL);  assert(res >= 0);
		res = engine->RegisterObjectMethod("Level", "string getSlot()", asMETHOD(LS_Level, getSlot), asCALL_THISCALL);  assert(res >= 0);
		res = engine->RegisterObjectMethod("Level", "string getPalette()", asMETHOD(LS_Level, getPalette), asCALL_THISCALL);  assert(res >= 0);
		res = engine->RegisterObjectMethod("Level", "int getSectorCount()", asMETHOD(LS_Level, getSectorCount), asCALL_THISCALL);  assert(res >= 0);
		res = engine->RegisterObjectMethod("Level", "int getEntityCount()", asMETHOD(LS_Level, getEntityCount), asCALL_THISCALL);  assert(res >= 0);
		res = engine->RegisterObjectMethod("Level", "int getLevelNoteCount()", asMETHOD(LS_Level, getLevelNoteCount), asCALL_THISCALL);  assert(res >= 0);
		res = engine->RegisterObjectMethod("Level", "int getGuidelineCount()", asMETHOD(LS_Level, getGuidelineCount), asCALL_THISCALL);  assert(res >= 0);
		res = engine->RegisterObjectMethod("Level", "int getMinLayer()", asMETHOD(LS_Level, getMinLayer), asCALL_THISCALL);  assert(res >= 0);
		res = engine->RegisterObjectMethod("Level", "int getMaxLayer()", asMETHOD(LS_Level, getMaxLayer), asCALL_THISCALL);  assert(res >= 0);
		res = engine->RegisterObjectMethod("Level", "float2 getParallax()", asMETHOD(LS_Level, getParallax), asCALL_THISCALL);  assert(res >= 0);
		// TODO Vec3f s_level.bounds[2]
		//      Types: Sector, Entity, LevelNote, Guideline
		// -- Setters --
		res = engine->RegisterObjectMethod("Level", "void setName(const string &in)", asMETHOD(LS_Level, setName), asCALL_THISCALL);  assert(res >= 0);
		res = engine->RegisterObjectMethod("Level", "void setPalette(const string &in)", asMETHOD(LS_Level, setPalette), asCALL_THISCALL);  assert(res >= 0);
		res = engine->RegisterObjectMethod("Level", "void findSector(const string &in)", asMETHOD(LS_Level, findSector), asCALL_THISCALL);  assert(res >= 0);
		// Script variable.
		res = engine->RegisterGlobalProperty("Level level", this);  assert(res >= 0);
		return res >= 0;
	}
}
#endif