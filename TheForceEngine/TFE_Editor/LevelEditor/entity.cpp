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

	const char* getEntityVarStr(EntityVarId varId)
	{
		if (varId >= EVARID_COUNT) { return ""; }
		return c_entityVarStr[varId];
	}

	bool loadEntityData()
	{
		s_entityList.clear();

		char entityDefPath[TFE_MAX_PATH];
		const char* progPath = TFE_Paths::getPath(TFE_PathType::PATH_PROGRAM);
		sprintf(entityDefPath, "%sEditorDef/EntityDef.ini", progPath);
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
						var.def.id = EVARID_EYE;
						var.def.type = EVARTYPE_BOOL;
						var.value.bValue = strcasecmp(tokens[1].c_str(), "true") == 0;
						curEntity->var.push_back(var);
					} break;
					case EKEY_RADIUS:
					{
						EntityVar var;
						var.def.id = EVARID_RADIUS;
						var.def.type = EVARTYPE_FLOAT;
						var.value.fValue = strtof(tokens[1].c_str(), &endPtr);
						curEntity->var.push_back(var);
					} break;
					case EKEY_HEIGHT:
					{
						EntityVar var;
						var.def.id = EVARID_HEIGHT;
						var.def.type = EVARTYPE_FLOAT;
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
				#if 0
					s32 texW = cell->w;
					s32 texH = cell->h;
					s32 scale = max(1, min(4, s32(w) / (sprite->rect[2] - sprite->rect[0])));
					texW *= scale;
					texH *= scale;

					ImVec2 cursor = ImGui::GetCursorPos();
					cursor.x += f32((frame->offsetX - sprite->rect[0]) * scale);
					cursor.y += f32((frame->offsetY - sprite->rect[1]) * scale);
					ImGui::SetCursorPos(cursor);

					ImGui::Image(TFE_RenderBackend::getGpuPtr(sprite->texGpu),
						ImVec2((f32)texW, (f32)texH), ImVec2(u0, v0), ImVec2(u1, v1));
				#endif
				}
			}
			else
			{
				// TODO: other types, like 3D.
			}
		}

		return true;
	}
}
