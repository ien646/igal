#pragma once

#include <string>

#if defined(WIN32) || defined(_WIN32)
	typedef std::wstring fs_str_t;
#else
	typedef std::string fs_str_t;
#endif