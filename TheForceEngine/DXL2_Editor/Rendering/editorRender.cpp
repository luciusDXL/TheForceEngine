#include "grid2d.h"
#include <DXL2_System/system.h>
#include <DXL2_System/math.h>
#include <DXL2_RenderBackend/renderBackend.h>
#include <DXL2_Asset/modelAsset.h>
#include <DXL2_Asset/textureAsset.h>
#include <DXL2_Editor/levelEditorData.h>
//Rendering 2d
#include <DXL2_Editor/Rendering/lineDraw2d.h>
#include <DXL2_Editor/Rendering/grid2d.h>
#include <DXL2_Editor/Rendering/trianglesColor2d.h>
#include <DXL2_Editor/Rendering/trianglesTextured2d.h>
//Rendering 3d
#include <DXL2_Editor/Rendering/grid3d.h>
#include <DXL2_Editor/Rendering/lineDraw3d.h>
#include <DXL2_Editor/Rendering/trianglesColor3d.h>

#include <algorithm>

namespace DXL2_EditorRender
{
	static TextureGpu* s_filterMap = nullptr;

	bool init()
	{
		s_filterMap = DXL2_RenderBackend::createFilterTexture();

		// 2D
		LineDraw2d::init();
		Grid2d::init();
		TriColoredDraw2d::init();
		TriTexturedDraw2d::init();
		// 3D
		Grid3d::init();
		LineDraw3d::init();
		TrianglesColor3d::init();

		return s_filterMap != nullptr;
	}

	void destroy()
	{
		// delete s_filterMap;
		// 2D
		LineDraw2d::destroy();
		Grid2d::destroy();
		TriColoredDraw2d::destroy();
		TriTexturedDraw2d::destroy();
		// 3D
		Grid3d::destroy();
		LineDraw3d::destroy();
		TrianglesColor3d::destroy();
	}

	static std::vector<Vec3f> s_vertexBuffer3d;

	void drawModel3d_Bounds(const Model* model, const Vec3f* pos, const Mat3* orientation, f32 width, u32 color)
	{
		Vec3f oobb[8];
		const Vec3f  aabb[8]=
		{
			{model->localAabb[0].x, model->localAabb[0].y, model->localAabb[0].z},
			{model->localAabb[1].x, model->localAabb[0].y, model->localAabb[0].z},
			{model->localAabb[1].x, model->localAabb[0].y, model->localAabb[1].z},
			{model->localAabb[0].x, model->localAabb[0].y, model->localAabb[1].z},

			{model->localAabb[0].x, model->localAabb[1].y, model->localAabb[0].z},
			{model->localAabb[1].x, model->localAabb[1].y, model->localAabb[0].z},
			{model->localAabb[1].x, model->localAabb[1].y, model->localAabb[1].z},
			{model->localAabb[0].x, model->localAabb[1].y, model->localAabb[1].z},
		};

		const Vec3f* mat33 = orientation->m;

		for (u32 v = 0; v < 8; v++)
		{
			oobb[v].x = aabb[v].x*mat33[0].x + aabb[v].y*mat33[0].y + aabb[v].z*mat33[0].z + pos->x;
			oobb[v].y = aabb[v].x*mat33[1].x + aabb[v].y*mat33[1].y + aabb[v].z*mat33[1].z + pos->y;
			oobb[v].z = aabb[v].x*mat33[2].x + aabb[v].y*mat33[2].y + aabb[v].z*mat33[2].z + pos->z;
		}

		for (u32 i = 0; i < 8; i++)
		{
			u32 i0 = (i & 3), i1 = (i + 1) & 3;
			if (i >= 4) { i0 += 4; i1 += 4; }

			const Vec3f line[] = { oobb[i0], oobb[i1] };
			LineDraw3d::addLine(width, line, &color);
		}
		for (u32 i = 0; i < 4; i++)
		{
			const u32 i0 = i, i1 = i + 4;
			const Vec3f line[] = { oobb[i0], oobb[i1] };
			LineDraw3d::addLine(width, line, &color);
		}
	}

	void drawModel3d(const EditorSector* sector, const Model* model, const Vec3f* pos, const Mat3* orientation, const u32* pal, u32 alpha)
	{
		const Vec3f* mat33 = orientation->m;

		const ModelObject* obj = model->objects.data();
		for (u32 i = 0; i < model->objectCount; i++, obj++)
		{
			// Transform vertices.
			const u32 vcount = (u32)obj->vertices.size();
			const Vertex3f* vtx = obj->vertices.data();
			const Vertex2f* texVtx = obj->textureVertices.data();

			const s32 textureIndex = obj->textureIndex;
			const Texture* texture = (textureIndex >= 0) ? model->textures[textureIndex] : nullptr;
			const EditorTexture* tex = LevelEditorData::createTexture(texture);

			s_vertexBuffer3d.resize(vcount);
			Vec3f* dstVtx = s_vertexBuffer3d.data();
			for (u32 v = 0; v < vcount; v++, vtx++, dstVtx++)
			{
				dstVtx->x = vtx->x*mat33[0].x + vtx->y*mat33[0].y + vtx->z*mat33[0].z + pos->x;
				dstVtx->y = vtx->x*mat33[1].x + vtx->y*mat33[1].y + vtx->z*mat33[1].z + pos->y;
				dstVtx->z = vtx->x*mat33[2].x + vtx->y*mat33[2].y + vtx->z*mat33[2].z + pos->z;
			}

			// Draw triangles.
			const u32 triCount = (u32)obj->triangles.size();
			const ModelPolygon* tri = obj->triangles.data();
			const TexturePolygon* texTri = obj->textureTriangles.data();
			dstVtx = s_vertexBuffer3d.data();
			for (u32 t = 0; t < triCount; t++, tri++)
			{
				const Vec3f triVtx[3] = { dstVtx[tri->i0], dstVtx[tri->i1], dstVtx[tri->i2] };
				const u32 clr32 = (pal[tri->color] & 0x00ffffff) | alpha;
				if (tri->shading == SHADING_PLANE && tex)
				{
					Vec2f triUv[3];
					Vec2f triUv1[3];

					const f32 floorOffset[] = { sector->floorTexture.offsetX, sector->floorTexture.offsetY };
					triUv[0] = { triVtx[0].x, triVtx[0].z };
					triUv[1] = { triVtx[1].x, triVtx[1].z };
					triUv[2] = { triVtx[2].x, triVtx[2].z };

					triUv1[0] = { triVtx[0].x * 0.125f + floorOffset[0], triVtx[0].z * 0.125f + floorOffset[1] };
					triUv1[1] = { triVtx[1].x * 0.125f + floorOffset[0], triVtx[1].z * 0.125f + floorOffset[1] };
					triUv1[2] = { triVtx[2].x * 0.125f + floorOffset[0], triVtx[2].z * 0.125f + floorOffset[1] };

					TrianglesColor3d::addTexturedTriangle(triVtx, triUv, triUv1, 0x00ffffff | alpha, tex->texture, TrianglesColor3d::TRANS_BLEND);
				}
				else if (tri->shading >= SHADING_TEXTURE && tex)
				{
					const f32 dx = std::max(fabsf(triVtx[1].x - triVtx[0].x), fabsf(triVtx[2].x - triVtx[0].x));
					const f32 dy = std::max(fabsf(triVtx[1].y - triVtx[0].y), fabsf(triVtx[2].y - triVtx[0].y));
					const f32 dz = std::max(fabsf(triVtx[1].z - triVtx[0].z), fabsf(triVtx[2].z - triVtx[0].z));

					Vec2f triUv[3];
					if (dy < dx && dy < dz)
					{
						for (u32 v = 0; v < 3; v++) { triUv[v] = { triVtx[v].x, triVtx[v].z }; }
					}
					else if (dx < dy && dx < dz)
					{
						for (u32 v = 0; v < 3; v++) { triUv[v] = { triVtx[v].y, triVtx[v].z }; }
					}
					else
					{
						for (u32 v = 0; v < 3; v++) { triUv[v] = { triVtx[v].x, triVtx[v].y }; }
					}

					Vec2f triUv1[3] = { { texVtx[texTri->i0].x, -texVtx[texTri->i0].y }, { texVtx[texTri->i1].x, -texVtx[texTri->i1].y }, { texVtx[texTri->i2].x, -texVtx[texTri->i2].y } };
					TrianglesColor3d::addTexturedTriangle(triVtx, triUv, triUv1, 0x00ffffff | alpha, tex->texture, TrianglesColor3d::TRANS_BLEND);
				}
				else
				{
					TrianglesColor3d::addTriangle(triVtx, nullptr, clr32, true);
				}

				if (texTri) { texTri++; }
			}

			const u32 quadCount = (u32)obj->quads.size();
			const ModelPolygon* quad = obj->quads.data();
			const TexturePolygon* qtex = obj->textureQuads.data();
			for (u32 q = 0; q < quadCount; q++, quad++)
			{
				const Vec3f triVtx[6] = { dstVtx[quad->i0], dstVtx[quad->i1], dstVtx[quad->i2], dstVtx[quad->i0], dstVtx[quad->i2], dstVtx[quad->i3] };
				const u32 clr32 = (pal[quad->color] & 0x00ffffff) | alpha;
				const u32 color[] = { clr32, clr32 };
				const u32 texColor[] = { 0x00ffffff | alpha, 0x00ffffff | alpha };
				if (quad->shading == SHADING_PLANE && tex)
				{
					Vec2f triUv[6];
					Vec2f triUv1[6];
					for (u32 v = 0; v < 6; v++) { triUv[v] = { triVtx[v].x, triVtx[v].z }; }

					const f32 floorOffset[] = { sector->floorTexture.offsetX, sector->floorTexture.offsetY };
					triUv1[0] = { triVtx[0].x * 0.125f + floorOffset[0], triVtx[0].z * 0.125f + floorOffset[1] };
					triUv1[1] = { triVtx[1].x * 0.125f + floorOffset[0], triVtx[1].z * 0.125f + floorOffset[1] };
					triUv1[2] = { triVtx[2].x * 0.125f + floorOffset[0], triVtx[2].z * 0.125f + floorOffset[1] };

					triUv1[3] = { triVtx[3].x * 0.125f + floorOffset[0], triVtx[3].z * 0.125f + floorOffset[1] };
					triUv1[4] = { triVtx[4].x * 0.125f + floorOffset[0], triVtx[4].z * 0.125f + floorOffset[1] };
					triUv1[5] = { triVtx[5].x * 0.125f + floorOffset[0], triVtx[5].z * 0.125f + floorOffset[1] };

					TrianglesColor3d::addTexturedTriangles(2, triVtx, triUv, triUv1, texColor, tex->texture, TrianglesColor3d::TRANS_BLEND);
				}
				else if (quad->shading >= SHADING_TEXTURE && tex)
				{
					const f32 dx = std::max(fabsf(triVtx[1].x - triVtx[0].x), fabsf(triVtx[2].x - triVtx[0].x));
					const f32 dy = std::max(fabsf(triVtx[1].y - triVtx[0].y), fabsf(triVtx[2].y - triVtx[0].y));
					const f32 dz = std::max(fabsf(triVtx[1].z - triVtx[0].z), fabsf(triVtx[2].z - triVtx[0].z));

					// Grid
					Vec2f triUv[6];
					if (dy < dx && dy < dz)
					{
						for (u32 v = 0; v < 6; v++) { triUv[v] = { triVtx[v].x, triVtx[v].z }; }
					}
					else if (dx < dy && dx < dz)
					{
						for (u32 v = 0; v < 6; v++) { triUv[v] = { triVtx[v].y, triVtx[v].z }; }
					}
					else
					{
						for (u32 v = 0; v < 6; v++) { triUv[v] = { triVtx[v].x, triVtx[v].y }; }
					}

					// Texture
					Vec2f triUv1[6] = { { texVtx[qtex->i0].x, 1.0f-texVtx[qtex->i0].y }, { texVtx[qtex->i1].x, 1.0f-texVtx[qtex->i1].y }, { texVtx[qtex->i2].x, 1.0f-texVtx[qtex->i2].y },
									    { texVtx[qtex->i0].x, 1.0f-texVtx[qtex->i0].y }, { texVtx[qtex->i2].x, 1.0f-texVtx[qtex->i2].y }, { texVtx[qtex->i3].x, 1.0f-texVtx[qtex->i3].y } };

					TrianglesColor3d::addTexturedTriangles(2, triVtx, triUv, triUv1, texColor, tex->texture, TrianglesColor3d::TRANS_BLEND);
				}
				else
				{
					TrianglesColor3d::addTriangles(2, triVtx, nullptr, color, true);
				}

				if (qtex) { qtex++; }
			}
		}
	}

	static std::vector<Vec2f> s_vertexBuffer;

	void drawModel2d_Bounds(const Model* model, const Vec3f* pos, const Mat3* orientation, u32 color, bool highlight)
	{
		Vec2f oobb[4];
		const Vec3f aabb[4] =
		{
			{model->localAabb[0].x, model->localAabb[0].y, model->localAabb[0].z},
			{model->localAabb[1].x, model->localAabb[0].y, model->localAabb[0].z},
			{model->localAabb[1].x, model->localAabb[0].y, model->localAabb[1].z},
			{model->localAabb[0].x, model->localAabb[0].y, model->localAabb[1].z},
		};

		f32 scale = highlight ? 1.5f : 1.0f;
		const Vec3f* mat33 = orientation->m;
		for (u32 v = 0; v < 4; v++)
		{
			oobb[v].x = (aabb[v].x*mat33[0].x + aabb[v].y*mat33[0].y + aabb[v].z*mat33[0].z)*scale + pos->x;
			oobb[v].z = (aabb[v].x*mat33[2].x + aabb[v].y*mat33[2].y + aabb[v].z*mat33[2].z)*scale + pos->z;
		}

		const Vec2f bounds2d[]=
		{
			oobb[0], oobb[1], oobb[2],
			oobb[0], oobb[2], oobb[3]
		};

		u32 colorBg[] = { color , color };
		TriColoredDraw2d::addTriangles(2, bounds2d, colorBg);
	}

	void drawModel2d(const EditorSector* sector, const Model* model, const Vec2f* pos, const Mat3* orientation, const u32* pal, u32 alpha)
	{
		const Vec3f* mat33 = orientation->m;

		const ModelObject* obj = model->objects.data();
		for (u32 i = 0; i < model->objectCount; i++, obj++)
		{
			// Transform vertices.
			const u32 vcount = (u32)obj->vertices.size();
			const Vertex3f* vtx = obj->vertices.data();
			const Vertex2f* texVtx = obj->textureVertices.data();

			const s32 textureIndex = obj->textureIndex;
			const Texture* texture = (textureIndex >= 0) ? model->textures[textureIndex] : nullptr;
			const EditorTexture* tex = LevelEditorData::createTexture(texture);

			s_vertexBuffer.resize(vcount);
			Vec2f* dstVtx = s_vertexBuffer.data();
			for (u32 v = 0; v < vcount; v++, vtx++, dstVtx++)
			{
				dstVtx->x = vtx->x*mat33[0].x + vtx->y*mat33[0].y + vtx->z*mat33[0].z + pos->x;
				dstVtx->z = vtx->x*mat33[2].x + vtx->y*mat33[2].y + vtx->z*mat33[2].z + pos->z;
			}

			// Draw triangles.
			const u32 triCount = (u32)obj->triangles.size();
			const ModelPolygon* tri = obj->triangles.data();
			const TexturePolygon* texTri = obj->textureTriangles.data();
			dstVtx = s_vertexBuffer.data();
			for (u32 t = 0; t < triCount; t++, tri++)
			{
				const Vec2f triVtx[3] = { dstVtx[tri->i0], dstVtx[tri->i1], dstVtx[tri->i2] };
				Vec2f u = { triVtx[1].x - triVtx[0].x, triVtx[1].z - triVtx[0].z };
				Vec2f v = { triVtx[2].x - triVtx[0].x, triVtx[2].z - triVtx[0].z };
				if (u.z*v.x - u.x*v.z < 0.0f) { continue; }

				const u32 clr32 = (pal[tri->color] & 0x00ffffff) | alpha;
				if (tri->shading == SHADING_PLANE && tex)
				{
					Vec2f triUv[3];
					const f32 floorOffset[] = { sector->floorTexture.offsetX, sector->floorTexture.offsetY };
					triUv[0] = { triVtx[0].x * 0.125f + floorOffset[0], triVtx[0].z * 0.125f + floorOffset[1] };
					triUv[1] = { triVtx[1].x * 0.125f + floorOffset[0], triVtx[1].z * 0.125f + floorOffset[1] };
					triUv[2] = { triVtx[2].x * 0.125f + floorOffset[0], triVtx[2].z * 0.125f + floorOffset[1] };

					TriTexturedDraw2d::addTriangle(triVtx, triUv, 0x00ffffff | alpha, tex->texture);
				}
				else if (tri->shading >= SHADING_TEXTURE && tex)
				{
					Vec2f triUv[3] = { { texVtx[texTri->i0].x, 1.0f-texVtx[texTri->i0].y }, { texVtx[texTri->i1].x, 1.0f-texVtx[texTri->i1].y }, { texVtx[texTri->i2].x, 1.0f-texVtx[texTri->i2].y } };
					TriTexturedDraw2d::addTriangle(triVtx, triUv, 0x00ffffff | alpha, tex->texture);
				}
				else
				{
					TriColoredDraw2d::addTriangle(triVtx, clr32);
				}
				if (texTri) { texTri++; }
			}

			const u32 quadCount = (u32)obj->quads.size();
			const ModelPolygon* quad = obj->quads.data();
			const TexturePolygon* qtex = obj->textureQuads.data();
			for (u32 q = 0; q < quadCount; q++, quad++)
			{
				const Vec2f triVtx[6] = { dstVtx[quad->i0], dstVtx[quad->i1], dstVtx[quad->i2], dstVtx[quad->i0], dstVtx[quad->i2], dstVtx[quad->i3] };
				Vec2f u = { triVtx[1].x - triVtx[0].x, triVtx[1].z - triVtx[0].z };
				Vec2f v = { triVtx[2].x - triVtx[0].x, triVtx[2].z - triVtx[0].z };
				if (u.z*v.x - u.x*v.z < 0.0f) { continue; }

				const u32 clr32 = (pal[quad->color] & 0x00ffffff) | alpha;
				const u32 color[] = { clr32, clr32 };
				const u32 texColor[] = { 0x00ffffff | alpha, 0x00ffffff | alpha };
				if (quad->shading == SHADING_PLANE && tex)
				{
					Vec2f triUv[6];
					const f32 floorOffset[] = { sector->floorTexture.offsetX, sector->floorTexture.offsetY };
					triUv[0] = { triVtx[0].x * 0.125f + floorOffset[0], triVtx[0].z * 0.125f + floorOffset[1] };
					triUv[1] = { triVtx[1].x * 0.125f + floorOffset[0], triVtx[1].z * 0.125f + floorOffset[1] };
					triUv[2] = { triVtx[2].x * 0.125f + floorOffset[0], triVtx[2].z * 0.125f + floorOffset[1] };

					triUv[3] = { triVtx[3].x * 0.125f + floorOffset[0], triVtx[3].z * 0.125f + floorOffset[1] };
					triUv[4] = { triVtx[4].x * 0.125f + floorOffset[0], triVtx[4].z * 0.125f + floorOffset[1] };
					triUv[5] = { triVtx[5].x * 0.125f + floorOffset[0], triVtx[5].z * 0.125f + floorOffset[1] };

					TriTexturedDraw2d::addTriangles(2, triVtx, triUv, texColor, tex->texture);
				}
				else if (quad->shading >= SHADING_TEXTURE && tex)
				{
					Vec2f triUv[6] = { { texVtx[qtex->i0].x, 1.0f-texVtx[qtex->i0].y }, { texVtx[qtex->i1].x, 1.0f-texVtx[qtex->i1].y }, { texVtx[qtex->i2].x, 1.0f-texVtx[qtex->i2].y },
					                   { texVtx[qtex->i0].x, 1.0f-texVtx[qtex->i0].y }, { texVtx[qtex->i2].x, 1.0f-texVtx[qtex->i2].y }, { texVtx[qtex->i3].x, 1.0f-texVtx[qtex->i3].y } };
					TriTexturedDraw2d::addTriangles(2, triVtx, triUv, texColor, tex->texture);
				}
				else
				{
					TriColoredDraw2d::addTriangles(2, triVtx, color);
				}
				if (qtex) { qtex++; }
			}
		}
	}

	TextureGpu* getFilterMap()
	{
		return s_filterMap;
	}
}