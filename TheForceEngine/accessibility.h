#pragma once
#include <string>
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
		long msRemaining;
		CaptionType type;
		CaptionEnv env;
	};

	void init();
	void clear();
	void draw();
	void addCaption(Caption caption);
	void onSoundPlay(char name[], CaptionEnv env);

	//True if captions or subtitles are enabled for cutscenes
	bool cutsceneCaptionsEnabled();
	//True if captions or subtitles are enabled for gameplay
	bool gameplayCaptionsEnabled();
}