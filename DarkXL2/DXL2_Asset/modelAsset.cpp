#include "modelAsset.h"
#include "textureAsset.h"
#include <DXL2_System/system.h>
#include <DXL2_Asset/assetSystem.h>
#include <DXL2_Archive/archive.h>
#include <DXL2_System/parser.h>
#include <assert.h>
#include <map>
#include <algorithm>

namespace DXL2_Model
{
	typedef std::map<std::string, Model*> ModelMap;
	static ModelMap s_models;
	static std::vector<char> s_buffer;
	static const char* c_defaultGob = "DARK.GOB";

	bool parseModel(Model* model);

	Model* get(const char* name)
	{
		ModelMap::iterator iModel = s_models.find(name);
		if (iModel != s_models.end())
		{
			return iModel->second;
		}

		// It doesn't exist yet, try to load the font.
		if (!DXL2_AssetSystem::readAssetFromGob(c_defaultGob, name, s_buffer))
		{
			return nullptr;
		}

		Model* model = new Model;
		parseModel(model);
		model->polygonCount = 0;
		u32 vertexOffset = 0;
		
		// Find the center and local bounds.
		Vec3f center = { 0,0,0 };
		model->localAabb[0] = { FLT_MAX, FLT_MAX, FLT_MAX };
		model->localAabb[1] = {-FLT_MAX,-FLT_MAX,-FLT_MAX };
		u32 count = 0;
		for (u32 o = 0; o < model->objectCount; o++)
		{
			model->objects[o].center = { 0.0f, 0.0f, 0.0f };
			model->objects[o].vertexOffset = vertexOffset;
			vertexOffset += (u32)model->objects[o].vertices.size();
			model->polygonCount += (u32)model->objects[o].triangles.size();
			model->polygonCount += (u32)model->objects[o].quads.size();
			
			Vec3f aabbMin = { model->objects[o].vertices[0].x, model->objects[o].vertices[0].y, model->objects[o].vertices[0].z };
			Vec3f aabbMax = aabbMin;
			for (u32 v = 0; v < model->objects[o].vertices.size(); v++)
			{
				center.x += model->objects[o].vertices[v].x;
				center.y += model->objects[o].vertices[v].y;
				center.z += model->objects[o].vertices[v].z;

				model->objects[o].center.x += model->objects[o].vertices[v].x;
				model->objects[o].center.y += model->objects[o].vertices[v].y;
				model->objects[o].center.z += model->objects[o].vertices[v].z;

				aabbMin.x = std::min(aabbMin.x, model->objects[o].vertices[v].x);
				aabbMin.y = std::min(aabbMin.y, model->objects[o].vertices[v].y);
				aabbMin.z = std::min(aabbMin.z, model->objects[o].vertices[v].z);

				aabbMax.x = std::max(aabbMax.x, model->objects[o].vertices[v].x);
				aabbMax.y = std::max(aabbMax.y, model->objects[o].vertices[v].y);
				aabbMax.z = std::max(aabbMax.z, model->objects[o].vertices[v].z);

				count++;
			}

			model->localAabb[0].x = std::min(model->localAabb[0].x, aabbMin.x);
			model->localAabb[0].y = std::min(model->localAabb[0].y, aabbMin.y);
			model->localAabb[0].z = std::min(model->localAabb[0].z, aabbMin.z);
			model->localAabb[1].x = std::max(model->localAabb[1].x, aabbMax.x);
			model->localAabb[1].y = std::max(model->localAabb[1].y, aabbMax.y);
			model->localAabb[1].z = std::max(model->localAabb[1].z, aabbMax.z);

			const f32 scale = 1.0f / f32(model->objects[o].vertices.size());
			model->objects[o].center.x *= scale;
			model->objects[o].center.y *= scale;
			model->objects[o].center.z *= scale;

			Vec3f diff = { aabbMax.x - aabbMin.x, aabbMax.y - aabbMin.y, aabbMax.z - aabbMin.z };
			model->objects[o].radius = sqrtf(diff.x*diff.x + diff.y*diff.y + diff.z*diff.z) * 0.5f;
		}
		const f32 scale = 1.0f / (count);
		center.x *= scale;
		center.y *= scale;
		center.z *= scale;

		// Then find the radius.
		f32 maxDistSq = 0.0f;
		for (u32 o = 0; o < model->objectCount; o++)
		{
			for (u32 v = 0; v < model->objects[o].vertices.size(); v++)
			{
				f32 dx = model->objects[o].vertices[v].x - center.x;
				f32 dy = model->objects[o].vertices[v].y - center.y;
				f32 dz = model->objects[o].vertices[v].z - center.z;

				f32 distSq = dx * dx + dy * dy + dz * dz;
				if (distSq > maxDistSq) { maxDistSq = distSq; }
			}
		}
		model->center = center;
		model->radius = sqrtf(maxDistSq);
		model->vertexCount = vertexOffset;

		s_models[name] = model;
		return model;
	}

	void freeAll()
	{
		ModelMap::iterator iModel = s_models.begin();
		for (; iModel != s_models.end(); ++iModel)
		{
			Model* model = iModel->second;
			delete model;
		}
		s_models.clear();
	}

	enum ValueType
	{
		VALUE_VERTICES = 0,
		VALUE_TRIANGLES,
		VALUE_QUADS,
		VALUE_TEXTURE_VERTICES,
		VALUE_TEXTURE_TRIANGLES,
		VALUE_TEXTURE_QUADS,
		VALUE_COUNT,
		VALUE_INVALID = VALUE_COUNT
	};

	static const char* c_shadingName[SHADING_COUNT] =
	{
		"FLAT",    // SHADING_FLAT,
		"GOURAUD", // SHADING_GOURAUD,
		"VERTEX",  // SHADING_VERTEX,
		"TEXTURE", // SHADING_TEXTURE,
		"GOURTEX", // SHADING_GOURTEX,
		"PLANE",   // SHADING_PLANE,
	};

	PolygonShading getShading(const char* name)
	{
		for (u32 i = 0; i < SHADING_COUNT; i++)
		{
			if (strcasecmp(name, c_shadingName[i]) == 0)
			{
				return PolygonShading(i);
			}
		}
		return SHADING_FLAT;
	}

	bool parseModel(Model* model)
	{
		if (s_buffer.empty()) { return false; }

		const size_t len = s_buffer.size();

		DXL2_Parser parser;
		size_t bufferPos = 0;
		parser.init(s_buffer.data(), len);
		parser.addCommentString("//");
		parser.addCommentString("#");
		parser.enableBlockComments();

		std::vector<std::string> textures;
		model->objectCount = 0;
		model->vertexCount = 0;
		model->polygonCount = 0;
		s32 objectId = -1;
		ValueType valueType = VALUE_INVALID;
		ModelObject* object = nullptr;
		u32 valueIndex = 0;
		
		while (bufferPos < len)
		{
			const char* line = parser.readLine(bufferPos);
			if (!line) { break; }

			TokenList tokens;
			parser.tokenizeLine(line, tokens);
			if (tokens.size() < 1) { continue; }

			char* endPtr = nullptr;
			if (strcasecmp("OBJECTS", tokens[0].c_str()) == 0)
			{
				model->objectCount = strtoul(tokens[1].c_str(), &endPtr, 10);
				model->objects.resize(model->objectCount);
			}
			else if (strcasecmp("VERTICES", tokens[0].c_str()) == 0)
			{
				if (objectId < 0)
				{
					model->vertexCount = strtoul(tokens[1].c_str(), &endPtr, 10);
				}
				else
				{
					object->vertices.resize(strtoul(tokens[1].c_str(), &endPtr, 10));
					valueType = VALUE_VERTICES;
					valueIndex = 0;
				}
			}
			else if (strcasecmp("POLYGONS", tokens[0].c_str()) == 0)
			{
				if (objectId < 0)
				{
					model->polygonCount = strtoul(tokens[1].c_str(), &endPtr, 10);
				}
			}
			else if (strcasecmp("TRIANGLES", tokens[0].c_str()) == 0)
			{
				if (objectId >= 0)
				{
					object->triangles.resize(strtoul(tokens[1].c_str(), &endPtr, 10));
					valueType = VALUE_TRIANGLES;
					valueIndex = 0;
				}
			}
			else if (strcasecmp("QUADS", tokens[0].c_str()) == 0)
			{
				if (objectId >= 0)
				{
					object->quads.resize(strtoul(tokens[1].c_str(), &endPtr, 10));
					valueType = VALUE_QUADS;
					valueIndex = 0;
				}
			}
			else if (strcasecmp("TEXTURES", tokens[0].c_str()) == 0)
			{
				textures.reserve(strtoul(tokens[1].c_str(), &endPtr, 10));
			}
			else if (strcasecmp("TEXTURE:", tokens[0].c_str()) == 0)
			{
				if (objectId >= 0 && tokens.size() > 1)
				{
					object->textureIndex = (s16)strtol(tokens[1].c_str(), &endPtr, 10);
				}
				else if (objectId < 0)
				{
					if (tokens.size() < 2)
					{
						textures.push_back("");
					}
					else
					{
						textures.push_back(tokens[1]);
					}
				}
			}
			else if (strcasecmp("TEXTURE", tokens[0].c_str()) == 0)
			{
				if (objectId >= 0 && tokens.size() > 1)
				{
					if (strcasecmp(tokens[1].c_str(), "VERTICES") == 0)
					{
						object->textureVertices.resize(strtoul(tokens[2].c_str(), &endPtr, 10));
						valueType = VALUE_TEXTURE_VERTICES;
						valueIndex = 0;
					}
					else if (strcasecmp(tokens[1].c_str(), "TRIANGLES") == 0)
					{
						object->textureTriangles.resize(strtoul(tokens[2].c_str(), &endPtr, 10));
						valueType = VALUE_TEXTURE_TRIANGLES;
						valueIndex = 0;
					}
					else if (strcasecmp(tokens[1].c_str(), "QUADS") == 0)
					{
						object->textureQuads.resize(strtoul(tokens[2].c_str(), &endPtr, 10));
						valueType = VALUE_TEXTURE_QUADS;
						valueIndex = 0;
					}
					else
					{
						object->textureIndex = (s16)strtol(tokens[1].c_str(), &endPtr, 10);
					}
				}
			}
			else if (strcasecmp("OBJECT", tokens[0].c_str()) == 0)
			{
				objectId++;
				if (objectId >= model->objects.size())
				{
					model->objects.resize(objectId + 1);
					model->objectCount = objectId + 1;
				}

				object = &model->objects[objectId];
				object->textureIndex = -1;

				if (tokens.size() > 1)
				{
					object->name = tokens[1];
				}

				valueType = VALUE_INVALID;
			}
			else if (valueType == VALUE_VERTICES)
			{
				assert(tokens.size() >= 4);
				if (valueIndex >= object->vertices.size()) { object->vertices.resize(valueIndex + 1); }

				object->vertices[valueIndex].num = (u16)strtoul(tokens[0].c_str(), &endPtr, 10);
				object->vertices[valueIndex].x = (f32)strtod(tokens[1].c_str(), &endPtr);
				object->vertices[valueIndex].y = (f32)strtod(tokens[2].c_str(), &endPtr);
				object->vertices[valueIndex].z = (f32)strtod(tokens[3].c_str(), &endPtr);
				valueIndex++;
			}
			else if (valueType == VALUE_TRIANGLES)
			{
				assert(tokens.size() >= 6);
				if (valueIndex >= object->triangles.size()) { object->triangles.resize(valueIndex + 1); }

				object->triangles[valueIndex].num = (u16)strtoul(tokens[0].c_str(), &endPtr, 10);
				object->triangles[valueIndex].i0 = (u16)strtoul(tokens[1].c_str(), &endPtr, 10);
				object->triangles[valueIndex].i1 = (u16)strtoul(tokens[2].c_str(), &endPtr, 10);
				object->triangles[valueIndex].i2 = (u16)strtoul(tokens[3].c_str(), &endPtr, 10);
				object->triangles[valueIndex].i3 = 0;

				object->triangles[valueIndex].color = (u8)strtoul(tokens[4].c_str(), &endPtr, 10);
				object->triangles[valueIndex].shading = getShading(tokens[5].c_str());

				valueIndex++;
			}
			else if (valueType == VALUE_QUADS)
			{
				assert(tokens.size() >= 7);
				if (valueIndex >= object->quads.size()) { object->quads.resize(valueIndex + 1); }

				object->quads[valueIndex].num = (u16)strtoul(tokens[0].c_str(), &endPtr, 10);
				object->quads[valueIndex].i0 = (u16)strtoul(tokens[1].c_str(), &endPtr, 10);
				object->quads[valueIndex].i1 = (u16)strtoul(tokens[2].c_str(), &endPtr, 10);
				object->quads[valueIndex].i2 = (u16)strtoul(tokens[3].c_str(), &endPtr, 10);
				object->quads[valueIndex].i3 = (u16)strtoul(tokens[4].c_str(), &endPtr, 10);

				object->quads[valueIndex].color = (u8)strtoul(tokens[5].c_str(), &endPtr, 10);
				object->quads[valueIndex].shading = getShading(tokens[6].c_str());

				valueIndex++;
			}
			else if (valueType == VALUE_TEXTURE_VERTICES)
			{
				if (valueIndex >= object->textureVertices.size()) { object->textureVertices.resize(valueIndex + 1); }

				object->textureVertices[valueIndex].num = (u16)strtoul(tokens[0].c_str(), &endPtr, 10);
				object->textureVertices[valueIndex].x = (f32)strtod(tokens[1].c_str(), &endPtr);
				object->textureVertices[valueIndex].y = tokens.size() > 2 ? (f32)strtod(tokens[2].c_str(), &endPtr) : object->textureVertices[valueIndex].x;
				valueIndex++;
			}
			else if (valueType == VALUE_TEXTURE_TRIANGLES)
			{
				assert(tokens.size() >= 4);
				if (valueIndex >= object->textureTriangles.size()) { object->textureTriangles.resize(valueIndex + 1); }

				object->textureTriangles[valueIndex].num = (u16)strtoul(tokens[0].c_str(), &endPtr, 10);
				object->textureTriangles[valueIndex].i0 = (u16)strtoul(tokens[1].c_str(), &endPtr, 10);
				object->textureTriangles[valueIndex].i1 = (u16)strtoul(tokens[2].c_str(), &endPtr, 10);
				object->textureTriangles[valueIndex].i2 = (u16)strtoul(tokens[3].c_str(), &endPtr, 10);
				object->textureTriangles[valueIndex].i3 = 0;
				valueIndex++;
			}
			else if (valueType == VALUE_TEXTURE_QUADS)
			{
				if (tokens.size() < 5) { continue; }
				if (valueIndex >= object->textureQuads.size()) { object->textureQuads.resize(valueIndex + 1); }

				object->textureQuads[valueIndex].num = (u16)strtoul(tokens[0].c_str(), &endPtr, 10);
				object->textureQuads[valueIndex].i0 = (u16)strtoul(tokens[1].c_str(), &endPtr, 10);
				object->textureQuads[valueIndex].i1 = (u16)strtoul(tokens[2].c_str(), &endPtr, 10);
				object->textureQuads[valueIndex].i2 = (u16)strtoul(tokens[3].c_str(), &endPtr, 10);
				object->textureQuads[valueIndex].i3 = (u16)strtoul(tokens[4].c_str(), &endPtr, 10);
				valueIndex++;
			}
		}

		// Resize based on actual object count.
		model->objectCount = objectId + 1;
		model->objects.resize(model->objectCount);

		// Now load textures.
		const size_t texCount = textures.size();
		model->textures.resize(std::max(texCount, size_t(1)));
		if (texCount == 0)
		{
			model->textures[0] = nullptr;
		}
		else
		{
			for (size_t t = 0; t < texCount; t++)
			{
				model->textures[t] = DXL2_Texture::get(textures[t].c_str());
			}
		}

		return true;
	}
};
