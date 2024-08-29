#pragma once
#include <string>
#include <vector>
#include <TFE_System/system.h>
#include <TFE_FileSystem/physfswrapper.h>

namespace TFE_A11Y {
	///////////////////////////////////////////
	// Enums/structs
	///////////////////////////////////////////
	enum A11yStatus
	{
		CC_NOT_LOADED, CC_LOADED, CC_ERROR
	};

	enum CaptionEnv 
	{
		CC_GAMEPLAY, CC_CUTSCENE
	};

	enum CaptionType
	{
		CC_VOICE, CC_EFFECT
	};

	struct Caption 
	{
		std::string text;
		s64 microsecondsRemaining;
		CaptionType type;
		CaptionEnv env;
	};

	///////////////////////////////////////////
	// Constants
	///////////////////////////////////////////
	const std::string FILE_NAME_START = "subtitles-";
	const std::string FILE_NAME_EXT = ".txt";
	const f32 DEFAULT_LINE_HEIGHT = 20;

	///////////////////////////////////////////
	// Functions
	///////////////////////////////////////////

	// Initialize the A11y system. 
	void init();

	// Refresh the list of caption and font files, and reload the current caption file
	// (if any is loaded).
	void refreshFiles();

	// Fonts
	TFEFileList getFontFiles();
	std::string getCurrentFontFile();
	void setPendingFont(const std::string path);
	bool hasPendingFont();
	void loadPendingFont();

	// Captions
	TFEFileList getCaptionFiles();
	std::string getCurrentCaptionFile();
	A11yStatus getCaptionSystemStatus();
	void loadCaptions(const std::string path);
	void clearActiveCaptions();
	Vec2f drawCaptions();
	Vec2f drawExampleCaptions();
	void focusCaptions();
	void enqueueCaption(Caption caption);
	void onSoundPlay(char* name, CaptionEnv env);

	// True if captions or subtitles are enabled for cutscenes
	bool cutsceneCaptionsEnabled();
	// True if captions or subtitles are enabled for gameplay
	bool gameplayCaptionsEnabled();
}