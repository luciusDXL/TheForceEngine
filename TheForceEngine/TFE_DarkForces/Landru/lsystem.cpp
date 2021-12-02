#include "lsystem.h"
#include "lactor.h"
#include "lactorAnim.h"
#include "lactorCust.h"
#include "lactorDelt.h"
#include "lcanvas.h"
#include "lfade.h"
#include "lfont.h"
#include "ltimer.h"
#include "lpalette.h"
#include "lview.h"
#include "ldraw.h"
#include <TFE_Archive/lfdArchive.h>
#include <TFE_System/system.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Renderer/virtualFramebuffer.h>
#include <assert.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	static JBool s_lsystemInit = JFALSE;
	static LfdArchive s_archive = {};

	void lsystem_init()
	{
		if (s_lsystemInit) { return; }

		s_lsystemInit = JTRUE;
		lcanvas_init(320, 200);
		ltime_init();
		lview_init();
		lpalette_init();
		lfade_init();
		lfont_init();

		lactor_init();
		lactorDelt_init();
		lactorAnim_init();
		lactorCust_init();

		FilePath lfdPath;
		if (TFE_Paths::getFilePath("menu.lfd", &lfdPath))
		{
			s_archive.open(lfdPath.path);
			TFE_Paths::addLocalArchive(&s_archive);
			lfont_load("font8", 0);
			lfont_set(0);

			TFE_Paths::removeLastArchive();
		}

		// TODO: Load core fonts, system palette, etc.
	}

	void lsystem_destroy()
	{
		if (!s_lsystemInit) { return; }

		s_lsystemInit = JFALSE;
		lcanvas_destroy();
		lview_destroy();
		lpalette_destroy();
		lfade_destroy();
		lfont_destroy();

		lactorCust_destroy();
		lactorAnim_destroy();
		lactorDelt_destroy();
		lactor_destroy();
	}
}  // namespace TFE_DarkForces