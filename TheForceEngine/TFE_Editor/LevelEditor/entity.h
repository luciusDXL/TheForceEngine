#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <vector>
#include <string>

namespace LevelEditor
{
	enum EntityKey
	{
		EKEY_ENTITY = 0,
		EKEY_CLASS,
		EKEY_LOGIC,
		EKEY_ICON,
		EKEY_PLACEMENT,
		EKEY_ASSET,
		EKEY_ASSET_OFFSET_Y,
		EKEY_ASSET2,
		EKEY_ASSET2_OFFSET_Y,
		EKEY_EYE,
		EKEY_RADIUS,
		EKEY_HEIGHT,
		EKEY_COUNT,
		EKEY_UNKNOWN = EKEY_COUNT
	};

	enum EntityType
	{
		ETYPE_SPIRIT = 0,
		ETYPE_SAFE,
		ETYPE_SPRITE,
		ETYPE_FRAME,
		ETYPE_3D,
		ETYPE_COUNT,
		ETYPE_UNKNOWN
	};

	enum EntityVarId
	{
		EVARID_EYE = 0,
		EVARID_RADIUS,
		EVARID_HEIGHT,
		EVARID_COUNT
	};

	enum EntityVarType
	{
		EVARTYPE_BOOL = 0,
		EVARTYPE_FLOAT,
		EVARTYPE_INT,
		EVARTYPE_COUNT
	};

	struct EntityVarDef
	{
		EntityVarId id;
		EntityVarType type;
	};

	struct EntityVarValue
	{
		union
		{
			bool bValue;
			f32  fValue;
			s32  iValue;
		};
	};

	struct EntityVar
	{
		EntityVarDef def;
		EntityVarValue value;
	};

	struct Entity
	{
		s32 id;
		std::string name;
		std::string assetName;
		EntityType type = ETYPE_UNKNOWN;
		std::vector<std::string> logic;
		std::vector<EntityVar> var;

		TextureGpu* image = nullptr;
		Vec2f uv[2] = { {0.0f, 0.0f}, {1.0f,  1.0f} };
		Vec2f st[2] = { {0.0f, 0.0f}, {64.0f, 64.0f} };
		Vec2f size = { 1.0f, 1.0f };

		Vec3f bounds[2] = { 0 };
		Vec3f offset = { 0 };
		Vec3f offsetAdj = { 0 };
	};
	extern std::vector<Entity> s_entityList;

	struct EditorObject
	{
		s32 entityId;
		Vec3f pos;
		f32 angle;
	};

	bool loadEntityData();
}
