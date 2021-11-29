#pragma once
//////////////////////////////////////////////////////////////////////
// Dark Forces
// Handles "Crawl" rendering.
//////////////////////////////////////////////////////////////////////
#include <TFE_System/types.h>
#include "lactor.h"
#include "cutscene_film.h"

namespace TFE_DarkForces
{
	void openCrawl(LActor* actor, Film* film);
	void closeCrawl(LActor* actor);
}  // namespace TFE_DarkForces