// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2025 Marius Spix <marius.spix@web.de>

#include "config.h"

#if USE_TKRZW

#include "database-tkrzw.h"
#include "filesys.h"
#include "settings.h"
#include "exceptions.h"
#include "remoteplayer.h"
#include "server/player_sao.h"
#include "util/serialize.h"
#include "util/string.h"
#include <set>
#include <sstream>

template<typename T>
void DatabaseTkrzwBase<T>::openDBM(const std::string &file)
{
    openDBM(tag<T>{}, file);
}

template<typename T>
DatabaseTkrzwBase<T>::~DatabaseTkrzwBase()
{
	if (dbm) {
		dbm->Close();
		delete dbm;
	}
}

template<>
void DatabaseTkrzwBase<tkrzw::HashDBM>::openDBM(tag<tkrzw::HashDBM>, const std::string &file)
{
	dbm = new tkrzw::HashDBM();
	tkrzw::HashDBM::TuningParameters tuning;
	tuning.num_buckets = 1048576;
	tuning.align_pow = 8;
	tuning.cache_buckets = true;

	auto status = dbm->OpenAdvanced(file, true, tkrzw::File::OPEN_DEFAULT, tuning);
	if (!status.IsOK())
		throw DatabaseException("Tkrzw HashDBM open failed: " + tkrzw::ToString(status));
}

template<>
void DatabaseTkrzwBase<tkrzw::TreeDBM>::openDBM(tag<tkrzw::TreeDBM>, const std::string &file)
{
	dbm = new tkrzw::TreeDBM();
	tkrzw::TreeDBM::TuningParameters tuning;
	tuning.max_page_size = 8192;

	auto status = dbm->OpenAdvanced(file, true, tkrzw::File::OPEN_DEFAULT, tuning);
	if (!status.IsOK())
		throw DatabaseException("Tkrzw TreeDBM open failed: " + tkrzw::ToString(status));
}

/*MapDatabase*/
MapDatabaseTkrzw::MapDatabaseTkrzw(const std::string &file)
{
    openDBM(file + DIR_DELIM + "map.tkh");
}
MapDatabaseTkrzw::~MapDatabaseTkrzw() { }

void MapDatabaseTkrzw::beginSave() { }
void MapDatabaseTkrzw::endSave() { dbm->Synchronize(true); }

bool MapDatabaseTkrzw::saveBlock(const v3s16 &pos, std::string_view data)
{
    std::string key = i64tos(getBlockAsInteger(pos));
	auto status = dbm->Set(key, data);
	return status.IsOK();
}

void MapDatabaseTkrzw::loadBlock(const v3s16 &pos, std::string *block)
{
    std::string key = i64tos(getBlockAsInteger(pos));
	std::string value;
	auto status = dbm->Get(key, &value);
	if (status.IsOK())
		block->assign(value);
	else
		block->clear();
}

bool MapDatabaseTkrzw::deleteBlock(const v3s16 &pos)
{
    std::string key = i64tos(getBlockAsInteger(pos));
	dbm->Remove(key);
	return true;
}

void MapDatabaseTkrzw::listAllLoadableBlocks(std::vector<v3s16> &dst)
{
    auto iter = dbm->MakeIterator();
	iter->First();
	std::string key;
	while (iter->Get(&key, nullptr).IsOK()) {
		dst.push_back(getIntegerAsBlock(stoi64(key)));
		iter->Next();
	}
}

/* PlayerDatabase */
PlayerDatabaseTkrzw::PlayerDatabaseTkrzw(const std::string &file)
{
    openDBM(file + DIR_DELIM + "players.tkh");
}
PlayerDatabaseTkrzw::~PlayerDatabaseTkrzw() { }

void PlayerDatabaseTkrzw::savePlayer(RemotePlayer *player)
{
    PlayerSAO *sao = player->getPlayerSAO();
	sanity_check(sao);

	std::ostringstream os(std::ios_base::binary);

	writeU8(os, 1); // version
	writeU16(os, sao->getHP());
	writeV3F32(os, sao->getBasePosition());
	writeF32(os, sao->getLookPitch());
	writeF32(os, sao->getRotation().Y);
	writeU16(os, sao->getBreath());

	// Meta (attributes)
	const auto &stringvars = sao->getMeta().getStrings();
	writeU32(os, stringvars.size());
	for (const auto &it : stringvars) {
		os << serializeString16(it.first);
		os << serializeString32(it.second);
	}

	// Inventory
	player->inventory.serialize(os);

	dbm->Set(player->getName(), os.str());
	dbm->Synchronize(false);

	player->onSuccessfulSave();
}


bool PlayerDatabaseTkrzw::loadPlayer(RemotePlayer *player, PlayerSAO *sao)
{
    std::string raw;
	auto status = dbm->Get(player->getName(), &raw);
	if (!status.IsOK())
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

	// Inventory
	try {
		player->inventory.deSerialize(is);
	} catch (SerializationError &e) {
		errorstream << "Failed to deserialize player inventory. player_name="
			<< player->getName() << " " << e.what() << std::endl;
	}

	return true;
}

bool PlayerDatabaseTkrzw::removePlayer(const std::string &name)
{
    dbm->Remove(name);
	return true;
}

void PlayerDatabaseTkrzw::listPlayers(std::vector<std::string> &res)
{
    auto iter = dbm->MakeIterator();
	iter->First();
	std::string key;
	while (iter->Get(&key, nullptr).IsOK()) {
		res.push_back(key);
		iter->Next();
	}
}

/* AuthDatabase */
AuthDatabaseTkrzw::AuthDatabaseTkrzw(const std::string &file)
{
    openDBM(file + DIR_DELIM + "auth.tkh");
}
AuthDatabaseTkrzw::~AuthDatabaseTkrzw() { }

bool AuthDatabaseTkrzw::getAuth(const std::string &name, AuthEntry &res)
{
    std::string raw;
	auto status = dbm->Get(name, &raw);
	if (!status.IsOK())
		return false;

	std::istringstream is(raw, std::ios_base::binary);

	if (readU8(is) > 1)
		return false;

	res.id = 1;
	res.name = name;
	res.password = deSerializeString16(is);

	u16 privilege_count = readU16(is);
	res.privileges.clear();
	res.privileges.reserve(privilege_count);
	for (u16 i = 0; i < privilege_count; i++)
		res.privileges.push_back(deSerializeString16(is));

	res.last_login = readS64(is);

	return true;
}

bool AuthDatabaseTkrzw::saveAuth(const AuthEntry &authEntry)
{
    std::ostringstream os(std::ios_base::binary);
	writeU8(os, 1); // version
	os << serializeString16(authEntry.password);

	size_t privilege_count = authEntry.privileges.size();
	FATAL_ERROR_IF(privilege_count > U16_MAX,
		"Unsupported number of privileges");
	writeU16(os, privilege_count);
	for (const std::string &priv : authEntry.privileges)
		os << serializeString16(priv);

	writeS64(os, authEntry.last_login);

	dbm->Set(authEntry.name, os.str());
	dbm->Synchronize(false);
	return true;
}

bool AuthDatabaseTkrzw::createAuth(AuthEntry &authEntry)
{
    return saveAuth(authEntry);
}

bool AuthDatabaseTkrzw::deleteAuth(const std::string &name)
{
    dbm->Remove(name);
	return true;
}

void AuthDatabaseTkrzw::listNames(std::vector<std::string> &res)
{
    auto iter = dbm->MakeIterator();
	iter->First();
	std::string key;
	while (iter->Get(&key, nullptr).IsOK()) {
		res.push_back(key);
		iter->Next();
	}
}

/* ModStorageDatabase */
ModStorageDatabaseTkrzw::ModStorageDatabaseTkrzw(const std::string &file)
{
    openDBM(file + DIR_DELIM + "mod_storage.tkt");
}
ModStorageDatabaseTkrzw::~ModStorageDatabaseTkrzw() { }

void ModStorageDatabaseTkrzw::getModEntries(const std::string &modname, StringMap *storage)
{
    auto iter = dbm->MakeIterator();
	iter->Jump(modname + ":");
	std::string key, value;
	while (iter->Get(&key, &value).IsOK()) {
		if (!str_starts_with(key, modname + ":"))
			break;
		(*storage)[key.substr(modname.size() + 1)] = value;
		iter->Next();
	}
}

void ModStorageDatabaseTkrzw::getModKeys(const std::string &modname, std::vector<std::string> *storage)
{
    auto iter = dbm->MakeIterator();
	std::string prefix = modname + ":";
	iter->Jump(prefix);

	std::string key;
	while (iter->Get(&key, nullptr).IsOK()) {
		if (!str_starts_with(key, prefix))
			break;
		storage->emplace_back(key.substr(prefix.size()));
		iter->Next();
	}
}

bool ModStorageDatabaseTkrzw::hasModEntry(const std::string &modname, const std::string &key)
{
    std::string value;
	auto status = dbm->Get(modname + ":" + key, &value);
    return status.IsOK();
}

bool ModStorageDatabaseTkrzw::getModEntry(const std::string &modname, const std::string &key, std::string *value)
{
    auto status = dbm->Get(modname + ":" + key, value);
	return status.IsOK();
}

bool ModStorageDatabaseTkrzw::setModEntry(const std::string &modname, const std::string &key, std::string_view value)
{
    dbm->Set(modname + ":" + key, value);
	dbm->Synchronize(false);
	return true;
}

bool ModStorageDatabaseTkrzw::removeModEntry(const std::string &modname, const std::string &key)
{
    dbm->Remove(modname + ":" + key);
	return true;
}

bool ModStorageDatabaseTkrzw::removeModEntries(const std::string &modname)
{
    auto iter = dbm->MakeIterator();
	iter->Jump(modname + ":");
	std::string key;
	while (iter->Get(&key, nullptr).IsOK()) {
		if (!str_starts_with(key, modname + ":"))
			break;
		dbm->Remove(key);
		iter->Next();
	}
	return true;
}


void ModStorageDatabaseTkrzw::listMods(std::vector<std::string> *res)
{
    std::set<std::string> mods;

    auto iter = dbm->MakeIterator();
	iter->First();
	std::string key;
	while (iter->Get(&key, nullptr).IsOK()) {
		auto pos = key.find(':');
		if (pos != std::string::npos)
			mods.insert(key.substr(0, pos));
		iter->Next();
	}

	res->insert(res->end(), mods.begin(), mods.end());
}

#endif // USE_TKRZW

