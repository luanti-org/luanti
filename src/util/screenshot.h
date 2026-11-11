// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <string>

namespace video {
	class IVideoDriver;
}

/**
 * Take a screenshot and save it to disk
 * @param driver Video driver to use for the screenshot
 * @param filename_out Output parameter that receives the path to the saved screenshot
 * @return true if the screenshot was saved successfully, false otherwise
 */
bool takeScreenshotAutoName(video::IVideoDriver *driver, std::string &filename_out);

/**
 * Take a screenshot and save it to an explicit path.
 * @param driver Video driver to use for the screenshot
 * @param filepath Output path, including extension
 * @return true if the screenshot was saved successfully, false otherwise
 */
bool takeScreenshotToPath(video::IVideoDriver *driver, const std::string &filepath);
