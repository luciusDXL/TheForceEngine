#pragma once
#include <string>
using std::string;

namespace TFE_A11Y {
	enum CaptionEnv {
		CC_Gameplay, CC_Cutscene
	};

	struct Caption {
		string text;
		long msRemaining;
		CaptionEnv env;
	};

	void init();
	void draw();
	void addCaption(Caption caption);
	void onSoundPlay(char name[], CaptionEnv env);
}