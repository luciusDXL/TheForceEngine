#include <TFE_Ui/ui.h>
#include <TFE_Ui/markdown.h>
#include <TFE_Asset/imageAsset.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderBackend/textureGpu.h>
#include <TFE_FileSystem/physfswrapper.h>
#include "imGUI/imgui.h"
#include "imGUI/imgui_markdown.h"

#include <string>
#include <vector>
#include <map>

#ifdef _WIN32
	// Following includes for Windows LinkCallback
	#define WIN32_LEAN_AND_MEAN
	#include <Windows.h>
	#include "Shellapi.h"
#endif

namespace TFE_Markdown
{
	typedef std::map<std::string, TextureGpu*> TextureMap;
	static TextureMap s_textures;
	static ImFont* s_baseFont;
	static ImFontConfig s_fontCfg = {};

	static LocalLinkCallback s_localLinkCb = nullptr;

	void linkCallback(ImGui::MarkdownLinkCallbackData data);
	ImGui::MarkdownImageData imageCallback(ImGui::MarkdownLinkCallbackData data);

	static ImGui::MarkdownConfig s_mdConfig{ linkCallback, imageCallback, nullptr, { { NULL, true }, { NULL, true }, { NULL, false } } };

	bool init(f32 baseFontSize)
	{
		unsigned int fbs1, fbs2;
		char *fb1, *fb2;

		vpFile ttf1(VPATH_TFE, "Fonts/DroidSans.ttf", &fb1, &fbs1);
		vpFile ttf2(VPATH_TFE, "Fonts/DroidSans-Bold.ttf", &fb2, &fbs2);
		if (!ttf1 || !ttf2)
		{
			return false;
		}

		ImGuiIO& io = ImGui::GetIO();
		s_baseFont = io.Fonts->AddFontFromMemoryTTF(fb1, fbs1, baseFontSize);

		s_fontCfg.FontDataOwnedByAtlas = false;	// indicate to ImGui to not free() the fontdata buffer.
		// Bold
		s_mdConfig.boldFont = io.Fonts->AddFontFromMemoryTTF(fb2, fbs2, baseFontSize, &s_fontCfg);
		// Heading H1
		s_mdConfig.headingFormats[0].font = io.Fonts->AddFontFromMemoryTTF(fb2, fbs2, baseFontSize * 2, &s_fontCfg);
		// Heading H2
		s_mdConfig.headingFormats[1].font = io.Fonts->AddFontFromMemoryTTF(fb2, fbs2, baseFontSize * 5/3, &s_fontCfg);
		// Heading H3 -> imgui may delete the atlas here.
		s_mdConfig.headingFormats[2].font = io.Fonts->AddFontFromMemoryTTF(fb2, fbs2, baseFontSize * 4/3);

		return true;
	}
	
	void shutdown()
	{
	}

	void draw(const char* text)
	{
		ImGui::PushFont(s_baseFont);
		ImGui::Markdown(text, strlen(text), s_mdConfig);
		ImGui::PopFont();
	}

	void registerLocalLinkCB(LocalLinkCallback cb)
	{
		s_localLinkCb = cb;
	}

	void linkCallback(ImGui::MarkdownLinkCallbackData data)
	{
		const std::string url(data.link, data.linkLength);
		const size_t pos = url.find("local://");
		if (pos != std::string::npos)
		{
			if (s_localLinkCb) { s_localLinkCb(url.c_str() + pos + strlen("local://")); }
			return;
		}

#ifdef _WIN32
		ShellExecute(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
	}

	TextureGpu* getTexture(const char* path)
	{
		TextureMap::iterator iTexture = s_textures.find(path);
		if (iTexture != s_textures.end())
		{
			return iTexture->second;
		}

		const SDL_Surface* image = TFE_Image::get(path);
		if (!image) { return nullptr; }

		TextureGpu* texture = TFE_RenderBackend::createTexture(image->w, image->h, (u32*)image->pixels);
		if (texture) { s_textures[path] = texture; }
		return texture;
	}

	ImGui::MarkdownImageData imageCallback(ImGui::MarkdownLinkCallbackData data)
	{
		const std::string imagePath(data.link, data.linkLength);
		const TextureGpu* texture = getTexture(imagePath.c_str());
		ImGui::MarkdownImageData imageData = {};
		if (texture)
		{
			imageData.isValid = true;
			imageData.user_texture_id = TFE_RenderBackend::getGpuPtr(texture);
			imageData.size = { (f32)texture->getWidth(), (f32)texture->getHeight() };
		};
		return imageData;
	}
}
