#pragma once

#include "../defs.h"

#if defined(WIN32) || defined(_WIN32)

void execProc(const fs_str_t& cmdLine);

#endif