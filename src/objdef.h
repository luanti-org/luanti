// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2015 kwolekr, Ryan Kwolek <kwolekr@minetest.net>

#pragma once

#include "util/basic_macros.h" // DISABLE_CLASS_COPY
#include "irrlichttypes.h"
#include <string>
#include <vector>

class IGameDef;
class NodeDefManager;

#define OBJDEF_INVALID_INDEX ((u32)(-1))
#define OBJDEF_INVALID_HANDLE 0
#define OBJDEF_HANDLE_SALT 0x00585e6fu
#define OBJDEF_MAX_ITEMS (1 << 18)
#define OBJDEF_UID_MASK ((1 << 7) - 1)

typedef u32 ObjDefHandle;

enum ObjDefType {
	OBJDEF_GENERIC,
	OBJDEF_BIOME,
	OBJDEF_ORE,
	OBJDEF_DECORATION,
	OBJDEF_SCHEMATIC,
};

class ObjDef {
public:
	virtual ~ObjDef() = default;

	// Only implemented by child classes (leafs in class hierarchy)
	// Should create new object of its own type, call cloneTo() of parent class
	// and copy its own instance variables over
	virtual ObjDef *clone() const = 0;

	u32 index;
	u32 uid;
	ObjDefHandle handle;
	std::string name;

protected:
	// Only implemented by classes that have children themselves
	// by copying the defintion and changing that argument type (!!!)
	// Should defer to parent class cloneTo() if applicable and then copy
	// over its own properties
	void cloneTo(ObjDef *def) const;
};

// WARNING: Ownership of ObjDefs is transferred to the ObjDefManager it is
// added/set to.  Note that ObjDefs managed by ObjDefManager are NOT refcounted,
// so the same ObjDef instance must not be referenced multiple
// TODO: const correctness for getter methods
class ObjDefManager {
public:
	ObjDefManager(IGameDef *gamedef, ObjDefType type);
	virtual ~ObjDefManager();
	DISABLE_CLASS_COPY(ObjDefManager);

	// T *clone() const; // implemented in child class with correct type

	virtual const char *getObjectTitle() const { return "ObjDef"; }

	virtual void clear();
	virtual ObjDef *getByName(const std::string &name) const;

	//// Add new/get/set object definitions by handle
	virtual ObjDefHandle add(ObjDef *obj);
	virtual ObjDef *get(ObjDefHandle handle) const;
	virtual ObjDef *set(ObjDefHandle handle, ObjDef *obj);

	//// Raw variants that work on indexes
	virtual u32 addRaw(ObjDef *obj);

	// It is generally assumed that getRaw() will always return a valid object
	// This won't be true if people do odd things such as call setRaw() with NULL
	virtual ObjDef *getRaw(u32 index) const;
	virtual ObjDef *setRaw(u32 index, ObjDef *obj);

	size_t getNumObjects() const { return m_objects.size(); }
	ObjDefType getType() const { return m_objtype; }
	const NodeDefManager *getNodeDef() const { return m_ndef; }

	u32 validateHandle(ObjDefHandle handle) const;
	static ObjDefHandle createHandle(u32 index, ObjDefType type, u32 uid);
	static bool decodeHandle(ObjDefHandle handle, u32 *index,
		ObjDefType *type, u32 *uid);

protected:
	ObjDefManager() {};
	// Helper for child classes to implement clone()
	void cloneTo(ObjDefManager *mgr) const;

	const NodeDefManager *m_ndef;
	std::vector<ObjDef *> m_objects;
	ObjDefType m_objtype;
};
