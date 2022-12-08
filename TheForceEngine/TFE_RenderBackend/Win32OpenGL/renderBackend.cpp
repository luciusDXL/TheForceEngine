#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderBackend/dynamicTexture.h>
#include <TFE_RenderBackend/textureGpu.h>
#include <TFE_RenderBackend/Win32OpenGL/openGL_Caps.h>
#include <TFE_Settings/settings.h>
#include <TFE_Ui/ui.h>
#include <TFE_Asset/imageAsset.h>	// For image saving, this should be refactored...
#include <TFE_System/profiler.h>
#include <TFE_PostProcess/blit.h>
#include <TFE_PostProcess/postprocess.h>
#include "renderTarget.h"
#include "screenCapture.h"
#include <SDL.h>
#include <GL/glew.h>
#include <stdio.h>
#include <assert.h>
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#undef min
#undef max

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "sdl2.lib")
#endif

namespace TFE_RenderBackend
{
	static const f32 c_tallScreenThreshold = 1.32f;	// 4:3 + epsilon.

	// Screenshot stuff... needs to be refactored.
	static char s_screenshotPath[TFE_MAX_PATH];
	static bool s_screenshotQueued = false;

	static WindowState m_windowState;
	static void* m_window;
	static DynamicTexture* s_virtualDisplay = nullptr;
	static DynamicTexture* s_palette = nullptr;
	static TextureGpu* s_virtualRenderTexture = nullptr;
	static RenderTarget* s_virtualRenderTarget = nullptr;
	static ScreenCapture*  s_screenCapture = nullptr;

	static RenderTarget* s_copyTarget = nullptr;

	static u32 s_virtualWidth, s_virtualHeight;
	static u32 s_virtualWidthUi;
	static u32 s_virtualWidth3d;

	static bool s_widescreen = false;
	static bool s_asyncFrameBuffer = true;
	static bool s_gpuColorConvert = false;
	static bool s_useRenderTarget = false;
	static DisplayMode s_displayMode;
	static f32 s_clearColor[4] = { 0.0f };
	static u32 s_rtWidth, s_rtHeight;

	static Blit* s_postEffectBlit;
	static std::vector<SDL_Rect> s_displayBounds;

	void drawVirtualDisplay();
	void setupPostEffectChain(bool useDynamicTexture);
		
	SDL_Window* createWindow(const WindowState& state)
	{
		u32 windowFlags = SDL_WINDOW_OPENGL;
		bool windowed = !(state.flags & WINFLAG_FULLSCREEN);

		TFE_Settings_Window* windowSettings = TFE_Settings::getWindowSettings();
		
		s32 x = windowSettings->x, y = windowSettings->y;
		s32 displayIndex = getDisplayIndex(x, y);
		assert(displayIndex >= 0);
		
		if (windowed)
		{
			y = std::max(32, y);
			windowSettings->y = y;
			windowFlags |= SDL_WINDOW_RESIZABLE;
		}
		else
		{
			MonitorInfo monitorInfo;
			getDisplayMonitorInfo(displayIndex, &monitorInfo);

			x = monitorInfo.x;
			y = monitorInfo.y;
			windowFlags |= SDL_WINDOW_BORDERLESS;
		}

		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, true);
		SDL_Window* window = SDL_CreateWindow(state.name, x, y, state.width, state.height, windowFlags);
		SDL_GLContext context = SDL_GL_CreateContext(window);

		//swap buffer at the monitors rate
		SDL_GL_SetSwapInterval((state.flags & WINFLAG_VSYNC) ? 1 : 0);

		//GLEW is an OpenGL Loading Library used to reach GL functions
		//Sets all functions available
		glewExperimental = GL_TRUE;
		const GLenum err = glewInit();
		if (err != GLEW_OK)
		{
			printf("Failed to initialize GLEW");
			return nullptr;
		}
		OpenGL_Caps::queryCapabilities();
		TFE_System::logWrite(LOG_MSG, "RenderBackend", "OpenGL Device Tier: %d", OpenGL_Caps::getDeviceTier());

		MonitorInfo monitorInfo;
		getDisplayMonitorInfo(displayIndex, &monitorInfo);
		// High resolution displays (> 1080p) tend to be very high density, so increase the scale somewhat.
		s32 uiScale = 100;
		if (monitorInfo.h >= 2160) // 4k+
		{
			uiScale = 125 * monitorInfo.h / 1080;	// scale based on 1080p being the base.
		}
		else if (monitorInfo.h >= 1440) // 1440p
		{
			uiScale = 150;
		}

		TFE_Ui::init(window, context, uiScale);
		return window;
	}
		
	bool init(const WindowState& state)
	{
		m_window = createWindow(state);
		m_windowState = state;

		if (!TFE_PostProcess::init())
		{
			return false;
		}
		// TODO: Move effect creation into post effect system.
		s_postEffectBlit = new Blit();
		s_postEffectBlit->init();
		s_postEffectBlit->enableFeatures(BLIT_GPU_COLOR_CONVERSION);
		
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClearDepth(0.0f);

		s_palette = new DynamicTexture();
		s_palette->create(256, 1, 2);

		s_screenCapture = new ScreenCapture();
		s_screenCapture->create(m_windowState.width, m_windowState.height, 4);

		TFE_RenderState::clear();

		return m_window != nullptr;
	}

	void destroy()
	{
		delete s_screenCapture;

		// TODO: Move effect destruction into post effect system.
		s_postEffectBlit->destroy();
		delete s_postEffectBlit;

		TFE_PostProcess::destroy();
		TFE_Ui::shutdown();

		delete s_virtualDisplay;
		delete s_virtualRenderTarget;
		SDL_DestroyWindow((SDL_Window*)m_window);

		s_virtualDisplay = nullptr;
		s_virtualRenderTarget = nullptr;
		m_window = nullptr;
	}

	bool getVsyncEnabled()
	{
		return SDL_GL_GetSwapInterval() > 0;
	}

	void enableVsync(bool enable)
	{
		SDL_GL_SetSwapInterval(enable ? 1 : 0);
	}

	void setClearColor(const f32* color)
	{
		glClearColor(color[0], color[1], color[2], color[3]);
		glClearDepth(0.0f);

		memcpy(s_clearColor, color, sizeof(f32) * 4);
	}
		
	void swap(bool blitVirtualDisplay)
	{
		// Blit the texture or render target to the screen.
		if (blitVirtualDisplay) { drawVirtualDisplay(); }
		else { glClear(GL_COLOR_BUFFER_BIT); }

		TFE_ZONE_BEGIN(systemUi, "System UI");
		// Handle the UI.
		TFE_Ui::render();
		// Reset the state due to UI changes.
		TFE_RenderState::clear();
		TFE_ZONE_END(systemUi);

		TFE_ZONE_BEGIN(swapGpu, "GPU Swap Buffers");
		// Update the window.
		SDL_GL_SwapWindow((SDL_Window*)m_window);
		TFE_ZONE_END(swapGpu);

		if (s_screenshotQueued)
		{
			s_screenshotQueued = false;
			s_screenCapture->captureFrame(s_screenshotPath);
		}
		s_screenCapture->update();
	}

	void captureScreenToMemory(u32* mem)
	{
		s_screenCapture->captureFrontBufferToMemory(mem);
	}

	void queueScreenshot(const char* screenshotPath)
	{
		strcpy(s_screenshotPath, screenshotPath);
		s_screenshotQueued = true;
	}
		
	void startGifRecording(const char* path)
	{
		s_screenCapture->beginRecording(path);
	}

	void stopGifRecording()
	{
		s_screenCapture->endRecording();
	}

	void updateSettings()
	{
		TFE_Settings_Window* windowSettings = TFE_Settings::getWindowSettings();
		if (!(m_windowState.flags & WINFLAG_FULLSCREEN))
		{
			SDL_GetWindowPosition((SDL_Window*)m_window, &windowSettings->x, &windowSettings->y);
		}
	}

	void resize(s32 width, s32 height)
	{
		TFE_Settings_Window* windowSettings = TFE_Settings::getWindowSettings();

		m_windowState.width = width;
		m_windowState.height = height;

		windowSettings->width = width;
		windowSettings->height = height;
		if (!(m_windowState.flags & WINFLAG_FULLSCREEN))
		{
			m_windowState.baseWindowWidth = width;
			m_windowState.baseWindowHeight = height;

			windowSettings->baseWidth = width;
			windowSettings->baseHeight = height;
		}
		glViewport(0, 0, width, height);
		setupPostEffectChain(!s_useRenderTarget);

		s_screenCapture->resize(width, height);
	}

	void enumerateDisplays()
	{
		// Get the displays and their bounds.
		s32 displayCount = SDL_GetNumVideoDisplays();
		s_displayBounds.resize(displayCount);
		for (s32 i = 0; i < displayCount; i++)
		{
			SDL_GetDisplayBounds(i, &s_displayBounds[i]);
		}
	}
		
	s32 getDisplayCount()
	{
		enumerateDisplays();
		return (s32)s_displayBounds.size();
	}

	s32 getDisplayIndex(s32 x, s32 y)
	{
		enumerateDisplays();

		// Then determine which display the window sits in.
		s32 displayIndex = -1;
		for (size_t i = 0; i < s_displayBounds.size(); i++)
		{
			if (x >= s_displayBounds[i].x && x < s_displayBounds[i].x + s_displayBounds[i].w &&
				y >= s_displayBounds[i].y && y < s_displayBounds[i].x + s_displayBounds[i].h)
			{
				displayIndex = s32(i);
				break;
			}
		}

		return displayIndex;
	}
		
	bool getDisplayMonitorInfo(s32 displayIndex, MonitorInfo* monitorInfo)
	{
		enumerateDisplays();
		if (displayIndex >= (s32)s_displayBounds.size())
		{
			return false;
		}

		monitorInfo->x = s_displayBounds[displayIndex].x;
		monitorInfo->y = s_displayBounds[displayIndex].y;
		monitorInfo->w = s_displayBounds[displayIndex].w;
		monitorInfo->h = s_displayBounds[displayIndex].h;
		return true;
	}

	f32 getDisplayRefreshRate()
	{
		s32 x, y;
		SDL_GetWindowPosition((SDL_Window*)m_window, &x, &y);
		s32 displayIndex = getDisplayIndex(x, y);
		if (displayIndex >= 0)
		{
			SDL_DisplayMode mode = { 0 };
			SDL_GetDesktopDisplayMode(displayIndex, &mode);
			return (f32)mode.refresh_rate;
		}
		return 0.0f;
	}

	void getCurrentMonitorInfo(MonitorInfo* monitorInfo)
	{
		TFE_Settings_Window* windowSettings = TFE_Settings::getWindowSettings();

		s32 x = windowSettings->x, y = windowSettings->y;
		s32 displayIndex = getDisplayIndex(x, y);
		assert(displayIndex >= 0);

		getDisplayMonitorInfo(displayIndex, monitorInfo);
	}

	void enableFullscreen(bool enable)
	{
		TFE_Settings_Window* windowSettings = TFE_Settings::getWindowSettings();
		windowSettings->fullscreen = enable;

		if (enable)
		{
			SDL_GetWindowPosition((SDL_Window*)m_window, &windowSettings->x, &windowSettings->y);
			s32 displayIndex = getDisplayIndex(windowSettings->x, windowSettings->y);
			if (displayIndex < 0)
			{
				displayIndex = 0;
				windowSettings->x = s_displayBounds[0].x;
				windowSettings->y = s_displayBounds[0].y + 32;
				windowSettings->baseWidth  = std::min(windowSettings->baseWidth,  (u32)s_displayBounds[0].w);
				windowSettings->baseHeight = std::min(windowSettings->baseHeight, (u32)s_displayBounds[0].h);
			}
			m_windowState.monitorWidth  = s_displayBounds[displayIndex].w;
			m_windowState.monitorHeight = s_displayBounds[displayIndex].h;

			m_windowState.flags |= WINFLAG_FULLSCREEN;

			SDL_RestoreWindow((SDL_Window*)m_window);
			SDL_SetWindowResizable((SDL_Window*)m_window, SDL_FALSE);
			SDL_SetWindowBordered((SDL_Window*)m_window, SDL_FALSE);

			SDL_SetWindowSize((SDL_Window*)m_window, m_windowState.monitorWidth, m_windowState.monitorHeight);
			SDL_SetWindowPosition((SDL_Window*)m_window, s_displayBounds[displayIndex].x, s_displayBounds[displayIndex].y);

			m_windowState.width  = m_windowState.monitorWidth;
			m_windowState.height = m_windowState.monitorHeight;
		}
		else
		{
			m_windowState.flags &= ~WINFLAG_FULLSCREEN;

			SDL_RestoreWindow((SDL_Window*)m_window);
			SDL_SetWindowResizable((SDL_Window*)m_window, SDL_TRUE);
			SDL_SetWindowBordered((SDL_Window*)m_window, SDL_TRUE);

			SDL_SetWindowSize((SDL_Window*)m_window, m_windowState.baseWindowWidth, m_windowState.baseWindowHeight);
			SDL_SetWindowPosition((SDL_Window*)m_window, windowSettings->x, windowSettings->y);

			m_windowState.width  = m_windowState.baseWindowWidth;
			m_windowState.height = m_windowState.baseWindowHeight;
		}

		glViewport(0, 0, m_windowState.width, m_windowState.height);
		setupPostEffectChain(!s_useRenderTarget);
		s_screenCapture->resize(m_windowState.width, m_windowState.height);
	}

	void clearWindow()
	{
		glClear(GL_COLOR_BUFFER_BIT);
	}

	void getDisplayInfo(DisplayInfo* displayInfo)
	{
		assert(displayInfo);

		displayInfo->width = m_windowState.width;
		displayInfo->height = m_windowState.height;
		displayInfo->refreshRate = (m_windowState.flags & WINFLAG_VSYNC) != 0 ? m_windowState.refreshRate : 0.0f;
	}

	// New version of the function.
	bool createVirtualDisplay(const VirtualDisplayInfo& vdispInfo)
	{
		if (s_virtualDisplay)
		{
			delete s_virtualDisplay;
		}
		if (s_virtualRenderTarget)
		{
			delete s_virtualRenderTarget;
		}
		s_virtualDisplay = nullptr;
		s_virtualRenderTarget = nullptr;

		s_virtualWidth = vdispInfo.width;
		s_virtualHeight = vdispInfo.height;
		s_virtualWidthUi = vdispInfo.widthUi;
		s_virtualWidth3d = vdispInfo.width3d;
		s_displayMode = vdispInfo.mode;
		s_widescreen = (vdispInfo.flags & VDISP_WIDESCREEN) != 0;
		s_asyncFrameBuffer = (vdispInfo.flags & VDISP_ASYNC_FRAMEBUFFER) != 0;
		s_gpuColorConvert = (vdispInfo.flags & VDISP_GPU_COLOR_CONVERT) != 0;
		s_useRenderTarget = (vdispInfo.flags & VDISP_RENDER_TARGET) != 0;

		bool result = false;
		if (s_useRenderTarget)
		{
			s_virtualRenderTarget = new RenderTarget();
			s_virtualRenderTexture = new TextureGpu();

			result  = s_virtualRenderTexture->create(s_virtualWidth, s_virtualHeight);
			result &= s_virtualRenderTarget->create(s_virtualRenderTexture, true);

			// The renderer will handle this instead.
			s_postEffectBlit->disableFeatures(BLIT_GPU_COLOR_CONVERSION);
			setupPostEffectChain(false);
		}
		else
		{
			s_virtualDisplay = new DynamicTexture();
			if (s_gpuColorConvert)
			{
				s_postEffectBlit->enableFeatures(BLIT_GPU_COLOR_CONVERSION);
			}
			else
			{
				s_postEffectBlit->disableFeatures(BLIT_GPU_COLOR_CONVERSION);
			}

			setupPostEffectChain(true);
			result = s_virtualDisplay->create(s_virtualWidth, s_virtualHeight, s_asyncFrameBuffer ? 2 : 1, s_gpuColorConvert ? DTEX_R8 : DTEX_RGBA8);
		}
		return result;
	}

	u32 getVirtualDisplayWidth2D()
	{
		return s_virtualWidthUi;
	}

	u32 getVirtualDisplayWidth3D()
	{
		return s_virtualWidth3d;
	}

	u32 getVirtualDisplayHeight()
	{
		return s_virtualHeight;
	}

	u32 getVirtualDisplayOffset2D()
	{
		if (s_virtualWidth <= s_virtualWidthUi) { return 0; }
		return (s_virtualWidth - s_virtualWidthUi) >> 1;
	}

	u32 getVirtualDisplayOffset3D()
	{
		if (s_virtualWidth <= s_virtualWidth3d) { return 0; }
		return (s_virtualWidth - s_virtualWidth3d) >> 1;
	}

	void* getVirtualDisplayGpuPtr()
	{
		return (void*)(iptr)s_virtualDisplay->getTexture()->getHandle();
	}

	bool getWidescreen()
	{
		return s_widescreen;
	}

	bool getFrameBufferAsync()
	{
		return s_asyncFrameBuffer;
	}

	bool getGPUColorConvert()
	{
		return s_gpuColorConvert;
	}

	void updateVirtualDisplay(const void* buffer, size_t size)
	{
		TFE_ZONE("Update Virtual Display");
		if (s_virtualDisplay)
		{
			s_virtualDisplay->update(buffer, size);
		}
	}

	void bindVirtualDisplay()
	{
		if (s_virtualRenderTarget)
		{
			s_virtualRenderTarget->bind();
		}
	}

	void clearVirtualDisplay(f32* color, bool clearColor)
	{
		if (s_virtualDisplay)
		{
			// TODO
		}
		else if (s_virtualRenderTarget)
		{
			s_virtualRenderTarget->clear(color, 1.0f, 0, clearColor);
		}
	}

	void copyToVirtualDisplay(RenderTargetHandle src)
	{
		RenderTarget::copy(s_virtualRenderTarget, (RenderTarget*)src);
	}
		
	void copyBackbufferToRenderTarget(RenderTargetHandle dst)
	{
		s_copyTarget = (RenderTarget*)dst;
	}

	void setPalette(const u32* palette)
	{
		if (palette && getGPUColorConvert())
		{
			TFE_ZONE("Update Palette");
			s_palette->update(palette, 256 * sizeof(u32));
		}
	}

	const TextureGpu* getPaletteTexture()
	{
		return s_palette->getTexture();
	}

	void setColorCorrection(bool enabled, const ColorCorrection* color/* = nullptr*/)
	{
		if (s_postEffectBlit->featureEnabled(BLIT_GPU_COLOR_CORRECTION) != enabled)
		{
			if (enabled) { s_postEffectBlit->enableFeatures(BLIT_GPU_COLOR_CORRECTION); }
			else { s_postEffectBlit->disableFeatures(BLIT_GPU_COLOR_CORRECTION); }

			setupPostEffectChain(!s_useRenderTarget);
		}

		if (color)
		{
			s_postEffectBlit->setColorCorrectionParameters(color);
		}
	}

	void drawVirtualDisplay()
	{
		TFE_ZONE("Draw Virtual Display");
		if (!s_virtualDisplay && !s_virtualRenderTarget) { return; }

		// Only clear if (1) s_virtualDisplay == null or (2) s_displayMode != DMODE_STRETCH
		if (s_displayMode != DMODE_STRETCH)
		{
			glClear(GL_COLOR_BUFFER_BIT);
		}
		TFE_PostProcess::execute();
	}

	// GPU commands
	// core gpu functionality for UI and editor.
	// Render target.
	RenderTargetHandle createRenderTarget(u32 width, u32 height, bool hasDepthBuffer)
	{
		RenderTarget* newTarget = new RenderTarget();
		TextureGpu* texture = new TextureGpu();
		texture->create(width, height);
		newTarget->create(texture, hasDepthBuffer);

		return RenderTargetHandle(newTarget);
	}

	void freeRenderTarget(RenderTargetHandle handle)
	{
		if (!handle) { return; }

		RenderTarget* renderTarget = (RenderTarget*)handle;
		delete renderTarget->getTexture();
		delete renderTarget;
	}

	void bindRenderTarget(RenderTargetHandle handle)
	{
		RenderTarget* renderTarget = (RenderTarget*)handle;
		renderTarget->bind();

		const TextureGpu* texture = renderTarget->getTexture();
		s_rtWidth = texture->getWidth();
		s_rtHeight = texture->getHeight();
	}

	void clearRenderTarget(RenderTargetHandle handle, const f32* clearColor, f32 clearDepth)
	{
		RenderTarget* renderTarget = (RenderTarget*)handle;
		renderTarget->clear(clearColor, clearDepth);
		setClearColor(s_clearColor);
	}

	void clearRenderTargetDepth(RenderTargetHandle handle, f32 clearDepth)
	{
		RenderTarget* renderTarget = (RenderTarget*)handle;
		renderTarget->clearDepth(clearDepth);
	}

	void copyRenderTarget(RenderTargetHandle dst, RenderTargetHandle src)
	{
		if (!dst || !src) { return; }
		RenderTarget::copy((RenderTarget*)dst, (RenderTarget*)src);
	}

	void unbindRenderTarget()
	{
		RenderTarget::unbind();
		glViewport(0, 0, m_windowState.width, m_windowState.height);

		if (s_copyTarget)
		{
			RenderTarget::copyBackbufferToTarget(s_copyTarget);
			s_copyTarget = nullptr;
		}
	}

	const TextureGpu* getRenderTargetTexture(RenderTargetHandle rtHandle)
	{
		RenderTarget* renderTarget = (RenderTarget*)rtHandle;
		return renderTarget->getTexture();
	}

	void getRenderTargetDim(RenderTargetHandle rtHandle, u32* width, u32* height)
	{
		RenderTarget* renderTarget = (RenderTarget*)rtHandle;
		const TextureGpu* texture = renderTarget->getTexture();
		*width = texture->getWidth();
		*height = texture->getHeight();
	}

	TextureGpu* createTexture(u32 width, u32 height, u32 channels)
	{
		TextureGpu* texture = new TextureGpu();
		texture->create(width, height, channels);
		return texture;
	}

	TextureGpu* createTextureArray(u32 width, u32 height, u32 layers, u32 channels)
	{
		TextureGpu* texture = new TextureGpu();
		texture->createArray(width, height, layers, channels);
		return texture;
	}

	// Create a GPU version of a texture, assumes RGBA8 and returns a GPU handle.
	TextureGpu* createTexture(u32 width, u32 height, const u32* data, MagFilter magFilter)
	{
		TextureGpu* texture = new TextureGpu();
		texture->createWithData(width, height, data, magFilter);
		return texture;
	}

	void freeTexture(TextureGpu* texture)
	{
		if (!texture) { return; }
		delete texture;
	}

	void getTextureDim(TextureGpu* texture, u32* width, u32* height)
	{
		*width = texture->getWidth();
		*height = texture->getHeight();
	}

	void* getGpuPtr(const TextureGpu* texture)
	{
		return (void*)(iptr)texture->getHandle();
	}

	void drawIndexedTriangles(u32 triCount, u32 indexStride, u32 indexStart)
	{
		glDrawElements(GL_TRIANGLES, triCount * 3, indexStride == 4 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, (void*)(iptr)(indexStart * indexStride));
	}

	void drawLines(u32 lineCount)
	{
		glDrawArrays(GL_LINES, 0, lineCount * 2);
	}

	// Setup the Post effect chain based on current settings.
	// TODO: Move out of render backend since this should be independent of the backend.
	void setupPostEffectChain(bool useDynamicTexture)
	{
		s32 x = 0, y = 0;
		s32 w = m_windowState.width;
		s32 h = m_windowState.height;
		f32 aspect = f32(w) / f32(h);

		if (s_displayMode == DMODE_ASPECT_CORRECT && aspect > c_tallScreenThreshold && !s_widescreen)
		{
			// Calculate width based on height.
			w = 4 * m_windowState.height / 3;
			h = m_windowState.height;

			// pillarbox
			x = std::max(0, ((s32)m_windowState.width - w) / 2);
		}
		else if (s_displayMode == DMODE_ASPECT_CORRECT && (!s_widescreen || aspect <= c_tallScreenThreshold))
		{
			// Disable widescreen.
			s_widescreen = false;
			TFE_Settings::getGraphicsSettings()->widescreen = false;

			// Calculate height based on width.
			h = 3 * m_windowState.width / 4;
			w = m_windowState.width;

			// letterbox
			y = std::max(0, ((s32)m_windowState.height - h) / 2);
		}
		TFE_PostProcess::clearEffectStack();

		if (useDynamicTexture)
		{
			const PostEffectInput blitInputs[] =
			{
				{ PTYPE_DYNAMIC_TEX, s_virtualDisplay },
				{ PTYPE_DYNAMIC_TEX, s_palette }
			};
			TFE_PostProcess::appendEffect(s_postEffectBlit, TFE_ARRAYSIZE(blitInputs), blitInputs, nullptr, x, y, w, h);
		}
		else
		{
			const PostEffectInput blitInputs[] =
			{
				{ PTYPE_TEXTURE, (void*)s_virtualRenderTarget->getTexture() },
				{ PTYPE_DYNAMIC_TEX, s_palette }
			};
			TFE_PostProcess::appendEffect(s_postEffectBlit, TFE_ARRAYSIZE(blitInputs), blitInputs, nullptr, x, y, w, h);
		}
	}
}  // namespace
