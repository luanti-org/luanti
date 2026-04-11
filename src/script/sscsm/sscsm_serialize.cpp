// SPDX-FileCopyrightText: 2026 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "sscsm_serialize.h"
#include "sscsm_irequest.h"
#include "sscsm_ievent.h"
#include "sscsm_requests.h"
#include "sscsm_events.h"
#include "exceptions.h"
#include "util/serialize.h"
#include <sstream>

std::unique_ptr<ISSCSMRequest> deserializeSSCSMRequest(const SerializedSSCSMRequest &data)
{
	std::istringstream is{data, std::ios::binary};
	auto tag = static_cast<SSCSMRequestType>(readU8(is));
	switch (tag) {
	case SSCSMRequestType::PollNextEvent:
		return std::make_unique<SSCSMRequestPollNextEvent>(
				SSCSMRequestPollNextEvent::deserializeBody(is));
	case SSCSMRequestType::SetFatalError:
		return std::make_unique<SSCSMRequestSetFatalError>(
				SSCSMRequestSetFatalError::deserializeBody(is));
	case SSCSMRequestType::Print:
		return std::make_unique<SSCSMRequestPrint>(
				SSCSMRequestPrint::deserializeBody(is));
	case SSCSMRequestType::Log:
		return std::make_unique<SSCSMRequestLog>(
				SSCSMRequestLog::deserializeBody(is));
	case SSCSMRequestType::GetNode:
		return std::make_unique<SSCSMRequestGetNode>(
				SSCSMRequestGetNode::deserializeBody(is));
	case SSCSMRequestType::DisplayChatMessage:
		return std::make_unique<SSCSMRequestDisplayChatMessage>(
				SSCSMRequestDisplayChatMessage::deserializeBody(is));
	}
	throw SerializationError("Unknown SSCSM request type tag");
}

std::unique_ptr<ISSCSMEvent> deserializeSSCSMEvent(std::istream &is)
{
	auto tag = static_cast<SSCSMEventType>(readU8(is));
	switch (tag) {
	case SSCSMEventType::TearDown:
		return std::make_unique<SSCSMEventTearDown>(
				SSCSMEventTearDown::deserializeBody(is));
	case SSCSMEventType::UpdateVFSFiles:
		return std::make_unique<SSCSMEventUpdateVFSFiles>(
				SSCSMEventUpdateVFSFiles::deserializeBody(is));
	case SSCSMEventType::LoadMods:
		return std::make_unique<SSCSMEventLoadMods>(
				SSCSMEventLoadMods::deserializeBody(is));
	case SSCSMEventType::OnStep:
		return std::make_unique<SSCSMEventOnStep>(
				SSCSMEventOnStep::deserializeBody(is));
	case SSCSMEventType::UpdateContentDefs:
		return std::make_unique<SSCSMEventUpdateContentDefs>(
				SSCSMEventUpdateContentDefs::deserializeBody(is));
	}
	throw SerializationError("Unknown SSCSM event type tag");
}

void SSCSMRequestPollNextEvent::Answer::serializeBody(std::ostream &os) const
{
	serializeSSCSMEventInto(*next_event, os);
}

SSCSMRequestPollNextEvent::Answer SSCSMRequestPollNextEvent::Answer::deserializeBody(std::istream &is)
{
	Answer a;
	a.next_event = deserializeSSCSMEvent(is);
	return a;
}
