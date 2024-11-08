// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2023 DS

#pragma once

#include "al_helpers.h"

namespace sound {

/**
 * Struct for AL and ALC extensions
 */
struct ALExtensions
{
	explicit ALExtensions(const ALCdevice *deviceHandle [[maybe_unused]]);

#ifdef AL_SOFT_direct_channels_remix
	bool have_ext_AL_SOFT_direct_channels_remix = false;
#endif
};

}
