#include <TFE_System/profiler.h>
#include <TFE_Jedi/Math/fixedPoint.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Renderer/jediRenderer.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderBackend/vertexBuffer.h>
#include <TFE_RenderShared/lineDraw2d.h>

#include "screenDrawGPU.h"


namespace TFE_Jedi
{
	static bool s_initialized = false;

	void screenGPU_init()
	{
		if (!s_initialized)
		{
			TFE_RenderShared::init();
		}
		s_initialized = true;
	}

	void screenGPU_destroy()
	{
		if (s_initialized)
		{
			TFE_RenderShared::destroy();
		}
		s_initialized = false;
	}
		
	void screenGPU_beginLines(u32 width, u32 height)
	{
		TFE_RenderShared::lineDraw2d_begin(width, height);
	}

	void screenGPU_endLines()
	{
		TFE_RenderShared::lineDraw2d_drawLines();
	}

	void screenGPU_drawPoint(ScreenRect* rect, s32 x, s32 z, u8 color)
	{
		u32 color32 = vfb_getPalette()[color];
		u32 colors[] = { color32, color32 };
		Vec2f vtx[] = { f32(x), f32(z), f32(x), f32(z) };

		TFE_RenderShared::lineDraw2d_addLine(1.5f, vtx, colors);
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
	}

	void screenGPU_blitTextureLit(TextureData* texture, DrawRect* rect, s32 x0, s32 y0, u8 lightLevel, JBool forceTransparency)
	{
	}

	// Scaled versions.
	void screenGPU_blitTextureScaled(TextureData* texture, DrawRect* rect, s32 x0, s32 y0, fixed16_16 xScale, fixed16_16 yScale, JBool forceTransparency)
	{
	}

	void screenGPU_blitTextureLitScaled(TextureData* texture, DrawRect* rect, s32 x0, s32 y0, fixed16_16 xScale, fixed16_16 yScale, u8 lightLevel, JBool forceTransparency)
	{
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