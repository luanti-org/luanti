// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2022 sfan5 <sfan5@live.de>

#pragma once

#include <string>
#include <vector>
#include "irrlichttypes.h"
#include "util/basic_macros.h"

extern "C" {
#include <lua.h>
}

/*
	This file defines an in-memory representation of Lua objects including
	support for functions and userdata. It it used to move data between Lua
	states and cannot be used for persistence or network transfer.
*/

#define INSTR_SETTABLE     (-10)
#define INSTR_POP          (-11)
#define INSTR_PUSHREF      (-12)
#define INSTR_SETMETATABLE (-13)

/**
 * Represents a single instruction that pushes a new value or operates with existing ones.
 */
struct PackedInstr
{
	s16 type; // LUA_T* or INSTR_*
	u16 set_into; // set into table on stack
	bool keep_ref; // referenced later by INSTR_PUSHREF?
	bool pop; // remove from stack?
	// Note: the remaining members are named by type, not usage
	union {
		bool bdata; // boolean: value
		lua_Number ndata; // number: value
		struct {
			u16 uidata1, uidata2; // table: narr | nrec
		};
		struct {
			/*
				SETTABLE: key index | value index
				POP: indices to remove
				PUSHREF: index of referenced instr | unused
				otherwise w/ set_into: numeric key | unused
			*/
			s32 sidata1, sidata2;
		};
		void *ptrdata; // userdata: implementation defined
	};
	/*
		- string: value
		- function: buffer
		- w/ set_into: string key (no null bytes!)
		- userdata: name in registry
		- INSTR_SETMETATABLE: name of the metatable
	*/
	std::string sdata;

	PackedInstr() : type(0), set_into(0), keep_ref(false), pop(false) {}
};

/**
 * A packed value can be a primitive like a string or number but also a table
 * including all of its contents. It is made up of a linear stream of
 * instructions that build the final value when executed.
 */
struct PackedValue
{
	std::vector<PackedInstr> i;
	// Indicates whether there are any userdata pointers that need to be deallocated
	bool contains_userdata = false;

	PackedValue() = default;
	~PackedValue();

	DISABLE_CLASS_COPY(PackedValue)

	ALLOW_CLASS_MOVE(PackedValue)
};

/*
 * Packing callback: Turns a Lua value at given index into a void*
 */
typedef void *(*PackInFunc)(lua_State *L, int idx);
/*
 * Unpacking callback: Turns a void* back into the Lua value (left on top of stack)
 *
 * Note that this function must take ownership of the pointer, so make sure
 * to free or keep the memory.
 * `L` can be nullptr to indicate that data should just be discarded.
 */
typedef void (*PackOutFunc)(lua_State *L, void *ptr);
/*
 * Register a packable type with the name of its metatable.
 *
 * Even though the callbacks are global this must be called for every Lua state
 * that supports objects of this type.
 * This function is thread-safe.
 */
void script_register_packer(lua_State *L, const char *regname,
		PackInFunc fin, PackOutFunc fout);

// Pack a Lua value
PackedValue *script_pack(lua_State *L, int idx);
// Unpack a Lua value (left on top of stack)
// Note that this may modify the PackedValue, reusability is not guaranteed!
void script_unpack(lua_State *L, PackedValue *val);

// Dump contents of PackedValue to stdout for debugging
void script_dump_packed(const PackedValue *val);
