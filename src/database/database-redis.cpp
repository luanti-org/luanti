// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2014 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "config.h"

#if USE_REDIS

#include "database-redis.h"

#include "settings.h"
#include "log.h"
#include "exceptions.h"
#include "irrlicht_changes/printing.h"
#include "util/string.h"

#include <hiredis.h>
#include <cassert>

/*
 * Redis is not a good fit for Minetest and only still supported for legacy as
 * well as advanced use case reasons, see:
 * <https://github.com/luanti-org/luanti/issues/14822>
 *
 * Do NOT extend this backend with any new functionality.
 */

Database_Redis::Database_Redis(Settings &conf)
{
	std::string tmp;
	try {
		tmp = conf.get("redis_address");
		hash = conf.get("redis_hash");
	} catch (SettingNotFoundException &) {
		throw SettingNotFoundException("Set redis_address and "
			"redis_hash in world.mt to use the redis backend");
	}
	const char *addr = tmp.c_str();
	int port = conf.exists("redis_port") ? conf.getU16("redis_port") : 6379;
	// if redis_address contains '/' assume unix socket, else hostname/ip
	ctx = tmp.find('/') != std::string::npos ? redisConnectUnix(addr) : redisConnect(addr, port);
	if (!ctx) {
		throw DatabaseException("Cannot allocate redis context");
	} else if (ctx->err) {
		std::string err = std::string("Connection error: ") + ctx->errstr;
		redisFree(ctx);
		throw DatabaseException(err);
	}
	if (conf.exists("redis_password")) {
		tmp = conf.get("redis_password");
		redisReply *reply = static_cast<redisReply *>(redisCommand(ctx, "AUTH %s", tmp.c_str()));
		if (!reply)
			throw DatabaseException("Redis authentication failed");
		if (reply->type == REDIS_REPLY_ERROR) {
			std::string err = "Redis authentication failed: " + std::string(reply->str, reply->len);
			freeReplyObject(reply);
			throw DatabaseException(err);
		}
		freeReplyObject(reply);
	}

	dstream << "Note: When storing data in Redis you need to ensure that eviction"
		" is disabled, or you risk DATA LOSS." << std::endl;
}

Database_Redis::~Database_Redis()
{
	redisFree(ctx);
}

void Database_Redis::beginSave()
{
	redisReply *reply = static_cast<redisReply *>(redisCommand(ctx, "MULTI"));
	if (!reply) {
		throw DatabaseException(std::string(
			"Redis command 'MULTI' failed: ") + ctx->errstr);
	}
	freeReplyObject(reply);
}

void Database_Redis::endSave()
{
	redisReply *reply = static_cast<redisReply *>(redisCommand(ctx, "EXEC"));
	if (!reply) {
		throw DatabaseException(std::string(
			"Redis command 'EXEC' failed: ") + ctx->errstr);
	}
	freeReplyObject(reply);
}

bool Database_Redis::saveBlock(const v3s16 &pos, std::string_view data)
{
	std::string tmp = i64tos(getBlockAsInteger(pos));

	redisReply *reply = static_cast<redisReply *>(redisCommand(ctx, "HSET %s %s %b",
			hash.c_str(), tmp.c_str(), data.data(), data.size()));
	if (!reply) {
		warningstream << "saveBlock: redis command 'HSET' failed on "
			"block " << pos << ": " << ctx->errstr << std::endl;
		freeReplyObject(reply);
		return false;
	}

	if (reply->type == REDIS_REPLY_ERROR) {
		warningstream << "saveBlock: saving block " << pos
			<< " failed: " << std::string(reply->str, reply->len) << std::endl;
		freeReplyObject(reply);
		return false;
	}

	freeReplyObject(reply);
	return true;
}

void Database_Redis::loadBlock(const v3s16 &pos, std::string *block)
{
	std::string tmp = i64tos(getBlockAsInteger(pos));
	redisReply *reply = static_cast<redisReply *>(redisCommand(ctx,
			"HGET %s %s", hash.c_str(), tmp.c_str()));

	if (!reply) {
		throw DatabaseException(std::string(
			"Redis command 'HGET %s %s' failed: ") + ctx->errstr);
	}

	switch (reply->type) {
	case REDIS_REPLY_STRING: {
		block->assign(reply->str, reply->len);
		freeReplyObject(reply);
		return;
	}
	case REDIS_REPLY_ERROR: {
		std::string errstr(reply->str, reply->len);
		freeReplyObject(reply);
		errorstream << "loadBlock: loading block " << pos
			<< " failed: " << errstr << std::endl;
		throw DatabaseException(std::string(
			"Redis command 'HGET %s %s' errored: ") + errstr);
	}
	case REDIS_REPLY_NIL: {
		block->clear();
		freeReplyObject(reply);
		return;
	}
	}

	errorstream << "loadBlock: loading block " << pos
		<< " returned invalid reply type " << reply->type
		<< ": " << std::string(reply->str, reply->len) << std::endl;
	freeReplyObject(reply);
	throw DatabaseException(std::string(
		"Redis command 'HGET %s %s' gave invalid reply."));
}

bool Database_Redis::deleteBlock(const v3s16 &pos)
{
	std::string tmp = i64tos(getBlockAsInteger(pos));

	redisReply *reply = static_cast<redisReply *>(redisCommand(ctx,
		"HDEL %s %s", hash.c_str(), tmp.c_str()));
	if (!reply) {
		throw DatabaseException(std::string(
			"Redis command 'HDEL %s %s' failed: ") + ctx->errstr);
	} else if (reply->type == REDIS_REPLY_ERROR) {
		warningstream << "deleteBlock: deleting block " << pos
			<< " failed: " << std::string(reply->str, reply->len) << std::endl;
		freeReplyObject(reply);
		return false;
	}

	freeReplyObject(reply);
	return true;
}

void Database_Redis::listAllLoadableBlocks(std::vector<v3s16> &dst)
{
	redisReply *reply = static_cast<redisReply *>(redisCommand(ctx, "HKEYS %s", hash.c_str()));
	if (!reply) {
		throw DatabaseException(std::string(
			"Redis command 'HKEYS %s' failed: ") + ctx->errstr);
	}
	switch (reply->type) {
	case REDIS_REPLY_ARRAY:
		dst.reserve(reply->elements);
		for (size_t i = 0; i < reply->elements; i++) {
			assert(reply->element[i]->type == REDIS_REPLY_STRING);
			dst.push_back(getIntegerAsBlock(stoi64(reply->element[i]->str)));
		}
		break;
	case REDIS_REPLY_ERROR:
		throw DatabaseException(std::string(
			"Failed to get keys from database: ") +
			std::string(reply->str, reply->len));
	}
	freeReplyObject(reply);
}

#endif // USE_REDIS

