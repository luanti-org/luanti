// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "test.h"

#include <sstream>

#include "gamedef.h"
#include "inventory.h"
#include "itemdef.h"
#include "object_properties.h"
#include "activeobject.h"
#include "nodemetadata.h"
#include "exceptions.h"
#include "util/serialize.h"

class TestInventory : public TestBase {
public:
	TestInventory() { TestManager::registerTestModule(this); }
	const char *getName() { return "TestInventory"; }

	void runTests(IGameDef *gamedef);

	void testItemStack(IItemDefManager *idef);
	void testSerializeDeserialize(IItemDefManager *idef);
	void testModifiers(IItemDefManager *idef);
	void testFormats(IItemDefManager *idef);

	static const char *serialized_inventory_in;
	static const char *serialized_inventory_out;
	static const char *serialized_inventory_inc;
};

static TestInventory g_test_instance;

void TestInventory::runTests(IGameDef *gamedef)
{
	TEST(testItemStack, gamedef->getItemDefManager());
	TEST(testSerializeDeserialize, gamedef->getItemDefManager());
	TEST(testModifiers, gamedef->getItemDefManager());
	TEST(testFormats, gamedef->getItemDefManager());
}

////////////////////////////////////////////////////////////////////////////////

void TestInventory::testItemStack(IItemDefManager *idef)
{
	auto *widef = dynamic_cast<IWritableItemDefManager *>(idef);
	UASSERT(widef);

	// Craftitem / Node
	{
		ItemStack stack;
		std::istringstream is("foo:bar_baz", std::ios::binary);
		stack.deSerialize(is, idef);

		UASSERT(stack.name == "foo:bar_baz");
		UASSERT(stack.count == 1);
		UASSERT(stack.wear == 0);

		// Surplus spaces are NOT ignored. Count defaults to 1. "10" goes nowhere.
		is = std::istringstream("foo:bar_baz   10", std::ios::binary);
		stack.deSerialize(is, idef);

		UASSERT(stack.name == "foo:bar_baz");
		UASSERT(stack.count == 1);
		UASSERT(stack.wear == 0);

	}

	// Tool
	{
		ItemDefinition def;
		def.name = "foo:bar_tool";
		def.type = ItemType::ITEM_TOOL;
		widef->registerItem(def);

		ItemStack stack;
		std::istringstream is("foo:bar_tool 10 4321", std::ios::binary);
		stack.deSerialize(is, idef);

		UASSERT(stack.name == "foo:bar_tool");
		UASSERT(stack.count == 1); // tools cannot stack
		UASSERT(stack.wear == 4321);

		widef->unregisterItem(def.name);
	}

	// Obscurities
	{
		// Unsigned underflow
		ItemStack stack;
		std::istringstream is("foo:negative -6 -4321", std::ios::binary);
		stack.deSerialize(is, idef);

		UASSERTEQ(s32, stack.count, UINT16_MAX -    6 + 1);
		UASSERTEQ(s32, stack.wear,  UINT16_MAX - 4321 + 1);

		// Unsigned overflow
		is = std::istringstream("foo:overflow 65537 65538", std::ios::binary);
		stack.deSerialize(is, idef);

		UASSERT(stack.count == 1);
		UASSERT(stack.wear == 2);
	}
}

void TestInventory::testFormats(IItemDefManager *idef)
{
	Inventory inv(idef);
	auto *list = inv.addList("main", 1);
	ItemStack stack("default:dirt", 1, 0, idef);
	stack.metadata.setString("description", "ordinary metadata");
	stack.metadata.addModifier("test:modifier");
	list->changeItem(0, stack);
	for (auto protocol : {52, 53, 54}) {
		std::stringstream stream;
		inv.serialize(stream, false, nodeModifiersSupportForProtocol(protocol));
		Inventory loaded(idef);
		loaded.deSerialize(stream);
		const auto &meta = loaded.getList("main")->getItem(0).metadata;
		UASSERTEQ(std::string, meta.getString("description"), "ordinary metadata");
		UASSERT(meta.getModifiersList().empty() == (protocol < 55));
		if (protocol < 55) {
			ItemStack plain = stack;
			plain.metadata.removeModifier("test:modifier");
			UASSERTEQ(std::string, stack.getItemString(true, false), plain.getItemString());
		}
	}
	// Wield-item properties must use the same downgrade policy.
	ObjectProperties props;
	props.visual = OBJECTVISUAL_WIELDITEM;
	props.wield_item = stack.getItemString();
	props.textures = {"default:dirt"}; // Deprecated item-name field, not an itemstring.
	for (auto protocol : {52, 53, 54}) {
		std::stringstream stream;
		props.serialize(stream, protocol);
		ObjectProperties loaded;
		loaded.deSerialize(stream);
		UASSERT(loaded.textures == props.textures);
		ItemStack item;
		item.deSerialize(loaded.wield_item, idef);
		UASSERT(item.metadata.getModifiersList().empty() == (protocol < 55));
		UASSERTEQ(std::string, item.metadata.getString("description"), "ordinary metadata");
	}

	// Queued payloads retain their values, including across message copies.
	const auto property_packet = [&props](u16 protocol) {
		std::ostringstream os(std::ios::binary);
		writeU8(os, AO_CMD_SET_PROPERTIES);
		props.serialize(os, protocol);
		return os.str();
	};
	ActiveObjectMessage original(42, true, property_packet(54), property_packet(53));
	ActiveObjectMessage queued = original;
	props.wield_item.clear();
	original = ActiveObjectMessage(0);

	std::string legacy_payload;
	std::string current_payload;
	for (u16 protocol : {53, 54, 52, 54, 53}) {
		std::string packet;
		queued.appendTo(packet, protocol);
		std::istringstream input(packet, std::ios::binary);
		UASSERTEQ(u16, readU16(input), 42);
		std::string payload = deSerializeString16(input);
		std::istringstream properties_stream(payload, std::ios::binary);
		UASSERTEQ(u8, readU8(properties_stream), AO_CMD_SET_PROPERTIES);
		ObjectProperties loaded;
		loaded.deSerialize(properties_stream);
		ItemStack item;
		item.deSerialize(loaded.wield_item, idef);
		UASSERTEQ(std::string, item.metadata.getString("description"), "ordinary metadata");
		UASSERT(item.metadata.getModifiersList().empty() == (protocol < 55));

		std::string &previous = protocol < 55 ? legacy_payload : current_payload;
		if (!previous.empty())
			UASSERTEQ(std::string, payload, previous);
		previous = payload;
	}

	// Node inventories are carried in blocks and metadata-update packets.
	NodeMetadataList nodes;
	auto *node = new NodeMetadata(idef);
	node->getInventory()->addList("main", 1)->changeItem(0, stack);
	nodes.set({0, 0, 0}, node);
	for (u8 block_version : {29, 30}) {
		std::stringstream stream;
		nodes.serialize(stream, block_version, false);
		NodeMetadataList loaded;
		loaded.deSerialize(stream, idef);
		const auto &item = loaded.get({0, 0, 0})->getInventory()->getList("main")->getItem(0);
		UASSERT(item.metadata.getModifiersList().empty() == (block_version == 29));
	}
	UASSERTEQ(size_t, list->getItem(0).metadata.getModifiersList().size(), 1);
}

void TestInventory::testModifiers(IItemDefManager *idef)
{
	ItemStackMetadata a, b;
	SimpleMetadata fields;
	UASSERT(a.addModifier("test:first"));
	UASSERT(a != b && b != a);
	UASSERT(!(a == b) && !(b == a));
	UASSERT(a == fields && fields == a);
	UASSERT(!(a != fields) && !(fields != a));
	const IMetadata &base_a = a, &base_b = b;
	UASSERT(base_a == base_b && base_b == base_a);
	UASSERT(b.addModifier("test:first"));
	UASSERT(a == b && !(a != b));
	UASSERT(a.addModifier("test:second"));
	UASSERT(b.addModifier("test:second"));
	UASSERT(b.addModifier("test:first"));
	UASSERT(a != b); // Application order matters.
	UASSERT(b.setModifiers({"test:first", "test:second"}));
	b.setString("field", "different");
	UASSERT(a != b && base_a != base_b);

	ItemStackMetadata limited;
	std::vector<std::string> full;
	for (int i = 0; i < 16; ++i) {
		full.push_back("test:limit_" + std::to_string(i));
		UASSERT(limited.addModifier(full.back()));
	}
	limited.setModified(false);
	UASSERT(!limited.addModifier("test:extra"));
	UASSERT(!limited.isModified());
	UASSERT(limited.getModifiersList() == full);
	UASSERT(limited.addModifier(full.front()));
	auto repeated = full;
	repeated.insert(repeated.end(), full.begin(), full.end());
	UASSERT(limited.setModifiers(std::move(repeated)));
	UASSERT(limited.getModifiersList() == full);
	auto oversized = full;
	oversized.emplace_back("test:extra");
	limited.setModified(false);
	UASSERT(!limited.setModifiers(std::move(oversized)));
	UASSERT(!limited.isModified());
	UASSERT(limited.getModifiersList() == full);

	std::stringstream encoded;
	limited.serialize(encoded, true);
	ItemStackMetadata decoded;
	decoded.deSerialize(encoded);
	UASSERT(decoded.getModifiersList() == full);
	std::string payload("\x01\x01");
	for (const auto &name : full)
		payload += name + '\x03';
	payload += "test:extra\x03";
	std::stringstream invalid(serializeJsonStringIfNeeded(payload));
	EXCEPTION_CHECK(SerializationError, decoded.deSerialize(invalid));

	ItemStackMetadata names;
	UASSERT(names.addModifier("test_mod:Name_123"));
	for (const std::string name : {"", "plain", ":name", "mod:", "Mod:name",
			"mod:two:parts", "mod:space name", "mod:dash-name", "mod:bad\x01"}) {
		UASSERT(!names.addModifier(name));
		UASSERT(!names.setModifiers({name}));
	}
	UASSERT(!names.addModifier(std::string("mod:nul\0name", 12)));
	UASSERT(!names.addModifier("mod:" + std::string(65532, 'a')));

	ItemStack stack("default:dirt", 1, 0, idef);
	auto &meta = stack.metadata;
	UASSERT(meta.getModifiersList().empty());
	UASSERT(!meta.addModifier(""));
	UASSERT(meta.addModifier("test:first"));
	UASSERT(meta.addModifier("missing:second"));
	UASSERT(meta.addModifier("test:first"));
	UASSERT(meta.getModifiersList() ==
			(std::vector<std::string>{"missing:second", "test:first"}));
	UASSERT(meta.isModified());
	meta.setString("description", "Modified dirt");

	ItemStack restored;
	std::stringstream stream;
	stack.serialize(stream);
	restored.deSerialize(stream, idef);
	UASSERT(restored == stack);
	UASSERT(restored.metadata.getModifiersList() == meta.getModifiersList());
	restored.metadata.removeModifier("missing:second");
	UASSERT(restored.metadata.getModifiersList() ==
			std::vector<std::string>{"test:first"});
	UASSERT(!(restored == stack));
	restored.metadata.removeModifier("missing:second");
	UASSERT(restored.metadata.getModifiersList() ==
			std::vector<std::string>{"test:first"});
	UASSERT(meta.getModifiersList().size() == 2); // Copy has independent metadata.
	restored.metadata.removeModifier("test:first");
	UASSERT(restored.metadata.getModifiersList().empty());
	UASSERTEQ(std::string, restored.metadata.getString("description"), "Modified dirt");
	meta.clear();
	UASSERT(meta.getModifiersList().empty());
	UASSERT(meta.empty());

	// An ordinary legacy field with the old proposed name is unrelated.
	meta.setString("node_modifiers", "[17]");
	std::stringstream legacy;
	meta.serialize(legacy, false);
	UASSERTEQ(std::string, legacy.str(),
			serializeJsonStringIfNeeded(std::string("\x01node_modifiers\x02[17]\x03")));
	ItemStackMetadata loaded;
	loaded.deSerialize(legacy);
	UASSERT(loaded.getModifiersList().empty());
	UASSERTEQ(std::string, loaded.getString("node_modifiers"), "[17]");
	loaded.addModifier("test:first");
	loaded.setString("node_modifiers", "not JSON");
	UASSERT(loaded.getModifiersList() == std::vector<std::string>{"test:first"});
	std::stringstream extended;
	loaded.serialize(extended, true);
	meta.deSerialize(extended);
	UASSERT(meta == loaded);
	UASSERTEQ(std::string, meta.getString("node_modifiers"), "not JSON");
	UASSERT(meta.getStrings().size() == 1);

	// Generic metadata equality compares only fields, symmetrically.
	SimpleMetadata fields_only;
	fields_only.setString("node_modifiers", "not JSON");
	const IMetadata &generic = meta;
	UASSERT(generic == fields_only);
	UASSERT(fields_only == generic);
	meta.removeModifier("test:first");
	UASSERT(generic == fields_only);
	UASSERT(fields_only == generic);

	meta.clear();
	meta.addModifier("test:only");
	UASSERT(!meta.empty());
	UASSERT(meta.getStrings().empty());
	meta.setModified(false);
	meta.clear();
	UASSERT(meta.empty());
	UASSERT(meta.isModified());

	// Loading a legacy stack into a used object clears its modifier vector.
	meta.addModifier("test:old");
	std::stringstream legacy_text(serializeJsonStringIfNeeded("old metadata"));
	meta.deSerialize(legacy_text);
	UASSERT(meta.getModifiersList().empty());
	UASSERTEQ(std::string, meta.getString(""), "old metadata");

	// The new section extends the legacy field encoding without binary lengths.
	meta.clear();
	meta.setString("key", "value\x04"); // Other control bytes remain ordinary data.
	meta.addModifier("test:first");
	meta.addModifier("test:second");
	std::stringstream section;
	meta.serialize(section, true);
	UASSERTEQ(std::string, deSerializeJsonStringIfNeeded(section),
			"\x01key\x02value\x04\x03\x01test:first\x03test:second\x03");
	for (char delimiter : {'\x01', '\x02', '\x03'})
		UASSERT(!meta.addModifier(std::string("test:") + delimiter + "bad"));
	meta.clear();
	meta.addModifier("test:only");
	std::stringstream only;
	meta.serialize(only, true);
	loaded.deSerialize(only);
	UASSERT(loaded == meta);
	for (const std::string invalid : {
			"\x01\x01test:unterminated", "\x01\x01\x03",
			"\x01\x01test:bad\x02name\x03", "\x01\x01test:bad\x01name\x03",
			"\x01\x01test:duplicate\x03test:duplicate\x03"}) {
		std::stringstream input(serializeJsonStringIfNeeded(invalid));
		EXCEPTION_CHECK(SerializationError, loaded.deSerialize(input));
	}

}

void TestInventory::testSerializeDeserialize(IItemDefManager *idef)
{
	Inventory inv(idef);
	std::istringstream is(serialized_inventory_in, std::ios::binary);

	inv.deSerialize(is);
	UASSERT(inv.getList("0"));
	UASSERT(!inv.getList("main"));

	inv.getList("0")->setName("main");
	UASSERT(!inv.getList("0"));
	UASSERT(inv.getList("main"));
	UASSERTEQ(u32, inv.getList("main")->getWidth(), 3);

	inv.getList("main")->setWidth(5);
	std::ostringstream inv_os(std::ios::binary);
	inv.serialize(inv_os, false, false);
	UASSERTEQ(std::string, inv_os.str(), serialized_inventory_out);

	inv.setModified(false);
	inv_os.str("");
	inv_os.clear();
	inv.serialize(inv_os, true, false);
	UASSERTEQ(std::string, inv_os.str(), serialized_inventory_inc);

	ItemStack leftover = inv.getList("main")->takeItem(7, 99 - 12);
	ItemStack wanted = ItemStack("default:dirt", 99 - 12, 0, idef);
	UASSERT(leftover == wanted);
	leftover = inv.getList("main")->getItem(7);
	wanted.count = 12;
	UASSERT(leftover == wanted);
}

const char *TestInventory::serialized_inventory_in =
	"List 0 10\n"
	"Width 3\n"
	"Empty\n"
	"Empty\n"
	"Item default:cobble 61\n"
	"Empty\n"
	"Empty\n"
	"Item default:dirt 71\n"
	"Empty\n"
	"Item default:dirt 99\n"
	"Item default:cobble 38\n"
	"Empty\n"
	"EndInventoryList\n"
	"List abc 1\n"
	"Item default:stick 3\n"
	"Width 0\n"
	"EndInventoryList\n"
	"EndInventory\n";

const char *TestInventory::serialized_inventory_out =
	"List main 10\n"
	"Width 5\n"
	"Empty\n"
	"Empty\n"
	"Item default:cobble 61\n"
	"Empty\n"
	"Empty\n"
	"Item default:dirt 71\n"
	"Empty\n"
	"Item default:dirt 99\n"
	"Item default:cobble 38\n"
	"Empty\n"
	"EndInventoryList\n"
	"List abc 1\n"
	"Width 0\n"
	"Item default:stick 3\n"
	"EndInventoryList\n"
	"EndInventory\n";

const char *TestInventory::serialized_inventory_inc =
	"KeepList main\n"
	"KeepList abc\n"
	"EndInventory\n";
