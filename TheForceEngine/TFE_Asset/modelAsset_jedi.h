#pragma once
//////////////////////////////////////////////////////////////////////
// Jedi specific structures for 3DOs
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include <string>
#include <vector>

#define MAX_VERTEX_COUNT_3DO 500
#define MAX_POLYGON_COUNT_3DO 400

struct Texture;

enum PolygonShadingFlags
{
	PSHADE_FLAT = 0,
	PSHADE_GOURAUD = 1,
	PSHADE_TEXTURE = 2,
	PSHADE_GOURAUD_TEXTURE = PSHADE_GOURAUD | PSHADE_TEXTURE,
	PSHADE_PLANE = 4,
};

enum ModelFlags
{
	MFLAG_VERTEX_LIT = (1 << 1),
	MFLAG_DRAW_VERTICES = (1 << 2),
};

struct vec2
{
	s32 x, y;
};

struct vec3
{
	s32 x, y, z;
};

// sizeof(Polygon) = 36
struct Polygon
{
	s32 index;
	s32 shading;
	s32 p08;
	s32 color;
	Texture* texture;
	s32 vertexCount;
	s32* indices;
	vec2* uv;
	union { s32 zAve; f32 zAvef; };
	s32 p24;
};

struct JediModel
{
	s32 isBridge;		// this 3D object is a 3D "bridge" which gets special sorting. All 3D objects with '_' in their name get this flag.
	s32 vertexCount;
	vec3* vertices;
	s32 polygonCount;
	Polygon* polygons;
	vec3* vertexNormals;
	vec3* polygonNormals;
	s32 flags;
	s32 textureCount;
	Texture** textures;
	s32 radius;
};

namespace TFE_Model_Jedi
{
	JediModel* get(const char* name);
	void freeAll();
}
