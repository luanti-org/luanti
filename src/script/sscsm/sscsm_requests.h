// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "sscsm_irequest.h"
#include "sscsm_ievent.h"
#include "sscsm_serialize.h"
#include "mapnode.h"
#include "map.h"
#include "chatmessage.h"
#include "client/client.h"
#include "log_internal.h"
#include "util/serialize.h"
#include "util/string.h"

// Poll the next event (e.g. on_globalstep)
struct SSCSMRequestPollNextEvent final : public ISSCSMRequest
{
	static constexpr SSCSMRequestType TYPE = SSCSMRequestType::PollNextEvent;

	struct Answer final : public ISSCSMAnswer
	{
		std::unique_ptr<ISSCSMEvent> next_event;

		void serializeBody(std::ostream &os) const;
		static Answer deserializeBody(std::istream &is);
	};

	SSCSMRequestType getType() const override { return TYPE; }
	void serializeBody(std::ostream &os) const override {}
	static SSCSMRequestPollNextEvent deserializeBody(std::istream &is)
	{
		return SSCSMRequestPollNextEvent{};
	}

	SerializedSSCSMAnswer exec(Client *client) override
	{
		FATAL_ERROR("SSCSMRequestPollNextEvent needs to be handled by SSCSMControler::runEvent()");
	}
};

// Some error occured in the SSCSM env
struct SSCSMRequestSetFatalError final : public ISSCSMRequest
{
	static constexpr SSCSMRequestType TYPE = SSCSMRequestType::SetFatalError;

	struct Answer final : public ISSCSMAnswer
	{
		void serializeBody(std::ostream &os) const {}
		static Answer deserializeBody(std::istream &is) { return Answer{}; }
	};

	std::string reason;

	SSCSMRequestType getType() const override { return TYPE; }
	void serializeBody(std::ostream &os) const override
	{
		os << serializeString32(reason);
	}
	static SSCSMRequestSetFatalError deserializeBody(std::istream &is)
	{
		SSCSMRequestSetFatalError r;
		r.reason = deSerializeString32(is);
		return r;
	}

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
	static constexpr SSCSMRequestType TYPE = SSCSMRequestType::Print;

	struct Answer final : public ISSCSMAnswer
	{
		void serializeBody(std::ostream &os) const {}
		static Answer deserializeBody(std::istream &is) { return Answer{}; }
	};

	std::string text;

	SSCSMRequestType getType() const override { return TYPE; }
	void serializeBody(std::ostream &os) const override
	{
		os << serializeString32(text);
	}
	static SSCSMRequestPrint deserializeBody(std::istream &is)
	{
		SSCSMRequestPrint r;
		r.text = deSerializeString32(is);
		return r;
	}

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
	static constexpr SSCSMRequestType TYPE = SSCSMRequestType::Log;

	struct Answer final : public ISSCSMAnswer
	{
		void serializeBody(std::ostream &os) const {}
		static Answer deserializeBody(std::istream &is) { return Answer{}; }
	};

	std::string text;
	LogLevel level;

	SSCSMRequestType getType() const override { return TYPE; }
	void serializeBody(std::ostream &os) const override
	{
		writeU8(os, static_cast<u8>(level));
		os << serializeString32(text);
	}
	static SSCSMRequestLog deserializeBody(std::istream &is)
	{
		SSCSMRequestLog r;
		r.level = static_cast<LogLevel>(readU8(is));
		r.text = deSerializeString32(is);
		return r;
	}

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

// core.display_chat_message(text)
struct SSCSMRequestDisplayChatMessage final : public ISSCSMRequest
{
	static constexpr SSCSMRequestType TYPE = SSCSMRequestType::DisplayChatMessage;

	struct Answer final : public ISSCSMAnswer
	{
		void serializeBody(std::ostream &os) const {}
		static Answer deserializeBody(std::istream &is) { return Answer{}; }
	};

	std::string text;

	SSCSMRequestType getType() const override { return TYPE; }
	void serializeBody(std::ostream &os) const override
	{
		os << serializeString32(text);
	}
	static SSCSMRequestDisplayChatMessage deserializeBody(std::istream &is)
	{
		SSCSMRequestDisplayChatMessage r;
		r.text = deSerializeString32(is);
		return r;
	}

	SerializedSSCSMAnswer exec(Client *client) override
	{
		client->pushToChatQueue(new ChatMessage(utf8_to_wide(text)));
		return serializeSSCSMAnswer(Answer{});
	}
};

// core.get_node(pos)
struct SSCSMRequestGetNode final : public ISSCSMRequest
{
	static constexpr SSCSMRequestType TYPE = SSCSMRequestType::GetNode;

	struct Answer final : public ISSCSMAnswer
	{
		MapNode node;
		bool is_pos_ok;

		void serializeBody(std::ostream &os) const
		{
			writeU16(os, node.param0);
			writeU8(os, node.param1);
			writeU8(os, node.param2);
			writeU8(os, is_pos_ok ? 1 : 0);
		}
		static Answer deserializeBody(std::istream &is)
		{
			Answer a;
			a.node.param0 = readU16(is);
			a.node.param1 = readU8(is);
			a.node.param2 = readU8(is);
			a.is_pos_ok = readU8(is) != 0;
			return a;
		}
	};

	v3s16 pos;

	SSCSMRequestType getType() const override { return TYPE; }
	void serializeBody(std::ostream &os) const override
	{
		writeV3S16(os, pos);
	}
	static SSCSMRequestGetNode deserializeBody(std::istream &is)
	{
		SSCSMRequestGetNode r;
		r.pos = readV3S16(is);
		return r;
	}

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
