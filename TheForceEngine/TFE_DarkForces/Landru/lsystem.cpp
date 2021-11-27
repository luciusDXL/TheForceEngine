#include "lsystem.h"
#include "lcanvas.h"
#include "ltimer.h"
#include "lpalette.h"
#include "lview.h"
#include "ldraw.h"
#include <TFE_System/system.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Renderer/virtualFramebuffer.h>
#include <assert.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	static JBool s_lsystemInit = JFALSE;

	void lsystem_init()
	{
		if (s_lsystemInit) { return; }
		ltime_init();
		lview_init();
		lpalette_init();

		// TODO: Load core fonts, system palette, etc.
	}

	void lsystem_destroy()
	{
		s_lsystemInit = JFALSE;
		lview_destroy();
		lpalette_destroy();
	}
}  // namespace TFE_DarkForces