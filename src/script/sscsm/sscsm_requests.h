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
#include "itemdef.h"
#include "nodedef.h"

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

// Some error occurred in the SSCSM env
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

// Fetch all currently known item and node definitions, to populate
// core.registered_items/_nodes/_tools/_craftitems on the SSCSM side.
// TODO: aliases and the content-id name mapping are not sent yet.
struct SSCSMRequestGetItemDefs final : public ISSCSMRequest
{
	struct Answer final : public ISSCSMAnswer
	{
		std::vector<ItemDefinition> items;
		std::vector<ContentFeatures> nodes;
	};

	SerializedSSCSMAnswer exec(Client *client) override
	{
		Answer answer{};

		std::set<std::string> item_names;
		client->idef()->getAll(item_names);
		answer.items.reserve(item_names.size());
		for (const std::string &name : item_names)
			answer.items.push_back(client->idef()->get(name));

		const NodeDefManager *ndef = client->ndef();
		for (u32 c = 0; c < ndef->size(); c++) {
			const ContentFeatures &cf = ndef->get(c);
			if (cf.name.empty())
				continue; // unregistered/removed id
			answer.nodes.push_back(cf);
			// null visuals since they're not thread safe to copy
			answer.nodes.back().visuals = nullptr;
		}

		return serializeSSCSMAnswer(std::move(answer));
	}
};
