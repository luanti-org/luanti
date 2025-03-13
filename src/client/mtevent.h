// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include "irrlichttypes.h"

class MtEvent
{
public:
	enum Type : u8
	{
		VIEW_BOBBING_STEP = 0,
		CAMERA_PUNCH_LEFT,
		CAMERA_PUNCH_RIGHT,
		PLAYER_FALLING_DAMAGE,
		PLAYER_DAMAGE,
		NODE_DUG,
		PLAYER_JUMP,
		PLAYER_REGAIN_GROUND,
		TYPE_MAX,
	};

	virtual ~MtEvent() = default;
	virtual Type getType() const = 0;
};

// An event with no parameters and customizable name
class SimpleTriggerEvent : public MtEvent
{
	Type type;

public:
	SimpleTriggerEvent(Type type) : type(type) {}
	Type getType() const override { return type; }
};

class MtEventReceiver
{
public:
	virtual ~MtEventReceiver() = default;
	virtual void onEvent(MtEvent *e) = 0;
};

typedef void (*event_receive_func)(MtEvent *e, void *data);

class MtEventManager
{
public:
	virtual ~MtEventManager() = default;
	virtual void put(MtEvent *e) = 0;
	virtual void reg(MtEvent::Type type, event_receive_func f, void *data) = 0;
	// If data==NULL, every occurrence of f is deregistered.
	virtual void dereg(MtEvent::Type type, event_receive_func f, void *data) = 0;
};
