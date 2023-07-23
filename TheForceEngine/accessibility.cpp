#include <cstring>
#include <chrono>
#include <map>
#include <string>

#include "accessibility.h"
#include <TFE_Ui/imGUI/imgui.h>
#include <TFE_System/system.h>
#include <TFE_FrontEndUI/console.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_System/parser.h>
#include <TFE_RenderBackend/renderBackend.h>

using namespace std::chrono;
using std::string;

namespace TFE_A11Y { //a11y is industry slang for accessibility

	const bool WRAP_TEXT = true;

	int maxLines = 5;
	float maxDuration = 10000;
	float fontScale = 1.0;
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

			//convert file name to lower-case
			string name = _strlwr((char*)tokens[0].c_str());
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
		TFE_System::logWrite(LOG_ERROR, "a11y", name);

		if (captionMap.count(string(name))) 
		{
			Caption caption = captionMap[string(name)]; //copy
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
			const float LINE_HEIGHT = 23;
			auto time = system_clock::now().time_since_epoch();
			auto elapsed = time - lastTime;
			auto elapsedMS = duration_cast<milliseconds>(elapsed).count();

			DisplayInfo display;
			TFE_RenderBackend::getDisplayInfo(&display);

			screenWidth = display.width;
			screenHeight = display.height;
			int lineHeight = LINE_HEIGHT * fontScale;
			ImVec2 windowSize = ImVec2(screenWidth * .6, lineHeight * maxLines);
			ImGui::SetNextWindowSize(windowSize);
			//TFE_System::logWrite(LOG_ERROR, "a11y", (std::to_string(screenWidth) + " " + std::to_string(subtitleWindowSize.x) + ", " + std::to_string(screenHeight) + " " + std::to_string(subtitleWindowSize.y)).c_str());
			if (active[0].env == CC_Gameplay)
			{
				ImGui::SetNextWindowPos(ImVec2((float)((screenWidth - windowSize.x) / 2), (float)(lineHeight * 2)));
				ImGui::Begin("##Captions", &t, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs);
			}
			else
			{
				ImGui::SetNextWindowPos(ImVec2((float)((screenWidth - windowSize.x) / 2), screenHeight - windowSize.y - lineHeight));
				ImGui::Begin("##Captions", &t, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs);
			}
			ImGui::SetWindowFontScale(fontScale);

			int totalLines = 0;
			for (int i = 0; i < active.size() && totalLines <= maxLines; i++)
			{
				Caption* title = &active[i];
				float wrapWidth = WRAP_TEXT ? 1.0f : -1.0f;
				auto textSize = ImGui::CalcTextSize(title->text.c_str(), 0, false, wrapWidth);
				int lines = round(textSize.y / (lineHeight * lineHeight));
				totalLines += lines;

				//TFE_System::logWrite(LOG_ERROR, "a11y", (std::to_string(i) + " " + std::to_string(textSize.y) + ", " + std::to_string(lineHeight) + " " + std::to_string(lines) + " " + std::to_string(totalLines) + " " + std::to_string(windowSize.y)).c_str());

				if (WRAP_TEXT)
				{
					ImGui::TextWrapped(title->text.c_str());
				}
				else
				{
					ImGui::SetCursorPosX((windowSize.x - textSize.x) * 0.5f);
					ImGui::Text(title->text.c_str());
				}

				title->msRemaining -= elapsedMS;
				if (title->msRemaining <= 0) {
					active.erase(active.begin() + i);
					i--;
				}
			}

			ImGui::End();
			lastTime = time;
		}
	}
}
