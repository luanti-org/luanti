// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "sscsm_irequest.h"
#include "sscsm_ievent.h"
#include "mapnode.h"
#include "map.h"
#include "client/client.h"
#include "client/localplayer.h"
#include "hud_element.h"
#include "log_internal.h"
#include "script/common/c_content.h"
#include <optional>

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

// core.hud_add(form)
struct SSCSMRequestHudAdd final : public ISSCSMRequest
{
	struct Answer final : public ISSCSMAnswer
	{
		u32 id = 0;
	};

	HudElement elem;

	SerializedSSCSMAnswer exec(Client *client) override
	{
		LocalPlayer *player = client->getEnv().getLocalPlayer();
		u32 id = player->csm_hud.add(std::make_unique<HudElement>(std::move(elem)));

		Answer answer;
		answer.id = id;
		return serializeSSCSMAnswer(std::move(answer));
	}
};

// core.hud_remove(id)
struct SSCSMRequestHudRemove final : public ISSCSMRequest
{
	struct Answer final : public ISSCSMAnswer
	{
		bool ok = false;
	};

	u32 id;

	SerializedSSCSMAnswer exec(Client *client) override
	{
		LocalPlayer *player = client->getEnv().getLocalPlayer();

		Answer answer;
		answer.ok = player->csm_hud.remove(id);
		return serializeSSCSMAnswer(std::move(answer));
	}
};

// core.hud_get(id), and step 1 of core.hud_change(id, stat, data)
struct SSCSMRequestHudGet final : public ISSCSMRequest
{
	struct Answer final : public ISSCSMAnswer
	{
		bool found = false;
		HudElement elem;
	};

	u32 id;

	SerializedSSCSMAnswer exec(Client *client) override
	{
		LocalPlayer *player = client->getEnv().getLocalPlayer();
		HudElement *e = player->csm_hud.get(id);

		Answer answer;
		answer.found = (e != nullptr);
		if (e)
			answer.elem = *e;
		return serializeSSCSMAnswer(std::move(answer));
	}
};

// core.hud_get_all()
struct SSCSMRequestHudGetAll final : public ISSCSMRequest
{
	struct Answer final : public ISSCSMAnswer
	{
		std::vector<std::pair<u32, HudElement>> elems;
	};

	SerializedSSCSMAnswer exec(Client *client) override
	{
		LocalPlayer *player = client->getEnv().getLocalPlayer();

		Answer answer;
		const auto &elements = player->csm_hud.getElements();
		for (u32 id = 0; id < elements.size(); id++) {
			if (elements[id])
				answer.elems.emplace_back(id, *elements[id]);
		}
		return serializeSSCSMAnswer(std::move(answer));
	}
};

// Step 2 of core.hud_change: apply the stat computed on the SSCSM thread
// via read_hud_change against a scratch element onto the main-thread element.
struct SSCSMRequestHudChange final : public ISSCSMRequest
{
	struct Answer final : public ISSCSMAnswer
	{
		bool ok = false;
	};

	u32 id;
	HudElementStat stat;
	HudElement value;

	SerializedSSCSMAnswer exec(Client *client) override
	{
		LocalPlayer *player = client->getEnv().getLocalPlayer();
		HudElement *elem = player->csm_hud.get(id);

		Answer answer;
		answer.ok = (elem != nullptr);
		if (elem)
			copy_hud_stat(stat, value, elem);
		return serializeSSCSMAnswer(std::move(answer));
	}
};
