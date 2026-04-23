// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "sscsm_ievent.h"
#include "sscsm_serialize.h"
#include "debug.h"
#include "irrlichttypes.h"
#include "sscsm_environment.h"
#include "util/serialize.h"

struct SSCSMEventTearDown final : public ISSCSMEvent
{
	static constexpr SSCSMEventType TYPE = SSCSMEventType::TearDown;

	SSCSMEventType getType() const override { return TYPE; }
	void serializeBody(std::ostream &os) const override {}
	static SSCSMEventTearDown deserializeBody(std::istream &is)
	{
		return SSCSMEventTearDown{};
	}

	void exec(SSCSMEnvironment *env) override
	{
		FATAL_ERROR("SSCSMEventTearDown needs to be handled by SSCSMEnvironment::run()");
	}
};

struct SSCSMEventUpdateVFSFiles final : public ISSCSMEvent
{
	static constexpr SSCSMEventType TYPE = SSCSMEventType::UpdateVFSFiles;

	// pairs are virtual path and file content
	std::vector<std::pair<std::string, std::string>> files;

	SSCSMEventType getType() const override { return TYPE; }
	void serializeBody(std::ostream &os) const override
	{
		writeU32(os, static_cast<u32>(files.size()));
		for (const auto &p : files) {
			os << serializeString32(p.first);
			os << serializeString32(p.second);
		}
	}
	static SSCSMEventUpdateVFSFiles deserializeBody(std::istream &is)
	{
		SSCSMEventUpdateVFSFiles e;
		u32 n = readU32(is);
		e.files.reserve(n);
		for (u32 i = 0; i < n; ++i) {
			std::string vpath = deSerializeString32(is);
			std::string content = deSerializeString32(is);
			e.files.emplace_back(std::move(vpath), std::move(content));
		}
		return e;
	}

	void exec(SSCSMEnvironment *env) override
	{
		env->updateVFSFiles(std::move(files));
	}
};

struct SSCSMEventLoadMods final : public ISSCSMEvent
{
	static constexpr SSCSMEventType TYPE = SSCSMEventType::LoadMods;

	// modnames and paths to init.lua file, in load order
	std::vector<std::pair<std::string, std::string>> mods;

	SSCSMEventType getType() const override { return TYPE; }
	void serializeBody(std::ostream &os) const override
	{
		writeU32(os, static_cast<u32>(mods.size()));
		for (const auto &p : mods) {
			os << serializeString16(p.first);
			os << serializeString16(p.second);
		}
	}
	static SSCSMEventLoadMods deserializeBody(std::istream &is)
	{
		SSCSMEventLoadMods e;
		u32 n = readU32(is);
		e.mods.reserve(n);
		for (u32 i = 0; i < n; ++i) {
			std::string name = deSerializeString16(is);
			std::string path = deSerializeString16(is);
			e.mods.emplace_back(std::move(name), std::move(path));
		}
		return e;
	}

	void exec(SSCSMEnvironment *env) override
	{
		env->getScript()->load_mods(mods);
	}
};

struct SSCSMEventOnStep final : public ISSCSMEvent
{
	static constexpr SSCSMEventType TYPE = SSCSMEventType::OnStep;

	f32 dtime;

	SSCSMEventType getType() const override { return TYPE; }
	void serializeBody(std::ostream &os) const override
	{
		writeF32(os, dtime);
	}
	static SSCSMEventOnStep deserializeBody(std::istream &is)
	{
		SSCSMEventOnStep e;
		e.dtime = readF32(is);
		return e;
	}

	void exec(SSCSMEnvironment *env) override
	{
		env->getScript()->environment_step(dtime);
	}
};

struct SSCSMEventUpdateContentDefs final : public ISSCSMEvent
{
	static constexpr SSCSMEventType TYPE = SSCSMEventType::UpdateContentDefs;

	// pairs of (content_id, name) for all known nodes
	std::vector<std::pair<u16, std::string>> defs;

	SSCSMEventType getType() const override { return TYPE; }
	void serializeBody(std::ostream &os) const override
	{
		writeU32(os, static_cast<u32>(defs.size()));
		for (const auto &p : defs) {
			writeU16(os, p.first);
			os << serializeString16(p.second);
		}
	}
	static SSCSMEventUpdateContentDefs deserializeBody(std::istream &is)
	{
		SSCSMEventUpdateContentDefs e;
		u32 n = readU32(is);
		e.defs.reserve(n);
		for (u32 i = 0; i < n; ++i) {
			u16 id = readU16(is);
			std::string name = deSerializeString16(is);
			e.defs.emplace_back(id, std::move(name));
		}
		return e;
	}

	void exec(SSCSMEnvironment *env) override
	{
		env->getScript()->set_content_defs(defs);
	}
};

struct SSCSMEventOnModChannelMessage final : public ISSCSMEvent
{
	static constexpr SSCSMEventType TYPE = SSCSMEventType::OnModChannelMessage;

	std::string channel;
	std::string sender;
	std::string message;

	SSCSMEventType getType() const override { return TYPE; }
	void serializeBody(std::ostream &os) const override
	{
		os << serializeString16(channel);
		os << serializeString16(sender);
		os << serializeString32(message);
	}
	static SSCSMEventOnModChannelMessage deserializeBody(std::istream &is)
	{
		SSCSMEventOnModChannelMessage e;
		e.channel = deSerializeString16(is);
		e.sender = deSerializeString16(is);
		e.message = deSerializeString32(is);
		return e;
	}

	void exec(SSCSMEnvironment *env) override
	{
		env->getScript()->on_modchannel_message(channel, sender, message);
	}
};

struct SSCSMEventOnModChannelSignal final : public ISSCSMEvent
{
	static constexpr SSCSMEventType TYPE = SSCSMEventType::OnModChannelSignal;

	std::string channel;
	u8 signal;

	SSCSMEventType getType() const override { return TYPE; }
	void serializeBody(std::ostream &os) const override
	{
		os << serializeString16(channel);
		writeU8(os, signal);
	}
	static SSCSMEventOnModChannelSignal deserializeBody(std::istream &is)
	{
		SSCSMEventOnModChannelSignal e;
		e.channel = deSerializeString16(is);
		e.signal = readU8(is);
		return e;
	}

	void exec(SSCSMEnvironment *env) override
	{
		env->getScript()->on_modchannel_signal(channel, signal);
	}
};

