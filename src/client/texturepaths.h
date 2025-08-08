// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <string>
#include <vector>

// Texture paths get cached and this clears the cache.
void clearTextureNameCache();

// Find out the full path of an image by trying different filename extensions.
// If failed, return "".
std::string getImagePath(std::string_view path);

/* Gets the path to a texture by first checking if the texture exists
 * in texture_path and if not, using the data path.
 *
 * Checks all supported extensions by replacing the original extension.
 *
 * If not found, returns "".
 *
 * Utilizes a thread-safe cache.
*/
std::string getTexturePath(const std::string &filename, bool *is_base_pack = nullptr);

// Returns all dictionaries found from the "texture_path" setting.
std::vector<std::string> getTextureDirs();
