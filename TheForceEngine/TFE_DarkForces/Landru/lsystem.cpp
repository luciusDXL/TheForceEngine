#include "lsystem.h"
#include "lactor.h"
#include "lactorAnim.h"
#include "lactorCust.h"
#include "lactorDelt.h"
#include "lcanvas.h"
#include "lfade.h"
#include "lfont.h"
#include "lmusic.h"
#include "lsound.h"
#include "ltimer.h"
#include "lpalette.h"
#include "lview.h"
#include "ldraw.h"
#include <TFE_Archive/lfdArchive.h>
#include <TFE_System/system.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_Memory/memoryRegion.h>
#include <TFE_Jedi/Math/core_math.h>
#include <TFE_Jedi/Renderer/virtualFramebuffer.h>
#include <assert.h>

using namespace TFE_Jedi;

namespace TFE_DarkForces
{
	static JBool s_lsystemInit = JFALSE;
	static LfdArchive s_archive = {};
	static LfdArchive s_soundFx = {};
	static MemoryRegion* s_lmem = nullptr;
	static MemoryRegion* s_lscene = nullptr;
	MemoryRegion* s_alloc = nullptr;

	enum LandruConstants
	{
		LANDRU_MEMORY_BASE   = 4 * 1024 * 1024, // 4 MB
		CUTSCENE_MEMORY_BASE = 8 * 1024 * 1024, // 8 MB
	};
	
	void lsystem_init()
	{
		if (s_lsystemInit) { return; }
		s_lmem = TFE_Memory::region_create("Landru", LANDRU_MEMORY_BASE);
		s_lscene = TFE_Memory::region_create("Cutscene", CUTSCENE_MEMORY_BASE);
		lsystem_setAllocator(LALLOC_PERSISTENT);

		s_lsystemInit = JTRUE;
		lcanvas_init(320, 200);
		ltime_init();
		lview_init();
		lpalette_init();
		lfade_init();
		lfont_init();
		lmusic_init();
		lsound_init();

		lactor_init();
		lactorDelt_init();
		lactorAnim_init();
		lactorCust_init();

		FilePath lfdPath;
		if (TFE_Paths::getFilePath("menu.lfd", &lfdPath))
		{
			s_archive.open(lfdPath.path);
			TFE_Paths::addLocalArchive(&s_archive);

			// Default font used by in-game UI.
			lfont_load("font8", 0);
			lfont_set(0);

			s_archive.close();
			TFE_Paths::removeLastArchive();
		}

		FilePath sfxPath;
		if (TFE_Paths::getFilePath("jedisfx.lfd", &sfxPath))
		{
			s_soundFx.open(sfxPath.path);
			TFE_Paths::addLocalArchive(&s_soundFx);
		}
	}

	void lsystem_destroy()
	{
		if (!s_lsystemInit) { return; }
		vfb_forceToBlack();

		s_lsystemInit = JFALSE;
		lcanvas_destroy();
		lview_destroy();
		lpalette_destroy();
		lfade_destroy();
		lfont_destroy();
		lmusic_destroy();
		lsound_destroy();

		lactorCust_destroy();
		lactorAnim_destroy();
		lactorDelt_destroy();
		lactor_destroy();

		TFE_Memory::region_destroy(s_lmem);
		TFE_Memory::region_destroy(s_lscene);
	}

	void lsystem_setAllocator(LAllocator alloc)
	{
		s_alloc = (alloc == LALLOC_PERSISTENT) ? s_lmem : s_lscene;
	}

	void lsystem_clearAllocator(LAllocator alloc)
	{
		MemoryRegion* region = (alloc == LALLOC_PERSISTENT) ? s_lmem : s_lscene;
		TFE_Memory::region_clear(region);
	}
}  // namespace TFE_DarkForces