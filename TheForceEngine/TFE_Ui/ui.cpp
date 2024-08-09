#include <TFE_Ui/ui.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FileSystem/fileutil.h>

#include "imGUI/imgui.h"
#include "imGUI/imgui_impl_sdl2.h"
#include "imGUI/imgui_impl_opengl3.h"
#include "imGUI/ImGuiFileDialog.h"
#include "IGFDIconFont.h"
#include "markdown.h"
#include <SDL.h>

namespace TFE_Ui
{
const char* glsl_version = "#version 130";
static s32 s_uiScale = 100;
static const ImWchar igfd_icons_ranges[] = {ICON_MIN_IGFD, ICON_MAX_IGFD, 0};
static ImFont* igfd_font = nullptr;

bool init(void* window, void* context, s32 uiScale)
{
	s_uiScale = uiScale;
	const f32 fontsize = 13 * s_uiScale / 100;

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// Setup Platform/Renderer bindings
	ImGui_ImplSDL2_InitForOpenGL((SDL_Window *)window, context);
	ImGui_ImplOpenGL3_Init(glsl_version);

	// Set the default font (13 px)
	// TODO: Allow scaled UI, so loading a different font for larger scales.
	if (s_uiScale <= 100)
	{
		io.Fonts->AddFontDefault();
	}
	else
	{
		char fp[TFE_MAX_PATH];
		sprintf(fp, "Fonts/DroidSansMono.ttf");
		TFE_Paths::mapSystemPath(fp);
		io.Fonts->AddFontFromFileTTF(fp, fontsize);
	}

	// Add Font for ImGuiFileDialog Icons
	ImFontConfig igfd_icons;
	igfd_icons.MergeMode  = true;
	igfd_icons.PixelSnapH = true;
	igfd_font = io.Fonts->AddFontFromMemoryCompressedBase85TTF(FONT_ICON_BUFFER_NAME_IGFD, fontsize, &igfd_icons, igfd_icons_ranges);

	TFE_Markdown::init(fontsize);

	return true;
}

void shutdown()
{
	TFE_Markdown::shutdown();

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
}

void setUiScale(s32 scale)
{
	s_uiScale = scale;
}

s32 getUiScale()
{
	return s_uiScale;
}

void setUiInput(const void* inputEvent)
{
	const SDL_Event* sdlEvent = (SDL_Event*)inputEvent;
	ImGui_ImplSDL2_ProcessEvent(sdlEvent);
}

void begin()
{
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();
}

void render()
{
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void invalidateFontAtlas()
{
	ImGui_ImplOpenGL3_DestroyFontsTexture();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
// General TFE tries to keep paths consistently using forward slashes for readability, consistency and
// generally they work equally well on Linux, Mac and Windows.
/////////////////////////////////////////////////////////////////////////////////////////////////////////
// *However* some specific APIs (usually of the Win32 variety) require backslashes.
// So FileUtil::convertToOSPath() must be used with initial paths to convert to the correct slash type.
// This can be a bit of a waste but I'd rather have consistent paths through most of the application
// and restrict the ugliness to as small an area as possible.
/////////////////////////////////////////////////////////////////////////////////////////////////////////
static void igfd_init(const char* initPath, IGFD::FileDialogConfig& config)
{
	char initPathOS[TFE_MAX_PATH];

	memset(initPathOS, 0, TFE_MAX_PATH);
	if (initPath)
		FileUtil::convertToOSPath(initPath, initPathOS);
	else
		strcpy(initPathOS, ".");

	config.path = initPathOS;
	config.countSelectionMax = 1;
	config.flags = ImGuiFileDialogFlags_Default | ImGuiFileDialogFlags_CaseInsensitiveExtentionFiltering | ImGuiFileDialogFlags_DisableThumbnailMode;
}

static void igfd_open(const char* title, const char* filters, IGFD::FileDialogConfig& config)
{
	ImGuiFileDialog::Instance()->OpenDialog("ImGFileDialogKey", title, filters, config);
	ImGuiFileDialog::Instance()->SetFileStyle(IGFD_FileStyleByTypeDir, "", ImVec4(1.0f, 1.0f, 0.0f, 0.9f), ICON_IGFD_FOLDER);
	ImGuiFileDialog::Instance()->SetFileStyle(IGFD_FileStyleByTypeFile, "", ImVec4(0.5f, 1.0f, 0.9f, 0.9f), ICON_IGFD_FILE);
}

void openFileDialog(const char* title, const char* initPath, const char* filters/* = { ".*" }*/, bool multiSelect/* = false*/)
{
	IGFD::FileDialogConfig config;

	igfd_init(initPath, config);
	config.countSelectionMax = multiSelect ? 0 : 1;
	config.flags &= ~ImGuiFileDialogFlags_ConfirmOverwrite;
	config.flags |= ImGuiFileDialogFlags_DisableCreateDirectoryButton;
	igfd_open(title, filters, config);
}

void saveFileDialog(const char* title, const char* initPath, const char* filters/* = { ".*" }*/, bool forceOverwrite/* = false*/)
{
	IGFD::FileDialogConfig config;

	igfd_init(initPath, config);
	if (forceOverwrite)
		config.flags &= ~ImGuiFileDialogFlags_ConfirmOverwrite;
	else
		config.flags |= ImGuiFileDialogFlags_ConfirmOverwrite;

	igfd_open(title, filters, config);
}

void directorySelectDialog(const char* title, const char* initPath, bool forceInitPath/* = false*/)
{
	IGFD::FileDialogConfig config;

	igfd_init(initPath, config);
	igfd_open(title, nullptr, config);
}

bool renderFileDialog(FileResult& inOutPath)
{
	// assume a minimum size of 640x480 pixels, below that interacting
	// with the dialog becomes a chore.
	bool done = false;

	if (igfd_font)
		ImGui::PushFont(igfd_font);

	if (ImGuiFileDialog::Instance()->Display("ImGFileDialogKey", ImGuiWindowFlags_NoCollapse, ImVec2(640, 480)))
	{
		if (ImGuiFileDialog::Instance()->IsOk())
		{
			std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
			std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath() + "/";
			inOutPath.push_back(filePathName);
			inOutPath.push_back(filePath);
		}
		ImGuiFileDialog::Instance()->Close();
		done = true;		// dialog was closed.
	}
	if (igfd_font)
		ImGui::PopFont();

	return done;
}

} // namespace TFE_Ui
