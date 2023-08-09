#include <cstring>
#include <chrono>
#include <map>

#include "accessibility.h"
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FrontEndUI/console.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_Settings/settings.h>
#include <TFE_System/parser.h>
#include <TFE_System/system.h>
#include <TFE_Ui/imGUI/imgui.h>
#include <TFE_Ui/ui.h>

using namespace std::chrono;
using std::string;

namespace TFE_A11Y  // a11y is industry slang for accessibility
{
	///////////////////////////////////////////
	// Forward Declarations
	///////////////////////////////////////////
	void findCaptionFiles(const char path[]);
	void onFileError(const string path);
	void addCaption(const ConsoleArgList& args);
	void drawCaptions(std::vector<Caption>* captions);
	string toUpper(string input);
	string toLower(string input);
	string toFileName(string language);
	void loadScreenSize();
	ImVec2 calcWindowSize(f32* fontScale, CaptionEnv env);
	s64 secondsToMicroseconds(f32 seconds);
	s64 calculateDuration(const string text);

	///////////////////////////////////////////
	// Constants
	///////////////////////////////////////////
	const f32 MAX_CAPTION_WIDTH = 1200;
	const f32 DEFAULT_LINE_HEIGHT = 20;
	const f32 LINE_PADDING = 5;
	// Base duration of a caption
	const s64 BASE_DURATION_MICROSECONDS = secondsToMicroseconds(0.9f);
	// How many microseconds are added to the caption duration per character
	const s64 MICROSECONDS_PER_CHAR = secondsToMicroseconds(0.07f);
	const s32 MAX_CAPTION_CHARS[] = // Keyed by font size
	{
		160, 160, 120, 78
	};
	const s32 CUTSCENE_MAX_LINES[] = // Keyed by font size
	{
		5, 5, 4, 3
	};

	///////////////////////////////////////////
	// Static vars
	///////////////////////////////////////////
	static A11yStatus s_status = CC_NOT_LOADED;
	static std::vector<string> s_captionFilePaths;
	static std::vector<string> s_captionFileNames;
	static string s_currentCaptionFileName;
	static string s_currentCaptionFilePath;
	static s64 s_maxDuration = secondsToMicroseconds(10.0f);
	static DisplayInfo s_display;
	static u32 s_screenWidth;
	static u32 s_screenHeight;
	static bool s_active = true;
	static bool s_logSFXNames = false;
	static system_clock::duration s_lastTime;

	static FileStream s_captionsStream;
	static char* s_captionsBuffer;
	static TFE_Parser s_parser;
	static std::map<string, Caption> s_captionMap;
	static std::vector<Caption> s_activeCaptions;
	static std::vector<Caption> s_exampleCaptions;

	// Initialize the Accessibility system. Only call this once on application launch.
	void init()
	{
		assert(s_status == CC_NOT_LOADED);
		CCMD("showCaption", addCaption, 1, "Display a test caption. Example: showCaption \"Hello, world\"");
		CVAR_BOOL(s_logSFXNames, "d_logSFXNames", CVFLAG_DO_NOT_SERIALIZE, "If enabled, log the name of each sound effect that plays.");

		// Get all caption file names from the Captions directories; we will use this to populate the
		// dropdown in the Accessibility settings menu.
		// First we check the User Documents directory for custom subtitle files added by the user.
		char docsCaptionsDir[TFE_MAX_PATH];
		const char* docsDir = TFE_Paths::getPath(PATH_USER_DOCUMENTS);
		sprintf(docsCaptionsDir, "%sCaptions/", docsDir);
		findCaptionFiles(docsCaptionsDir);
		// Then we check the Program directory for subtitle files that shipped with TFE
		char programCaptionsDir[TFE_MAX_PATH];
		const char* programDir = TFE_Paths::getPath(PATH_PROGRAM);
		sprintf(programCaptionsDir, "%sCaptions/", programDir);
		findCaptionFiles(programCaptionsDir);
		
		string search = toFileName(TFE_Settings::getA11ySettings()->language);
		for (s32 i = 0; i < s_captionFilePaths.size(); i++)
		{
			string path = s_captionFilePaths.at(i);
			if (path.find(search) != string::npos) {
				loadCaptions(path);
				break;
			}
		}

		// If the language didn't load, default to English
		if (s_status != CC_LOADED)
		{
			string fileName = programCaptionsDir + toFileName("en");
			loadCaptions(fileName);
		}
	}

	void findCaptionFiles(const char path[])
	{
		FileList dirList;
		FileUtil::readDirectory(path, "txt", dirList);
		if (!dirList.empty())
		{
			const size_t count = dirList.size();
			const std::string* dir = dirList.data();
			for (size_t d = 0; d < count; d++)
			{
				if (dir[d].substr(0, FILE_NAME_START.length()) == FILE_NAME_START) 
				{ 
					s_captionFileNames.push_back(dir[d]);
					s_captionFilePaths.push_back(path + dir[d]);
				}
			}
		}
	}

	// Get the file names of all sub/caption files we detect in the Captions directory
	std::vector<string> getCaptionFileNames() { return s_captionFileNames; }
	// Get the full paths of all sub/caption files we detect in the Captions directory
	std::vector<string> getCaptionFilePaths() { return s_captionFilePaths; }

	// The name of the currently selected Caption file
	string getCurrentCaptionFileName() { return s_currentCaptionFileName; }
	// The full path of the currently selected Caption file
	string getCurrentCaptionFilePath() { return s_currentCaptionFilePath; }

	// Load the caption file with the given name and parse the subs/captions from it
	void loadCaptions(const string path)
	{
		s_captionMap.clear();

		// Try to open the file
		s_currentCaptionFilePath = path;
		if (!s_captionsStream.open(path.c_str(), Stream::AccessMode::MODE_READ))
		{
			onFileError(path);
			return;
		}

		// Parse language name; for example, if file name is "subtitles-de.txt", the language is "de".
		// The idea is for the language name to be an ISO 639-1 two-letter code, but for now the system
		// doesn't actually care how long the language name is or whether it's a valid 639-1 code.
		s32 start = path.find(FILE_NAME_START);
		if (start != string::npos)
		{
			s_currentCaptionFileName = path.substr(start);
			string language = path.substr(start + FILE_NAME_START.length());
			language = language.substr(0, language.length() - FILE_NAME_EXT.length());
			TFE_Settings::getA11ySettings()->language = language;
		}

		auto size = (u32)s_captionsStream.getSize();
		s_captionsBuffer = (char*)malloc(size);
		s_captionsStream.readBuffer(s_captionsBuffer, size);

		s_parser.init(s_captionsBuffer, size);
		// Ignore commented lines
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

			// Optional third field is duration in seconds, mainly useful for cutscenes
			if (tokens.size() > 2)
			{
				try
				{
					caption.microsecondsRemaining = secondsToMicroseconds(std::stof(tokens[2]));
				}
				catch(...) {
				}
			}
			if (caption.microsecondsRemaining <= 0)
			{
				// Calculate caption duration based on text length
				caption.microsecondsRemaining = calculateDuration(caption.text);
				if (caption.microsecondsRemaining > s_maxDuration) caption.microsecondsRemaining = (s64)s_maxDuration;
			}
			assert(caption.microsecondsRemaining > 0);
			
			if (caption.text[0] == '[') caption.type = CC_EFFECT;
			else caption.type = CC_VOICE;

			string name = toLower(tokens[0]);
			s_captionMap[name] = caption;
		};

		s_status = CC_LOADED;
		delete s_captionsBuffer;
	}

	A11yStatus getStatus() { return s_status; }

	void onFileError(const string path)
	{
		string error = "Couldn't find caption file at " + path;
		TFE_System::logWrite(LOG_ERROR, "a11y", error.c_str());
		s_status = CC_ERROR;
		// TODO: display an error dialog
	}

	void clearActiveCaptions() 
	{
		s_activeCaptions.clear();
		s_exampleCaptions.clear();
	}

	void onSoundPlay(char* name, CaptionEnv env)
	{
		const TFE_Settings_A11y* settings = TFE_Settings::getA11ySettings();
		// Don't add caption if captions are disabled for the current env
		if (env == CC_CUTSCENE && !settings->showCutsceneCaptions && !settings->showCutsceneSubtitles) { return; }
		if (env == CC_GAMEPLAY && !settings->showGameplayCaptions && !settings->showGameplaySubtitles) { return; }

		if (s_logSFXNames) { TFE_System::logWrite(LOG_ERROR, "a11y", name); }

		string nameLower = toLower(name);

		if (s_captionMap.count(nameLower))
		{
			Caption caption = s_captionMap[nameLower]; // Copy
			caption.env = env;
			// Don't add caption if the last caption has the same text
			if (s_activeCaptions.size() > 0 && s_activeCaptions.back().text == caption.text) { return; }

			if (env == CC_CUTSCENE)
			{
				// Don't add caption if this type of caption is disabled
				if (caption.type == CC_EFFECT && !settings->showCutsceneCaptions) return;
				else if (caption.type == CC_VOICE && !settings->showCutsceneSubtitles) return;

				s32 maxLines = CUTSCENE_MAX_LINES[settings->cutsceneFontSize];

				// Split caption into chunks if it's very long and would take up too much screen
				// real estate at the selected font size (e.g. narration before Talay).
				loadScreenSize();
				f32 fontScale;
				auto windowSize = calcWindowSize(&fontScale, env);
				const f32 CHAR_WIDTH = 10.12f;
				assert(fontScale > 0);
				s32 maxCharsPerLine = (s32)(windowSize.x / (CHAR_WIDTH * fontScale));
				s32 maxChars = maxCharsPerLine * maxLines;
				s32 count = 1;
				while (caption.text.length() > maxChars)
				{
					Caption next = caption; // Copy
					s32 spaceIndex = 0;
					for (s32 i = 0; i < maxLines; i++)
					{
						spaceIndex = (s32)next.text.rfind(' ', spaceIndex + maxCharsPerLine);
						if (spaceIndex < 0) { break; }
					}
					if (spaceIndex > 0)
					{
						f32 ratio = spaceIndex / (f32)caption.text.length();
						next.text = next.text.substr(0, spaceIndex);
						next.microsecondsRemaining = s64(next.microsecondsRemaining * ratio);  // Fixes a float to int warning.
						addCaption(next);
						caption.text = caption.text.substr(spaceIndex + 1);
						caption.microsecondsRemaining -= next.microsecondsRemaining;
						count++;
					}
					else { break; }
				}
			}
			else if (env == CC_GAMEPLAY)
			{
				// Don't add caption if this type of caption is disabled
				if (caption.type == CC_EFFECT && !settings->showGameplayCaptions) return;
				else if (caption.type == CC_VOICE && !settings->showGameplaySubtitles) return;
			}

			addCaption(caption);
		}
	}

	void addCaption(const ConsoleArgList& args)
	{
		if (args.size() < 2) { return; }

		Caption caption;
		caption.text = args[1];
		caption.env = CC_CUTSCENE;
		caption.microsecondsRemaining = calculateDuration(caption.text);
		caption.type = CC_VOICE;

		addCaption(caption);
	}

	void addCaption(Caption caption)
	{
		if (s_activeCaptions.size() == 0) {
			s_lastTime = system_clock::now().time_since_epoch();
		}
		s_activeCaptions.push_back(caption);
		//TFE_System::logWrite(LOG_ERROR, "a11y", std::to_string(active.size()).c_str());
	}

	void drawCaptions()
	{
		if (s_activeCaptions.size() > 0)
		{
			drawCaptions(&s_activeCaptions);
		}
	}

	void drawCaptions(std::vector<Caption>* captions)
	{
		TFE_Settings_A11y* settings = TFE_Settings::getA11ySettings();

		// Track time elapsed since last update
		system_clock::duration time = system_clock::now().time_since_epoch();
		system_clock::duration elapsed = time - s_lastTime;
		// Microseconds, not milliseconds
		s64 elapsedMS = duration_cast<microseconds>(elapsed).count();

		loadScreenSize();
		s32 maxLines;

		// Calculate font size and window dimensions
		if (captions->at(0).env == CC_GAMEPLAY)
		{
			maxLines = settings->gameplayMaxTextLines;
			// If too many captions, remove oldest captions
			while (captions->size() > maxLines)	captions->erase(captions->begin());
		}
		else // Cutscene
		{
			maxLines = maxLines = CUTSCENE_MAX_LINES[settings->cutsceneFontSize];
		}

		// Init ImGui window
		f32 fontScale;
		ImVec2 windowSize = calcWindowSize(&fontScale, captions->at(0).env);
		ImGui::SetNextWindowSize(windowSize);
		s32 lineHeight = (s32)(DEFAULT_LINE_HEIGHT * fontScale);

		RGBA* fontColor;
		//TFE_System::logWrite(LOG_ERROR, "a11y", (std::to_string(screenWidth) + " " + std::to_string(subtitleWindowSize.x) + ", " + std::to_string(screenHeight) + " " + std::to_string(subtitleWindowSize.y)).c_str());
		if (captions->at(0).env == CC_GAMEPLAY)
		{
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, settings->gameplayTextBackgroundAlpha)); // Window bg
			f32 borderAlpha = settings->showGameplayTextBorder ? settings->gameplayTextBackgroundAlpha : 0;
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.5f, 0.5f, 0.5f, borderAlpha)); // Window border
			ImGui::SetNextWindowPos(ImVec2((f32)((s_screenWidth - windowSize.x) / 2), DEFAULT_LINE_HEIGHT * 2.0f));
			ImGui::Begin("##Captions", &s_active, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs);
			fontColor = &settings->gameplayFontColor;

			elapsedMS *= (s64)settings->gameplayTextSpeed;
		}
		else // Cutscenes
		{
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, settings->cutsceneTextBackgroundAlpha)); // Window bg
			f32 borderAlpha = settings->showCutsceneTextBorder ? settings->cutsceneTextBackgroundAlpha : 0;
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.5f, 0.5f, 0.5f, borderAlpha)); // Window border
			ImGui::SetNextWindowPos(ImVec2((f32)((s_screenWidth - windowSize.x) / 2), s_screenHeight - DEFAULT_LINE_HEIGHT), 0, ImVec2(0, 1));
			ImGui::Begin("##Captions", &s_active, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs);
			fontColor = &settings->cutsceneFontColor;

			elapsedMS = (s64)(elapsedMS * settings->cutsceneTextSpeed);
		}
		ImGui::SetWindowFontScale(fontScale);
		ImGui::PopStyleColor(); // Window border
		ImGui::PopStyleColor(); // Window bg

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(fontColor->getRedF(), fontColor->getGreenF(), fontColor->getBlueF(), fontColor->getAlphaF()));

		// Display each caption
		s32 totalLines = 0;
		for (s32 i = 0; i < captions->size() && totalLines < maxLines; i++)
		{
			Caption* title = &captions->at(i);
			bool wrapText = (title->env == CC_CUTSCENE); // Wrapped for cutscenes, centered for gameplay
			f32 wrapWidth = wrapText ? windowSize.x : -1.0f;
			auto textSize = ImGui::CalcTextSize(title->text.c_str(), 0, false, wrapWidth);
			if (!wrapText && textSize.x > windowSize.x)
			{
				wrapText = true;
				wrapWidth = windowSize.x;
				textSize = ImGui::CalcTextSize(title->text.c_str(), 0, false, wrapWidth);
			}

			s32 lines = (s32)round(textSize.y / lineHeight);
			totalLines += lines;

			//TFE_System::logWrite(LOG_ERROR, "a11y", (std::to_string(i) + " " + std::to_string(textSize.y) + ", " + std::to_string(lineHeight) + " " + std::to_string(lines) + " " + std::to_string(totalLines) + " " + std::to_string(windowSize.y)).c_str());
			if (wrapText)
			{
				ImGui::TextWrapped("%s", title->text.c_str());
			}
			else
			{
				ImGui::SetCursorPosX((windowSize.x - textSize.x) * 0.5f);
				ImGui::Text("%s", title->text.c_str());
			}

			// Reduce the caption's time remaining, removing it from the list if it's out of time
			if (i == 0)
			{
				if (!TFE_Console::isOpen()) // Don't advance timer while console is open
				{
					title->microsecondsRemaining -= elapsedMS;
					if (title->microsecondsRemaining <= 0) {
						captions->erase(captions->begin() + i);
						i--;
					}
				}
			}
		}
		ImGui::PopStyleColor();

		ImGui::End();
		s_lastTime = time;
	}

	void drawExampleCaptions()
	{
		if (s_exampleCaptions.size() == 0)
		{
			Caption caption = s_captionMap["example_cutscene"]; // Copy
			caption.env = CC_CUTSCENE;
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

	///////////////////////////////////////////
	// Helpers
	///////////////////////////////////////////
	string toUpper(string input)
	{
		u8* p = (u8*)input.c_str();
		while (*p)
		{
			*p = toupper((u8)*p);
			p++;
		}
		return input;
	}

	string toLower(string input)
	{
		u8* p = (u8*)input.c_str();
		while (*p)
		{
			*p = tolower((u8)*p);
			p++;
		}
		return input;
	}
	
	string toFileName(string language)
	{
		return FILE_NAME_START + language + FILE_NAME_EXT;
	}

	void loadScreenSize()
	{
		TFE_RenderBackend::getDisplayInfo(&s_display);
		s_screenWidth = s_display.width;
		s_screenHeight = s_display.height;
	}

	ImVec2 calcWindowSize(f32* fontScale, CaptionEnv env)
	{
		TFE_Settings_A11y* settings = TFE_Settings::getA11ySettings();
		*fontScale = s_screenHeight / 1024.0f; // Scale based on window resolution
		*fontScale = (f32)fmax(*fontScale, 1);

		f32 maxWidth = MAX_CAPTION_WIDTH;
		f32 windowWidth = s_screenWidth * 0.8f;
		if (env == CC_GAMEPLAY)
		{
			*fontScale += (f32)(settings->gameplayFontSize * s_screenHeight / 1280.0f);
			maxWidth += 100 * settings->gameplayFontSize;
		}
		else
		{
			*fontScale += (f32)(settings->cutsceneFontSize * s_screenHeight / 1280.0f);
			maxWidth += 100 * settings->cutsceneFontSize;
		}
		*fontScale = (f32)fmax(*fontScale, 1);

		if (windowWidth > maxWidth) windowWidth = maxWidth;
		ImVec2 windowSize = ImVec2(windowWidth, 0); // Auto-size vertically
		return windowSize;
	}

	s64 secondsToMicroseconds(f32 seconds)
	{
		return (s64)(seconds * 1000000);
	}

	s64 calculateDuration(const string text) {
		return BASE_DURATION_MICROSECONDS + text.length() * MICROSECONDS_PER_CHAR;
	}
}
