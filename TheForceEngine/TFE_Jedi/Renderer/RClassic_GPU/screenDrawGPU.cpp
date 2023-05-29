#include <TFE_System/profiler.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Renderer/jediRenderer.h>
#include <TFE_Jedi/Renderer/RClassic_GPU/lighting.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderBackend/shader.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderBackend/indexBuffer.h>
#include <TFE_RenderShared/lineDraw2d.h>
#include <TFE_RenderShared/quadDraw2d.h>
#include <TFE_RenderShared/texturePacker.h>

#include "screenDrawGPU.h"
#include "rsectorGPU.h"

namespace TFE_Jedi
{
	static bool s_initialized = false;
	extern Vec3f s_cameraPos;

	enum ScreenQuadShader
	{
		SQ_SHADER_UI = 0,
		SQ_SHADER_LIT,
		SQ_SHADER_COUNT
	};

	struct ScreenQuadVertex
	{
		Vec4f posUv;			// position (xy) and texture coordinates (zw)
		u32 textureId_Color;	// packed: rg = textureId or 0xffff; b = color index; a = light level.
	};

	static const AttributeMapping c_quadAttrMapping[] =
	{
		{ATTR_POS,   ATYPE_FLOAT, 4, 0, false},
		{ATTR_COLOR, ATYPE_UINT8, 4, 0, true}
	};
	static const u32 c_quadAttrCount = TFE_ARRAYSIZE(c_quadAttrMapping);

	static VertexBuffer s_scrQuadVb[SQ_SHADER_COUNT];	// Dynamic vertex buffer.
	static IndexBuffer  s_scrQuadIb;	// Static index buffer.
	static Shader       s_scrQuadShader[SQ_SHADER_COUNT];
	static ScreenQuadVertex* s_scrQuads[SQ_SHADER_COUNT] = { nullptr };

	static s32 s_screenQuadCount[SQ_SHADER_COUNT] = { 0 };
	static s32 s_svScaleOffset[SQ_SHADER_COUNT] = { -1, -1 };
	static s32 s_svCameraPos[SQ_SHADER_COUNT] = { -1, -1 };
	static u32 s_scrQuadsWidth;
	static u32 s_scrQuadsHeight;

	enum Constants
	{
		SCR_MAX_QUAD_COUNT = 4096,
	};

	ScreenQuadVertex* allocateQuad(ScreenQuadShader shader);

	bool screenGPU_loadShader(s32 index)
	{
		ShaderDefine defines[16] = {};
		s32 defineCount = 0;
		if (index == SQ_SHADER_LIT)
		{
			defines[defineCount].name = "OPT_DYNAMIC_LIGHTING";
			defines[defineCount].value = "1";
			defineCount++;

			defines[defineCount].name = "OPT_BLOOM";
			defines[defineCount].value = "1";
			defineCount++;
		}

		if (!s_scrQuadShader[index].load("Shaders/gpu_render_quad.vert", "Shaders/gpu_render_quad.frag", defineCount, defines, SHADER_VER_STD))
		{
			return false;
		}

		s_svScaleOffset[index] = s_scrQuadShader[index].getVariableId("ScaleOffset");
		s_svCameraPos[index] = s_scrQuadShader[index].getVariableId("CameraPos");
		if (s_svScaleOffset[index] < 0)
		{
			return false;
		}

		s_scrQuadShader[index].bindTextureNameToSlot("Colormap",     0);
		s_scrQuadShader[index].bindTextureNameToSlot("Palette",      1);
		s_scrQuadShader[index].bindTextureNameToSlot("Textures",     2);
		s_scrQuadShader[index].bindTextureNameToSlot("TextureTable", 3);

		s_scrQuadShader[index].bindTextureNameToSlot("lightPosition",  4);
		s_scrQuadShader[index].bindTextureNameToSlot("lightData",      5);
		s_scrQuadShader[index].bindTextureNameToSlot("lightClusters",  6);
		s_scrQuadShader[index].bindTextureNameToSlot("shadowMaps",     7);

		return true;
	}

	void screenGPU_init()
	{
		if (!s_initialized)
		{
			TFE_RenderShared::init();
			TFE_RenderShared::quadInit();

			// Create the index and vertex buffer for quads.
			for (s32 s = 0; s < SQ_SHADER_COUNT; s++)
			{
				s_scrQuadVb[s].create(SCR_MAX_QUAD_COUNT * 4, sizeof(ScreenQuadVertex), c_quadAttrCount, c_quadAttrMapping, true);
				if (s == 0)
				{
					u16 indices[SCR_MAX_QUAD_COUNT * 6];
					u16* idx = indices;
					for (s32 q = 0, vtx = 0; q < SCR_MAX_QUAD_COUNT; q++, idx += 6, vtx += 4)
					{
						idx[0] = vtx + 0;
						idx[1] = vtx + 1;
						idx[2] = vtx + 2;

						idx[3] = vtx + 0;
						idx[4] = vtx + 2;
						idx[5] = vtx + 3;
					}
					s_scrQuadIb.create(SCR_MAX_QUAD_COUNT * 6, sizeof(u16), false, indices);
				}
				s_scrQuads[s] = (ScreenQuadVertex*)malloc(sizeof(ScreenQuadVertex) * SCR_MAX_QUAD_COUNT * 4);
				s_screenQuadCount[s] = 0;

				// Shaders and variables.
				screenGPU_loadShader(ScreenQuadShader(s));
			}
		}
		s_initialized = true;
	}

	void screenGPU_destroy()
	{
		if (s_initialized)
		{
			TFE_RenderShared::destroy();
			TFE_RenderShared::quadDestroy();
			s_scrQuadIb.destroy();
			for (s32 s = 0; s < SQ_SHADER_COUNT; s++)
			{
				s_scrQuadVb[s].destroy();
				free(s_scrQuads[s]);
				s_scrQuads[s] = nullptr;
			}
		}
		s_initialized = false;
	}

	void screenGPU_setHudTextureCallbacks(s32 count, TextureListCallback* callbacks)
	{
		if (count)
		{
			TexturePacker* texturePacker = texturepacker_getGlobal();
			if (!texturepacker_hasReservedPages(texturePacker))
			{
				for (s32 i = 0; i < count; i++)
				{
					texturepacker_pack(callbacks[i], POOL_GAME);
				}
				texturepacker_commit();
				texturepacker_reserveCommitedPages(texturePacker);
			}
		}
	}
		
	void screenGPU_beginQuads(u32 width, u32 height)
	{
		for (s32 s = 0; s < SQ_SHADER_COUNT; s++)
		{
			s_screenQuadCount[s] = 0;
		}
		s_scrQuadsWidth = width;
		s_scrQuadsHeight = height;
	}

	void screenGPU_endQuads()
	{
		for (s32 s = 0; s < SQ_SHADER_COUNT; s++)
		{
			if (!s_screenQuadCount[s])
			{
				continue;
			}
			s_scrQuadVb[s].update(s_scrQuads[s], sizeof(ScreenQuadVertex) * s_screenQuadCount[s] * 4);
			TFE_RenderState::setStateEnable(false, STATE_CULLING | STATE_DEPTH_TEST | STATE_DEPTH_WRITE | STATE_BLEND);
			
			s_scrQuadShader[s].bind();
			s_scrQuadVb[s].bind();
			s_scrQuadIb.bind();

			// Bind Uniforms & Textures.
			const f32 scaleX = 2.0f / f32(s_scrQuadsWidth);
			const f32 scaleY = 2.0f / f32(s_scrQuadsHeight);
			const f32 offsetX = -1.0f;
			const f32 offsetY = -1.0f;

			const f32 scaleOffset[] = { scaleX, scaleY, offsetX, offsetY };
			s_scrQuadShader[s].setVariable(s_svScaleOffset[s], SVT_VEC4, scaleOffset);
			s_scrQuadShader[s].setVariable(s_svCameraPos[s], SVT_VEC3, s_cameraPos.m);

			const TextureGpu* palette  = TFE_RenderBackend::getPaletteTexture();
			const TextureGpu* colormap = TFE_Sectors_GPU::getColormap();
			colormap->bind(0);
			palette->bind(1);

			TexturePacker* texturePacker = texturepacker_getGlobal();
			texturePacker->texture->bind(2);
			texturePacker->textureTableGPU.bind(3);
			lighting_bind(4);

			TFE_RenderBackend::drawIndexedTriangles(2 * s_screenQuadCount[s], sizeof(u16));

			s_scrQuadVb[s].unbind();
			s_scrQuadShader[s].unbind();
		}
		lighting_unbind(4);
	}
		
	void screenGPU_beginLines(u32 width, u32 height)
	{
		TFE_RenderShared::lineDraw2d_begin(width, height);
	}

	void screenGPU_endLines()
	{
		TFE_RenderShared::lineDraw2d_drawLines();
	}
		
	void screenGPU_beginImageQuads(u32 width, u32 height)
	{
		TFE_RenderShared::quadDraw2d_begin(width, height);
	}

	void screenGPU_endImageQuads()
	{
		TFE_RenderShared::quadDraw2d_draw();
	}

	void screenGPU_addImageQuad(s32 x0, s32 z0, s32 x1, s32 z1, TextureGpu* texture)
	{
		u32 colors[] = { 0xffffffff, 0xffffffff };
		Vec2f vtx[] = { f32(x0), f32(z0), f32(x1), f32(z1) };
		TFE_RenderShared::quadDraw2d_add(vtx, colors, texture);
	}

	void screenGPU_addImageQuad(s32 x0, s32 z0, s32 x1, s32 z1, f32 u0, f32 u1, TextureGpu* texture)
	{
		u32 colors[] = { 0xffffffff, 0xffffffff };
		Vec2f vtx[] = { f32(x0), f32(z0), f32(x1), f32(z1) };
		TFE_RenderShared::quadDraw2d_add(vtx, colors, u0, u1, texture);
	}

	void screenGPU_drawPoint(ScreenRect* rect, s32 x, s32 z, u8 color)
	{
		u32 color32 = vfb_getPalette()[color];
		u32 colors[] = { color32, color32 };

		// Draw a diamond shape
		f32 size = max(f32(s_scrQuadsHeight / 400), 0.5f);
		const Vec2f vtx[]=
		{
			// left
			f32(x-size), f32(z),
			// middle
			f32(x), f32(z+size),
			// right
			f32(x+size), f32(z),
			// bottom
			f32(x), f32(z-size),
			// left
			f32(x-size), f32(z),
		};
		TFE_RenderShared::lineDraw2d_addLine(1.5f, &vtx[0], colors);
		TFE_RenderShared::lineDraw2d_addLine(1.5f, &vtx[1], colors);
		TFE_RenderShared::lineDraw2d_addLine(1.5f, &vtx[2], colors);
		TFE_RenderShared::lineDraw2d_addLine(1.5f, &vtx[3], colors);
	}

	void screenGPU_drawLine(ScreenRect* rect, s32 x0, s32 z0, s32 x1, s32 z1, u8 color)
	{
		u32 color32 = vfb_getPalette()[color];
		u32 colors[] = { color32, color32 };
		Vec2f vtx[] = { f32(x0), f32(z0), f32(x1), f32(z1) };

		TFE_RenderShared::lineDraw2d_addLine(1.5f, vtx, colors);
	}

	void screenGPU_blitTexture(TextureData* texture, DrawRect* rect, s32 x0, s32 y0, JBool forceTransparency, JBool forceOpaque)
	{
		if (s_screenQuadCount[SQ_SHADER_UI] >= SCR_MAX_QUAD_COUNT)
		{
			return;
		}

		u32 textureId_Color = texture->textureId;
		ScreenQuadVertex* quad = allocateQuad(SQ_SHADER_UI);

		f32 fx0 = f32(x0);
		f32 fy0 = f32(y0);
		f32 fx1 = f32(x0 + texture->width);
		f32 fy1 = f32(y0 + texture->height);

		f32 u0 = 0.0f, v0 = 0.0f;
		f32 u1 = f32(texture->width);
		f32 v1 = f32(texture->height);
		quad[0].posUv = { fx0, fy0, u0, v1 };
		quad[1].posUv = { fx1, fy0, u1, v1 };
		quad[2].posUv = { fx1, fy1, u1, v0 };
		quad[3].posUv = { fx0, fy1, u0, v0 };

		quad[0].textureId_Color = textureId_Color;
		quad[1].textureId_Color = textureId_Color;
		quad[2].textureId_Color = textureId_Color;
		quad[3].textureId_Color = textureId_Color;
	}

	void screenGPU_blitTextureLit(TextureData* texture, DrawRect* rect, s32 x0, s32 y0, u8 lightLevel, JBool forceTransparency)
	{
		if (s_screenQuadCount[SQ_SHADER_UI] >= SCR_MAX_QUAD_COUNT)
		{
			return;
		}

		fixed16_16 x1 = x0 + intToFixed16(texture->width);
		fixed16_16 y1 = y0 + intToFixed16(texture->height);
		s32 textureId = texture->textureId;

		u8 color = 0;
		u32 textureId_Color = textureId | (u32(color) << 16u) | (u32(lightLevel) << 24u);

		ScreenQuadVertex* quad = allocateQuad(SQ_SHADER_UI);

		f32 fx0 = fixed16ToFloat(x0);
		f32 fy0 = fixed16ToFloat(y0);
		f32 fx1 = fixed16ToFloat(x1);
		f32 fy1 = fixed16ToFloat(y1);

		f32 u0 = 0.0f, v0 = 0.0f;
		f32 u1 = f32(texture->width);
		f32 v1 = f32(texture->height);
		quad[0].posUv = { fx0, fy0, u0, v1 };
		quad[1].posUv = { fx1, fy0, u1, v1 };
		quad[2].posUv = { fx1, fy1, u1, v0 };
		quad[3].posUv = { fx0, fy1, u0, v0 };

		quad[0].textureId_Color = textureId_Color;
		quad[1].textureId_Color = textureId_Color;
		quad[2].textureId_Color = textureId_Color;
		quad[3].textureId_Color = textureId_Color;
	}

	void screenGPU_drawColoredQuad(fixed16_16 x0, fixed16_16 y0, fixed16_16 x1, fixed16_16 y1, u8 color)
	{
		f32 fx0 = fixed16ToFloat(x0);
		f32 fy0 = fixed16ToFloat(y0);
		f32 fx1 = fixed16ToFloat(x1);
		f32 fy1 = fixed16ToFloat(y1);
		u32 textureId_Color = 0xffffu | (u32(color) << 16u) | (31u << 24u);

		ScreenQuadVertex* quad = allocateQuad(SQ_SHADER_UI);

		quad[0].posUv = { fx0, fy0, 0.0f, 1.0f };
		quad[1].posUv = { fx1, fy0, 1.0f, 1.0f };
		quad[2].posUv = { fx1, fy1, 1.0f, 0.0f };
		quad[3].posUv = { fx0, fy1, 0.0f, 0.0f };

		quad[0].textureId_Color = textureId_Color;
		quad[1].textureId_Color = textureId_Color;
		quad[2].textureId_Color = textureId_Color;
		quad[3].textureId_Color = textureId_Color;
	}

	ScreenQuadVertex* allocateQuad(ScreenQuadShader shader)
	{
		ScreenQuadVertex* quad = &s_scrQuads[shader][s_screenQuadCount[shader] * 4];
		s_screenQuadCount[shader]++;
		return quad;
	}

	// Scaled versions.
	void screenGPU_blitTextureScaled(TextureData* texture, DrawRect* rect, fixed16_16 x0, fixed16_16 y0, fixed16_16 xScale, fixed16_16 yScale, u8 lightLevel, JBool forceTransparency, JBool applyDynamicLights)
	{
		const ScreenQuadShader shader = applyDynamicLights ? SQ_SHADER_LIT : SQ_SHADER_UI;
		if (s_screenQuadCount[shader] >= SCR_MAX_QUAD_COUNT)
		{
			return;
		}

		fixed16_16 x1 = x0 + mul16(intToFixed16(texture->width),  xScale);
		fixed16_16 y1 = y0 + mul16(intToFixed16(texture->height), yScale);
		s32 textureId = texture->textureId;

		u8 color = applyDynamicLights ? 1 : 0;
		u32 textureId_Color = textureId | (u32(color) << 16u) | (u32(lightLevel) << 24u);

		ScreenQuadVertex* quad = allocateQuad(shader);
		f32 fx0 = fixed16ToFloat(x0);
		f32 fy0 = fixed16ToFloat(y0);
		f32 fx1 = fixed16ToFloat(x1);
		f32 fy1 = fixed16ToFloat(y1);

		f32 u0 = 0.0f, v0 = 0.0f;
		f32 u1 = f32(texture->width);
		f32 v1 = f32(texture->height);
		quad[0].posUv = { fx0, fy0, u0, v1 };
		quad[1].posUv = { fx1, fy0, u1, v1 };
		quad[2].posUv = { fx1, fy1, u1, v0 };
		quad[3].posUv = { fx0, fy1, u0, v0 };
								  
		quad[0].textureId_Color = textureId_Color;
		quad[1].textureId_Color = textureId_Color;
		quad[2].textureId_Color = textureId_Color;
		quad[3].textureId_Color = textureId_Color;
	}

	void screenGPU_blitTexture(ScreenImage* texture, DrawRect* rect, s32 x0, s32 y0)
	{
	}

	void screenGPU_blitTextureScaled(ScreenImage* texture, DrawRect* rect, s32 x0, s32 y0, fixed16_16 xScale, fixed16_16 yScale)
	{
	}

	void screenGPU_blitTextureLitScaled(ScreenImage* texture, DrawRect* rect, s32 x0, s32 y0, fixed16_16 xScale, fixed16_16 yScale, u8 lightLevel)
	{
	}

	void screenGPU_blitTextureIScale(TextureData* texture, DrawRect* rect, s32 x0, s32 y0, s32 scale)
	{
	}

}  // TFE_Jedi