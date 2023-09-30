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
	static Project* s_curProject = nullptr;

	Project* getProject()
	{
		return s_curProject;
	}

	void closeProject()
	{
		// TODO: Clear project specific editor data.
		s_curProject = nullptr;
	}

	void saveProject()
	{
	}

	bool ui_loadProject()
	{
		return false;
	}

	void ui_newProject()
	{
	}
}