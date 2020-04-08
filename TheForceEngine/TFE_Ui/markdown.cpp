#include <TFE_Ui/ui.h>
#include <TFE_Ui/markdown.h>
#include <TFE_Asset/imageAsset.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_RenderBackend/textureGpu.h>
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

	static LocalLinkCallback s_localLinkCb = nullptr;

	void linkCallback(ImGui::MarkdownLinkCallbackData data);
	ImGui::MarkdownImageData imageCallback(ImGui::MarkdownLinkCallbackData data);

	static ImGui::MarkdownConfig s_mdConfig{ linkCallback, imageCallback, nullptr, { { NULL, true }, { NULL, true }, { NULL, false } } };

	bool init(f32 baseFontSize)
	{
		ImGuiIO& io = ImGui::GetIO();

		s_baseFont = io.Fonts->AddFontFromFileTTF("Fonts/DroidSans.ttf", baseFontSize);
		// Bold
		s_mdConfig.boldFont = io.Fonts->AddFontFromFileTTF("Fonts/DroidSans-Bold.ttf", baseFontSize);
		// Heading H1
		s_mdConfig.headingFormats[0].font = io.Fonts->AddFontFromFileTTF("Fonts/DroidSans-Bold.ttf", baseFontSize * 2);
		// Heading H2
		s_mdConfig.headingFormats[1].font = io.Fonts->AddFontFromFileTTF("Fonts/DroidSans-Bold.ttf", baseFontSize * 5/3);
		// Heading H3
		s_mdConfig.headingFormats[2].font = io.Fonts->AddFontFromFileTTF("Fonts/DroidSans-Bold.ttf", baseFontSize * 4/3);

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

		const Image* image = TFE_Image::get(path);
		if (!image) { return nullptr; }

		TextureGpu* texture = TFE_RenderBackend::createTexture(image->width, image->height, image->data);
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
