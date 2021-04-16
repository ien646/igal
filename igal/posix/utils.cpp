#include "utils.h"

#ifdef _POSIX_VERSION

#include <unistd.h>

fs_str_t getExeDir()
{
	char buff[PATH_MAX];
	readlink("/proc/self/exe", buff, sizeof(buff) - 1);
	return fs_str_t(buff);
}

#endif