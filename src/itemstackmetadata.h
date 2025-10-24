// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2017-8 rubenwardy <rw@rubenwardy.com>

#pragma once

#include "metadata.h"
#include "tool.h"
#include "itemdef.h"

#include <optional>


class ItemStackMetadata : public SimpleMetadata
{
public:
	ItemStackMetadata()
	{}

	// Overrides
	void clear() override;
	bool setString(const std::string &name, std::string_view var) override;

	void serialize(std::ostream &os) const;
	void deSerialize(std::istream &is);

	const std::optional<ToolCapabilities> &getToolCapabilitiesOverride() const
	{
		return toolcaps_override;
	}

	void setToolCapabilities(const ToolCapabilities &caps);
	void clearToolCapabilities();

	const std::optional<WearBarParams> &getWearBarParamOverride() const
	{
		return wear_bar_override;
	}

	void setWearBarParams(const WearBarParams &params);
	void clearWearBarParams();

	// Item image overrides

	const std::optional<ItemImageDef> &getInventoryImageOverride() const
	{
		return inventory_image_override;
	}
	void setInventoryImage(const ItemImageDef &params);
	void clearInventoryImage();

	const std::optional<ItemImageDef> &getInventoryOverlayOverride() const
	{
		return inventory_overlay_override;
	}
	void setOverlayImage(const ItemImageDef &params);
	void clearOverlayImage();

	const std::optional<ItemImageDef> &getWieldImageOverride() const
	{
		return wield_image_override;
	}
	void setWieldImage(const ItemImageDef &params);
	void clearWieldImage();

	const std::optional<ItemImageDef> &getWieldOverlayOverride() const
	{
		return wield_overlay_override;
	}
	void setWieldOverlay(const ItemImageDef &params);
	void clearWieldOverlay();

private:
	void updateToolCapabilities();
	void updateWearBarParams();
	void updateInventoryImage();
	void updateOverlayImage();
	void updateWieldImage();
	void updateWieldOverlay();

	void updateAll();

	std::optional<ToolCapabilities> toolcaps_override;
	std::optional<WearBarParams> wear_bar_override;
	std::optional<ItemImageDef> inventory_image_override;
	std::optional<ItemImageDef> inventory_overlay_override;
	std::optional<ItemImageDef> wield_image_override;
	std::optional<ItemImageDef> wield_overlay_override;
};
