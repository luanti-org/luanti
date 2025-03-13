// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2015 kwolekr, Ryan Kwolek <kwolekr@minetest.net>

#include "objdef.h"
#include "util/numeric.h"
#include "log.h"
#include "gamedef.h"
#include "porting.h" // strcasecmp

#include <cassert>

ObjDefManager::ObjDefManager(IGameDef *gamedef, ObjDefType type)
{
	m_objtype = type;
	m_ndef = gamedef ? gamedef->getNodeDefManager() : NULL;
}


ObjDefManager::~ObjDefManager()
{
	for (size_t i = 0; i != m_objects.size(); i++)
		delete m_objects[i];
}


ObjDefHandle ObjDefManager::add(ObjDef *obj)
{
	assert(obj);

	if (obj->name.length() && getByName(obj->name))
		return OBJDEF_INVALID_HANDLE;

	u32 index = addRaw(obj);
	if (index == OBJDEF_INVALID_INDEX)
		return OBJDEF_INVALID_HANDLE;

	obj->handle = createHandle(index, m_objtype, obj->uid);
	return obj->handle;
}


ObjDef *ObjDefManager::get(ObjDefHandle handle) const
{
	u32 index = validateHandle(handle);
	return (index != OBJDEF_INVALID_INDEX) ? getRaw(index) : NULL;
}


ObjDef *ObjDefManager::set(ObjDefHandle handle, ObjDef *obj)
{
	u32 index = validateHandle(handle);
	if (index == OBJDEF_INVALID_INDEX)
		return NULL;

	ObjDef *oldobj = setRaw(index, obj);

	obj->uid    = oldobj->uid;
	obj->index  = oldobj->index;
	obj->handle = oldobj->handle;

	return oldobj;
}


u32 ObjDefManager::addRaw(ObjDef *obj)
{
	size_t nobjects = m_objects.size();
	if (nobjects >= OBJDEF_MAX_ITEMS)
		return -1;

	obj->index = nobjects;

	// Ensure UID is nonzero so that a valid handle == OBJDEF_INVALID_HANDLE
	// is not possible.  The slight randomness bias isn't very significant.
	obj->uid = myrand() & OBJDEF_UID_MASK;
	if (obj->uid == 0)
		obj->uid = 1;

	m_objects.push_back(obj);

	infostream << "ObjDefManager: added " << getObjectTitle()
		<< ": name=\"" << obj->name
		<< "\" index=" << obj->index
		<< " uid="     << obj->uid
		<< std::endl;

	return nobjects;
}


ObjDef *ObjDefManager::getRaw(u32 index) const
{
	return m_objects[index];
}


ObjDef *ObjDefManager::setRaw(u32 index, ObjDef *obj)
{
	ObjDef *old_obj = m_objects[index];
	m_objects[index] = obj;
	return old_obj;
}


ObjDef *ObjDefManager::getByName(const std::string &name) const
{
	for (size_t i = 0; i != m_objects.size(); i++) {
		ObjDef *obj = m_objects[i];
		if (obj && !strcasecmp(name.c_str(), obj->name.c_str()))
			return obj;
	}

	return NULL;
}


void ObjDefManager::clear()
{
	for (size_t i = 0; i != m_objects.size(); i++)
		delete m_objects[i];

	m_objects.clear();
}


u32 ObjDefManager::validateHandle(ObjDefHandle handle) const
{
	ObjDefType type;
	u32 index;
	u32 uid;

	bool is_valid =
		(handle != OBJDEF_INVALID_HANDLE)         &&
		decodeHandle(handle, &index, &type, &uid) &&
		(type == m_objtype)                       &&
		(index < m_objects.size())                &&
		(m_objects[index]->uid == uid);

	return is_valid ? index : -1;
}


ObjDefHandle ObjDefManager::createHandle(u32 index, ObjDefType type, u32 uid)
{
	ObjDefHandle handle = 0;
	set_bits(&handle, 0, 18, index);
	set_bits(&handle, 18, 6, type);
	set_bits(&handle, 24, 7, uid);

	u32 parity = calc_parity(handle);
	set_bits(&handle, 31, 1, parity);

	return handle ^ OBJDEF_HANDLE_SALT;
}


bool ObjDefManager::decodeHandle(ObjDefHandle handle, u32 *index,
	ObjDefType *type, u32 *uid)
{
	handle ^= OBJDEF_HANDLE_SALT;

	u32 parity = get_bits(handle, 31, 1);
	set_bits(&handle, 31, 1, 0);
	if (parity != calc_parity(handle))
		return false;

	*index = get_bits(handle, 0, 18);
	*type  = (ObjDefType)get_bits(handle, 18, 6);
	*uid   = get_bits(handle, 24, 7);
	return true;
}

// Cloning

void ObjDef::cloneTo(ObjDef *def) const
{
	def->index = index;
	def->uid = uid;
	def->handle = handle;
	def->name = name;
}

void ObjDefManager::cloneTo(ObjDefManager *mgr) const
{
	mgr->m_ndef = m_ndef;
	mgr->m_objects.reserve(m_objects.size());
	for (const auto &obj : m_objects)
		mgr->m_objects.push_back(obj->clone());
	mgr->m_objtype = m_objtype;
}
