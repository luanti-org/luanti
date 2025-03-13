#pragma once

#if defined USE_CMAKE_CONFIG_H
	#include "cmake_config.h" // IWYU pragma: export
#else
	#warning Missing configuration
#endif

/*
 * There are three ways a Minetest source file can be built:
 * 1) we are currently building it for exclusively linking into the client
 * 2) we are currently building it for exclusively linking into the server
 * 3) we are building it only once for linking into both the client and server
 * In case of 1 and 2 that means a single source file may be built twice if
 * both a client and server build was requested.
 *
 * These options map to the following macros:
 * 1) IS_CLIENT_BUILD = 1 and CHECK_CLIENT_BUILD() = 1
 * 2) IS_CLIENT_BUILD = 0 and CHECK_CLIENT_BUILD() = 0
 * 3) IS_CLIENT_BUILD = 0 and CHECK_CLIENT_BUILD() undefined
 * As a function style macro CHECK_CLIENT_BUILD() has the special property that it
 * cause a compile error if it used but not defined.
 *
 * v v v v v v v v v READ THIS PART v v v v v v v v v
 * So that object files can be safely shared, these macros need to be used like so:
 * - use IS_CLIENT_BUILD to exclude optional helpers in header files or similar
 * - use CHECK_CLIENT_BUILD() in all source files, or in headers where it
 *   influences program behavior or e.g. class structure
 * In practice this means any shared object files (case 3) cannot include any
 * code that references CHECK_CLIENT_BUILD(), because a compiler error will occur.
 * ^ ^ ^ ^ ^ ^ ^ ^ ^ READ THIS PART ^ ^ ^ ^ ^ ^ ^ ^ ^
 *
 * The background is that for any files built only once, we need to ensure that
 * they are perfectly ABI-compatible between client/server or it will not work.
 * This manifests either as a linker error (good case) or insidious memory corruption
 * that causes obscure runtime behavior (bad case).
 * Finally, note that the best option is to split code in such a way that usage
 * of these macros is not necessary.
 */
#if MT_BUILDTARGET == 1
#define IS_CLIENT_BUILD 1
#define CHECK_CLIENT_BUILD() 1
#elif MT_BUILDTARGET == 2
#define IS_CLIENT_BUILD 0
#define CHECK_CLIENT_BUILD() 0
#else
#define IS_CLIENT_BUILD 0
#endif
#undef MT_BUILDTARGET
