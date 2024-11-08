// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "config.h"

#if USE_LEVELDB

#include "database-leveldb.h"

#include "log.h"
#include "filesys.h"
#include "exceptions.h"
#include "remoteplayer.h"
#include "irrlicht_changes/printing.h"
#include "server/player_sao.h"
#include "util/serialize.h"
#include "util/string.h"

#include "leveldb/db.h"


#define ENSURE_STATUS_OK(s) \
	if (!(s).ok()) { \
		throw DatabaseException(std::string("LevelDB error: ") + \
				(s).ToString()); \
	}


Database_LevelDB::Database_LevelDB(const std::string &savedir)
{
	leveldb::Options options;
	options.create_if_missing = true;
	leveldb::DB *db;
	leveldb::Status status = leveldb::DB::Open(options,
		savedir + DIR_DELIM + "map.db", &db);
	ENSURE_STATUS_OK(status);
	m_database.reset(db);
}

bool Database_LevelDB::saveBlock(const v3s16 &pos, std::string_view data)
{
	leveldb::Slice data_s(data.data(), data.size());
	leveldb::Status status = m_database->Put(leveldb::WriteOptions(),
			i64tos(getBlockAsInteger(pos)), data_s);
	if (!status.ok()) {
		warningstream << "saveBlock: LevelDB error saving block "
			<< pos << ": " << status.ToString() << std::endl;
		return false;
	}

	return true;
}

void Database_LevelDB::loadBlock(const v3s16 &pos, std::string *block)
{
	leveldb::Status status = m_database->Get(leveldb::ReadOptions(),
		i64tos(getBlockAsInteger(pos)), block);

	if (!status.ok())
		block->clear();
}

bool Database_LevelDB::deleteBlock(const v3s16 &pos)
{
	leveldb::Status status = m_database->Delete(leveldb::WriteOptions(),
			i64tos(getBlockAsInteger(pos)));
	if (!status.ok()) {
		warningstream << "deleteBlock: LevelDB error deleting block "
			<< pos << ": " << status.ToString() << std::endl;
		return false;
	}

	return true;
}

void Database_LevelDB::listAllLoadableBlocks(std::vector<v3s16> &dst)
{
	std::unique_ptr<leveldb::Iterator> it(m_database->NewIterator(leveldb::ReadOptions()));
	for (it->SeekToFirst(); it->Valid(); it->Next()) {
		dst.push_back(getIntegerAsBlock(stoi64(it->key().ToString())));
	}
	ENSURE_STATUS_OK(it->status());  // Check for any errors found during the scan
}

PlayerDatabaseLevelDB::PlayerDatabaseLevelDB(const std::string &savedir)
{
	leveldb::Options options;
	options.create_if_missing = true;
	leveldb::DB *db;
	leveldb::Status status = leveldb::DB::Open(options,
		savedir + DIR_DELIM + "players.db", &db);
	ENSURE_STATUS_OK(status);
	m_database.reset(db);
}

void PlayerDatabaseLevelDB::savePlayer(RemotePlayer *player)
{
	/*
	u8 version = 1
	u16 hp
	v3f position
	f32 pitch
	f32 yaw
	u16 breath
	u32 attribute_count
	for each attribute {
		std::string name
		std::string (long) value
	}
	std::string (long) serialized_inventory
	*/

	std::ostringstream os(std::ios_base::binary);
	writeU8(os, 1);

	PlayerSAO *sao = player->getPlayerSAO();
	sanity_check(sao);
	writeU16(os, sao->getHP());
	writeV3F32(os, sao->getBasePosition());
	writeF32(os, sao->getLookPitch());
	writeF32(os, sao->getRotation().Y);
	writeU16(os, sao->getBreath());

	const auto &stringvars = sao->getMeta().getStrings();
	writeU32(os, stringvars.size());
	for (const auto &it : stringvars) {
		os << serializeString16(it.first);
		os << serializeString32(it.second);
	}

	player->inventory.serialize(os);

	leveldb::Status status = m_database->Put(leveldb::WriteOptions(),
		player->getName(), os.str());
	ENSURE_STATUS_OK(status);
	player->onSuccessfulSave();
}

bool PlayerDatabaseLevelDB::removePlayer(const std::string &name)
{
	leveldb::Status s = m_database->Delete(leveldb::WriteOptions(), name);
	return s.ok();
}

bool PlayerDatabaseLevelDB::loadPlayer(RemotePlayer *player, PlayerSAO *sao)
{
	std::string raw;
	leveldb::Status s = m_database->Get(leveldb::ReadOptions(),
		player->getName(), &raw);
	if (!s.ok())
		return false;
	std::istringstream is(raw, std::ios_base::binary);

	if (readU8(is) > 1)
		return false;

	sao->setHPRaw(readU16(is));
	sao->setBasePosition(readV3F32(is));
	sao->setLookPitch(readF32(is));
	sao->setPlayerYaw(readF32(is));
	sao->setBreath(readU16(is), false);

	u32 attribute_count = readU32(is);
	for (u32 i = 0; i < attribute_count; i++) {
		std::string name = deSerializeString16(is);
		std::string value = deSerializeString32(is);
		sao->getMeta().setString(name, value);
	}
	sao->getMeta().setModified(false);

	// This should always be last.
	try {
		player->inventory.deSerialize(is);
	} catch (SerializationError &e) {
		errorstream << "Failed to deserialize player inventory. player_name="
			<< player->getName() << " " << e.what() << std::endl;
	}

	return true;
}

void PlayerDatabaseLevelDB::listPlayers(std::vector<std::string> &res)
{
	std::unique_ptr<leveldb::Iterator> it(m_database->NewIterator(leveldb::ReadOptions()));
	res.clear();
	for (it->SeekToFirst(); it->Valid(); it->Next()) {
		res.push_back(it->key().ToString());
	}
}

AuthDatabaseLevelDB::AuthDatabaseLevelDB(const std::string &savedir)
{
	leveldb::Options options;
	options.create_if_missing = true;
	leveldb::DB *db;
	leveldb::Status status = leveldb::DB::Open(options,
		savedir + DIR_DELIM + "auth.db", &db);
	ENSURE_STATUS_OK(status);
	m_database.reset(db);
}

bool AuthDatabaseLevelDB::getAuth(const std::string &name, AuthEntry &res)
{
	std::string raw;
	leveldb::Status s = m_database->Get(leveldb::ReadOptions(), name, &raw);
	if (!s.ok())
		return false;
	std::istringstream is(raw, std::ios_base::binary);

	/*
	u8 version = 1
	std::string password
	u16 number of privileges
	for each privilege {
		std::string privilege
	}
	s64 last_login
	*/

	if (readU8(is) > 1)
		return false;

	res.id = 1;
	res.name = name;
	res.password = deSerializeString16(is);

	u16 privilege_count = readU16(is);
	res.privileges.clear();
	res.privileges.reserve(privilege_count);
	for (u16 i = 0; i < privilege_count; i++) {
		res.privileges.push_back(deSerializeString16(is));
	}

	res.last_login = readS64(is);
	return true;
}

bool AuthDatabaseLevelDB::saveAuth(const AuthEntry &authEntry)
{
	std::ostringstream os(std::ios_base::binary);
	writeU8(os, 1);
	os << serializeString16(authEntry.password);

	size_t privilege_count = authEntry.privileges.size();
	FATAL_ERROR_IF(privilege_count > U16_MAX,
		"Unsupported number of privileges");
	writeU16(os, privilege_count);
	for (const std::string &privilege : authEntry.privileges) {
		os << serializeString16(privilege);
	}

	writeS64(os, authEntry.last_login);
	leveldb::Status s = m_database->Put(leveldb::WriteOptions(),
		authEntry.name, os.str());
	return s.ok();
}

bool AuthDatabaseLevelDB::createAuth(AuthEntry &authEntry)
{
	return saveAuth(authEntry);
}

bool AuthDatabaseLevelDB::deleteAuth(const std::string &name)
{
	leveldb::Status s = m_database->Delete(leveldb::WriteOptions(), name);
	return s.ok();
}

void AuthDatabaseLevelDB::listNames(std::vector<std::string> &res)
{
	std::unique_ptr<leveldb::Iterator> it(m_database->NewIterator(leveldb::ReadOptions()));
	res.clear();
	for (it->SeekToFirst(); it->Valid(); it->Next()) {
		res.emplace_back(it->key().ToString());
	}
}

void AuthDatabaseLevelDB::reload()
{
	// No-op for LevelDB.
}

#endif // USE_LEVELDB
