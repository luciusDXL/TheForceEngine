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
#include <SDL.h>
#include <GL/glew.h>
#include <stdio.h>
#include <assert.h>
#include <algorithm>

#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>
#undef min
#undef max

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "sdl2.lib")

namespace TFE_RenderBackend
{
	// Screenshot stuff... needs to be refactored.
	static char s_screenshotPath[TFE_MAX_PATH];
	static bool s_screenshotQueued = false;
	static u32* s_screenshotBuffer = nullptr;

	static WindowState m_windowState;
	static void* m_window;
	static DynamicTexture* s_virtualDisplay = nullptr;
	static TextureGpu* s_palette = nullptr;
	static u32 m_virtualWidth, m_virtualHeight;
	static bool s_asyncFrameBuffer = true;
	static DisplayMode m_displayMode;
	static f32 s_clearColor[4] = { 0.0f };
	static u32 s_rtWidth, s_rtHeight;

	static Blit* s_postEffectBlit;

	void drawVirtualDisplay();
	void setupPostEffectChain();

	SDL_Window* createWindow(const WindowState& state)
	{
		u32 windowFlags = SDL_WINDOW_OPENGL;
		bool windowed = !(state.flags & WINFLAG_FULLSCREEN);

		TFE_Settings_Window* windowSettings = TFE_Settings::getWindowSettings();

		int x = windowSettings->x, y = windowSettings->y;
		if (windowed)
		{
			y = std::max(32, y);
			windowSettings->y = y;
			windowFlags |= SDL_WINDOW_RESIZABLE;
		}
		else
		{
			x = 0;
			y = 0;
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
			return false;
		}
		OpenGL_Caps::queryCapabilities();
		TFE_System::logWrite(LOG_MSG, "RenderBackend", "OpenGL Device Tier: %d", OpenGL_Caps::getDeviceTier());

		TFE_Ui::init(window, context, 100);

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
		
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClearDepth(0.0f);

		TFE_RenderState::clear();

		return m_window != nullptr;
	}

	void destroy()
	{
		// TODO: Move effect destruction into post effect system.
		s_postEffectBlit->destroy();
		delete s_postEffectBlit;

		TFE_PostProcess::destroy();
		TFE_Ui::shutdown();

		delete s_virtualDisplay;
		SDL_DestroyWindow((SDL_Window*)m_window);

		s_virtualDisplay = nullptr;
		m_window = nullptr;
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
			s_screenshotBuffer = (u32*)realloc(s_screenshotBuffer, m_windowState.width * m_windowState.height * 4u);

			glReadBuffer(GL_BACK);
			glReadPixels(0, 0, m_windowState.width, m_windowState.height, GL_RGBA, GL_UNSIGNED_BYTE, s_screenshotBuffer);

			TFE_Image::writeImage(s_screenshotPath, m_windowState.width, m_windowState.height, s_screenshotBuffer);
		}
	}
		
	void queueScreenshot(const char* screenshotPath)
	{
		strcpy(s_screenshotPath, screenshotPath);
		s_screenshotQueued = true;
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
		setupPostEffectChain();
	}

	void enableFullscreen(bool enable)
	{
		TFE_Settings_Window* windowSettings = TFE_Settings::getWindowSettings();
		windowSettings->fullscreen = enable;

		if (enable)
		{
			SDL_GetWindowPosition((SDL_Window*)m_window, &windowSettings->x, &windowSettings->y);

			m_windowState.flags |= WINFLAG_FULLSCREEN;

			SDL_RestoreWindow((SDL_Window*)m_window);
			SDL_SetWindowResizable((SDL_Window*)m_window, SDL_FALSE);
			SDL_SetWindowBordered((SDL_Window*)m_window, SDL_FALSE);

			SDL_SetWindowSize((SDL_Window*)m_window, m_windowState.monitorWidth, m_windowState.monitorHeight);
			SDL_SetWindowPosition((SDL_Window*)m_window, 0, 0);

			m_windowState.width = m_windowState.monitorWidth;
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

			m_windowState.width = m_windowState.baseWindowWidth;
			m_windowState.height = m_windowState.baseWindowHeight;
		}

		glViewport(0, 0, m_windowState.width, m_windowState.height);
		setupPostEffectChain();
	}

	void getDisplayInfo(DisplayInfo* displayInfo)
	{
		assert(displayInfo);

		displayInfo->width = m_windowState.width;
		displayInfo->height = m_windowState.height;
		displayInfo->refreshRate = (m_windowState.flags & WINFLAG_VSYNC) != 0 ? m_windowState.refreshRate : 0.0f;
	}

	// virtual display
	bool createVirtualDisplay(u32 width, u32 height, DisplayMode mode, bool asyncFramebuffer)
	{
		if (s_virtualDisplay)
		{
			delete s_virtualDisplay;
		}

		m_virtualWidth = width;
		m_virtualHeight = height;
		m_displayMode = mode;
		s_asyncFrameBuffer = asyncFramebuffer;

		s_virtualDisplay = new DynamicTexture();
		setupPostEffectChain();

		return s_virtualDisplay->create(width, height, s_asyncFrameBuffer ? 2 : 1);
	}

	void* getVirtualDisplayGpuPtr()
	{
		return (void*)(intptr_t)s_virtualDisplay->getTexture()->getHandle();
	}

	bool getFrameBufferAsync()
	{
		return s_asyncFrameBuffer;
	}

	void updateVirtualDisplay(const void* buffer, size_t size)
	{
		TFE_ZONE("Update Virtual Display");
		s_virtualDisplay->update(buffer, size);
	}

	void drawVirtualDisplay()
	{
		TFE_ZONE("Draw Virtual Display");
		if (!s_virtualDisplay) { return; }

		// Only clear if (1) s_virtualDisplay == null or (2) m_displayMode != DMODE_STRETCH
		if (m_displayMode != DMODE_STRETCH)
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

	void unbindRenderTarget()
	{
		RenderTarget::unbind();
		glViewport(0, 0, m_windowState.width, m_windowState.height);
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

	// Create a GPU version of a texture, assumes RGBA8 and returns a GPU handle.
	TextureGpu* createTexture(u32 width, u32 height, const u32* data, MagFilter magFilter)
	{
		TextureGpu* texture = new TextureGpu();
		texture->createWithData(width, height, data, magFilter);
		return texture;
	}

	void freeTexture(TextureGpu* texture)
	{
		delete texture;
	}

	void getTextureDim(TextureGpu* texture, u32* width, u32* height)
	{
		*width = texture->getWidth();
		*height = texture->getHeight();
	}

	void* getGpuPtr(const TextureGpu* texture)
	{
		return (void*)(intptr_t)texture->getHandle();
	}

	void drawIndexedTriangles(u32 triCount, u32 indexStride, u32 indexStart)
	{
		glDrawElements(GL_TRIANGLES, triCount * 3, indexStride == 4 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, (void*)(intptr_t)(indexStart * indexStride));
	}

	// Setup the Post effect chain based on current settings.
	// TODO: Move out of render backend since this should be independent of the backend.
	void setupPostEffectChain()
	{
		s32 x = 0, y = 0;
		s32 w = m_windowState.width;
		s32 h = m_windowState.height;
		if (m_displayMode == DMODE_4x3 && w >= h)
		{
			w = 4 * m_windowState.height / 3;
			h = m_windowState.height;

			// pillarbox
			x = (m_windowState.width - w) / 2;
		}
		else if (m_displayMode == DMODE_4x3)
		{
			h = 3 * m_windowState.width / 4;
			w = m_windowState.width;

			// letterbox
			y = (m_windowState.height - h) / 2;
		}
		TFE_PostProcess::clearEffectStack();

		const PostEffectInput blitInputs[]=
		{
			{ PTYPE_DYNAMIC_TEX, s_virtualDisplay },
			{ PTYPE_TEXTURE,     s_palette }
		};
		TFE_PostProcess::appendEffect(s_postEffectBlit, TFE_ARRAYSIZE(blitInputs), blitInputs, nullptr, x, y, w, h);
	}
}  // namespace
