// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "sscsm_irequest.h"
#include "sscsm_ievent.h"
#include "mapnode.h"
#include "map.h"
#include "client/client.h"

struct SSCSMRequestPollNextEvent : public ISSCSMRequest
{
	struct Answer : public ISSCSMAnswer
	{
		std::unique_ptr<ISSCSMEvent> next_event;
	};

	SerializedSSCSMAnswer exec(Client *client) override
	{
		FATAL_ERROR("SSCSMRequestPollNextEvent needs to be handled by SSCSMControler::runEvent()");
	}
};

struct SSCSMRequestGetNode : public ISSCSMRequest
{
	struct Answer : public ISSCSMAnswer
	{
		MapNode node;
	};

	v3s16 pos;

	SerializedSSCSMAnswer exec(Client *client) override
	{
		MapNode node = client->getEnv().getMap().getNode(pos);

		Answer answer{};
		answer.node = node;
		return serializeSSCSMAnswer(std::move(answer));
	}
};
