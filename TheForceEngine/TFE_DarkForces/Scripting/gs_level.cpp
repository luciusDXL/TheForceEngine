#include "gs_level.h"
#include "scriptTexture.h"
#include "scriptElev.h"
#include "scriptWall.h"
#include "scriptSector.h"
#include <TFE_System/system.h>
#include <TFE_ForceScript/ScriptAPI-Shared/scriptMath.h>
#include <TFE_Jedi/InfSystem/infState.h>
#include <TFE_Jedi/InfSystem/infTypesInternal.h>
#include <TFE_Jedi/InfSystem/infSystem.h>
#include <TFE_Jedi/Level/levelData.h>
#include <TFE_Jedi/Level/rwall.h>
#include <TFE_Jedi/Level/rsector.h>
#include <TFE_Jedi/Level/rtexture.h>

#ifdef ENABLE_FORCE_SCRIPT
#include <angelscript.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	ScriptSector GS_Level::getSectorById(s32 id)
	{
		if (id < 0 || id >= (s32)s_levelState.sectorCount)
		{
			TFE_System::logWrite(LOG_ERROR, "Level Script", "Runtime error, invalid sectorID %d.", id);
			id = -1; // Invalid ID, sector.isValid() == false
		}
		ScriptSector sector(id);
		return sector;
	}

	ScriptElev GS_Level::getElevator(s32 id)
	{
		if (id < 0 || id >= allocator_getCount(s_infSerState.infElevators))
		{
			TFE_System::logWrite(LOG_ERROR, "Level Script", "Runtime error, invalid elevator ID %d.", id);
			id = -1; // Invalid ID, elevator.isValid() == false
		}
		ScriptElev elev(id);
		return elev;
	}

	bool GS_Level::scriptRegister(ScriptAPI api)
	{
		ScriptElev scriptElev;
		ScriptTexture scriptTex;
		ScriptSector scriptSector;
		ScriptWall scriptWall;
		scriptElev.registerType();
		scriptTex.registerType();
		scriptWall.registerType();
		scriptSector.registerType();
		
		ScriptClassBegin("Level", "level", api);
		{
			// Enums
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
			ScriptObjMethod("Sector getSector(int)", getSectorById);
			ScriptObjMethod("Elevator getElevator(int)", getElevator);
			// -- Getters --
			ScriptLambdaPropertyGet("int get_minLayer()", s32, { return s_levelState.minLayer; });
			ScriptLambdaPropertyGet("int get_maxLayer()", s32, { return s_levelState.maxLayer; });
			ScriptLambdaPropertyGet("int get_sectorCount()", s32, { return (s32)s_levelState.sectorCount; });
			ScriptLambdaPropertyGet("int get_secretCount()", s32, { return s_levelState.secretCount; });
			ScriptLambdaPropertyGet("int get_textureCount()", s32, { return s_levelState.textureCount; });
			ScriptLambdaPropertyGet("float2 get_parallax()", TFE_ForceScript::float2, { return TFE_ForceScript::float2(fixed16ToFloat(s_levelState.parallax0), fixed16ToFloat(s_levelState.parallax1)); });

			// Gameplay sector pointers.
			ScriptLambdaPropertyGet("Sector get_bossSector()", ScriptSector, { ScriptSector sector(-1); if (s_levelState.bossSector) { sector.m_id = s_levelState.bossSector->id; } return sector; });
			ScriptLambdaPropertyGet("Sector get_mohcSector()", ScriptSector, { ScriptSector sector(-1); if (s_levelState.mohcSector) { sector.m_id = s_levelState.mohcSector->id; } return sector; });
			ScriptLambdaPropertyGet("Sector get_completeSector()", ScriptSector, { ScriptSector sector(-1); if (s_levelState.completeSector) { sector.m_id = s_levelState.completeSector->id; } return sector; });
		}
		ScriptClassEnd();
	}
}
#endif