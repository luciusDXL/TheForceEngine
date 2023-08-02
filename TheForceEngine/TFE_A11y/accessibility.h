#pragma once
#include <string>
#include <TFE_System/system.h>
using std::string;

namespace TFE_A11Y {
	enum CaptionEnv 
	{
		CC_Gameplay, CC_Cutscene
	};

	enum CaptionType
	{
		CC_Voice, CC_Effect
	};

	struct Caption 
	{
		string text;
		s64 microsecondsRemaining;
		CaptionType type;
		CaptionEnv env;
	};

	void init();
	void clearCaptions();
	void drawCaptions();
	void drawExampleCaptions();
	void focusCaptions();
	void addCaption(Caption caption);
	void onSoundPlay(char* name, CaptionEnv env);

	//True if captions or subtitles are enabled for cutscenes
	bool cutsceneCaptionsEnabled();
	//True if captions or subtitles are enabled for gameplay
	bool gameplayCaptionsEnabled();
}