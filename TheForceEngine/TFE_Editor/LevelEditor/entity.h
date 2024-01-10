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
		EVARTYPE_FLAGS,
		EVARTYPE_STRING_LIST,
		EVARTYPE_INPUT_STRING_PAIR,
		EVARTYPE_COUNT
	};

	struct EntityVarFlag
	{
		std::string name;
		s32 value;
	};

	struct EntityVarValue
	{
		union
		{
			bool bValue;
			f32  fValue;
			s32  iValue;
		};
		std::string sValue;
		std::string sValue1;	// Used when a "stringpair" is needed.
	};

	struct EntityVarDef
	{
		s32 id;
		std::string name;
		EntityVarType type;
		// Only used if type = EVARTYPE_FLAGS
		// Stored Value Type = int (index)
		std::vector<EntityVarFlag> flags;
		// Only used if type = EVARTYPE_STRING_LIST
		// Stored Value Type = string
		std::vector<std::string> strList;
		// Names, used for inputs.
		std::string name0;
		std::string name1;
		// Default value.
		EntityVarValue defValue;
		EntityVarValue defValue1;
	};
		
	struct EntityVar
	{
		s32 defId;
		EntityVarValue value;
	};

	struct Entity
	{
		s32 id = -1;
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

	enum LogicVarType
	{
		VAR_TYPE_STD = 0,
		VAR_TYPE_DEFAULT,
		VAR_TYPE_REQUIRED,
		VAR_TYPE_COUNT
	};

	struct LogicVar
	{
		s32 varId;
		LogicVarType type;
	};

	struct LogicDef
	{
		s32 id;
		std::string name;
		std::string tooltip;
		std::vector<LogicVar> var;
	};

	extern std::vector<Entity> s_entityList;
	extern std::vector<LogicDef> s_logicDefList;
	extern std::vector<EntityVarDef> s_varDefList;

	struct EditorObject
	{
		s32 entityId;
		Vec3f pos;
		f32 angle;
		s32 diff;
	};

	bool loadEntityData(const char* localDir);
	bool loadVariableData(const char* localDir);
	bool loadLogicData(const char* localDir);
	const char* getEntityVarName(s32 id);
}
