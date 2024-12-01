#include "gs_level.h"
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
	class ScriptTexture
	{
	public:
		ScriptTexture() : m_id(-1) {};
		ScriptTexture(s32 id) : m_id(id) {};
		void registerType();

	public:
		s32 m_id;
	};

	s32 getTextureIdFromData(TextureData** texData)
	{
		if (!texData || !*texData) { return -1; }

		s32 index;
		AssetPool pool;
		if (!bitmap_getTextureIndex(*texData, &index, &pool))
		{
			return -1;
		}
		return index;
	}
	bool isTextureValid(ScriptTexture* tex)
	{
		return tex->m_id >= 0;
	}

	void ScriptTexture::registerType()
	{
		s32 res = 0;
		asIScriptEngine* engine = (asIScriptEngine*)TFE_ForceScript::getEngine();

		ScriptValueType("LevelTexture");
		// Variables
		ScriptMemberVariable("int id", m_id);
		// Functions
		ScriptObjFunc("bool isValid()", isTextureValid);
	}

	class ScriptElev
	{
	public:
		ScriptElev() : m_id(-1) {};
		ScriptElev(s32 id) : m_id(id) {};
		void registerType();

	public:
		s32 m_id;
	};

	bool isScriptElevValid(ScriptElev* elev)
	{
		return elev->m_id >= 0 && elev->m_id < allocator_getCount(s_infSerState.infElevators);
	}
	// TODO- Replace by updateFlags and enum?
	bool getElevMaster(ScriptElev* elev)
	{
		if (!isScriptElevValid(elev)) { return false; }
		InfElevator* data = (InfElevator*)allocator_getByIndex(s_infSerState.infElevators, elev->m_id);
		return (data->updateFlags & ELEV_MASTER_ON) != 0;
	}
	
	void ScriptElev::registerType()
	{
		s32 res = 0;
		asIScriptEngine* engine = (asIScriptEngine*)TFE_ForceScript::getEngine();

		ScriptValueType("Elevator");
		// Variables
		ScriptMemberVariable("int id", m_id);
		// Functions
		ScriptObjFunc("bool isValid()", isScriptElevValid);
		// Properties
		ScriptPropertyGetFunc("bool get_master()", getElevMaster);
	}

	enum WallPart
	{
		WP_MIDDLE = 0,
		WP_TOP,
		WP_BOTTOM,
		WP_SIGN
	};

	class ScriptWall
	{
	public:
		ScriptWall() : m_sectorId(-1), m_wallId(-1) {};
		ScriptWall(s32 sectorId, s32 wallId) : m_sectorId(sectorId), m_wallId(wallId) {};
		void registerType();

	public:
		s32 m_sectorId;
		s32 m_wallId;
	};
		
	bool isScriptWallValid(ScriptWall* wall)
	{
		if (wall->m_sectorId < 0 || wall->m_sectorId >= (s32)s_levelState.sectorCount) { return false; }
		if (wall->m_wallId < 0 || wall->m_wallId >= s_levelState.sectors[wall->m_sectorId].wallCount) { return false; }
		return true;
	}
	bool isWallFlagSet(s32 index, u32 flag, ScriptWall* wall)
	{
		if (!isScriptWallValid(wall)) { return false; }
		if (index == 1) { return (s_levelState.sectors[wall->m_sectorId].walls[wall->m_wallId].flags1 & flag) != 0u; }
		else if (index == 2) { return (s_levelState.sectors[wall->m_sectorId].walls[wall->m_wallId].flags2 & flag) != 0u; }
		else if (index == 3) { return (s_levelState.sectors[wall->m_sectorId].walls[wall->m_wallId].flags3 & flag) != 0u; }
		return false;
	}
	void clearWallFlag(s32 index, u32 flag, ScriptWall* wall)
	{
		if (!isScriptWallValid(wall)) { return; }
		if (index == 1) { s_levelState.sectors[wall->m_sectorId].walls[wall->m_wallId].flags1 &= ~flag; }
		else if (index == 2) { s_levelState.sectors[wall->m_sectorId].walls[wall->m_wallId].flags2 &= ~flag; }
		else if (index == 3) { s_levelState.sectors[wall->m_sectorId].walls[wall->m_wallId].flags3 &= ~flag; }
	}
	void setWallFlag(s32 index, u32 flag, ScriptWall* wall)
	{
		if (!isScriptWallValid(wall)) { return; }
		if (index == 1) { s_levelState.sectors[wall->m_sectorId].walls[wall->m_wallId].flags1 |= flag; }
		else if (index == 2) { s_levelState.sectors[wall->m_sectorId].walls[wall->m_wallId].flags2 |= flag; }
		else if (index == 3) { s_levelState.sectors[wall->m_sectorId].walls[wall->m_wallId].flags3 |= flag; }
	}
	s32 getAdjoin(ScriptWall* wall)
	{
		if (!isScriptWallValid(wall)) { return -1; }
		RSector* next = s_levelState.sectors[wall->m_sectorId].walls[wall->m_wallId].nextSector;
		return next ? next->id : -1;
	}
	s32 getMirror(ScriptWall* wall)
	{
		if (!isScriptWallValid(wall)) { return -1; }
		RWall* next = s_levelState.sectors[wall->m_sectorId].walls[wall->m_wallId].mirrorWall;
		return next ? next->id : -1;
	}
	f32 getWallLight(ScriptWall* wall)
	{
		if (!isScriptWallValid(wall)) { return 0; }
		return fixed16ToFloat(s_levelState.sectors[wall->m_sectorId].walls[wall->m_wallId].wallLight);
	}
	ScriptTexture getWallTexture(WallPart part, ScriptWall* wall)
	{
		ScriptTexture tex(-1);
		if (isScriptWallValid(wall))
		{
			RWall* lvlWall = &s_levelState.sectors[wall->m_sectorId].walls[wall->m_wallId];
			switch (part)
			{
				case WP_MIDDLE: tex.m_id = getTextureIdFromData(lvlWall->midTex); break;
				case WP_TOP:    tex.m_id = getTextureIdFromData(lvlWall->topTex); break;
				case WP_BOTTOM: tex.m_id = getTextureIdFromData(lvlWall->botTex); break;
				case WP_SIGN:   tex.m_id = getTextureIdFromData(lvlWall->signTex); break;
			}
		}
		return tex;
	}
	TFE_ForceScript::float2 getWallTextureOffset(WallPart part, ScriptWall* wall)
	{
		TFE_ForceScript::float2 offset(0.0f, 0.0f);
		if (isScriptWallValid(wall))
		{
			RWall* lvlWall = &s_levelState.sectors[wall->m_sectorId].walls[wall->m_wallId];
			switch (part)
			{
				case WP_MIDDLE:
				{
					offset.x = fixed16ToFloat(lvlWall->midOffset.x);
					offset.y = fixed16ToFloat(lvlWall->midOffset.z);
				} break;
				case WP_TOP:
				{
					offset.x = fixed16ToFloat(lvlWall->topOffset.x);
					offset.y = fixed16ToFloat(lvlWall->topOffset.z);
				} break;
				case WP_BOTTOM:
				{
					offset.x = fixed16ToFloat(lvlWall->botOffset.x);
					offset.y = fixed16ToFloat(lvlWall->botOffset.z);
				} break;
				case WP_SIGN:
				{
					offset.x = fixed16ToFloat(lvlWall->signOffset.x);
					offset.y = fixed16ToFloat(lvlWall->signOffset.z);
				} break;
			}
		}
		return offset;
	}
	TFE_ForceScript::float2 getVertex(s32 index, ScriptWall* wall)
	{
		TFE_ForceScript::float2 vtx(0.0f, 0.0f);
		if (!isScriptWallValid(wall)) { return vtx; }

		RWall* lvlWall = &s_levelState.sectors[wall->m_sectorId].walls[wall->m_wallId];
		vtx.x = fixed16ToFloat(index == 0 ? lvlWall->w0->x : lvlWall->w1->x);
		vtx.y = fixed16ToFloat(index == 0 ? lvlWall->w0->z : lvlWall->w1->z);
		return vtx;
	}
	TFE_ForceScript::float2 getVertexBase(ScriptWall* wall)
	{
		TFE_ForceScript::float2 vtx(0.0f, 0.0f);
		if (!isScriptWallValid(wall)) { return vtx; }

		RWall* lvlWall = &s_levelState.sectors[wall->m_sectorId].walls[wall->m_wallId];
		vtx.x = fixed16ToFloat(lvlWall->worldPos0.x);
		vtx.y = fixed16ToFloat(lvlWall->worldPos0.z);
		return vtx;
	}
	void setAdjoin(s32 id, ScriptWall* wall)
	{
		if (!isScriptWallValid(wall)) { return; }
		RSector* sector = &s_levelState.sectors[wall->m_sectorId];
		RWall* lvlWall = &sector->walls[wall->m_wallId];
		if (id < 0 || id >= (s32)s_levelState.sectorCount)
		{
			lvlWall->nextSector = nullptr;

			sector_setupWallDrawFlags(sector);
		}
		else
		{
			lvlWall->nextSector = &s_levelState.sectors[id];

			sector_setupWallDrawFlags(sector);
			sector_setupWallDrawFlags(lvlWall->nextSector);
		}
	}
	void setMirror(s32 id, ScriptWall* wall)
	{
		if (!isScriptWallValid(wall)) { return; }
		RSector* sector = &s_levelState.sectors[wall->m_sectorId];
		RWall* lvlWall = &sector->walls[wall->m_wallId];
		RSector* next = lvlWall->nextSector;
		if (!next) { return; }

		if (id < 0 || id >= next->wallCount)
		{
			lvlWall->mirror = -1;
			lvlWall->mirrorWall = nullptr;
		}
		else
		{
			lvlWall->mirror = id;
			lvlWall->mirrorWall = &next->walls[id];
			sector_setupWallDrawFlags(lvlWall->nextSector);
		}
		sector_setupWallDrawFlags(sector);
	}
	void setWallLight(f32 light, ScriptWall* wall)
	{
		if (!isScriptWallValid(wall)) { return; }
		RSector* sector = &s_levelState.sectors[wall->m_sectorId];
		sector->walls[wall->m_wallId].wallLight = floatToFixed16(light);
		sector->dirtyFlags |= SDF_AMBIENT;
	}
	void setWallTexture(WallPart part, ScriptTexture tex, ScriptWall* wall)
	{
		if (isScriptWallValid(wall) && isTextureValid(&tex))
		{
			RSector* sector = &s_levelState.sectors[wall->m_sectorId];
			RWall* lvlWall = &sector->walls[wall->m_wallId];
			TextureData** data = &s_levelState.textures[tex.m_id];
			switch (part)
			{
				case WP_MIDDLE: lvlWall->midTex = data; break;
				case WP_TOP:    lvlWall->topTex = data; break;
				case WP_BOTTOM: lvlWall->botTex = data; break;
				case WP_SIGN:   lvlWall->signTex = data; break;
			}
		}
	}
	void setWallTextureOffset(WallPart part, TFE_ForceScript::float2 offset, ScriptWall* wall)
	{
		if (isScriptWallValid(wall))
		{
			RWall* lvlWall = &s_levelState.sectors[wall->m_sectorId].walls[wall->m_wallId];
			const vec2_fixed fixedOffset = { floatToFixed16(offset.x), floatToFixed16(offset.y) };
			switch (part)
			{
				case WP_MIDDLE: lvlWall->midOffset = fixedOffset; break;
				case WP_TOP:    lvlWall->topOffset = fixedOffset; break;
				case WP_BOTTOM: lvlWall->botOffset = fixedOffset; break;
				case WP_SIGN:   lvlWall->signOffset = fixedOffset; break;
			}
		}
	}
	void setVertex(TFE_ForceScript::float2 vtx, s32 index, ScriptWall* wall)
	{
		if (!isScriptWallValid(wall)) { return; }

		RSector* sector = &s_levelState.sectors[wall->m_sectorId];
		RWall* lvlWall = &sector->walls[wall->m_wallId];
		if (index == 0)
		{
			lvlWall->w0->x = floatToFixed16(vtx.x);
			lvlWall->w0->z = floatToFixed16(vtx.y);
		}
		else
		{
			lvlWall->w1->x = floatToFixed16(vtx.x);
			lvlWall->w1->z = floatToFixed16(vtx.y);
		}
		sector->dirtyFlags |= (SDF_VERTICES | SDF_WALL_SHAPE);
	}
		
	void ScriptWall::registerType()
	{
		s32 res = 0;
		asIScriptEngine* engine = (asIScriptEngine*)TFE_ForceScript::getEngine();

		ScriptEnumRegister("WallPart");
		ScriptEnumStr(WP_MIDDLE);
		ScriptEnumStr(WP_TOP);
		ScriptEnumStr(WP_BOTTOM);
		ScriptEnumStr(WP_SIGN);

		ScriptValueType("Wall");
		// Variables
		ScriptMemberVariable("int sectorId", m_sectorId);
		ScriptMemberVariable("int wallId", m_wallId);
		// Functions
		ScriptObjFunc("bool isValid()", isScriptWallValid);
		ScriptObjFunc("bool isFlagSet(int, uint)", isWallFlagSet);
		ScriptObjFunc("void clearFlag(int, uint)", clearWallFlag);
		ScriptObjFunc("void setFlag(int, uint)", setWallFlag);
		ScriptObjFunc("float2 getVertex(int index = 0)", getVertex);
		ScriptObjFunc("float2 getVertexBase()", getVertexBase);
		ScriptObjFunc("void setVertex(float2, int index = 0)", setVertex);
		ScriptObjFunc("LevelTexture getTexture(WallPart)", getWallTexture);
		ScriptObjFunc("float2 getTextureOffset(WallPart)", getWallTextureOffset);
		ScriptObjFunc("void setTexture(WallPart, LevelTexture)", setWallTexture);
		ScriptObjFunc("void setTextureOffset(WallPart, float2)", setWallTextureOffset);
		// Properties
		ScriptPropertyGetFunc("int get_adjoin()", getAdjoin);
		ScriptPropertyGetFunc("int get_mirror()", getMirror);
		ScriptPropertyGetFunc("float get_wallLight()", getWallLight);
		ScriptPropertySetFunc("void set_adjoin(int)", setAdjoin);
		ScriptPropertySetFunc("void set_mirror(int)", setMirror);
		ScriptPropertySetFunc("void set_wallLight(float)", setWallLight);
	}

	class ScriptSector
	{
	public:
		ScriptSector() : m_id(-1) {};
		ScriptSector(s32 id) : m_id(id) {};
		void registerType();

	public:
		s32 m_id;
	};

	f32 getFloorHeight(ScriptSector* sector)  { return sector->m_id >= 0 ? fixed16ToFloat(s_levelState.sectors[sector->m_id].floorHeight) : 0; }
	f32 getCeilHeight(ScriptSector* sector)   { return sector->m_id >= 0 ? fixed16ToFloat(s_levelState.sectors[sector->m_id].ceilingHeight) : 0; }
	f32 getSecondHeight(ScriptSector* sector) { return sector->m_id >= 0 ? fixed16ToFloat(s_levelState.sectors[sector->m_id].secHeight) : 0; }
	s32 getWallCount(ScriptSector* sector) { return sector->m_id >= 0 ? s_levelState.sectors[sector->m_id].wallCount : 0; }
	f32 getAmbient(ScriptSector* sector) { return sector->m_id >= 0 ? fixed16ToFloat(s_levelState.sectors[sector->m_id].ambient) : 0; }

	void setFloorHeight(f32 height, ScriptSector* sector)
	{ 
		if (sector->m_id < 0) { return; }
		const fixed16_16 offset = floatToFixed16(height) - s_levelState.sectors[sector->m_id].floorHeight;
		sector_adjustTextureWallOffsets_Floor(&s_levelState.sectors[sector->m_id], offset);
		sector_adjustTextureMirrorOffsets_Floor(&s_levelState.sectors[sector->m_id], offset);
		sector_adjustHeights(&s_levelState.sectors[sector->m_id], offset, 0, 0);
	}
	void setCeilHeight(f32 height, ScriptSector* sector)
	{
		if (sector->m_id < 0) { return; }
		const fixed16_16 offset = floatToFixed16(height) - s_levelState.sectors[sector->m_id].ceilingHeight;
		sector_adjustHeights(&s_levelState.sectors[sector->m_id], 0, offset, 0);
	}
	void setSecondHeight(f32 height, ScriptSector* sector)
	{
		if (sector->m_id < 0) { return; }
		const fixed16_16 offset = floatToFixed16(height) - s_levelState.sectors[sector->m_id].secHeight;
		sector_adjustHeights(&s_levelState.sectors[sector->m_id], 0, 0, offset);
	}
	void setAmbient(f32 ambient, ScriptSector* sector)
	{
		if (sector->m_id < 0) { return; }
		s_levelState.sectors[sector->m_id].ambient = floatToFixed16(ambient);
		s_levelState.sectors[sector->m_id].dirtyFlags |= SDF_AMBIENT;
	}
	bool isScriptSectorValid(ScriptSector* sector)
	{
		return sector->m_id >= 0 && sector->m_id < (s32)s_levelState.sectorCount;
	}
	bool isSectorFlagSet(s32 index, u32 flag, ScriptSector* sector)
	{
		if (!isScriptSectorValid(sector)) { return false; }
		if (index == 1) { return (s_levelState.sectors[sector->m_id].flags1 & flag) != 0u; }
		else if (index == 2) { return (s_levelState.sectors[sector->m_id].flags2 & flag) != 0u; }
		else if (index == 3) { return (s_levelState.sectors[sector->m_id].flags3 & flag) != 0u; }
		return false;
	}
	void clearSectorFlag(s32 index, u32 flag, ScriptSector* sector)
	{
		if (!isScriptSectorValid(sector)) { return; }
		if (index == 1) { s_levelState.sectors[sector->m_id].flags1 &= ~flag; }
		else if (index == 2) { s_levelState.sectors[sector->m_id].flags2 &= ~flag; }
		else if (index == 3) { s_levelState.sectors[sector->m_id].flags3 &= ~flag; }
	}
	void setSectorFlag(s32 index, u32 flag, ScriptSector* sector)
	{
		if (!isScriptSectorValid(sector)) { return; }
		if (index == 1) { s_levelState.sectors[sector->m_id].flags1 |= flag; }
		else if (index == 2) { s_levelState.sectors[sector->m_id].flags2 |= flag; }
		else if (index == 3) { s_levelState.sectors[sector->m_id].flags3 |= flag; }
	}
	TFE_ForceScript::float2 getCenterXZ(ScriptSector* sector)
	{
		TFE_ForceScript::float2 cen(0.0f, 0.0f);
		if (!isScriptSectorValid(sector)) { return cen; }
		cen.x = fixed16ToFloat(s_levelState.sectors[sector->m_id].boundsMin.x + s_levelState.sectors[sector->m_id].boundsMax.x) * 0.5f;
		cen.y = fixed16ToFloat(s_levelState.sectors[sector->m_id].boundsMin.z + s_levelState.sectors[sector->m_id].boundsMax.z) * 0.5f;
		return cen;
	}
	ScriptWall getWall(s32 index, ScriptSector* sector)
	{
		ScriptWall wall(-1, -1);
		if (isScriptSectorValid(sector))
		{
			if (index >= 0 && index < s_levelState.sectors[sector->m_id].wallCount)
			{
				wall.m_sectorId = sector->m_id;
				wall.m_wallId = index;
			}
		}
		return wall;
	}
	ScriptTexture getFloorTexture(ScriptSector* sector)
	{
		ScriptTexture tex(-1);
		if (isScriptSectorValid(sector))
		{
			tex.m_id = getTextureIdFromData(s_levelState.sectors[sector->m_id].floorTex);
		}
		return tex;
	}
	ScriptTexture getCeilTexture(ScriptSector* sector)
	{
		ScriptTexture tex(-1);
		if (isScriptSectorValid(sector))
		{
			tex.m_id = getTextureIdFromData(s_levelState.sectors[sector->m_id].ceilTex);
		}
		return tex;
	}
	TFE_ForceScript::float2 getFloorTextureOffset(ScriptSector* sector)
	{
		TFE_ForceScript::float2 offset(0.0f, 0.0f);
		if (isScriptSectorValid(sector))
		{
			offset.x = fixed16ToFloat(s_levelState.sectors[sector->m_id].floorOffset.x);
			offset.y = fixed16ToFloat(s_levelState.sectors[sector->m_id].floorOffset.z);
		}
		return offset;
	}
	TFE_ForceScript::float2 getCeilTextureOffset(ScriptSector* sector)
	{
		TFE_ForceScript::float2 offset(0.0f, 0.0f);
		if (isScriptSectorValid(sector))
		{
			offset.x = fixed16ToFloat(s_levelState.sectors[sector->m_id].ceilOffset.x);
			offset.y = fixed16ToFloat(s_levelState.sectors[sector->m_id].ceilOffset.z);
		}
		return offset;
	}
	void setFloorTexture(ScriptTexture tex, ScriptSector* sector)
	{
		if (isScriptSectorValid(sector) && isTextureValid(&tex))
		{
			s_levelState.sectors[sector->m_id].floorTex = &s_levelState.textures[tex.m_id];
		}
	}
	void setCeilTexture(ScriptTexture tex, ScriptSector* sector)
	{
		if (isScriptSectorValid(sector) && isTextureValid(&tex))
		{
			s_levelState.sectors[sector->m_id].ceilTex = &s_levelState.textures[tex.m_id];
		}
	}
	void setFloorTextureOffset(TFE_ForceScript::float2 offset, ScriptSector* sector)
	{
		if (isScriptSectorValid(sector))
		{
			s_levelState.sectors[sector->m_id].floorOffset.x = floatToFixed16(offset.x);
			s_levelState.sectors[sector->m_id].floorOffset.z = floatToFixed16(offset.y);
		}
	}
	void setCeilTextureOffset(TFE_ForceScript::float2 offset, ScriptSector* sector)
	{
		if (isScriptSectorValid(sector))
		{
			s_levelState.sectors[sector->m_id].ceilOffset.x = floatToFixed16(offset.x);
			s_levelState.sectors[sector->m_id].ceilOffset.z = floatToFixed16(offset.y);
		}
	}

	void ScriptSector::registerType()
	{
		s32 res = 0;
		asIScriptEngine* engine = (asIScriptEngine*)TFE_ForceScript::getEngine();
		// TODO: Objects

		ScriptValueType("Sector");
		// Variables
		ScriptMemberVariable("int id", m_id);
		// Functions
		ScriptObjFunc("bool isValid()", isScriptSectorValid);
		ScriptObjFunc("bool isFlagSet(int, uint)", isSectorFlagSet);
		ScriptObjFunc("void clearFlag(int, uint)", clearSectorFlag);
		ScriptObjFunc("void setFlag(int, uint)", setSectorFlag);
		ScriptObjFunc("float2 getCenterXZ()", getCenterXZ);
		ScriptObjFunc("Wall getWall(int)", getWall);
		// Properties
		ScriptPropertyGetFunc("float get_floorHeight()", getFloorHeight);
		ScriptPropertyGetFunc("float get_ceilHeight()", getCeilHeight);
		ScriptPropertyGetFunc("float get_secondHeight()", getSecondHeight);
		ScriptPropertyGetFunc("float get_ambient()", getAmbient);
		ScriptPropertyGetFunc("int get_wallCount()", getWallCount);
		ScriptPropertyGetFunc("LevelTexture get_floorTexture()", getFloorTexture);
		ScriptPropertyGetFunc("LevelTexture get_ceilTexture()", getCeilTexture);
		ScriptPropertyGetFunc("float2 get_floorTextureOffset()", getFloorTextureOffset);
		ScriptPropertyGetFunc("float2 get_ceilTextureOffset()", getCeilTextureOffset);
		ScriptPropertySetFunc("void set_floorHeight(float)", setFloorHeight);
		ScriptPropertySetFunc("void set_ceilHeight(float)", setCeilHeight);
		ScriptPropertySetFunc("void set_secondHeight(float)", setSecondHeight);
		ScriptPropertySetFunc("void set_ambient(float)", setAmbient);
		ScriptPropertySetFunc("void set_floorTexture(LevelTexture)", setFloorTexture);
		ScriptPropertySetFunc("void set_ceilTexture(LevelTexture)", setCeilTexture);
		ScriptPropertySetFunc("void set_floorTextureOffset(float2)", setFloorTextureOffset);
		ScriptPropertySetFunc("void set_ceilTextureOffset(float2)", setCeilTextureOffset);
	}

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
		ScriptElev elev(-1);
		if (id < 0 || id >= allocator_getCount(s_infSerState.infElevators))
		{
			TFE_System::logWrite(LOG_ERROR, "Level Script", "Runtime error, invalid elevator ID %d.", id);
		}
		else
		{
			elev.m_id = id;
		}
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