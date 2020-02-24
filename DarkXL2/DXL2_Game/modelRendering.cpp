#include "modelRendering.h"
#include "renderCommon.h"
#include <DXL2_Asset/textureAsset.h>
#include <DXL2_Asset/levelAsset.h>
#include <algorithm>
#include <vector>
#include <assert.h>

namespace DXL2_ModelRender
{
	#define POINT_SCALE 400

	struct Scanline
	{
		s32 x0, x1;
		s32 u0, u1;
		s32 v0, v1;
		s32 z0, z1;
		s32 l0, l1;
	};

	// 64 bits - sort object
	struct PolySort
	{
		u16 objectIndex;
		u16 polyIndex;
		f32 sortDepth;
	};

	enum PolyFlags
	{
		PF_TEXTURED = (1 << 0),
		PF_GOURAUD = (1 << 1),
		PF_PLANE = (1 << 2),
	};

	struct ClipPolygon
	{
		u32 flags;
		u32 count;
		Vec3f vtx[32];
		Vec2f uv[32];
		f32 lightLevel[32];
	};
	
	static DXL2_Renderer* s_renderer;
	const Sector* s_sector = nullptr;
	static const Vec3f* s_cameraPos;
	static const f32* s_cameraRot;

	static s32 s_width;
	static s32 s_height;
	static f32 s_halfWidth;
	static f32 s_halfHeight;
	static f32 s_heightOffset;
	static f32 s_focalLength;
	static f32 s_heightScale;
	static bool s_flipVert = false;
	static bool s_persCorrect = false;

	static s32 s_XClip[2];
	static const s16* s_YClipUpper;
	static const s16* s_YClipLower;
	static Vec3f s_planes[4];
	
	static std::vector<Scanline> s_scanlineArray;
	static std::vector<Vec3f> s_modelTransformed;
	static std::vector<PolySort> s_polygons;

	static Vec3f s_scale = { 1.0f, 1.0f, 1.0f };

	////////////////////////////////
	// Forward Declarations
	////////////////////////////////
	void drawTriangle(const Vec3f* v0, const Vec3f* v1, const Vec3f* v2, u8 color, s32 ambient);
	void drawQuad(const Vec3f* v0, const Vec3f* v1, const Vec3f* v2, const Vec3f* v3, u8 color, s32 ambient);
	void drawTexturedTriangle(const Vec3f* v0, const Vec3f* v1, const Vec3f* v2, const Vec2f* uv0, const Vec2f* uv1, const Vec2f* uv2, const TextureFrame* texture, s32 ambient);
	void drawTexturedQuad(const Vec3f* v0, const Vec3f* v1, const Vec3f* v2, const Vec3f* v3, const Vec2f* uv0, const Vec2f* uv1, const Vec2f* uv2, const Vec2f* uv3, const TextureFrame* texture, s32 ambient);
	void drawTriangleGouraud(const Vec3f* v0, const Vec3f* v1, const Vec3f* v2, u8 color, f32 l0, f32 l1, f32 l2);
	void drawQuadGouraud(const Vec3f* v0, const Vec3f* v1, const Vec3f* v2, const Vec3f* v3, u8 color, f32 l0, f32 l1, f32 l2, f32 l3);
	void drawTexturedGouraudTriangle(const Vec3f* v0, const Vec3f* v1, const Vec3f* v2, const Vec2f* uv0, const Vec2f* uv1, const Vec2f* uv2, s32 l0, s32 l1, s32 l2, const TextureFrame* texture);
	void drawTexturedGouraudQuad(const Vec3f* v0, const Vec3f* v1, const Vec3f* v2, const Vec3f* v3, const Vec2f* uv0, const Vec2f* uv1, const Vec2f* uv2, const Vec2f* uv3, s32 l0, s32 l1, s32 l2, s32 l3, const TextureFrame* texture);
	void drawPlaneTriangle(const Vec3f* v0, const Vec3f* v1, const Vec3f* v2, const TextureFrame* texture);
	void drawPlaneQuad(const Vec3f* v0, const Vec3f* v1, const Vec3f* v2, const Vec3f* v3, const TextureFrame* texture);
	void draw3dPoint(const Vec3f* p, u8 color);
	s32  sortPolygons(const void* a, const void* b);

	////////////////////////////////
	// API
	////////////////////////////////
	void init(DXL2_Renderer* renderer, s32 width, s32 height, f32 focalLength, f32 heightScale)
	{
		s_renderer = renderer;
		s_width = width;
		s_height = height;
		s_halfWidth = f32(s_width) * 0.5f;
		s_halfHeight = f32(s_height) * 0.5f;
		s_focalLength = focalLength;
		s_heightScale = heightScale;
		s_flipVert = false;

		s_XClip[0] = 0;
		s_XClip[1] = s_width - 1;
		s_YClipUpper = nullptr;
		s_YClipLower = nullptr;
		s_renderer->setHLineClip(s_XClip[0], s_XClip[1], s_YClipUpper, s_YClipLower);

		buildClipPlanes((s32)s_heightScale);
	}

	void flipVertical() { s_flipVert = true; }

	void enablePerspectiveCorrectTexturing(bool enable)
	{
		s_persCorrect = enable;
	}

	static const s32 c_aabbEdges[24]=
	{
		0, 1,
		1, 2,
		2, 3,
		3, 0,

		4, 5,
		5, 6,
		6, 7,
		7, 4,

		0, 4,
		1, 5,
		2, 6,
		3, 7
	};

	void buildRotationMatrix(Vec3f angles, Vec3f* mat)
	{
		if (angles.x == 0.0f && angles.y == 0.0f && angles.z == 0.0f)
		{
			// Identity.
			mat[0] = { 1.0f, 0.0f, 0.0f };
			mat[1] = { 0.0f, 1.0f, 0.0f };
			mat[2] = { 0.0f, 0.0f, 1.0f };
		}
		else if (angles.x == 0.0f && angles.z == 0.0f)
		{
			// Yaw only.
			const f32 ca = cosf(angles.y);
			const f32 sa = sinf(angles.y);
			mat[0] = { ca, 0.0f, sa };
			mat[1] = { 0.0f, 1.0f, 0.0f };
			mat[2] = { -sa, 0.0f, ca };
		}
		else
		{
			// Full orientation.
			const f32 cX = cosf(angles.x);
			const f32 sX = sinf(angles.x);
			const f32 cY = cosf(angles.y);
			const f32 sY = sinf(angles.y);
			const f32 cZ = cosf(angles.z);
			const f32 sZ = sinf(angles.z);

			mat[0] = { cZ * cY, cZ * sY * sX - sZ * cX, cZ * sY * cX + sZ * sX };
			mat[1] = { sZ * cY, sZ * sY * sX + cZ * cX, sZ * sY * cX - cZ * sX };
			mat[2] = {     -sY,                cY * sX,                cY * cX };
		}
	}

	// Inputs: model, position, orientation and camera data.
	// Output: screen rect - the screenspace area that the model covers: min = screenRect[0], max = screenRect[1]
	//         returns true if the model is at least partially on screen, false if not visible.
	// TODO: Save transform matrix.
	bool computeScreenSize(const Model* model, const Vec3f* modelOrientation, const Vec3f* modelPos, const Vec3f* cameraPos, const f32* cameraRot, f32 heightOffset, Vec2i* screenRect)
	{
		const f32 yaw = modelOrientation->y * PI / 180.0f;
		const f32 pitch = modelOrientation->x * PI / 180.0f;
		const f32 roll = modelOrientation->z * PI / 180.0f;

		Vec3f mat33[3];
		buildRotationMatrix({ roll, yaw, pitch }, mat33);

		// Transform the local AABB into view space.
		const Vec3f aabbLocal[] =
		{
			{ model->localAabb[0].x, model->localAabb[0].y, model->localAabb[0].z },
			{ model->localAabb[1].x, model->localAabb[0].y, model->localAabb[0].z },
			{ model->localAabb[1].x, model->localAabb[0].y, model->localAabb[1].z },
			{ model->localAabb[0].x, model->localAabb[0].y, model->localAabb[1].z },
			{ model->localAabb[0].x, model->localAabb[1].y, model->localAabb[0].z },
			{ model->localAabb[1].x, model->localAabb[1].y, model->localAabb[0].z },
			{ model->localAabb[1].x, model->localAabb[1].y, model->localAabb[1].z },
			{ model->localAabb[0].x, model->localAabb[1].y, model->localAabb[1].z }
		};
		Vec3f aabbView[8];
		Vec2f aabbScreen[16];

		const f32 clip = 1.0f;
		s32 zBehind = 0;
		for (u32 i = 0; i < 8; i++)
		{
			const f32 wx = aabbLocal[i].x*s_scale.x;
			const f32 wy = aabbLocal[i].y*s_scale.y;
			const f32 wz = aabbLocal[i].z*s_scale.z;

			const f32 vx = wx * mat33[0].x + wy * mat33[0].y + wz * mat33[0].z + modelPos->x - cameraPos->x;
			const f32 vy = wx * mat33[1].x + wy * mat33[1].y + wz * mat33[1].z + modelPos->y - cameraPos->y;
			const f32 vz = wx * mat33[2].x + wy * mat33[2].y + wz * mat33[2].z + modelPos->z - cameraPos->z;

			aabbView[i].x = vx * cameraRot[0] + vz * cameraRot[1];
			aabbView[i].y = vy;
			aabbView[i].z = -vx * cameraRot[1] + vz * cameraRot[0];

			if (aabbView[i].z < clip) { zBehind++; }
		}

		// Is the AABB completely behind the camera?
		if (zBehind == 8) { return false; }

		// If not in front, clip any edges that pass through the near plane.
		s32 index = 0;
		if (zBehind)
		{
			for (u32 e = 0; e < 12; e++)
			{
				const s32 i0 = c_aabbEdges[(e << 1)], i1 = c_aabbEdges[(e << 1) + 1];
				if (aabbView[i0].z >= clip)
				{
					const f32 vx = aabbView[i0].x;
					const f32 vy = aabbView[i0].y;
					const f32 vz = aabbView[i0].z;
					// project.
					const f32 rz = 1.0f / vz;
					aabbScreen[index].x = s_halfWidth + vx * s_focalLength * rz;
					aabbScreen[index].z = heightOffset + vy * s_heightScale * rz;
					index++;
				}
				if ((aabbView[i0].z < clip && aabbView[i1].z >= clip) || (aabbView[i0].z >= clip && aabbView[i1].z < clip))
				{
					// clip
					const f32 s = (clip - aabbView[i0].z) / (aabbView[i1].z - aabbView[i0].z);
					const f32 vx = aabbView[i0].x + s * (aabbView[i1].x - aabbView[i0].x);
					const f32 vy = aabbView[i0].y + s * (aabbView[i1].y - aabbView[i0].y);
					const f32 vz = aabbView[i0].z + s * (aabbView[i1].z - aabbView[i0].z);
					// project
					const f32 rz = 1.0f / vz;
					aabbScreen[index].x = s_halfWidth + vx * s_focalLength * rz;
					aabbScreen[index].z = heightOffset + vy * s_heightScale * rz;
					index++;
				}
			}
		}
		else
		{
			for (u32 i = 0; i < 8; i++)
			{
				const f32 vx = aabbView[i].x;
				const f32 vy = aabbView[i].y;
				const f32 vz = aabbView[i].z;
				// project.
				const f32 rz = 1.0f / vz;
				aabbScreen[index].x = s_halfWidth + vx * s_focalLength * rz;
				aabbScreen[index].z = heightOffset + vy * s_heightScale * rz;
				index++;
			}
		}
		// If no points are in front of the near plane then this model isn't visible.
		if (index < 1) { return false; }

		// Finally compute a screen rect.
		Vec2f rect[2] = { aabbScreen[0], aabbScreen[0] };
		for (s32 i = 1; i < index; i++)
		{
			rect[0].x = std::min(rect[0].x, aabbScreen[i].x);
			rect[0].z = std::min(rect[0].z, aabbScreen[i].z);
			rect[1].x = std::max(rect[1].x, aabbScreen[i].x);
			rect[1].z = std::max(rect[1].z, aabbScreen[i].z);
		}

		// Is the screen rect visible?
		if (rect[1].x < 0.0f || rect[1].z < 0.0f) { return false; }
		if (rect[0].x >= s_width || rect[0].z >= s_height) { return false; }

		// Finally write the results.
		screenRect[0].x = s32(rect[0].x);
		screenRect[0].z = s32(rect[0].z);
		screenRect[1].x = s32(rect[1].x);
		screenRect[1].z = s32(rect[1].z);
		return true;
	}
				
	void setClip(s32 x0, s32 x1, const s16* upperYMask, const s16* lowerYMask)
	{
		s_XClip[0] = x0;
		s_XClip[1] = x1;
		s_YClipUpper = upperYMask;
		s_YClipLower = lowerYMask;

		s_renderer->setHLineClip(x0, x1, upperYMask, lowerYMask);
	}

	void setModelScale(const Vec3f* scale)
	{
		s_scale = *scale;
	}
							
	void draw(const Model* model, const Vec3f* modelOrientation, const Vec3f* modelPos, const Vec3f* cameraPos, const f32* cameraRot, f32 heightOffset, const Sector* sector)
	{
		const f32 yaw = modelOrientation->y * PI / 180.0f;
		const f32 pitch = modelOrientation->x * PI / 180.0f;
		const f32 roll = modelOrientation->z * PI / 180.0f;

		Vec3f mat33[3];
		buildRotationMatrix({ roll, yaw, pitch }, mat33);

		s_heightOffset = heightOffset ? heightOffset : s_halfHeight;
		s_sector = sector;
		s_cameraPos = cameraPos;
		s_cameraRot = cameraRot;

		const u32 objectCount = model->objectCount;
		if (!objectCount) { return; }

		s_modelTransformed.resize(model->vertexCount);
		s_polygons.resize(model->polygonCount);

		// HACK: Dark forces uses the first color when using VERTEX shading.
		u8 vertexColor = 0;
		if (!model->objects[0].triangles.empty())
		{
			vertexColor = model->objects[0].triangles[0].color;
		}
		else
		{
			vertexColor = model->objects[0].quads[0].color;
		}

		const s32 ambient = sector ? sector->ambient : 31;

		// 1. Go through objects, transform vertices and add polygons to be sorted.
		Vec3f* transformed = s_modelTransformed.data();
		u32 polygonOffset = 0;
		f32 vertScale = s_flipVert ? -1.0f : 1.0f;
		for (u32 i = 0; i < objectCount; i++)
		{
			const ModelObject* obj = &model->objects[i];

			const u32 vertexCount = (u32)obj->vertices.size();
			const Vertex3f* vertices = obj->vertices.data();
			const u32 vertexOffset = obj->vertexOffset;
			for (u32 v = 0; v < vertexCount; v++)
			{
				f32 wx =  vertices[v].x * s_scale.x;
				f32 wy =  vertices[v].y * vertScale * s_scale.y;
				f32 wz =  vertices[v].z * s_scale.z;

				f32 vx = wx * mat33[0].x + wy * mat33[0].y + wz * mat33[0].z + modelPos->x - cameraPos->x;
				f32 vy = wx * mat33[1].x + wy * mat33[1].y + wz * mat33[1].z + modelPos->y - cameraPos->y;
				f32 vz = wx * mat33[2].x + wy * mat33[2].y + wz * mat33[2].z + modelPos->z - cameraPos->z;
				
				transformed[v + vertexOffset].x = vx * cameraRot[0] + vz * cameraRot[1];
				transformed[v + vertexOffset].y = vy;
				transformed[v + vertexOffset].z = -vx * cameraRot[1] + vz * cameraRot[0];
			}

			const u32 triCount = (u32)obj->triangles.size();
			const u32 quadCount = (u32)obj->quads.size();
			if (triCount)
			{
				const ModelPolygon* tri = obj->triangles.data();
				for (u32 t = 0; t < triCount; t++, tri++)
				{
					const Vec3f* v0 = &transformed[tri->i0 + vertexOffset];
					const Vec3f* v1 = &transformed[tri->i1 + vertexOffset];
					const Vec3f* v2 = &transformed[tri->i2 + vertexOffset];

					s_polygons[t + polygonOffset].objectIndex = i;
					s_polygons[t + polygonOffset].polyIndex = t;
					s_polygons[t + polygonOffset].sortDepth = (v0->z + v1->z + v2->z) * 0.3333333f;
				}
				polygonOffset += triCount;
			}
			if (quadCount)
			{
				const ModelPolygon* quad = obj->quads.data();
				for (u32 t = 0; t < quadCount; t++, quad++)
				{
					const Vec3f* v0 = &transformed[quad->i0 + vertexOffset];
					const Vec3f* v1 = &transformed[quad->i1 + vertexOffset];
					const Vec3f* v2 = &transformed[quad->i2 + vertexOffset];
					const Vec3f* v3 = &transformed[quad->i3 + vertexOffset];

					s_polygons[t + polygonOffset].objectIndex = i;
					s_polygons[t + polygonOffset].polyIndex = t + triCount;
					s_polygons[t + polygonOffset].sortDepth = (v0->z + v1->z + v2->z + v3->z) * 0.25f;
				}
				polygonOffset += quadCount;
			}
		}

		// Sort the polygons from back to front.
		std::qsort(s_polygons.data(), s_polygons.size(), sizeof(PolySort), sortPolygons);

		// Render polygons from back to front.
		const PolySort* sorted = s_polygons.data();
		const u32 count = model->polygonCount;
		for (u32 i = 0; i < model->polygonCount; i++, sorted++)
		{
			const ModelObject* obj = &model->objects[sorted->objectIndex];
			const s32 textureIndex = obj->textureIndex;
			const TextureFrame* texture = (textureIndex >= 0) && model->textures[textureIndex] ? &model->textures[textureIndex]->frames[0] : nullptr;

			const Vertex2f* texturedVertices = obj->textureVertices.data();
			const TexturePolygon* texturedTri = obj->textureTriangles.data();
			const TexturePolygon* texturedQuad = obj->textureQuads.data();
			const bool hasTexturing = texture && texturedVertices && (texturedTri || texturedQuad);

			const u32 triCount = (u32)obj->triangles.size();
			const u32 quadCount = (u32)obj->quads.size();

			if (sorted->polyIndex < triCount)
			{
				u32 triIndex = sorted->polyIndex;
				const ModelPolygon* tri = &obj->triangles[triIndex];

				const Vec3f* v0 = &transformed[tri->i0 + obj->vertexOffset];
				const Vec3f* v1 = &transformed[tri->i1 + obj->vertexOffset];
				const Vec3f* v2 = &transformed[tri->i2 + obj->vertexOffset];
				const u8 color = tri->color;
				if (tri->shading == SHADING_VERTEX)
				{
					// HACK: Dark Forces uses the first quad color for all quads when using shading VERTEX.
					draw3dPoint(v0, vertexColor);
					draw3dPoint(v1, vertexColor);
					draw3dPoint(v2, vertexColor);
					continue;
				}

				// For now stick with per-poly light falloff for everything.
				const s32 lightLevel = DXL2_RenderCommon::lightFalloff(ambient, sorted->sortDepth);

				if (hasTexturing && tri->shading == SHADING_GOURTEX)
				{
					const Vec2f uv0 = { texturedVertices[texturedTri[triIndex].i0].x, texturedVertices[texturedTri[triIndex].i0].y };
					const Vec2f uv1 = { texturedVertices[texturedTri[triIndex].i1].x, texturedVertices[texturedTri[triIndex].i1].y };
					const Vec2f uv2 = { texturedVertices[texturedTri[triIndex].i2].x, texturedVertices[texturedTri[triIndex].i2].y };

					// compute the light level at each vertex and interpolate.
					const s32 l0 = DXL2_RenderCommon::lightFalloff(ambient, v0->z);
					const s32 l1 = DXL2_RenderCommon::lightFalloff(ambient, v1->z);
					const s32 l2 = DXL2_RenderCommon::lightFalloff(ambient, v2->z);

					if (l0 == l1 && l0 == l2)
					{
						drawTexturedTriangle(v0, v1, v2, &uv0, &uv1, &uv2, texture, l0);
					}
					else
					{
						drawTexturedGouraudTriangle(v0, v1, v2, &uv0, &uv1, &uv2, l0, l1, l2, texture);
					}
				}
				else if (hasTexturing && tri->shading == SHADING_PLANE)
				{
					drawPlaneTriangle(v0, v1, v2, texture);
				}
				else if (hasTexturing && tri->shading == SHADING_TEXTURE)
				{
					const Vec2f uv0 = { texturedVertices[texturedTri[triIndex].i0].x, texturedVertices[texturedTri[triIndex].i0].y };
					const Vec2f uv1 = { texturedVertices[texturedTri[triIndex].i1].x, texturedVertices[texturedTri[triIndex].i1].y };
					const Vec2f uv2 = { texturedVertices[texturedTri[triIndex].i2].x, texturedVertices[texturedTri[triIndex].i2].y };

					drawTexturedTriangle(v0, v1, v2, &uv0, &uv1, &uv2, texture, lightLevel);
				}
				else if (tri->shading == SHADING_GOURAUD)
				{
					// compute the light level at each vertex and interpolate.
					const s32 l0 = DXL2_RenderCommon::lightFalloff(ambient, v0->z);
					const s32 l1 = DXL2_RenderCommon::lightFalloff(ambient, v1->z);
					const s32 l2 = DXL2_RenderCommon::lightFalloff(ambient, v2->z);
					if (l0 == l1 && l0 == l2)
					{
						drawTriangle(v0, v1, v2, color, l0);
					}
					else
					{
						drawTriangleGouraud(v0, v1, v2, color, (f32)l0, (f32)l1, (f32)l2);
					}
				}
				else
				{
					drawTriangle(v0, v1, v2, color, lightLevel);
				}
			}
			else
			{
				u32 quadIndex = sorted->polyIndex - triCount;
				const ModelPolygon* quad = &obj->quads[quadIndex];

				const Vec3f* v0 = &transformed[quad->i0 + obj->vertexOffset];
				const Vec3f* v1 = &transformed[quad->i1 + obj->vertexOffset];
				const Vec3f* v2 = &transformed[quad->i2 + obj->vertexOffset];
				const Vec3f* v3 = &transformed[quad->i3 + obj->vertexOffset];
				const u8 color = quad->color;
				if (quad->shading == SHADING_VERTEX)
				{
					// HACK: Dark Forces uses the first quad color for all quads when using shading VERTEX.
					draw3dPoint(v0, vertexColor);
					draw3dPoint(v1, vertexColor);
					draw3dPoint(v2, vertexColor);
					draw3dPoint(v3, vertexColor);
					continue;
				}

				// For now stick with per-poly light falloff for everything.
				const s32 lightLevel = DXL2_RenderCommon::lightFalloff(ambient, sorted->sortDepth);

				if (hasTexturing && quad->shading == SHADING_GOURTEX)
				{
					const Vec2f uv0 = { texturedVertices[texturedQuad[quadIndex].i0].x, texturedVertices[texturedQuad[quadIndex].i0].y };
					const Vec2f uv1 = { texturedVertices[texturedQuad[quadIndex].i1].x, texturedVertices[texturedQuad[quadIndex].i1].y };
					const Vec2f uv2 = { texturedVertices[texturedQuad[quadIndex].i2].x, texturedVertices[texturedQuad[quadIndex].i2].y };
					const Vec2f uv3 = { texturedVertices[texturedQuad[quadIndex].i3].x, texturedVertices[texturedQuad[quadIndex].i3].y };

					const s32 l0 = DXL2_RenderCommon::lightFalloff(ambient, v0->z);
					const s32 l1 = DXL2_RenderCommon::lightFalloff(ambient, v1->z);
					const s32 l2 = DXL2_RenderCommon::lightFalloff(ambient, v2->z);
					const s32 l3 = DXL2_RenderCommon::lightFalloff(ambient, v3->z);

					if (l0 == l1 && l0 == l2 && l0 == l3)
					{
						drawTexturedQuad(v0, v1, v2, v3, &uv0, &uv1, &uv2, &uv3, texture, l0);
					}
					else
					{
						drawTexturedGouraudQuad(v0, v1, v2, v3, &uv0, &uv1, &uv2, &uv3, l0, l1, l2, l3, texture);
					}
				}
				else if (hasTexturing && quad->shading == SHADING_PLANE)
				{
					drawPlaneQuad(v0, v1, v2, v3, texture);
				}
				else if (hasTexturing && quad->shading == SHADING_TEXTURE)
				{
					const Vec2f uv0 = { texturedVertices[texturedQuad[quadIndex].i0].x, texturedVertices[texturedQuad[quadIndex].i0].y };
					const Vec2f uv1 = { texturedVertices[texturedQuad[quadIndex].i1].x, texturedVertices[texturedQuad[quadIndex].i1].y };
					const Vec2f uv2 = { texturedVertices[texturedQuad[quadIndex].i2].x, texturedVertices[texturedQuad[quadIndex].i2].y };
					const Vec2f uv3 = { texturedVertices[texturedQuad[quadIndex].i3].x, texturedVertices[texturedQuad[quadIndex].i3].y };

					drawTexturedQuad(v0, v1, v2, v3, &uv0, &uv1, &uv2, &uv3, texture, lightLevel);
				}
				else if (quad->shading == SHADING_GOURAUD)
				{
					// compute the light level at each vertex and interpolate.
					const s32 l0 = DXL2_RenderCommon::lightFalloff(ambient, v0->z);
					const s32 l1 = DXL2_RenderCommon::lightFalloff(ambient, v1->z);
					const s32 l2 = DXL2_RenderCommon::lightFalloff(ambient, v2->z);
					const s32 l3 = DXL2_RenderCommon::lightFalloff(ambient, v3->z);
					if (l0 == l1 && l0 == l2 && l0 == l3)
					{
						drawQuad(v0, v1, v2, v3, color, l0);
					}
					else
					{
						drawQuadGouraud(v0, v1, v2, v3, color, (f32)l0, (f32)l1, (f32)l2, (f32)l3);
					}
				}
				else
				{
					drawQuad(v0, v1, v2, v3, color, lightLevel);
				}
			}
		}
	}

	void buildClipPlanes(s32 pitchOffset)
	{
		// X Planes are fairly standard.
		Vec2f pX = { 1.0f, s_focalLength / s_halfWidth };
		Vec2f n = { -pX.z, pX.x };
		f32 scale = 1.0f / sqrtf(n.x*n.x + n.z*n.z);
		n.x *= scale; n.z *= scale;

		s_planes[0].x = n.x;
		s_planes[0].z = n.z;
		s_planes[0].y = 0.0f;

		s_planes[1].x = -n.x;
		s_planes[1].z = n.z;
		s_planes[1].y = 0.0f;

		// Y Planes are sheared...
		Vec2f pY = { s_heightScale / s_halfHeight + f32(s_halfHeight + abs(pitchOffset)) / s_halfHeight, s_focalLength / s_halfWidth };
		n = { -pY.z, pY.x };
		scale = 1.0f / sqrtf(n.x*n.x + n.z*n.z);
		n.x *= scale; n.z *= scale;

		s_planes[2].x = 0.0f;
		s_planes[2].z = n.z;
		s_planes[2].y = n.x;

		s_planes[3].x = 0.0f;
		s_planes[3].z = n.z;
		s_planes[3].y = -n.x;
	}

	/////////////////////////////////////////////////
	// Internal Implementation
	/////////////////////////////////////////////////
	bool clipNearPlane(ClipPolygon* clipPoly, f32 clip)
	{
		ClipPolygon clipPolyNext;
		clipPolyNext.flags = clipPoly->flags;
		clipPolyNext.count = 0;
		for (u32 v = 0; v < clipPoly->count; v++)
		{
			const s32 i0 = v, i1 = (v + 1) % clipPoly->count;
			const Vec3f* p0 = &clipPoly->vtx[i0];
			const Vec3f* p1 = &clipPoly->vtx[i1];
			const Vec2f* uv0 = &clipPoly->uv[i0];
			const Vec2f* uv1 = &clipPoly->uv[i1];
			const f32 l0 = clipPoly->lightLevel[i0];
			const f32 l1 = clipPoly->lightLevel[i1];

			if (p0->z >= clip)
			{
				clipPolyNext.vtx[clipPolyNext.count] = *p0;
				if (clipPoly->flags & PF_TEXTURED)
				{
					clipPolyNext.uv[clipPolyNext.count] = *uv0;
				}
				if (clipPoly->flags & PF_GOURAUD)
				{
					clipPolyNext.lightLevel[clipPolyNext.count] = l0;
				}
				clipPolyNext.count++;
			}

			if ((p0->z < clip && p1->z >= clip) || (p0->z >= clip && p1->z < clip))
			{
				const f32 s = (clip - p0->z) / (p1->z - p0->z);
				clipPolyNext.vtx[clipPolyNext.count].x = p0->x + s * (p1->x - p0->x);
				clipPolyNext.vtx[clipPolyNext.count].y = p0->y + s * (p1->y - p0->y);
				clipPolyNext.vtx[clipPolyNext.count].z = p0->z + s * (p1->z - p0->z);

				if (clipPoly->flags & PF_TEXTURED)
				{
					clipPolyNext.uv[clipPolyNext.count].x = uv0->x + s * (uv1->x - uv0->x);
					clipPolyNext.uv[clipPolyNext.count].z = uv0->z + s * (uv1->z - uv0->z);
				}
				if (clipPoly->flags & PF_GOURAUD)
				{
					clipPolyNext.lightLevel[clipPolyNext.count] = l0 + s * (l1 - l0);
				}
				clipPolyNext.count++;
			}
		}
		*clipPoly = clipPolyNext;
		return clipPoly->count >= 3;
	}

	bool clipAgainstPlane(ClipPolygon* clipPoly, u32 index)
	{
		ClipPolygon clipPolyNext;
		clipPolyNext.flags = clipPoly->flags;
		clipPolyNext.count = 0;
		const Vec3f* plane = &s_planes[index];
		f32 clip = 0.0f;
		for (u32 v = 0; v < clipPoly->count; v++)
		{
			const s32 i0 = v, i1 = (v + 1) % clipPoly->count;
			const Vec3f* p0 = &clipPoly->vtx[i0];
			const Vec3f* p1 = &clipPoly->vtx[i1];
			const Vec2f* uv0 = &clipPoly->uv[i0];
			const Vec2f* uv1 = &clipPoly->uv[i1];
			const f32 l0 = clipPoly->lightLevel[i0];
			const f32 l1 = clipPoly->lightLevel[i1];
			// Plane distances.
			const f32 d0 = p0->x * plane->x + p0->y * plane->y + p0->z * plane->z;
			const f32 d1 = p1->x * plane->x + p1->y * plane->y + p1->z * plane->z;

			if (d0 > clip)
			{
				clipPolyNext.vtx[clipPolyNext.count] = clipPoly->vtx[i0];
				if (clipPoly->flags & PF_TEXTURED)
				{
					clipPolyNext.uv[clipPolyNext.count] = clipPoly->uv[i0];
				}
				if (clipPoly->flags & PF_GOURAUD)
				{
					clipPolyNext.lightLevel[clipPolyNext.count] = l0;
				}
				clipPolyNext.count++;
			}

			if ((d0 >= clip && d1 < clip) || (d0 < clip && d1 >= clip))
			{
				const f32 s = (clip - d0) / (d1 - d0);
				clipPolyNext.vtx[clipPolyNext.count].x = p0->x + s * (p1->x - p0->x);
				clipPolyNext.vtx[clipPolyNext.count].y = p0->y + s * (p1->y - p0->y);
				clipPolyNext.vtx[clipPolyNext.count].z = p0->z + s * (p1->z - p0->z);

				if (clipPoly->flags & PF_TEXTURED)
				{
					clipPolyNext.uv[clipPolyNext.count].x = uv0->x + s * (uv1->x - uv0->x);
					clipPolyNext.uv[clipPolyNext.count].z = uv0->z + s * (uv1->z - uv0->z);
				}
				if (clipPoly->flags & PF_GOURAUD)
				{
					clipPolyNext.lightLevel[clipPolyNext.count] = l0 + s * (l1 - l0);
				}
				clipPolyNext.count++;
			}
		}
		*clipPoly = clipPolyNext;
		return clipPoly->count >= 3;
	}
	
	void drawPolygon(ClipPolygon* clipPoly, u8 color, const TextureFrame* texture, s32 ambient)
	{
		f32 nearClip = 0.1f;
		bool behindNear = false;
		bool inFrontNear = true;
		for (u32 v = 0; v < clipPoly->count; v++)
		{
			if (clipPoly->vtx[v].z >= nearClip) { behindNear = false; }
			if (clipPoly->vtx[v].z < nearClip) { inFrontNear = false; }
		}
		if (behindNear) { return; }

		// Clip against the side planes first.
		for (u32 i = 0; i < 4; i++)
		{
			if (!clipAgainstPlane(clipPoly, i)) { return; }
		}

		// Clip against the near plane (if needed).
		if (!inFrontNear && !clipNearPlane(clipPoly, nearClip)) { return; }

		// Now that we have clipped against the frustum, it is ok to project.
		const f32 planeHeight = fabsf(clipPoly->vtx[0].y);
		bool facingDown = true;
		if (clipPoly->flags&PF_PLANE)
		{
			const Vec3f Uview = { clipPoly->vtx[1].x - clipPoly->vtx[0].x, clipPoly->vtx[1].y - clipPoly->vtx[0].y, clipPoly->vtx[1].z - clipPoly->vtx[0].z };
			const Vec3f Vview = { clipPoly->vtx[2].x - clipPoly->vtx[0].x, clipPoly->vtx[2].y - clipPoly->vtx[0].y, clipPoly->vtx[2].z - clipPoly->vtx[0].z };
			const f32 yN = Uview.z*Vview.x - Uview.x*Vview.z;
			facingDown = yN >= 0.0f;
		}

		for (u32 v = 0; v < clipPoly->count; v++)
		{
			const f32 rz = 1.0f / clipPoly->vtx[v].z;
			clipPoly->vtx[v].x = s_halfWidth + clipPoly->vtx[v].x * s_focalLength * rz;
			clipPoly->vtx[v].y = s_heightOffset + clipPoly->vtx[v].y * s_heightScale * rz;
		}

		// Handle backface culling after projection.
		const Vec3f U = { clipPoly->vtx[1].x - clipPoly->vtx[0].x, clipPoly->vtx[1].y - clipPoly->vtx[0].y, clipPoly->vtx[1].z - clipPoly->vtx[0].z };
		const Vec3f V = { clipPoly->vtx[2].x - clipPoly->vtx[0].x, clipPoly->vtx[2].y - clipPoly->vtx[0].y, clipPoly->vtx[2].z - clipPoly->vtx[0].z };
		const f32 zN = U.x*V.y - U.y*V.x;
		if ((zN < 0.0f && !s_flipVert) || (zN > 0.0f && s_flipVert)) { return; }

		assert(clipPoly->count >= 3);
		// 1. Find the minimum and maximum Y vertex.
		s32 yMin = (s32)clipPoly->vtx[0].y;
		s32 yMax = (s32)clipPoly->vtx[0].y;
		for (u32 v = 1; v < clipPoly->count; v++)
		{
			yMin = std::min(yMin, (s32)clipPoly->vtx[v].y);
			yMax = std::max(yMax, (s32)clipPoly->vtx[v].y);
		}

		// 2. Resize our span array to make sure its big enough.
		s32 yCount = (s32)yMax - (s32)yMin + 1;
		s_scanlineArray.resize(yCount);
		Scanline* scanlines = s_scanlineArray.data();
		for (s32 y = 0; y < yCount; y++)
		{
			scanlines[y].x0 = INT_MAX;
			scanlines[y].x1 = INT_MIN;
		}

		// 3. Loop through the edges and figure out the minimum and maximum x values.
		bool textureMapped = (clipPoly->flags&PF_TEXTURED) != 0 && texture;
		bool planeMapped = (clipPoly->flags&PF_PLANE) != 0 && texture;
		bool gouraud = (clipPoly->flags&PF_GOURAUD) != 0;
		if (textureMapped && s_persCorrect)
		{
			for (u32 v = 0; v < clipPoly->count; v++)
			{
				const f32 rz = 1.0f / clipPoly->vtx[v].z;
				clipPoly->vtx[v].z = rz;
				clipPoly->uv[v].x *= rz;
				clipPoly->uv[v].z *= rz;
			}
		}
		
		const s32 texWidth = texture ? texture->uvWidth : 0;
		const s32 texHeight = texture ? texture->uvHeight : 0;
		if (texWidth <= 0 || texHeight <= 0) { textureMapped = false; planeMapped = false; }
		for (u32 v = 0; v < clipPoly->count; v++)
		{
			const s32 i0 = v, i1 = (v + 1) % clipPoly->count;
			const Vec2i v0 = { (s32)clipPoly->vtx[i0].x, (s32)clipPoly->vtx[i0].y };
			const Vec2i v1 = { (s32)clipPoly->vtx[i1].x, (s32)clipPoly->vtx[i1].y };
			const Vec2f uv0 = { clipPoly->uv[i0].x, clipPoly->uv[i0].z };
			const Vec2f uv1 = { clipPoly->uv[i1].x, clipPoly->uv[i1].z };
			const f32 light0 = clipPoly->lightLevel[i0];
			const f32 light1 = clipPoly->lightLevel[i1];
			const f32 rz0 = clipPoly->vtx[i0].z;
			const f32 rz1 = clipPoly->vtx[i1].z;
			if (v0.z == v1.z) { continue; }

			s32 dx = v1.x - v0.x, dy = v1.z - v0.z;
			s32 incXH, incXL, incYH, incYL;
			s32 longDim, shortDim;

			s32 dy0 = dy;
			if (dx >= 0) { incXH = incXL = 1; }
			else { dx = -dx; incXH = incXL = -1; }
			if (dy >= 0) { incYH = incYL = 1; }
			else { dy = -dy; incYH = incYL = -1; }
			if (dx >= dy) { longDim = dx; shortDim = dy; incYL = 0; }
			else { longDim = dy; shortDim = dx; incXL = 0; }

			const s32 incDL = 2 * shortDim;
			const s32 incDH = 2 * shortDim - 2 * longDim;
			s32 d = 2 * shortDim - longDim;
			s32 xc = v0.x, yc = v0.z;

			s32 s0, t0, s1, t1, ds = 0, dt = 0, s = 0, t = 0;
			s32 z0=0, z1=0, z=0, dz=0;
			s32 l0=0, l1=0, l=0, dl=0;
			if (textureMapped)
			{
				s0 = s32(uv0.x * 1048576.0f * texWidth);
				t0 = s32(uv0.z * 1048576.0f * texHeight);
				s1 = s32(uv1.x * 1048576.0f * texWidth);
				t1 = s32(uv1.z * 1048576.0f * texHeight);
				ds = (s1 - s0) / longDim;
				dt = (t1 - t0) / longDim;
				s = s0; t = t0;

				if (s_persCorrect)
				{
					z0 = s32(rz0 * 1048576.0f);
					z1 = s32(rz1 * 1048576.0f);
					dz = (z1 - z0) / longDim;
					z = z0;
				}
			}
			if (gouraud)
			{
				l0 = s32(light0 * 65536.0f);
				l1 = s32(light1 * 65536.0f);
				dl = (l1 - l0) / longDim;
				l = l0;
			}

			for (s32 i = 0; i <= longDim; i++)
			{
				const s32 yIndex = yc - yMin;
				if (xc < scanlines[yIndex].x0)
				{
					scanlines[yIndex].x0 = xc;
					if (textureMapped)
					{
						scanlines[yIndex].u0 = s;
						scanlines[yIndex].v0 = t;
						scanlines[yIndex].z0 = z;
					}
					if (gouraud)
					{
						scanlines[yIndex].l0 = l;
					}
				}
				if (xc > scanlines[yIndex].x1)
				{
					scanlines[yIndex].x1 = xc;
					if (textureMapped)
					{
						scanlines[yIndex].u1 = s;
						scanlines[yIndex].v1 = t;
						scanlines[yIndex].z1 = z;
					}
					if (gouraud)
					{
						scanlines[yIndex].l1 = l;
					}
				}

				// Find the next point on the line.
				if (d >= 0) { xc += incXH; yc += incYH; d += incDH; }
				else { xc += incXL; yc += incYL; d += incDL; }
				s += ds; t += dt; z += dz; l += dl;
			}
		}

		// 4. Rasterize scanlines.
		// Y Clipping isn't pixel precise, so clip to the screen here.
		const s32 start = (yMin < 0) ? -yMin : 0;
		if (yCount + yMin > s_height)
		{
			yCount = s_height - yMin;
		}

		if (planeMapped)
		{
			// Constants for drawing spans with world to texel space scaling baking in.
			const f32 xOffset = s_cameraPos->x * c_worldToTexelScale;
			const f32 zOffset = s_cameraPos->z * c_worldToTexelScale;
			const f32 r0 = s_cameraRot[0] * c_worldToTexelScale;
			const f32 r1 = s_cameraRot[1] * c_worldToTexelScale;

			const f32 defaultOffsets[] = { 0.0f, 0.0f };
			const f32* texOffsets = defaultOffsets;
			s32 ambient = 31;
			if (s_sector)
			{
				const f32 floorOffset[] = { s_sector->floorTexture.offsetX, s_sector->floorTexture.offsetY };
				const f32 ceilOffset[] = { s_sector->ceilTexture.offsetX, s_sector->ceilTexture.offsetY };
				texOffsets = facingDown ? floorOffset : ceilOffset;
				ambient = s_sector->ambient;
			}

			for (s32 y = start; y < yCount; y++)
			{
				const f32 yf = fabsf(y + yMin - s_heightOffset);
				const f32 z = (s_heightScale / yf) * planeHeight;
				const s32 lightLevel = DXL2_RenderCommon::lightFalloff(ambient, z);

				s_renderer->drawSpanClip(scanlines[y].x0, scanlines[y].x1, y + yMin, z, r0, r1, xOffset - texOffsets[0] * c_worldToTexelScale, zOffset - texOffsets[1] * c_worldToTexelScale, lightLevel, texHeight, texWidth, texture->image);
			}
		}
		else if (textureMapped)
		{
			const s32 imageWidth = texture ? texture->width : 0;
			const s32 imageHeight = texture ? texture->height : 0;
			if (s_persCorrect)
			{
				if (gouraud)
				{
					for (s32 y = start; y < yCount; y++)
					{
						if (scanlines[y].x0 > scanlines[y].x1) { continue; }
						s_renderer->drawHLineTexturedGouraudPC(scanlines[y].x0, scanlines[y].x1, y + yMin, scanlines[y].u0, scanlines[y].v0, scanlines[y].u1, scanlines[y].v1,
							scanlines[y].z0, scanlines[y].z1, scanlines[y].l0, scanlines[y].l1, imageWidth, imageHeight, texture->image);
					}
				}
				else
				{
					for (s32 y = start; y < yCount; y++)
					{
						if (scanlines[y].x0 > scanlines[y].x1) { continue; }
						s_renderer->drawHLineTexturedPC(scanlines[y].x0, scanlines[y].x1, y + yMin, scanlines[y].u0, scanlines[y].v0, scanlines[y].u1, scanlines[y].v1,
							scanlines[y].z0, scanlines[y].z1, imageWidth, imageHeight, texture->image, ambient);
					}
				}
			}
			else if (gouraud)
			{
				for (s32 y = start; y < yCount; y++)
				{
					if (scanlines[y].x0 > scanlines[y].x1) { continue; }
					s_renderer->drawHLineTexturedGouraud(scanlines[y].x0, scanlines[y].x1, y + yMin, scanlines[y].u0, scanlines[y].v0, scanlines[y].u1, scanlines[y].v1, 
						scanlines[y].l0, scanlines[y].l1, imageWidth, imageHeight, texture->image);
				}
			}
			else
			{
				for (s32 y = start; y < yCount; y++)
				{
					if (scanlines[y].x0 > scanlines[y].x1) { continue; }
					s_renderer->drawHLineTextured(scanlines[y].x0, scanlines[y].x1, y + yMin, scanlines[y].u0, scanlines[y].v0, scanlines[y].u1, scanlines[y].v1, imageWidth, imageHeight, texture->image, ambient);
				}
			}
		}
		else if (gouraud)
		{
			for (s32 y = start; y < yCount; y++)
			{
				if (scanlines[y].x0 > scanlines[y].x1) { continue; }
				s_renderer->drawHLineGouraud(scanlines[y].x0, scanlines[y].x1, scanlines[y].l0, scanlines[y].l1, y + yMin, color);
			}
		}
		else
		{
			for (s32 y = start; y < yCount; y++)
			{
				if (scanlines[y].x0 > scanlines[y].x1) { continue; }
				s_renderer->drawHLine(scanlines[y].x0, scanlines[y].x1, y + yMin, color, ambient);
			}
		}
	}
		
	void drawTriangle(const Vec3f* v0, const Vec3f* v1, const Vec3f* v2, u8 color, s32 ambient)
	{
		ClipPolygon clipPoly;
		clipPoly.count = 3;
		clipPoly.flags = 0;
		clipPoly.vtx[0] = *v0;
		clipPoly.vtx[1] = *v1;
		clipPoly.vtx[2] = *v2;
		drawPolygon(&clipPoly, color, nullptr, ambient);
	}

	void drawQuad(const Vec3f* v0, const Vec3f* v1, const Vec3f* v2, const Vec3f* v3, u8 color, s32 ambient)
	{
		ClipPolygon clipPoly;
		clipPoly.count = 4;
		clipPoly.flags = 0;
		clipPoly.vtx[0] = *v0;
		clipPoly.vtx[1] = *v1;
		clipPoly.vtx[2] = *v2;
		clipPoly.vtx[3] = *v3;
		drawPolygon(&clipPoly, color, nullptr, ambient);
	}
	
	void drawTriangleGouraud(const Vec3f* v0, const Vec3f* v1, const Vec3f* v2, u8 color, f32 l0, f32 l1, f32 l2)
	{
		ClipPolygon clipPoly;
		clipPoly.count = 3;
		clipPoly.flags = PF_GOURAUD;
		clipPoly.vtx[0] = *v0;
		clipPoly.vtx[1] = *v1;
		clipPoly.vtx[2] = *v2;
		clipPoly.lightLevel[0] = l0;
		clipPoly.lightLevel[1] = l1;
		clipPoly.lightLevel[2] = l2;
		drawPolygon(&clipPoly, color, nullptr, 0);
	}

	void drawQuadGouraud(const Vec3f* v0, const Vec3f* v1, const Vec3f* v2, const Vec3f* v3, u8 color, f32 l0, f32 l1, f32 l2, f32 l3)
	{
		ClipPolygon clipPoly;
		clipPoly.count = 4;
		clipPoly.flags = PF_GOURAUD;
		clipPoly.vtx[0] = *v0;
		clipPoly.vtx[1] = *v1;
		clipPoly.vtx[2] = *v2;
		clipPoly.vtx[3] = *v3;
		clipPoly.lightLevel[0] = l0;
		clipPoly.lightLevel[1] = l1;
		clipPoly.lightLevel[2] = l2;
		clipPoly.lightLevel[3] = l3;
		drawPolygon(&clipPoly, color, nullptr, 0);
	}
		
	void drawPlaneTriangle(const Vec3f* v0, const Vec3f* v1, const Vec3f* v2, const TextureFrame* texture)
	{
		ClipPolygon clipPoly;
		clipPoly.count = 3;
		clipPoly.flags = PF_PLANE;
		clipPoly.vtx[0] = *v0;
		clipPoly.vtx[1] = *v1;
		clipPoly.vtx[2] = *v2;
		drawPolygon(&clipPoly, 31, texture, 0);
	}

	void drawPlaneQuad(const Vec3f* v0, const Vec3f* v1, const Vec3f* v2, const Vec3f* v3, const TextureFrame* texture)
	{
		ClipPolygon clipPoly;
		clipPoly.count = 4;
		clipPoly.flags = PF_PLANE;
		clipPoly.vtx[0] = *v0;
		clipPoly.vtx[1] = *v1;
		clipPoly.vtx[2] = *v2;
		clipPoly.vtx[3] = *v3;
		drawPolygon(&clipPoly, 31, texture, 0);
	}

	void drawTexturedTriangle(const Vec3f* v0, const Vec3f* v1, const Vec3f* v2, const Vec2f* uv0, const Vec2f* uv1, const Vec2f* uv2, const TextureFrame* texture, s32 ambient)
	{
		ClipPolygon clipPoly;
		clipPoly.count = 3;
		clipPoly.flags = PF_TEXTURED;
		clipPoly.vtx[0] = *v0;
		clipPoly.vtx[1] = *v1;
		clipPoly.vtx[2] = *v2;
		clipPoly.uv[0] = *uv0;
		clipPoly.uv[1] = *uv1;
		clipPoly.uv[2] = *uv2;
		drawPolygon(&clipPoly, 31, texture, ambient);
	}

	void drawTexturedQuad(const Vec3f* v0, const Vec3f* v1, const Vec3f* v2, const Vec3f* v3, const Vec2f* uv0, const Vec2f* uv1, const Vec2f* uv2, const Vec2f* uv3, const TextureFrame* texture, s32 ambient)
	{
		ClipPolygon clipPoly;
		clipPoly.count = 4;
		clipPoly.flags = PF_TEXTURED;
		clipPoly.vtx[0] = *v0;
		clipPoly.vtx[1] = *v1;
		clipPoly.vtx[2] = *v2;
		clipPoly.vtx[3] = *v3;
		clipPoly.uv[0] = *uv0;
		clipPoly.uv[1] = *uv1;
		clipPoly.uv[2] = *uv2;
		clipPoly.uv[3] = *uv3;
		drawPolygon(&clipPoly, 31, texture, ambient);
	}

	void drawTexturedGouraudTriangle(const Vec3f* v0, const Vec3f* v1, const Vec3f* v2, const Vec2f* uv0, const Vec2f* uv1, const Vec2f* uv2, s32 l0, s32 l1, s32 l2, const TextureFrame* texture)
	{
		ClipPolygon clipPoly;
		clipPoly.count = 3;
		clipPoly.flags = PF_TEXTURED | PF_GOURAUD;
		clipPoly.vtx[0] = *v0;
		clipPoly.vtx[1] = *v1;
		clipPoly.vtx[2] = *v2;
		clipPoly.uv[0] = *uv0;
		clipPoly.uv[1] = *uv1;
		clipPoly.uv[2] = *uv2;
		clipPoly.lightLevel[0] = f32(l0);
		clipPoly.lightLevel[1] = f32(l1);
		clipPoly.lightLevel[2] = f32(l2);
		drawPolygon(&clipPoly, 31, texture, 0);
	}

	void drawTexturedGouraudQuad(const Vec3f* v0, const Vec3f* v1, const Vec3f* v2, const Vec3f* v3, const Vec2f* uv0, const Vec2f* uv1, const Vec2f* uv2, const Vec2f* uv3, s32 l0, s32 l1, s32 l2, s32 l3, const TextureFrame* texture)
	{
		ClipPolygon clipPoly;
		clipPoly.count = 4;
		clipPoly.flags = PF_TEXTURED | PF_GOURAUD;
		clipPoly.vtx[0] = *v0;
		clipPoly.vtx[1] = *v1;
		clipPoly.vtx[2] = *v2;
		clipPoly.vtx[3] = *v3;
		clipPoly.uv[0] = *uv0;
		clipPoly.uv[1] = *uv1;
		clipPoly.uv[2] = *uv2;
		clipPoly.uv[3] = *uv3;
		clipPoly.lightLevel[0] = f32(l0);
		clipPoly.lightLevel[1] = f32(l1);
		clipPoly.lightLevel[2] = f32(l2);
		clipPoly.lightLevel[3] = f32(l3);
		drawPolygon(&clipPoly, 31, texture, 0);
	}

	void draw3dPoint(const Vec3f* p, u8 color)
	{
		if (p->z < 1.0f) { return; }

		const f32 rz = 1.0f / p->z;
		const s32 px = s32(s_halfWidth + p->x * s_focalLength * rz);
		const s32 py = s32(s_heightOffset + p->y * s_heightScale * rz);

		// Clip to the window
		if (px < s_XClip[0] || px > s_XClip[1]) { return; }
		if (s_YClipUpper && (py < s_YClipUpper[px-s_XClip[0]] || py > s_YClipLower[px-s_XClip[0]])) { return; }

		s32 scale = std::max((s_height / POINT_SCALE), 1);
		s_renderer->drawColoredQuad(px, py, scale, scale, color);
	}
		
	// Sort from back to front.
	s32 sortPolygons(const void* a, const void* b)
	{
		const PolySort* poly0 = (PolySort*)a;
		const PolySort* poly1 = (PolySort*)b;

		if (poly0->sortDepth > poly1->sortDepth) { return -1; }
		else if (poly0->sortDepth < poly1->sortDepth) { return 1; }
		return 0;
	}
}
