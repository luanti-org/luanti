// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2017-8 rubenwardy <rw@rubenwardy.com>

#pragma once

#include "metadata.h"
#include "tool.h"
#include "tileanimation.h"

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

	const std::optional<TileAnimationParams> &getInventoryImageAnimationOverride() const;
	const std::optional<TileAnimationParams> &getInventoryOverlayAnimationOverride() const;
	const std::optional<TileAnimationParams> &getWieldImageAnimationOverride() const;
	const std::optional<TileAnimationParams> &getWieldOverlayAnimationOverride() const;
	void setInventoryImageAnimation(const TileAnimationParams &params);
	void setInventoryOverlayAnimation(const TileAnimationParams &params);
	void setWieldImageAnimation(const TileAnimationParams &params);
	void setWieldOverlayAnimation(const TileAnimationParams &params);
	void clearInventoryImageAnimation();
	void clearInventoryOverlayAnimation();
	void clearWieldImageAnimation();
	void clearWieldOverlayAnimation();
private:
	void updateToolCapabilities();
	void updateWearBarParams();
	void updateInventoryImageAnimation();
	void updateInventoryOverlayAnimation();
	void updateWieldImageAnimation();
	void updateWieldOverlayAnimation();

	void updateAll();

	std::optional<ToolCapabilities> toolcaps_override;
	std::optional<WearBarParams> wear_bar_override;
	std::optional<TileAnimationParams> inventory_image_animation_override;
	std::optional<TileAnimationParams> inventory_overlay_animation_override;
	std::optional<TileAnimationParams> wield_image_animation_override;
	std::optional<TileAnimationParams> wield_overlay_animation_override;
};


