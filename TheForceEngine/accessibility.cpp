#include <cstring>
#include <chrono>
#include <map>
#include <string>

#include "accessibility.h"
#include <TFE_FileSystem/filestream.h>
#include <TFE_FrontEndUI/console.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Settings/settings.h>
#include <TFE_System/parser.h>
#include <TFE_System/system.h>
#include <TFE_Ui/imGUI/imgui.h>

using namespace std::chrono;
using std::string;

namespace TFE_A11Y { //a11y is industry slang for accessibility
	const float MAX_CAPTION_WIDTH = 1200;
	static float s_maxDuration = 10000;
	static u32 s_screenWidth;
	static u32 s_screenHeight;
	static bool s_active = true;
	static system_clock::duration s_lastTime;

	static FileStream s_captionsStream;
	static char* s_captionsBuffer;
	static TFE_Parser s_parser;
	static std::map<string, Caption> s_captionMap;
	static std::vector<Caption> s_activeCaptions;
	static std::vector<Caption> s_exampleCaptions;

	///////////////////////////////////////////
	// Forward Declarations
	///////////////////////////////////////////
	void addCaption(const ConsoleArgList& args);
	void drawCaptions(std::vector<Caption>* captions);

	string toUpper(string input)
	{
		unsigned char* p = (unsigned char*)input.c_str();
		while (*p) {
			*p = toupper((unsigned char)*p);
			p++;
		}

		return input;
	}	
	
	string toLower(string input)
	{
		unsigned char* p = (unsigned char*)input.c_str();
		while (*p) {
			*p = tolower((unsigned char)*p);
			p++;
		}

		return input;
	}

	void init()
	{
		CCMD("showCaption", addCaption, 1, "Display a test caption. Example: showCaption \"Hello, world\"");

		s_captionsStream.open("Captions/subtitles.txt", Stream::AccessMode::MODE_READ);
		auto size = (u32)s_captionsStream.getSize();
		s_captionsBuffer = (char*)malloc(size);
		s_captionsStream.readBuffer(s_captionsBuffer, size);

		s_parser.init(s_captionsBuffer, size);
		//ignore commented lines
		s_parser.addCommentString("#");
		s_parser.addCommentString("//");

		size_t bufferPos = 0;
		while (bufferPos < size)
		{
			const char* line = s_parser.readLine(bufferPos);
			if (!line) { break; }

			TokenList tokens;
			s_parser.tokenizeLine(line, tokens);
			if (tokens.size() < 2) { continue; }

			Caption caption = Caption();
			caption.text = tokens[1];

			//optional third field is duration in seconds, mainly useful for cutscenes
			if (tokens.size() > 2)
			{
				try
				{
					caption.msRemaining = (long)(std::stof(tokens[2]) * 1000);
				}
				catch(...) {
				}
			}
			if (caption.msRemaining <= 0)
			{
				//calculate caption duration based on text length
				caption.msRemaining = caption.text.length() * 70 + 900;
				if (caption.msRemaining > s_maxDuration) caption.msRemaining = s_maxDuration;
			}
			assert(caption.msRemaining > 0);
			
			if (caption.text[0] == '[') caption.type = CC_Effect;
			else caption.type == CC_Voice;

			string name = toLower(tokens[0]);
			s_captionMap[name] = caption;

			//TFE_System::logWrite(LOG_ERROR, "a11y", name.c_str());
			//TFE_System::logWrite(LOG_ERROR, "a11y", std::to_string(caption.msRemaining).c_str());
		};
	}

	void clearCaptions() 
	{
		s_activeCaptions.clear();
	}

	void onSoundPlay(char* name, CaptionEnv env)
	{
		auto settings = TFE_Settings::getA11ySettings();
		if (env == CC_Cutscene && !settings->showCutsceneCaptions && !settings->showCutsceneSubtitles) return;
		if (env == CC_Gameplay && !settings->showGameplayCaptions && !settings->showGameplaySubtitles) return;

		TFE_System::logWrite(LOG_ERROR, "a11y", name);

		string nameLower = toLower(name);

		if (s_captionMap.count(nameLower))
		{
			Caption caption = s_captionMap[nameLower]; //copy
			if (env == CC_Cutscene)
			{
				if (caption.type == CC_Effect && !settings->showCutsceneCaptions) return;
				else if (caption.type == CC_Voice && !settings->showCutsceneSubtitles) return;
			}
			if (env == CC_Gameplay)
			{
				if (caption.type == CC_Effect && !settings->showGameplayCaptions) return;
				else if (caption.type == CC_Voice && !settings->showGameplaySubtitles) return;
			}

			caption.env = env;
			TFE_System::logWrite(LOG_ERROR, "a11y", caption.text.c_str());
			addCaption(caption);
		}
	}

	void addCaption(const ConsoleArgList& args) {
		if (args.size() < 2) { return; }
		//strcpy(buffer, args[1].c_str());

		Caption caption;
		caption.text = args[1];
		caption.env = CC_Cutscene;
		caption.msRemaining = caption.text.length() * 90 + 750;
		caption.type == CC_Voice;

		addCaption(caption);
	}

	void addCaption(Caption caption) {
		if (s_activeCaptions.size() == 0) {
			s_lastTime = system_clock::now().time_since_epoch();
		}
		s_activeCaptions.push_back(caption);
		//TFE_System::logWrite(LOG_ERROR, "a11y", std::to_string(active.size()).c_str());
	}

	void drawCaptions() {
		if (s_activeCaptions.size() > 0)
		{
			drawCaptions(&s_activeCaptions);
		}
	}

	void drawCaptions(std::vector<Caption>* captions)
	{
		auto settings = TFE_Settings::getA11ySettings();

		//track time elapsed
		const float DEFAULT_LINE_HEIGHT = 20;
		const float LINE_PADDING = 5;
		auto s_time = system_clock::now().time_since_epoch();
		auto elapsed = s_time - s_lastTime;
		auto elapsedMS = duration_cast<milliseconds>(elapsed).count();

		DisplayInfo display;
		TFE_RenderBackend::getDisplayInfo(&display);
		s_screenWidth = display.width;
		s_screenHeight = display.height;
		int maxLines;
		float windowWidth = s_screenWidth * .8;
		float maxWidth = MAX_CAPTION_WIDTH;

		//calculate font size
		float fontScale = s_screenHeight / 1024.0f; //scale based on window resolution
		fontScale = fmax(fontScale, 1);
		if (captions->at(0).env == CC_Gameplay)
		{
			fontScale += (float)(settings->gameplayFontSize * s_screenHeight / 1280.0f);
			maxLines = settings->gameplayMaxTextLines;
			maxWidth += 100 * settings->gameplayFontSize;
		}
		else
		{
			fontScale += (float)(settings->cutsceneFontSize * s_screenHeight / 1280.0f);
			maxLines = 5;
			maxWidth += 100 * settings->cutsceneFontSize;
		}

		while (captions->size() > maxLines)
		{
			captions->erase(captions->begin());
		}

		//init window
		int lineHeight = DEFAULT_LINE_HEIGHT * fontScale;
		if (windowWidth > maxWidth) windowWidth = maxWidth;
		ImVec2 windowSize = ImVec2(windowWidth, 0); //auto-size vertically
		ImGui::SetNextWindowSize(windowSize);

		RGBA* fontColor;
		//TFE_System::logWrite(LOG_ERROR, "a11y", (std::to_string(screenWidth) + " " + std::to_string(subtitleWindowSize.x) + ", " + std::to_string(screenHeight) + " " + std::to_string(subtitleWindowSize.y)).c_str());
		if (captions->at(0).env == CC_Gameplay)
		{
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, settings->gameplayTextBackgroundAlpha)); //window bg
			float borderAlpha = settings->showGameplayTextBorder ? settings->gameplayTextBackgroundAlpha : 0;
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.5f, 0.5f, 0.5f, borderAlpha)); //window border
			ImGui::SetNextWindowPos(ImVec2((float)((s_screenWidth - windowSize.x) / 2), DEFAULT_LINE_HEIGHT * 2.0f));
			ImGui::Begin("##Captions", &s_active, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs);
			fontColor = &settings->gameplayFontColor;

			elapsedMS *= settings->gameplayTextSpeed;
		}
		else //cutscenes
		{
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, settings->cutsceneTextBackgroundAlpha)); //window bg
			float borderAlpha = settings->showCutsceneTextBorder ? settings->cutsceneTextBackgroundAlpha : 0;
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.5f, 0.5f, 0.5f, borderAlpha)); //window border
			ImGui::SetNextWindowPos(ImVec2((float)((s_screenWidth - windowSize.x) / 2), s_screenHeight - DEFAULT_LINE_HEIGHT), 0, ImVec2(0, 1));
			ImGui::Begin("##Captions", &s_active, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs);
			fontColor = &settings->cutsceneFontColor;

			elapsedMS *= settings->cutsceneTextSpeed;
		}
		ImGui::SetWindowFontScale(fontScale);
		ImGui::PopStyleColor(); //window border
		ImGui::PopStyleColor(); //window bg

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(fontColor->getRedF(), fontColor->getGreenF(), fontColor->getBlueF(), fontColor->getAlphaF()));

		//display each caption
		int totalLines = 0;
		for (int i = 0; i < captions->size() && totalLines < maxLines; i++)
		{
			Caption* title = &captions->at(i);
			bool wrapText = (title->env == CC_Cutscene); //wrapped for cutscenes, centered for gameplay
			float wrapWidth = wrapText ? windowSize.x : -1.0f;
			auto textSize = ImGui::CalcTextSize(title->text.c_str(), 0, false, wrapWidth);
			if (!wrapText && textSize.x > windowSize.x)
			{
				wrapText = true;
				wrapWidth = windowSize.x;
				textSize = ImGui::CalcTextSize(title->text.c_str(), 0, false, wrapWidth);
			}

			int lines = round(textSize.y / lineHeight);
			totalLines += lines;

			//TFE_System::logWrite(LOG_ERROR, "a11y", (std::to_string(i) + " " + std::to_string(textSize.y) + ", " + std::to_string(lineHeight) + " " + std::to_string(lines) + " " + std::to_string(totalLines) + " " + std::to_string(windowSize.y)).c_str());

			if (wrapText)
			{
				ImGui::TextWrapped(title->text.c_str());
			}
			else
			{
				ImGui::SetCursorPosX((windowSize.x - textSize.x) * 0.5f);
				ImGui::Text(title->text.c_str());
			}

			//reduce the caption's time remaining, removing it from the list if it's out of time
			if (i == 0)
			{
				title->msRemaining -= elapsedMS;
				if (title->msRemaining <= 0) {
					captions->erase(captions->begin() + i);
					i--;
				}
			}
		}
		ImGui::PopStyleColor();

		ImGui::End();
		s_lastTime = s_time;
	}

	void drawExampleCaptions()
	{
		if (s_exampleCaptions.size() == 0)
		{
			Caption caption;
			caption.text = "This is an example cutscene caption";
			caption.msRemaining = 500;
			caption.env = CaptionEnv::CC_Cutscene;
			caption.type = CC_Voice;
			s_exampleCaptions.push_back(caption);
		}
		drawCaptions(&s_exampleCaptions);
	}

	bool cutsceneCaptionsEnabled()
	{
		auto settings = TFE_Settings::getA11ySettings();
		return (settings->showCutsceneCaptions || settings->showCutsceneSubtitles);
	}	
	
	bool gameplayCaptionsEnabled()
	{
		auto settings = TFE_Settings::getA11ySettings();
		return (settings->showGameplayCaptions || settings->showGameplaySubtitles);
	}

	void focusCaptions()
	{
		ImGui::SetWindowFocus("##Captions");
	}
}
