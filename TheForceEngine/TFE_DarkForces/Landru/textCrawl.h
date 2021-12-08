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
	void openTextCrawl(LActor* actor, Film* film);
	void closeTextCrawl(LActor* actor);
}  // namespace TFE_DarkForces