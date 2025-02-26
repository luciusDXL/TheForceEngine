#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Editor
// A system built to view and edit Dark Forces data files.
// The viewing aspect needs to be put in place at the beginning
// in order to properly test elements in isolation without having
// to "play" the game as intended.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <TFE_System/parser.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Editor/EditorAsset/editorObj3D.h>
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
		EVARTYPE_COUNT,
		EVARTYPE_UNKNOWN = EVARTYPE_COUNT
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
		s32 id = -1;
		std::string name;
		EntityVarType type = EVARTYPE_UNKNOWN;
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

	struct EntityLogic
	{
		std::string name;
		std::vector<EntityVar> var;
	};

	struct SpriteView
	{
		Vec2f uv[2] = { {0.0f, 0.0f}, {1.0f,  1.0f} };
		Vec2f st[2] = { {0.0f, 0.0f}, {64.0f, 64.0f} };
	};
		
	struct Entity
	{
		s32 id = -1;
		s32 categories = 0;
		std::string name;
		std::string assetName;
		EntityType type = ETYPE_UNKNOWN;
		std::vector<EntityLogic> logic;
		std::vector<EntityVar> var;

		// Sprite
		TextureGpu* image = nullptr;
		std::vector<SpriteView> views;	// optional sprite views.
		Vec2f uv[2] = { {0.0f, 0.0f}, {1.0f,  1.0f} };
		Vec2f st[2] = { {0.0f, 0.0f}, {64.0f, 64.0f} };
		Vec2f size = { 1.0f, 1.0f };
				
		// 3D object.
		TFE_Editor::EditorObj3D* obj3d = nullptr;

		// Bounds and offsets.
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

	struct Category
	{
		s32 flag = 0;
		std::string name;
		std::string tooltip;
	};

	extern std::vector<Entity> s_entityDefList;
	extern std::vector<Entity> s_projEntityDefList;

	extern std::vector<LogicDef> s_logicDefList;
	extern std::vector<EntityVarDef> s_varDefList;
	extern std::vector<Category> s_categoryList;
	extern u32 s_enemyCategoryFlag;

	struct EditorObject
	{
		s32 entityId;
		Vec3f pos;
		f32 angle;
		f32 pitch, roll;
		s32 diff;

		// Derived (don't save).
		Mat3 transform;
	};

	bool loadEntityData(const char* localDir, bool projectList = false);
	bool loadSingleEntityData(Entity* entity);
	bool loadVariableData(const char* localDir);
	bool loadLogicData(const char* localDir);

	bool writeEntityDataToString(const Entity* entity, char* buffer, size_t bufferSize);
	bool writeEntityDataBinary(const Entity* entity, FileStream* file);
	bool readEntityDataBinary(FileStream* file, Entity* entity, s32 version);

	void saveProjectEntityList();

	const char* getEntityVarName(s32 id);
	EntityVarDef* getEntityVar(s32 id);

	s32 getVariableId(const char* varName);
	EntityVarDef* getEntityVar(s32 id);

	void parseValue(const TokenList& tokens, EntityVarType type, EntityVarValue* value);

	bool entityDefsEqual(const Entity* e0, const Entity* e1);
	bool entityDefsEqualIgnoreName(const Entity* e0, const Entity* e1);
	s32 getEntityDefId(const Entity* entity);

	s32 getLogicId(const char* name);
}
