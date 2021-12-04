#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Landru system, setup and teardown.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "lrect.h"

#define landru_alloc(size)        TFE_Memory::region_alloc(s_alloc, size)
#define landru_realloc(ptr, size) TFE_Memory::region_realloc(s_alloc, ptr, size)
#define landru_free(ptr)          TFE_Memory::region_free(s_alloc, ptr)
struct MemoryRegion;

enum LAllocator
{
	LALLOC_PERSISTENT = 0,
	LALLOC_CUTSCENE,
};

namespace TFE_DarkForces
{
	void lsystem_init();
	void lsystem_destroy();

	void lsystem_setAllocator(LAllocator alloc);
	void lsystem_clearAllocator(LAllocator alloc);

	extern MemoryRegion* s_alloc;
}  // namespace TFE_DarkForces