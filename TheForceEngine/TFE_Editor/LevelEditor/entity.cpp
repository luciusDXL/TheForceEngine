#include "entity.h"
#include "error.h"
#include "levelEditor.h"
#include "infoPanel.h"
#include "sharedState.h"
#include <TFE_Editor/editor.h>
#include <TFE_Editor/errorMessages.h>
#include <TFE_Editor/AssetBrowser/assetBrowser.h>
#include <TFE_Editor/EditorAsset/editorTexture.h>
#include <TFE_Editor/EditorAsset/editorFrame.h>
#include <TFE_Editor/EditorAsset/editorSprite.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_System/system.h>
#include <TFE_System/parser.h>
#include <TFE_System/math.h>
#include <TFE_Input/input.h>

#include <TFE_Ui/imGUI/imgui.h>
#include <algorithm>
#include <vector>
#include <string>
#include <map>

using namespace TFE_Editor;

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

	enum LogicKey
	{
		LKEY_LOGIC = 0,
		LKEY_TOOLTIP,
		LKEY_VAR,
		LKEY_DEFVAR,
		LKEY_REQVAR,
		LKEY_COUNT,
		LKEY_UNKNOWN = LKEY_COUNT
	};

	enum VarKey
	{
		VKEY_VAR = 0,
		VKEY_TYPE,
		VKEY_DEFAULT,
		VKEY_DEFAULT1,
		VKEY_FLAG,
		VKEY_VALUE,
		VKEY_NAME0,
		VKEY_NAME1,
		VKEY_COUNT,
		VKEY_UNKNOWN = VKEY_COUNT
	};

	const char* c_entityKeyStr[EKEY_COUNT] =
	{
		"Entity",          // EKEY_ENTITY
		"Class",           // EKEY_CLASS
		"Logic",           // EKEY_LOGIC
		"Icon",            // EKEY_ICON
		"Placement",       // EKEY_PLACEMENT
		"Asset",           // EKEY_ASSET
		"Asset_Offset_Y",  // EKEY_ASSET_OFFSET_Y
		"Asset2",          // EKEY_ASSET2
		"Asset2_Offset_Y", // EKEY_ASSET2_OFFSET_Y
		"Eye",             // EKEY_EYE
		"Radius",          // EKEY_RADIUS
		"Height",          // EKEY_HEIGHT
	};

	const char* c_logicKeyStr[LKEY_COUNT] =
	{
		"Logic",   // LKEY_LOGIC
		"Tooltip", // LKEY_TOOLTIP
		"Var",     // LKEY_VAR
		"DefVar",  // LKEY_DEFVAR
		"ReqVar",  // LKEY_REQVAR
	};

	const char* c_varKeyStr[VKEY_COUNT] = 
	{
		"Var",      // VKEY_VAR
		"Type",     // VKEY_TYPE
		"Default",  // VKEY_DEFAULT
		"Default1", // VKEY_DEFAULT1
		"Flag",     // VKEY_FLAG
		"Value",    // VKEY_VALUE
		"Name0",    // VKEY_NAME0
		"Name1"     // VKEY_NAME1
	};

	const char* c_entityTypeStr[ETYPE_COUNT] =
	{
		"Spirit", // ETYPE_SPIRIT
		"Safe",   // ETYPE_SAFE
		"Sprite", // ETYPE_SPRITE
		"Frame",  // ETYPE_FRAME
		"3D",     // ETYPE_3D
	};

	const char* c_entityVarStr[EVARID_COUNT] =
	{
		"Eye",    // EVARID_EYE
		"Radius", // EVARID_RADIUS
		"Height", // EVARID_HEIGHT
	};

	std::vector<Entity> s_entityList;
	std::vector<LogicDef> s_logicDefList;
	std::vector<EntityVarDef> s_varDefList;
	std::vector<u8> s_fileData;

	EntityKey getEntityKey(const char* key)
	{
		for (s32 k = 0; k < EKEY_COUNT; k++)
		{
			if (strcasecmp(c_entityKeyStr[k], key) == 0)
			{
				return EntityKey(k);
			}
		}
		return EKEY_UNKNOWN;
	}

	LogicKey getLogicKey(const char* key)
	{
		for (s32 k = 0; k < LKEY_COUNT; k++)
		{
			if (strcasecmp(c_logicKeyStr[k], key) == 0)
			{
				return LogicKey(k);
			}
		}
		return LKEY_UNKNOWN;
	}

	VarKey getVarKey(const char* key)
	{
		for (s32 k = 0; k < VKEY_COUNT; k++)
		{
			if (strcasecmp(c_varKeyStr[k], key) == 0)
			{
				return VarKey(k);
			}
		}
		return VKEY_UNKNOWN;
	}

	EntityType getEntityType(const char* type)
	{
		for (s32 t = 0; t < ETYPE_COUNT; t++)
		{
			if (strcasecmp(c_entityTypeStr[t], type) == 0)
			{
				return EntityType(t);
			}
		}
		return ETYPE_UNKNOWN;
	}

	const char* getEntityVarName(s32 id)
	{
		if (id < 0 || id >= (s32)s_varDefList.size()) { return ""; }
		return s_varDefList[id].name.c_str();
	}

	s32 getVariableId(const char* varName)
	{
		const s32 count = (s32)s_varDefList.size();
		const EntityVarDef* varDef = s_varDefList.data();
		for (s32 v = 0; v < count; v++, varDef++)
		{
			if (strcasecmp(varDef->name.c_str(), varName) == 0)
			{
				return v;
			}
		}
		return -1;
	}

	bool loadEntityData(const char* localDir)
	{
		s_entityList.clear();

		char entityDefPath[TFE_MAX_PATH];
		const char* progPath = TFE_Paths::getPath(TFE_PathType::PATH_PROGRAM);
		sprintf(entityDefPath, "%sEditorDef/%s/EntityDef.ini", progPath, localDir);
		FileUtil::fixupPath(entityDefPath);

		FileStream file;
		if (!file.open(entityDefPath, FileStream::MODE_READ))
		{
			LE_ERROR("Cannot open Entity Definitions - '%s'.", entityDefPath);
			return false;
		}
		size_t len = file.getSize();
		s_fileData.resize(len);
		file.readBuffer(s_fileData.data(), (u32)len);
		file.close();

		TFE_Parser parser;
		size_t bufferPos = 0;
		parser.init((char*)s_fileData.data(), s_fileData.size());
		parser.addCommentString("#");
		parser.enableColonSeperator();

		const char* line;
		char* endPtr = nullptr;
		Entity* curEntity = nullptr;
		TokenList tokens;
		line = parser.readLine(bufferPos);
		while (line)
		{
			parser.tokenizeLine(line, tokens);
			if (tokens.size() >= 2)
			{
				EntityKey key = getEntityKey(tokens[0].c_str());
				switch (key)
				{
					case EKEY_ENTITY:
					{
						s_entityList.push_back({});
						curEntity = &s_entityList.back();
						curEntity->id = s32(s_entityList.size()) - 1;
						curEntity->name = tokens[1];
					} break;
					case EKEY_CLASS:
					{
						curEntity->type = getEntityType(tokens[1].c_str());
					} break;
					case EKEY_LOGIC:
					{
						curEntity->logic.push_back(tokens[1]);
					} break;
					case EKEY_ICON:
					{
						curEntity->assetName = tokens[1];
					} break;
					case EKEY_PLACEMENT:
					{
						// TODO
					} break;
					case EKEY_ASSET:
					{
						curEntity->assetName = tokens[1];
					} break;
					case EKEY_ASSET_OFFSET_Y:
					{
						curEntity->offsetAdj.y += strtof(tokens[1].c_str(), &endPtr);
					} break;
					case EKEY_ASSET2:
					{
						// TODO
					} break;
					case EKEY_ASSET2_OFFSET_Y:
					{
						// TODO
					} break;
					case EKEY_EYE:
					{
						EntityVar var;
						var.defId = getVariableId("Eye");
						var.value.bValue = strcasecmp(tokens[1].c_str(), "true") == 0;
						curEntity->var.push_back(var);
					} break;
					case EKEY_RADIUS:
					{
						EntityVar var;
						var.defId = getVariableId("Radius");
						var.value.fValue = strtof(tokens[1].c_str(), &endPtr);
						curEntity->var.push_back(var);
					} break;
					case EKEY_HEIGHT:
					{
						EntityVar var;
						var.defId = getVariableId("Height");
						var.value.fValue = strtof(tokens[1].c_str(), &endPtr);
						curEntity->var.push_back(var);
					} break;
					case EKEY_UNKNOWN:
					default:
					{
						LE_WARNING("Invalid Entity Key '%s'.", tokens[0].c_str());
					}
				}
			}
			else
			{
				LE_WARNING("Invalid Key-Value pair '%s'.", line);
			}

			line = parser.readLine(bufferPos);
		}
		LE_INFO("Loaded %d entity definitions from '%s'.", (s32)s_entityList.size(), entityDefPath);

		// Now that the definitions are loaded, get the actual data...
		const s32 count = (s32)s_entityList.size();
		Entity* entity = s_entityList.data();
		for (s32 i = 0; i < count; i++, entity++)
		{
			// First figure out the display asset.
			if (entity->type == ETYPE_SPIRIT || entity->type == ETYPE_SAFE)
			{
				// Load as a PNG.
				char pngPath[TFE_MAX_PATH];
				sprintf(pngPath, "%sUI_Images/%s", progPath, entity->assetName.c_str());
				FileUtil::fixupPath(pngPath);

				entity->image = loadGpuImage(pngPath);
				entity->st[1] = { (f32)entity->image->getWidth(), (f32)entity->image->getHeight() };

				entity->size.x = 4.0f;
				entity->size.z = 4.0f;
			}
			else if (entity->type == ETYPE_FRAME)
			{
				// Load a texture from the frame.
				Asset* asset = AssetBrowser::findAsset(entity->assetName.c_str(), TYPE_FRAME);
				if (asset)
				{
					if (!asset->handle)
					{
						asset->handle = AssetBrowser::loadAssetData(asset);
					}

					EditorFrame* frame = (EditorFrame*)getAssetData(asset->handle);
					entity->image = frame->texGpu;
					entity->uv[0].z = 1.0f;
					entity->uv[1].z = 0.0f;
					entity->st[1] = { (f32)entity->image->getWidth(), (f32)entity->image->getHeight() };

					entity->size.x = frame->worldWidth;
					entity->size.z = frame->worldHeight;

					// Handle the offset.
					entity->offset = { frame->offsetX + entity->offsetAdj.x, frame->offsetY + entity->offsetAdj.y, 0.0f };
				}
			}
			else if (entity->type == ETYPE_SPRITE)
			{
				// Load a texture from the wax.
				Asset* asset = AssetBrowser::findAsset(entity->assetName.c_str(), TYPE_SPRITE);
				if (asset)
				{
					if (!asset->handle)
					{
						asset->handle = AssetBrowser::loadAssetData(asset);
					}

					EditorSprite* sprite = (EditorSprite*)getAssetData(asset->handle);
					entity->image = sprite->texGpu;

					// Get the actual cell...
					s32 frameIndex = sprite->anim[0].views[0].frameIndex[0];
					SpriteFrame* frame = &sprite->frame[frameIndex];
					SpriteCell* cell = &sprite->cell[frame->cellIndex];

					f32 u0 = f32(cell->u) / (f32)sprite->texGpu->getWidth();
					f32 v1 = f32(cell->v) / (f32)sprite->texGpu->getHeight();
					f32 u1 = f32(cell->u + cell->w) / (f32)sprite->texGpu->getWidth();
					f32 v0 = f32(cell->v + cell->h) / (f32)sprite->texGpu->getHeight();
					if (frame->flip) { std::swap(u0, u1); }

					entity->uv[0] = { u0, v0 };
					entity->uv[1] = { u1, v1 };

					entity->st[0] = { (f32)cell->u, (f32)cell->v };
					entity->st[1] = { f32(cell->u + cell->w), f32(cell->v + cell->h) };

					entity->size.x = frame->widthWS;
					entity->size.z = frame->heightWS;

					// Handle the offset.
					entity->offset = { (f32)frame->offsetX + entity->offsetAdj.x, (f32)frame->offsetY + entity->offsetAdj.y, 0.0f };
				}
			}
			else
			{
				// TODO: other types, like 3D.
			}
		}

		return true;
	}
	
	bool loadLogicData(const char* localDir)
	{
		s_logicDefList.clear();

		char logicDefPath[TFE_MAX_PATH];
		const char* progPath = TFE_Paths::getPath(TFE_PathType::PATH_PROGRAM);
		sprintf(logicDefPath, "%sEditorDef/%s/LogicDef.ini", progPath, localDir);
		FileUtil::fixupPath(logicDefPath);

		FileStream file;
		if (!file.open(logicDefPath, FileStream::MODE_READ))
		{
			LE_ERROR("Cannot open Logic Definitions - '%s'.", logicDefPath);
			return false;
		}
		size_t len = file.getSize();
		s_fileData.resize(len);
		file.readBuffer(s_fileData.data(), (u32)len);
		file.close();

		TFE_Parser parser;
		size_t bufferPos = 0;
		parser.init((char*)s_fileData.data(), s_fileData.size());
		parser.addCommentString("#");
		parser.enableColonSeperator();

		const char* line;
		char* endPtr = nullptr;
		LogicDef* curLogic = nullptr;
		TokenList tokens;
		line = parser.readLine(bufferPos);
		while (line)
		{
			parser.tokenizeLine(line, tokens);
			if (tokens.size() >= 2)
			{
				LogicKey key = getLogicKey(tokens[0].c_str());
				switch (key)
				{
					case LKEY_LOGIC:
					{
						s_logicDefList.push_back({});
						curLogic = &s_logicDefList.back();
						curLogic->id = s32(s_logicDefList.size()) - 1;
						curLogic->name = tokens[1];
					} break;
					case LKEY_TOOLTIP:
					{
						// Fix-up line-endings.
						char fixup[512];
						const s32 len = (s32)tokens[1].length();
						const char* src = tokens[1].c_str();
						s32 outLen = 0;
						for (s32 i = 0; i < len; i++)
						{
							if (src[i] != '\\' || src[i+1] != 'n')
							{
								fixup[outLen++] = src[i];
							}
							else
							{
								fixup[outLen++] = '\n';
								i++;
							}
						}
						fixup[outLen] = 0;
						curLogic->tooltip = fixup;
					} break;
					case LKEY_VAR:
					{
						curLogic->var.push_back({ getVariableId(tokens[1].c_str()), VAR_TYPE_STD });
					} break;
					case LKEY_DEFVAR:
					{
						curLogic->var.push_back({ getVariableId(tokens[1].c_str()), VAR_TYPE_DEFAULT });
					} break;
					case LKEY_REQVAR:
					{
						curLogic->var.push_back({ getVariableId(tokens[1].c_str()), VAR_TYPE_REQUIRED });
					} break;
					default:
					{
						LE_WARNING("Invalid key '%s' in LogicDef.ini", tokens[0].c_str());
					}
				}
			}
			line = parser.readLine(bufferPos);
		}

		return true;
	}

	bool loadVariableData(const char* localDir)
	{
		s_varDefList.clear();

		char varDefPath[TFE_MAX_PATH];
		const char* progPath = TFE_Paths::getPath(TFE_PathType::PATH_PROGRAM);
		sprintf(varDefPath, "%sEditorDef/%s/VarDef.ini", progPath, localDir);
		FileUtil::fixupPath(varDefPath);

		FileStream file;
		if (!file.open(varDefPath, FileStream::MODE_READ))
		{
			LE_ERROR("Cannot open Variable Definitions - '%s'.", varDefPath);
			return false;
		}
		size_t len = file.getSize();
		s_fileData.resize(len);
		file.readBuffer(s_fileData.data(), (u32)len);
		file.close();

		TFE_Parser parser;
		size_t bufferPos = 0;
		parser.init((char*)s_fileData.data(), s_fileData.size());
		parser.addCommentString("#");
		parser.enableColonSeperator();

		const char* line;
		char* endPtr = nullptr;
		EntityVarDef* curVar = nullptr;
		TokenList tokens;
		line = parser.readLine(bufferPos);
		while (line)
		{
			parser.tokenizeLine(line, tokens);
			if (tokens.size() >= 2)
			{
				VarKey key = getVarKey(tokens[0].c_str());
				switch (key)
				{
					case VKEY_VAR:
					{
						s_varDefList.push_back({});
						curVar = &s_varDefList.back();
						curVar->id = s32(s_varDefList.size()) - 1;
						curVar->name = tokens[1];
					} break;
					case VKEY_TYPE:
					{
						const char* typeStr = tokens[1].c_str();
						curVar->defValue.iValue = 0;
						curVar->defValue1.iValue = 0;
						if (strcasecmp(typeStr, "Bool") == 0)
						{
							curVar->type = EVARTYPE_BOOL;
						}
						else if (strcasecmp(typeStr, "Int") == 0)
						{
							curVar->type = EVARTYPE_INT;
						}
						else if (strcasecmp(typeStr, "Float") == 0)
						{
							curVar->type = EVARTYPE_FLOAT;
						}
						else if (strcasecmp(typeStr, "Flags") == 0)
						{
							curVar->type = EVARTYPE_FLAGS;
						}
						else if (strcasecmp(typeStr, "StringList") == 0)
						{
							curVar->type = EVARTYPE_STRING_LIST;
						}
						else if (strcasecmp(typeStr, "InputStringPair") == 0)
						{
							curVar->type = EVARTYPE_INPUT_STRING_PAIR;
						}
						else
						{
							curVar->type = EVARTYPE_BOOL; // Just assume bool.
							LE_WARNING("Variable Parsing Error: Unknown variable type '%s'", typeStr);
						}
					} break;
					case VKEY_DEFAULT:
					{
						const char* valueStr = tokens[1].c_str();
						switch (curVar->type)
						{
							case EVARTYPE_BOOL:
							{
								curVar->defValue.bValue = strcasecmp(valueStr, "True") == 0 || strcasecmp(valueStr, "1") == 0;
							} break;
							case EVARTYPE_FLOAT:
							{
								curVar->defValue.fValue = strtof(valueStr, &endPtr);
							} break;
							case EVARTYPE_INT:
							case EVARTYPE_FLAGS:
							{
								curVar->defValue.iValue = strtol(valueStr, &endPtr, 10);
							} break;
							case EVARTYPE_STRING_LIST:
							{
								curVar->defValue.sValue = valueStr;
							} break;
							case EVARTYPE_INPUT_STRING_PAIR:
							{
								curVar->defValue.sValue = valueStr;
							} break;
						}
					} break;
					case VKEY_DEFAULT1:
					{
						const char* valueStr = tokens[1].c_str();
						switch (curVar->type)
						{
							case EVARTYPE_INPUT_STRING_PAIR:
							{
								curVar->defValue1.sValue = valueStr;
							} break;
						}
					} break;
					case VKEY_FLAG:
					{
						if (tokens.size() >= 3)
						{
							curVar->flags.push_back({ tokens[1], strtol(tokens[2].c_str(), &endPtr, 10) });
						}
						else
						{
							LE_WARNING("Variable Parsing Error: Invalid 'Flag' format, it should be 'Flag: \"FlagName\", FlagIntValue' : '%s'", line);
						}
					} break;
					case VKEY_VALUE:
					{
						if (curVar->type == EVARTYPE_STRING_LIST)
						{
							curVar->strList.push_back(tokens[1]);
						}
						else
						{
							LE_WARNING("Variable Parsing Error: Invalid use of 'Value', it is only valid for the 'StringList' type: '%s'", line);
						}
					} break;
					case VKEY_NAME0:
					{
						curVar->name0 = tokens[1];
					} break;
					case VKEY_NAME1:
					{
						curVar->name1 = tokens[1];
					} break;
				}
			}
			line = parser.readLine(bufferPos);
		}

		return true;
	}
}
