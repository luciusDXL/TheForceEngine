#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Level/Map
// Handles the following:
// 1. Parsing the level data (.LEV)
// 2. Runtime storage and manipulation of level data
//    (vertices, lines, sectors)
//////////////////////////////////////////////////////////////////////
#include <DXL2_System/types.h>
#include <string>
#include <vector>

struct Texture;

enum PolygonShading
{
	SHADING_FLAT,
	SHADING_GOURAUD,
	SHADING_VERTEX,
	SHADING_TEXTURE,
	SHADING_GOURTEX,
	SHADING_PLANE,
	SHADING_COUNT
};

struct ModelPolygon
{
	u16 i0, i1, i2, i3;
	u16 num;
	u8 color;
	u8 shading;
};

struct TexturePolygon
{
	u16 i0, i1, i2, i3;
	u16 num;
	u16 pad16;
};

struct Vertex3f
{
	u16 num;
	u16 pad16;
	f32 x, y, z;
};

struct Vertex2f
{
	u16 num;
	u16 pad16;
	f32 x, y;
};

struct ModelObject
{
	s16 textureIndex;
	u16 pad16;
	u32 vertexOffset;

	// Name.
	std::string name;

	// 3D geometry.
	std::vector<Vertex3f> vertices;
	std::vector<ModelPolygon> triangles;
	std::vector<ModelPolygon> quads;

	// 2D texture mapping.
	std::vector<Vertex2f> textureVertices;
	std::vector<TexturePolygon> textureTriangles;
	std::vector<TexturePolygon> textureQuads;

	// Center for sorting.
	Vec3f center;
	f32 radius;
};

struct Model
{
	u32 objectCount;
	u32 vertexCount;
	u32 polygonCount;
	std::vector<Texture*> textures;
	std::vector<ModelObject> objects;

	Vec3f localAabb[2];
	Vec3f center;
	f32 radius;
};

namespace DXL2_Model
{
	Model* get(const char* name);
	void freeAll();
};
