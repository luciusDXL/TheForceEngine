#pragma once

// Get the GIT version while building.
#define STRINGIFY(x) #x

const char* c_gitVersion =
STRINGIFY(
#include "gitVersion.h"
);