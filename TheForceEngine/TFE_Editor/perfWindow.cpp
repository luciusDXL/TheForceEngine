#include "perfWindow.h"
#include <TFE_Ui/imGUI/imgui.h>
#include <algorithm>

namespace PerfWindow
{
static f64 s_aveDt = 0.0;
static f64 s_minDt = FLT_MAX, s_maxDt = -FLT_MAX;

static f64 s_curAveDt = 0.0;
static f64 s_curRange[2] = { 0.0 };

void setCurrentDt(f64 dt, u32 frame)
{
	s_minDt = std::min(dt, s_minDt);
	s_maxDt = std::max(dt, s_maxDt);
	s_aveDt += dt;

	if ((frame & 15) == 0)
	{
		s_aveDt /= 16.0;

		s_curAveDt = s_aveDt;
		s_curRange[0] = s_minDt;
		s_curRange[1] = s_maxDt;

		s_aveDt = 0.0;
		if ((frame & 63) == 0)
		{
			s_minDt = FLT_MAX; s_maxDt = -FLT_MAX;
		}
	}
}

void draw(bool* isActive)
{
	ImGui::Begin("Performance", isActive);
	ImVec2 windowPos = ImVec2(0.0f, 0.0f);
	ImGui::SetWindowPos("Performance", windowPos);

	ImGui::Text("FPS: %05.2f", 1.0/s_curAveDt);
	ImGui::Text("Frame Time: %05.2fms", s_curAveDt * 1000.0);
	ImGui::Text("Frame Time Range: %05.2fms, %05.2fms", s_curRange[0] * 1000.0, s_curRange[1] * 1000.0);
	ImGui::End();
}

}
