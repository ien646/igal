#include "utils.h"

#if defined(__linux__) || defined(__APPLE__) || defined(IGAL_PLATFORM_OVERRIDE_LINUX) || defined(IGAL_PLATFORM_OVERRIDE_MACOS)

#include <limits.h>
#include <unistd.h>

fs_str_t getExeDir()
{
	char buff[PATH_MAX];
	readlink("/proc/self/exe", buff, sizeof(buff) - 1);
	return fs_str_t(buff);
}

#endif