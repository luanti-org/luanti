// SPDX-FileCopyrightText: 2026 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "irrlichttypes.h"

// Type tag for SSCSM requests (cross-the-channel direction: SSCSM env -> main).
// Sent as the first byte of every serialized request.
enum class SSCSMRequestType : u8 {
	PollNextEvent = 0,
	SetFatalError = 1,
	Print = 2,
	Log = 3,
	GetNode = 4,
	DisplayChatMessage = 5,
	JoinModChannel = 6,
	LeaveModChannel = 7,
	SendModChannelMessage = 8,
	JoinClientModChannel = 9,
	LeaveClientModChannel = 10,
	SendClientModChannelMessage = 11,
};

// Type tag for SSCSM events (main -> SSCSM env).
// Embedded inside SSCSMRequestPollNextEvent::Answer.
enum class SSCSMEventType : u8 {
	TearDown = 0,
	UpdateVFSFiles = 1,
	LoadMods = 2,
	OnStep = 3,
	// 4 was UpdateContentDefs, removed — content defs now ride the
	// *core:content_defs* clientmod channel, see sscsm_bridge.lua.
	OnModChannelMessage = 5,
	OnModChannelSignal = 6,
	OnClientModChannelMessage = 7,
	OnClientModChannelSignal = 8,
};
