// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2017-8 rubenwardy <rw@rubenwardy.com>


#include "itemstackmetadata.h"
#include "util/serialize.h"
#include "util/strfnd.h"

#include <algorithm>
#include <optional>

#define DESERIALIZE_START '\x01'
#define DESERIALIZE_KV_DELIM '\x02'
#define DESERIALIZE_PAIR_DELIM '\x03'
#define DESERIALIZE_START_STR "\x01"
#define DESERIALIZE_KV_DELIM_STR "\x02"
#define DESERIALIZE_PAIR_DELIM_STR "\x03"

#define TOOLCAP_KEY "tool_capabilities"
#define WEAR_BAR_KEY "wear_color"
#define INVENTORY_IMAGE_KEY "inventory_image_def"
#define INVENTORY_OVERLAY_KEY "inventory_overlay_def"
#define WIELD_IMAGE_KEY "wield_image_def"
#define WIELD_OVERLAY_KEY "wield_overlay_def"

void ItemStackMetadata::clear()
{
	SimpleMetadata::clear();
	updateAll();
}

void ItemStackMetadata::updateAll()
{
	updateToolCapabilities();
	updateWearBarParams();
	updateInventoryImage();
	updateOverlayImage();
	updateWieldImage();
	updateWieldOverlay();
}

static void sanitize_string(std::string &str)
{
	str.erase(std::remove(str.begin(), str.end(), DESERIALIZE_START), str.end());
	str.erase(std::remove(str.begin(), str.end(), DESERIALIZE_KV_DELIM), str.end());
	str.erase(std::remove(str.begin(), str.end(), DESERIALIZE_PAIR_DELIM), str.end());
}

bool ItemStackMetadata::setString(const std::string &name, std::string_view var)
{
	std::string clean_name = name;
	std::string clean_var(var);
	sanitize_string(clean_name);
	sanitize_string(clean_var);

	bool result = SimpleMetadata::setString(clean_name, clean_var);
	if (clean_name == TOOLCAP_KEY)
		updateToolCapabilities();
	else if (clean_name == WEAR_BAR_KEY)
		updateWearBarParams();
	else if (clean_name == INVENTORY_IMAGE_KEY)
		updateInventoryImage();
	else if (clean_name == INVENTORY_OVERLAY_KEY)
		updateOverlayImage();
	else if (clean_name == WIELD_IMAGE_KEY)
		updateWieldImage();
	else if (clean_name == WIELD_OVERLAY_KEY)
		updateWieldOverlay();
	return result;
}

void ItemStackMetadata::serialize(std::ostream &os) const
{
	std::ostringstream os2(std::ios_base::binary);
	os2 << DESERIALIZE_START;
	for (const auto &stringvar : m_stringvars) {
		if (!stringvar.first.empty() || !stringvar.second.empty())
			os2 << stringvar.first << DESERIALIZE_KV_DELIM
				<< stringvar.second << DESERIALIZE_PAIR_DELIM;
	}
	os << serializeJsonStringIfNeeded(os2.str());
}

void ItemStackMetadata::deSerialize(std::istream &is)
{
	std::string in = deSerializeJsonStringIfNeeded(is);

	m_stringvars.clear();

	if (!in.empty()) {
		if (in[0] == DESERIALIZE_START) {
			Strfnd fnd(in);
			fnd.to(1);
			while (!fnd.at_end()) {
				std::string name = fnd.next(DESERIALIZE_KV_DELIM_STR);
				std::string var  = fnd.next(DESERIALIZE_PAIR_DELIM_STR);
				m_stringvars[name] = std::move(var);
			}
		} else {
			// BACKWARDS COMPATIBILITY
			m_stringvars[""] = std::move(in);
		}
	}
	updateAll();
}

void ItemStackMetadata::updateToolCapabilities()
{
	if (contains(TOOLCAP_KEY)) {
		toolcaps_override = ToolCapabilities();
		std::istringstream is(getString(TOOLCAP_KEY));
		toolcaps_override->deserializeJson(is);
	} else {
		toolcaps_override = std::nullopt;
	}
}

void ItemStackMetadata::setToolCapabilities(const ToolCapabilities &caps)
{
	std::ostringstream os;
	caps.serializeJson(os);
	setString(TOOLCAP_KEY, os.str());
}

void ItemStackMetadata::clearToolCapabilities()
{
	setString(TOOLCAP_KEY, "");
}

void ItemStackMetadata::updateWearBarParams()
{
	if (contains(WEAR_BAR_KEY)) {
		std::istringstream is(getString(WEAR_BAR_KEY));
		wear_bar_override = WearBarParams::deserializeJson(is);
	} else {
		wear_bar_override.reset();
	}
}

void ItemStackMetadata::setWearBarParams(const WearBarParams &params)
{
	std::ostringstream os;
	params.serializeJson(os);
	setString(WEAR_BAR_KEY, os.str());
}

void ItemStackMetadata::clearWearBarParams()
{
	setString(WEAR_BAR_KEY, "");
}

// Item image overrides

#define ITEM_IMAGE_FUNCTIONS(NAME, FIELD, KEY) \
void ItemStackMetadata::set##NAME(const ItemImageDef &def) \
{ \
	std::ostringstream os; \
	def.serializeJson(os); \
	setString(KEY, os.str()); \
} \
void ItemStackMetadata::clear##NAME() \
{ \
	setString(KEY, ""); \
} \
void ItemStackMetadata::update##NAME() \
{ \
	if (contains(KEY)) { \
		std::istringstream is(getString(KEY)); \
		FIELD = ItemImageDef::deserializeJson(is); \
	} else { \
		FIELD.reset(); \
	} \
}

ITEM_IMAGE_FUNCTIONS(InventoryImage, inventory_image_override,   INVENTORY_IMAGE_KEY)
ITEM_IMAGE_FUNCTIONS(OverlayImage,   inventory_overlay_override, INVENTORY_OVERLAY_KEY)
ITEM_IMAGE_FUNCTIONS(WieldImage,     wield_image_override,       WIELD_IMAGE_KEY)
ITEM_IMAGE_FUNCTIONS(WieldOverlay,   wield_overlay_override,     WIELD_OVERLAY_KEY)
#undef ITEM_IMAGE_FUNCTIONS
