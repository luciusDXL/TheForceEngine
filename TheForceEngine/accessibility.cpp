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
	int maxLines = 5;
	float maxDuration = 10000;
	u32 screenWidth;
	u32 screenHeight;
	float time = -1;
	bool t = true;
	system_clock::duration lastTime;
	char buffer[100];

	FileStream captionsStream;
	char* captionsBuffer;
	TFE_Parser parser;
	std::map<string, Caption> captionMap;
	std::vector<Caption> active;

	///////////////////////////////////////////
	// Forward Declarations
	///////////////////////////////////////////
	void addCaption(const ConsoleArgList& args);

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

		captionsStream.open("Captions/subtitles.txt", Stream::AccessMode::MODE_READ);
		auto size = (u32)captionsStream.getSize();
		captionsBuffer = (char*)malloc(size);
		captionsStream.readBuffer(captionsBuffer, size);

		parser.init(captionsBuffer, size);
		//ignore commented lines
		parser.addCommentString("#");
		parser.addCommentString("//");

		size_t bufferPos = 0;
		while (bufferPos < size)
		{
			const char* line = parser.readLine(bufferPos);
			if (!line) { break; }

			TokenList tokens;
			parser.tokenizeLine(line, tokens);
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
				caption.msRemaining = caption.text.length() * 90 + 750;
				if (caption.msRemaining > maxDuration) caption.msRemaining = maxDuration;
			}
			assert(caption.msRemaining > 0);
			
			if (caption.text[0] == '[') caption.type = CC_Effect;
			else caption.type == CC_Voice;

			string name = toLower(tokens[0]);
			captionMap[name] = caption;

			//TFE_System::logWrite(LOG_ERROR, "a11y", name.c_str());
			//TFE_System::logWrite(LOG_ERROR, "a11y", std::to_string(caption.msRemaining).c_str());
		};
	}

	void clear() 
	{
		active.clear();
	}

	void onSoundPlay(char name[], CaptionEnv env)
	{
		auto settings = TFE_Settings::getA11ySettings();
		if (env == CC_Cutscene && !settings->showCutsceneCaptions && !settings->showCutsceneSubtitles) return;
		if (env == CC_Gameplay && !settings->showGameplayCaptions && !settings->showGameplaySubtitles) return;

		TFE_System::logWrite(LOG_ERROR, "a11y", name);

		string nameLower = toLower(name);

		if (captionMap.count(nameLower))
		{
			Caption caption = captionMap[nameLower]; //copy
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
		if (active.size() == 0) {
			lastTime = system_clock::now().time_since_epoch();
		}
		active.push_back(caption);
		if (active.size() > maxLines) {
			active.erase(active.begin());
		}
		//TFE_System::logWrite(LOG_ERROR, "a11y", std::to_string(active.size()).c_str());
	}

	void draw() {
		if (active.size() > 0)
		{
			auto settings = TFE_Settings::getA11ySettings();

			//track time elapsed
			const float DEFAULT_LINE_HEIGHT = 20;
			const float LINE_PADDING = 5;
			auto time = system_clock::now().time_since_epoch();
			auto elapsed = time - lastTime;
			auto elapsedMS = duration_cast<milliseconds>(elapsed).count();

			DisplayInfo display;
			TFE_RenderBackend::getDisplayInfo(&display);
			screenWidth = display.width;
			screenHeight = display.height;

			//calculate font size
			float fontScale = screenHeight / 1024.0f; //scale based on window resolution
			fontScale = fmax(fontScale, 1);
			if (active[0].env == CC_Gameplay)
			{
				fontScale += (float)(settings->gameplayFontSize * screenHeight / 1280.0f);
			}
			else
			{
				fontScale += (float)(settings->cutsceneFontSize * screenHeight / 1280.0f);
			}

			//init window
			int lineHeight = DEFAULT_LINE_HEIGHT * fontScale;
			ImVec2 windowSize = ImVec2(screenWidth * .8, 0); //auto-size vertically
			ImGui::SetNextWindowSize(windowSize);

			RGBA* fontColor;
			//TFE_System::logWrite(LOG_ERROR, "a11y", (std::to_string(screenWidth) + " " + std::to_string(subtitleWindowSize.x) + ", " + std::to_string(screenHeight) + " " + std::to_string(subtitleWindowSize.y)).c_str());
			if (active[0].env == CC_Gameplay)
			{
				ImGui::SetNextWindowPos(ImVec2((float)((screenWidth - windowSize.x) / 2), DEFAULT_LINE_HEIGHT * 2.0f));
				ImGui::Begin("##Captions", &t, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs);
				fontColor = &settings->gameplayFontColor;
			}
			else
			{
				ImGui::SetNextWindowPos(ImVec2((float)((screenWidth - windowSize.x) / 2), screenHeight - lineHeight), ImGuiCond_Appearing, ImVec2(0, 1));
				ImGui::Begin("##Captions", &t, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs);
				fontColor = &settings->cutsceneFontColor;
			}
			ImGui::SetWindowFontScale(fontScale);

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(fontColor->getRedF(), fontColor->getGreenF(), fontColor->getBlueF(), fontColor->getAlphaF()));

			//display each caption
			int totalLines = 0;
			for (int i = 0; i < active.size() && totalLines < maxLines; i++)
			{
				Caption* title = &active[i];
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
				TFE_System::logWrite(LOG_ERROR, "a11y", string(to_string(lines) + " " + to_string(totalLines)).c_str());

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
				title->msRemaining -= elapsedMS;
				if (title->msRemaining <= 0) {
					active.erase(active.begin() + i);
					i--;
				}
			}
			ImGui::PopStyleColor();

			ImGui::End();
			lastTime = time;
		}
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
}
