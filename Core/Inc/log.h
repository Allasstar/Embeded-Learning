#pragma once

#include <stdio.h>

// DEBUG_LOG_ENABLED comes from CMakeLists.txt (Debug config only); in Release, LOG compiles to nothing
#ifdef DEBUG_LOG_ENABLED
#define LOG(...) printf(__VA_ARGS__)
#else
#define LOG(...) ((void)0)
#endif
