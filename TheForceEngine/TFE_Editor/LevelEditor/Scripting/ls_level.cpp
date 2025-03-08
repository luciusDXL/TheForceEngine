#include "ls_level.h"
#include <TFE_System/system.h>
#include <TFE_Editor/LevelEditor/infoPanel.h>
#include <TFE_Editor/LevelEditor/sharedState.h>
#include <TFE_Jedi/Level/rwall.h>
#include <TFE_Jedi/Level/rsector.h>
#include <angelscript.h>
namespace LevelEditor
{
	#define Vec2f_to_float2(v) TFE_ForceScript::float2(v.x, v.z)
	#define float2_to_Vec2f(v) Vec2f{v.x, v.y};

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

	bool LS_Level::scriptRegister(ScriptAPI api)
	{
		ScriptClassBegin("Level", "level", api);
		{
			// Enums
			ScriptEnumRegister("WallPart");
			ScriptEnumStr(WP_MID);
			ScriptEnumStr(WP_TOP);
			ScriptEnumStr(WP_BOT);
			ScriptEnumStr(WP_SIGN);

			ScriptEnumRegister("SectorFlags1");
			ScriptEnum("SECTOR_FLAG1_EXTERIOR",        SEC_FLAGS1_EXTERIOR);
			ScriptEnum("SECTOR_FLAG1_DOOR",            SEC_FLAGS1_DOOR);
			ScriptEnum("SECTOR_FLAG1_MAG_SEAL",        SEC_FLAGS1_MAG_SEAL);
			ScriptEnum("SECTOR_FLAG1_EXTERIOR_ADJOIN", SEC_FLAGS1_EXT_ADJ);
			ScriptEnum("SECTOR_FLAG1_ICE_FLOOR",       SEC_FLAGS1_ICE_FLOOR);
			ScriptEnum("SECTOR_FLAG1_SNOW_FLOOR",      SEC_FLAGS1_SNOW_FLOOR);
			ScriptEnum("SECTOR_FLAG1_EXPLODING_WALL",  SEC_FLAGS1_EXP_WALL);
			ScriptEnum("SECTOR_FLAG1_PIT",             SEC_FLAGS1_PIT);
			ScriptEnum("SECTOR_FLAG1_PIT_ADJOIN",      SEC_FLAGS1_EXT_FLOOR_ADJ);
			ScriptEnum("SECTOR_FLAG1_CRUSHING",        SEC_FLAGS1_CRUSHING);
			ScriptEnum("SECTOR_FLAG1_NOWALL_DRAW",     SEC_FLAGS1_NOWALL_DRAW);
			ScriptEnum("SECTOR_FLAG1_LOW_DAMAGE",      SEC_FLAGS1_LOW_DAMAGE);
			ScriptEnum("SECTOR_FLAG1_HIGH_DAMAGE",     SEC_FLAGS1_HIGH_DAMAGE);
			ScriptEnum("SECTOR_FLAG1_NO_SMART_OBJ",    SEC_FLAGS1_NO_SMART_OBJ);
			ScriptEnum("SECTOR_FLAG1_SMART_OBJ",       SEC_FLAGS1_SMART_OBJ);
			ScriptEnum("SECTOR_FLAG1_SAFE_SECTOR",     SEC_FLAGS1_SAFESECTOR);
			ScriptEnum("SECTOR_FLAG1_PLAYER",          SEC_FLAGS1_PLAYER);
			ScriptEnum("SECTOR_FLAG1_SECRET",          SEC_FLAGS1_SECRET);

			ScriptEnumRegister("WallFlags1");
			ScriptEnum("WALL_FLAGS1_TRANSPARENT_MIDTEX", WF1_ADJ_MID_TEX);
			ScriptEnum("WALL_FLAGS1_ILLUM_SIGN",         WF1_ILLUM_SIGN);
			ScriptEnum("WALL_FLAGS1_TEX_FLIP",           WF1_FLIP_HORIZ);
			ScriptEnum("WALL_FLAGS1_CHANGE_WALL_LIGHT",  WF1_CHANGE_WALL_LIGHT);
			ScriptEnum("WALL_FLAGS1_TEX_ANCHORED",       WF1_TEX_ANCHORED);
			ScriptEnum("WALL_FLAGS1_WALL_MORPHS",        WF1_WALL_MORPHS);
			ScriptEnum("WALL_FLAGS1_SCROLL_TOP_TEX",     WF1_SCROLL_TOP_TEX);
			ScriptEnum("WALL_FLAGS1_SCROLL_MID_TEX",     WF1_SCROLL_MID_TEX);
			ScriptEnum("WALL_FLAGS1_SCROLL_BOT_TEX",     WF1_SCROLL_BOT_TEX);
			ScriptEnum("WALL_FLAGS1_SCROLL_SIGN_TEX",    WF1_SCROLL_SIGN_TEX);
			ScriptEnum("WALL_FLAGS1_HIDE_ON_MAP",        WF1_HIDE_ON_MAP);
			ScriptEnum("WALL_FLAGS1_SHOW_NORMAL_ON_MAP", WF1_SHOW_NORMAL_ON_MAP);
			ScriptEnum("WALL_FLAGS1_SIGN_ANCHORED",      WF1_SIGN_ANCHORED);
			ScriptEnum("WALL_FLAGS1_DAMAGE_WALL",        WF1_DAMAGE_WALL);
			ScriptEnum("WALL_FLAGS1_SHOW_AS_LEDGE_ON_MAP", WF1_SHOW_AS_LEDGE_ON_MAP);
			ScriptEnum("WALL_FLAGS1_SHOW_AS_DOOR_ON_MAP", WF1_SHOW_AS_DOOR_ON_MAP);

			ScriptEnumRegister("WallFlags3");
			ScriptEnum("WALL_FLAGS3_ALWAYS_WALK",      WF3_ALWAYS_WALK);
			ScriptEnum("WALL_FLAGS3_SOLID",            WF3_SOLID_WALL);
			ScriptEnum("WALL_FLAGS3_PLAYER_WALK_ONLY", WF3_PLAYER_WALK_ONLY);
			ScriptEnum("WALL_FLAGS3_CANNOT_FIRE_THRU", WF3_CANNOT_FIRE_THROUGH);

			// Functions
			ScriptObjMethod("void findSector(const string &in)", findSector);
			ScriptObjMethod("void findSectorById(int)", findSectorById);
			// -- Getters --
			ScriptLambdaPropertyGet("string get_name()", std::string, { return s_level.name; });
			ScriptLambdaPropertyGet("string get_slot()", std::string, { return s_level.slot; });
			ScriptLambdaPropertyGet("string get_palette()", std::string, { return s_level.palette; });
			ScriptLambdaPropertyGet("int get_sectorCount()", s32, { return (s32)s_level.sectors.size(); });
			ScriptLambdaPropertyGet("int get_entityCount()", s32, { return (s32)s_level.entities.size(); });
			ScriptLambdaPropertyGet("int get_levelNoteCount()", s32, { return (s32)s_level.notes.size(); });
			ScriptLambdaPropertyGet("int get_guidelineCount()", s32, { return (s32)s_level.guidelines.size(); });
			ScriptLambdaPropertyGet("int get_minLayer()", s32, { return s_level.layerRange[0]; });
			ScriptLambdaPropertyGet("int get_maxLayer()", s32, { return s_level.layerRange[1]; });
			ScriptLambdaPropertyGet("float2 get_parallax()", TFE_ForceScript::float2, { return Vec2f_to_float2(s_level.parallax); });
			// TODO Vec3f s_level.bounds[2]
			//      Types: Sector, Entity, LevelNote, Guideline
			// -- Setters --
			ScriptLambdaPropertySet("void set_name(const string &in)", (std::string& name), { s_level.name = name; });
			ScriptLambdaPropertySet("void set_palette(const string &in)", (std::string& palette), { s_level.palette = palette; });
			
			// Setup a script variables.
			ScriptMemberVariable("int index", m_index);
		}
		ScriptClassEnd();
	}
}
