// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#pragma once

#include <cstdint>
#include <cassert>

//! 8 bit unsigned variable.
typedef uint8_t u8;

//! 8 bit signed variable.
typedef int8_t s8;

//! 8 bit character variable.
/** This is a typedef for char, it ensures portability of the engine. */
typedef char c8;

//! 16 bit unsigned variable.
typedef uint16_t u16;

//! 16 bit signed variable.
typedef int16_t s16;

//! 32 bit unsigned variable.
typedef uint32_t u32;

//! 32 bit signed variable.
typedef int32_t s32;

//! 64 bit unsigned variable.
typedef uint64_t u64;

//! 64 bit signed variable.
typedef int64_t s64;

//! 32 bit floating point variable.
/** This is a typedef for float, it ensures portability of the engine. */
typedef float f32;

//! 64 bit floating point variable.
/** This is a typedef for double, it ensures portability of the engine. */
typedef double f64;

//! Note: cannot assume that positional arguments are supported (not on Windows)
#define snprintf_irr snprintf

//! Type name for character type used by the file system (legacy).
typedef char fschar_t;
#define _IRR_TEXT(X) X

// Invokes undefined behavior for unreachable code optimization
// Note: an assert(false) is included first to catch this in debug builds
#if defined(__cpp_lib_unreachable)
#include <utility>
#define IRR_CODE_UNREACHABLE() do { assert(false); std::unreachable(); } while(0)
#elif defined(__has_builtin)
#if __has_builtin(__builtin_unreachable)
#define IRR_CODE_UNREACHABLE() do { assert(false); __builtin_unreachable(); } while(0)
#endif
#elif defined(_MSC_VER)
#define IRR_CODE_UNREACHABLE() do { assert(false); __assume(false); } while(0)
#endif
#ifndef IRR_CODE_UNREACHABLE
#define IRR_CODE_UNREACHABLE() (void)0
#endif

//! creates four CC codes used in Irrlicht for simple ids
/** some compilers can create those by directly writing the
code like 'code', but some generate warnings so we use this macro here */
#define MAKE_IRR_ID(c0, c1, c2, c3)                             \
	((u32)(u8)(c0) | ((u32)(u8)(c1) << 8) | \
			((u32)(u8)(c2) << 16) | ((u32)(u8)(c3) << 24))
