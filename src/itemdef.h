// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2013 Kahrl <kahrl@gmx.net>

#pragma once

#include "irrlichttypes_bloated.h"
#include <string>
#include <iostream>
#include <optional>
#include <set>
#include "itemgroup.h"
#include "sound.h"
#include "texture_override.h" // TextureOverride
#include "tool.h"
#include "util/pointabilities.h"
#include "util/pointedthing.h"

struct ToolCapabilities;
struct ItemDefinition;

/*
	Base item definition
*/

enum ItemType : u8
{
	ITEM_NONE,
	ITEM_NODE,
	ITEM_CRAFT,
	ITEM_TOOL,
	ItemType_END // Dummy for validity check
};

enum TouchInteractionMode : u8
{
	LONG_DIG_SHORT_PLACE,
	SHORT_DIG_LONG_PLACE,
	TouchInteractionMode_USER, // Meaning depends on client-side settings
	TouchInteractionMode_END, // Dummy for validity check
};

struct TouchInteraction
{
	TouchInteractionMode pointed_nothing;
	TouchInteractionMode pointed_node;
	TouchInteractionMode pointed_object;

	TouchInteraction();
	// Returns the right mode for the pointed thing and resolves any occurrence
	// of TouchInteractionMode_USER into an actual mode.
	TouchInteractionMode getMode(const ItemDefinition &selected_def,
			PointedThingType pointed_type) const;
	void serialize(std::ostream &os) const;
	void deSerialize(std::istream &is);
};

struct ItemDefinition
{
	/*
		Basic item properties
	*/
	ItemType type;
	std::string name; // "" = hand
	std::string description; // Shown in tooltip.
	std::string short_description;

	/*
		Visual properties
	*/
	std::string inventory_image; // Optional for nodes, mandatory for tools/craftitems
	std::string inventory_overlay; // Overlay of inventory_image.
	std::string wield_image; // If empty, inventory_image or mesh (only nodes) is used
	std::string wield_overlay; // Overlay of wield_image.
	std::string palette_image; // If specified, the item will be colorized based on this
	video::SColor color; // The fallback color of the node.
	v3f wield_scale;

	/*
		Item stack and interaction properties
	*/
	u16 stack_max;
	bool usable;
	bool liquids_pointable;
	std::optional<Pointabilities> pointabilities;

	// They may be NULL. If non-NULL, deleted by destructor
	ToolCapabilities *tool_capabilities;

	std::optional<WearBarParams> wear_bar_params;

	ItemGroupList groups;
	SoundSpec sound_place;
	SoundSpec sound_place_failed;
	SoundSpec sound_use, sound_use_air;
	f32 range;

	// Client shall immediately place this node when player places the item.
	// Server will update the precise end result a moment later.
	// "" = no prediction
	std::string node_placement_prediction;
	std::optional<u8> place_param2;
	bool wallmounted_rotate_vertical;

	TouchInteraction touch_interaction;

	/*
		Some helpful methods
	*/
	ItemDefinition();
	ItemDefinition(const ItemDefinition &def);
	ItemDefinition& operator=(const ItemDefinition &def);
	~ItemDefinition();
	void reset();
	void serialize(std::ostream &os, u16 protocol_version) const;
	void deSerialize(std::istream &is, u16 protocol_version);
private:
	void resetInitial();
};

class IItemDefManager
{
public:
	IItemDefManager() = default;

	virtual ~IItemDefManager() = default;

	// Get item definition
	virtual const ItemDefinition& get(const std::string &name) const=0;
	// Get alias definition
	virtual const std::string &getAlias(const std::string &name) const=0;
	// Get set of all defined item names and aliases
	virtual void getAll(std::set<std::string> &result) const=0;
	// Check if item is known
	virtual bool isKnown(const std::string &name) const=0;

	virtual void serialize(std::ostream &os, u16 protocol_version)=0;
};

class IWritableItemDefManager : public IItemDefManager
{
public:
	IWritableItemDefManager() = default;

	virtual ~IWritableItemDefManager() = default;

	// Replace the textures of registered nodes with the ones specified in
	// the texture pack's override.txt files
	virtual void applyTextureOverrides(const std::vector<TextureOverride> &overrides)=0;

	// Remove all registered item and node definitions and aliases
	// Then re-add the builtin item definitions
	virtual void clear()=0;
	// Register item definition
	virtual void registerItem(const ItemDefinition &def)=0;
	virtual void unregisterItem(const std::string &name)=0;
	// Set an alias so that items named <name> will load as <convert_to>.
	// Alias is not set if <name> has already been defined.
	// Alias will be removed if <name> is defined at a later point of time.
	virtual void registerAlias(const std::string &name,
			const std::string &convert_to)=0;

	virtual void deSerialize(std::istream &is, u16 protocol_version)=0;
};

IWritableItemDefManager* createItemDefManager();
