#include "ls_level.h"
#include <TFE_System/system.h>
#include <TFE_Editor/LevelEditor/infoPanel.h>
#include <TFE_Editor/LevelEditor/sharedState.h>
#include <TFE_Jedi/Level/rwall.h>
#include <TFE_Jedi/Level/rsector.h>

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

	void LS_Level::findSectorById(s32 id)
	{
		if (id < 0 || id >= (s32)s_level.sectors.size())
		{
			infoPanelAddMsg(LE_MSG_WARNING, "ID %d is out of range.", id);
		}
		else
		{
			selectAndScrollToSector(&s_level.sectors[id]);
		}
	}

	bool LS_Level::scriptRegister(asIScriptEngine* engine)
	{
		s32 res = 0;
		// Object Type
		res = engine->RegisterObjectType("Level", sizeof(LS_Level), asOBJ_VALUE | asOBJ_POD); assert(res >= 0);
		// Enums
		res = engine->RegisterEnum("WallPart"); assert(res >= 0);
		res = engine->RegisterEnumValue("WallPart", "PART_MID", WP_MID); assert(res >= 0);
		res = engine->RegisterEnumValue("WallPart", "PART_TOP", WP_TOP); assert(res >= 0);
		res = engine->RegisterEnumValue("WallPart", "PART_BOT", WP_BOT); assert(res >= 0);
		res = engine->RegisterEnumValue("WallPart", "PART_SIGN", WP_SIGN); assert(res >= 0);
				
		res = engine->RegisterEnum("SectorFlags1"); assert(res >= 0);
		res = engine->RegisterEnumValue("SectorFlags1", "SECTOR_FLAG1_EXTERIOR", SEC_FLAGS1_EXTERIOR); assert(res >= 0);
		res = engine->RegisterEnumValue("SectorFlags1", "SECTOR_FLAG1_DOOR", SEC_FLAGS1_DOOR); assert(res >= 0);
		res = engine->RegisterEnumValue("SectorFlags1", "SECTOR_FLAG1_MAG_SEAL", SEC_FLAGS1_MAG_SEAL); assert(res >= 0);
		res = engine->RegisterEnumValue("SectorFlags1", "SECTOR_FLAG1_EXTERIOR_ADJOIN", SEC_FLAGS1_EXT_ADJ); assert(res >= 0);
		res = engine->RegisterEnumValue("SectorFlags1", "SECTOR_FLAG1_ICE_FLOOR", SEC_FLAGS1_ICE_FLOOR); assert(res >= 0);
		res = engine->RegisterEnumValue("SectorFlags1", "SECTOR_FLAG1_SNOW_FLOOR", SEC_FLAGS1_SNOW_FLOOR); assert(res >= 0);
		res = engine->RegisterEnumValue("SectorFlags1", "SECTOR_FLAG1_EXPLODING_WALL", SEC_FLAGS1_EXP_WALL); assert(res >= 0);
		res = engine->RegisterEnumValue("SectorFlags1", "SECTOR_FLAG1_PIT", SEC_FLAGS1_PIT); assert(res >= 0);
		res = engine->RegisterEnumValue("SectorFlags1", "SECTOR_FLAG1_PIT_ADJOIN", SEC_FLAGS1_EXT_FLOOR_ADJ); assert(res >= 0);
		res = engine->RegisterEnumValue("SectorFlags1", "SECTOR_FLAG1_CRUSHING", SEC_FLAGS1_CRUSHING); assert(res >= 0);
		res = engine->RegisterEnumValue("SectorFlags1", "SECTOR_FLAG1_NOWALL_DRAW", SEC_FLAGS1_NOWALL_DRAW); assert(res >= 0);
		res = engine->RegisterEnumValue("SectorFlags1", "SECTOR_FLAG1_LOW_DAMAGE", SEC_FLAGS1_LOW_DAMAGE); assert(res >= 0);
		res = engine->RegisterEnumValue("SectorFlags1", "SECTOR_FLAG1_HIGH_DAMAGE", SEC_FLAGS1_HIGH_DAMAGE); assert(res >= 0);
		res = engine->RegisterEnumValue("SectorFlags1", "SECTOR_FLAG1_NO_SMART_OBJ", SEC_FLAGS1_NO_SMART_OBJ); assert(res >= 0);
		res = engine->RegisterEnumValue("SectorFlags1", "SECTOR_FLAG1_SMART_OBJ", SEC_FLAGS1_SMART_OBJ); assert(res >= 0);
		res = engine->RegisterEnumValue("SectorFlags1", "SECTOR_FLAG1_SAFE_SECTOR", SEC_FLAGS1_SAFESECTOR); assert(res >= 0);
		res = engine->RegisterEnumValue("SectorFlags1", "SECTOR_FLAG1_PLAYER", SEC_FLAGS1_PLAYER); assert(res >= 0);
		res = engine->RegisterEnumValue("SectorFlags1", "SECTOR_FLAG1_SECRET", SEC_FLAGS1_SECRET); assert(res >= 0);

		res = engine->RegisterEnum("WallFlags1"); assert(res >= 0);
		res = engine->RegisterEnumValue("WallFlags1", "WALL_FLAGS1_TRANSPARENT_MIDTEX", WF1_ADJ_MID_TEX); assert(res >= 0);
		res = engine->RegisterEnumValue("WallFlags1", "WALL_FLAGS1_ILLUM_SIGN", WF1_ILLUM_SIGN); assert(res >= 0);
		res = engine->RegisterEnumValue("WallFlags1", "WALL_FLAGS1_TEX_FLIP", WF1_FLIP_HORIZ); assert(res >= 0);
		res = engine->RegisterEnumValue("WallFlags1", "WALL_FLAGS1_CHANGE_WALL_LIGHT", WF1_CHANGE_WALL_LIGHT); assert(res >= 0);
		res = engine->RegisterEnumValue("WallFlags1", "WALL_FLAGS1_TEX_ANCHORED", WF1_TEX_ANCHORED); assert(res >= 0);
		res = engine->RegisterEnumValue("WallFlags1", "WALL_FLAGS1_WALL_MORPHS", WF1_WALL_MORPHS); assert(res >= 0);
		res = engine->RegisterEnumValue("WallFlags1", "WALL_FLAGS1_SCROLL_TOP_TEX", WF1_SCROLL_TOP_TEX); assert(res >= 0);
		res = engine->RegisterEnumValue("WallFlags1", "WALL_FLAGS1_SCROLL_MID_TEX", WF1_SCROLL_MID_TEX); assert(res >= 0);
		res = engine->RegisterEnumValue("WallFlags1", "WALL_FLAGS1_SCROLL_BOT_TEX", WF1_SCROLL_BOT_TEX); assert(res >= 0);
		res = engine->RegisterEnumValue("WallFlags1", "WALL_FLAGS1_SCROLL_SIGN_TEX", WF1_SCROLL_SIGN_TEX); assert(res >= 0);
		res = engine->RegisterEnumValue("WallFlags1", "WALL_FLAGS1_HIDE_ON_MAP", WF1_HIDE_ON_MAP); assert(res >= 0);
		res = engine->RegisterEnumValue("WallFlags1", "WALL_FLAGS1_SHOW_NORMAL_ON_MAP", WF1_SHOW_NORMAL_ON_MAP); assert(res >= 0);
		res = engine->RegisterEnumValue("WallFlags1", "WALL_FLAGS1_SIGN_ANCHORED", WF1_SIGN_ANCHORED); assert(res >= 0);
		res = engine->RegisterEnumValue("WallFlags1", "WALL_FLAGS1_DAMAGE_WALL", WF1_DAMAGE_WALL); assert(res >= 0);
		res = engine->RegisterEnumValue("WallFlags1", "WALL_FLAGS1_SHOW_AS_LEDGE_ON_MAP", WF1_SHOW_AS_LEDGE_ON_MAP); assert(res >= 0);
		res = engine->RegisterEnumValue("WallFlags1", "WALL_FLAGS1_SHOW_AS_DOOR_ON_MAP", WF1_SHOW_AS_DOOR_ON_MAP); assert(res >= 0);

		res = engine->RegisterEnum("WallFlags3"); assert(res >= 0);
		res = engine->RegisterEnumValue("WallFlags3", "WALL_FLAGS3_ALWAYS_WALK", WF3_ALWAYS_WALK); assert(res >= 0);
		res = engine->RegisterEnumValue("WallFlags3", "WALL_FLAGS3_SOLID", WF3_SOLID_WALL); assert(res >= 0);
		res = engine->RegisterEnumValue("WallFlags3", "WALL_FLAGS3_PLAYER_WALK_ONLY", WF3_PLAYER_WALK_ONLY); assert(res >= 0);
		res = engine->RegisterEnumValue("WallFlags3", "WALL_FLAGS3_CANNOT_FIRE_THRU", WF3_CANNOT_FIRE_THROUGH); assert(res >= 0);

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
		res = engine->RegisterObjectMethod("Level", "void findSectorById(int)", asMETHOD(LS_Level, findSectorById), asCALL_THISCALL);  assert(res >= 0);
		// Script variable.
		res = engine->RegisterGlobalProperty("Level level", this);  assert(res >= 0);
		return res >= 0;
	}
}
#endif