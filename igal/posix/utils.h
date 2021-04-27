#pragma once

#include "../defs.h"

#if defined(__linux__) || defined(__APPLE__) || defined(IGAL_PLATFORM_OVERRIDE_LINUX) || defined(IGAL_PLATFORM_OVERRIDE_MACOS)

fs_str_t getExeDir();

#endif