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

	const ToolCapabilities &getToolCapabilities(
			const ToolCapabilities &default_caps) const
	{
		return toolcaps_override.has_value() ? *toolcaps_override : default_caps;
	}

	void setToolCapabilities(const ToolCapabilities &caps);
	void clearToolCapabilities();

	const std::optional<WearBarParams> &getWearBarParamOverride() const
	{
		return wear_bar_override;
	}

	void setWearBarParams(const WearBarParams &params);
	void clearWearBarParams();

	// Templated to prevent code duplication
	enum AnimationType {
		InventoryImage,
		InventoryOverlay,
		WieldImage,
		WieldOverlay
	};

	template<AnimationType t>
	const std::optional<TileAnimationParams> &getAnimationOverride() const
	{
		return animation_overrides[t];
	}

	template<AnimationType>
	void setAnimation(const TileAnimationParams &params);
	template<AnimationType>
	void clearAnimation();

private:
	void updateToolCapabilities();
	void updateWearBarParams();
	void updateAll();

	template<AnimationType>
	void updateAnimation();

	std::optional<ToolCapabilities> toolcaps_override;
	std::optional<WearBarParams> wear_bar_override;
	std::optional<TileAnimationParams> animation_overrides[4];
};

// Need to declare all
extern template void ItemStackMetadata::setAnimation<ItemStackMetadata::InventoryImage>(const TileAnimationParams &params);
extern template void ItemStackMetadata::setAnimation<ItemStackMetadata::InventoryOverlay>(const TileAnimationParams &params);
extern template void ItemStackMetadata::setAnimation<ItemStackMetadata::WieldImage>(const TileAnimationParams &params);
extern template void ItemStackMetadata::setAnimation<ItemStackMetadata::WieldOverlay>(const TileAnimationParams &params);
extern template void ItemStackMetadata::clearAnimation<ItemStackMetadata::InventoryImage>();
extern template void ItemStackMetadata::clearAnimation<ItemStackMetadata::InventoryOverlay>();
extern template void ItemStackMetadata::clearAnimation<ItemStackMetadata::WieldImage>();
extern template void ItemStackMetadata::clearAnimation<ItemStackMetadata::WieldOverlay>();


