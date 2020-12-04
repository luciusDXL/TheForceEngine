#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine GPU Library
// This handles low level access to the GPU device and is used by
// both the software and hardware rendering systems in order to
// provide basic access.
// 
// Renderers will create a virtual display or render target.
// swap() handles blitting this result and then rendering UI
// on top.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include <TFE_RenderBackend/renderState.h>
#include <TFE_RenderBackend/textureGpu.h>

enum WindowFlags
{
	WINFLAG_FULLSCREEN = 1 << 0,
	WINFLAG_VSYNC = 1 << 1,
};

enum DisplayMode
{
	DMODE_STRETCH = 0,		// Stretch the virtual display over the whole window, useful for widescreen.
	DMODE_ASPECT_CORRECT,	// Letter/Pillar box to maintain the correct aspect ratio.
	DMODE_COUNT
};

struct WindowState
{
	char name[256];
	u32 width;
	u32 height;
	u32 baseWindowWidth;
	u32 baseWindowHeight;
	u32 monitorWidth;
	u32 monitorHeight;
	u32 flags;
	f32 refreshRate;
};

struct DisplayInfo
{
	u32 width;
	u32 height;
	f32 refreshRate;	//0.0 if vsync not enabled.
};

enum VirtualDisplayFlags
{
	VDISP_WIDESCREEN		= (1 << 0),		// Widescreen output
	VDISP_ASYNC_FRAMEBUFFER = (1 << 1),		// Asynchronous framebuffer upload, trades latency for performance.
	VDISP_GPU_COLOR_CONVERT = (1 << 2),		// Use Gpu color conversion, reducing upload bandwidth for a small GPU cost.
	VDISP_DEFAULT = (VDISP_ASYNC_FRAMEBUFFER | VDISP_GPU_COLOR_CONVERT)
};

struct VirtualDisplayInfo
{
	DisplayMode mode = DMODE_ASPECT_CORRECT;	// Output display mode.
	u32 flags = VDISP_DEFAULT;					// See VirtualDisplayFlags.

	u32 width;		// full width
	u32 height;		// full height
	u32 widthUi;	// width for 2D game UI and cutscenes.
	u32 width3d;	// width for 3D drawing.

	// offset3d = (width - width3d)>>1
	// offsetUi = (width - widthUi)>>1
	// stride = width
};

struct ColorCorrection
{
	f32 brightness;
	f32 contrast;
	f32 saturation;
	f32 gamma;
};

typedef void* RenderTargetHandle;
class TextureGpu;

namespace TFE_RenderBackend
{
	bool init(const WindowState& state);
	void destroy();

	void setClearColor(const f32* color);
	void swap(bool blitVirtualDisplay);
	void queueScreenshot(const char* screenshotPath);

	void resize(s32 width, s32 height);
	void enableFullscreen(bool enable);

	void getDisplayInfo(DisplayInfo* displayInfo);
	void updateSettings();

	// virtual display
	bool createVirtualDisplay(const VirtualDisplayInfo& vdispInfo);
	void updateVirtualDisplay(const void* buffer, size_t size);
	void setPalette(const u32* palette);
	void setColorCorrection(bool enabled, const ColorCorrection* color = nullptr);
	bool getWidescreen();
	bool getFrameBufferAsync();
	bool getGPUColorConvert();
	void* getVirtualDisplayGpuPtr();

	u32 getVirtualDisplayWidth2D();
	u32 getVirtualDisplayWidth3D();
	u32 getVirtualDisplayHeight();
	u32 getVirtualDisplayOffset2D();
	u32 getVirtualDisplayOffset3D();

	// core gpu functionality for UI and editor.
	// Render target.
	RenderTargetHandle createRenderTarget(u32 width, u32 height, bool hasDepthBuffer = false);
	void freeRenderTarget(RenderTargetHandle handle);
	void bindRenderTarget(RenderTargetHandle handle);
	void clearRenderTarget(RenderTargetHandle handle, const f32* clearColor, f32 clearDepth = 1.0f);
	void clearRenderTargetDepth(RenderTargetHandle handle, f32 clearDepth = 1.0f);
	void unbindRenderTarget();
	const TextureGpu* getRenderTargetTexture(RenderTargetHandle rtHandle);
	void getRenderTargetDim(RenderTargetHandle rtHandle, u32* width, u32* height);
	   
	// Create a GPU version of a texture, assumes RGBA8 and returns a GPU handle.
	TextureGpu* createTexture(u32 width, u32 height, const u32* data, MagFilter magFilter = MAG_FILTER_NONE);
	void freeTexture(TextureGpu* texture);
	void getTextureDim(TextureGpu* texture, u32* width, u32* height);
	void* getGpuPtr(const TextureGpu* texture);

	// Generic triangle draw.
	// triCount : number of triangles to draw.
	// indexStride : index buffer stride in bytes.
	// indexOffset : starting index.
	void drawIndexedTriangles(u32 triCount, u32 indexStride, u32 indexStart = 0u);
};
