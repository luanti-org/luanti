// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "sscsm_irequest.h"
#include "sscsm_ievent.h"
#include "mapnode.h"
#include "map.h"
#include "client/client.h"
#include "log_internal.h"

// Poll the next event (e.g. on_globalstep)
struct SSCSMRequestPollNextEvent final : public ISSCSMRequest
{
	struct Answer final : public ISSCSMAnswer
	{
		std::unique_ptr<ISSCSMEvent> next_event;
	};

	SerializedSSCSMAnswer exec(Client *client) override
	{
		FATAL_ERROR("SSCSMRequestPollNextEvent needs to be handled by SSCSMControler::runEvent()");
	}
};

// Some error occured in the SSCSM env
struct SSCSMRequestSetFatalError final : public ISSCSMRequest
{
	struct Answer final : public ISSCSMAnswer
	{
	};

	std::string reason;

	SerializedSSCSMAnswer exec(Client *client) override
	{
		client->setFatalError("[SSCSM] " + reason);

		return serializeSSCSMAnswer(Answer{});
	}
};

// print(text)
// FIXME: override global loggers to use this in sscsm process
struct SSCSMRequestPrint final : public ISSCSMRequest
{
	struct Answer final : public ISSCSMAnswer
	{
	};

	std::string text;

	SerializedSSCSMAnswer exec(Client *client) override
	{
		rawstream << text << std::endl;

		return serializeSSCSMAnswer(Answer{});
	}
};

// core.log(level, text)
// FIXME: override global loggers to use this in sscsm process
struct SSCSMRequestLog final : public ISSCSMRequest
{
	struct Answer final : public ISSCSMAnswer
	{
	};

	std::string text;
	LogLevel level;

	SerializedSSCSMAnswer exec(Client *client) override
	{
		if (level >= LL_MAX) {
			throw MisbehavedSSCSMException("Tried to log at non-existent level.");
		} else {
			g_logger.log(level, text);
		}

		return serializeSSCSMAnswer(Answer{});
	}
};

// core.get_node(pos)
struct SSCSMRequestGetNode final : public ISSCSMRequest
{
	struct Answer final : public ISSCSMAnswer
	{
		MapNode node;
		bool is_pos_ok;
	};

	v3s16 pos;

	SerializedSSCSMAnswer exec(Client *client) override
	{
		bool is_pos_ok = false;
		MapNode node = client->getEnv().getMap().getNode(pos, &is_pos_ok);

		Answer answer{};
		answer.node = node;
		answer.is_pos_ok = is_pos_ok;
		return serializeSSCSMAnswer(std::move(answer));
	}
};
