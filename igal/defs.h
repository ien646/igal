#pragma once

#include <string>

#ifdef WIN32
	typedef std::wstring fs_str_t;
#else
	typedef std::string fs_str_t;
#endif