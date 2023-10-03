#include "editorProject.h"
#include <TFE_Editor/editorConfig.h>
#include <TFE_Editor/editor.h>
#include <TFE_RenderBackend/renderBackend.h>
#include <TFE_System/system.h>
#include <TFE_System/parser.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/fileutil.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_Archive/archive.h>
#include <TFE_Ui/ui.h>

#include <TFE_Ui/imGUI/imgui.h>

namespace TFE_Editor
{
	static Project s_curProject = {};

	Project* project_get()
	{
		return &s_curProject;
	}

	void project_close()
	{
		s_curProject = {};
	}
		
	void project_save()
	{
	}

	bool project_load(const char* filepath)
	{
		return false;
	}

	bool project_editUi(bool newProject)
	{
		// Make sure we are *either* creating a new project or editing an active project.
		if (!newProject && !s_curProject.active) { return false; }
		if (newProject)
		{
			s_curProject = {};
			s_curProject.active = true;
		}



		return false;
	}
}