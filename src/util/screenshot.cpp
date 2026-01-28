// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "screenshot.h"
#include "filesys.h"
#include "gettime.h"
#include "porting.h"
#include "settings.h"
#include "util/string.h"
#include "util/numeric.h"
#include "gettext.h"
#include "log.h"
#include "debug.h"
#include <IVideoDriver.h>
#include <ctime>

#define SCREENSHOT_MAX_SERIAL_TRIES 1000

bool takeScreenshot(video::IVideoDriver *driver, std::string &filename_out)
{
	sanity_check(driver);

	video::IImage* const raw_image = driver->createScreenShot();

	if (!raw_image) {
		errorstream << "Could not take screenshot" << std::endl;
		return false;
	}

	const struct tm tm = mt_localtime();

	char timestamp_c[64];
	strftime(timestamp_c, sizeof(timestamp_c), "%Y%m%d_%H%M%S", &tm);

	std::string screenshot_dir = g_settings->get("screenshot_path");
	if (!fs::IsPathAbsolute(screenshot_dir))
		screenshot_dir = porting::path_user + DIR_DELIM + screenshot_dir;

	std::string filename_base = screenshot_dir
			+ DIR_DELIM
			+ std::string("screenshot_")
			+ timestamp_c;
	std::string filename_ext = "." + g_settings->get("screenshot_format");

	// Create the directory if it doesn't already exist.
	// Otherwise, saving the screenshot would fail.
	fs::CreateAllDirs(screenshot_dir);

	u32 quality = (u32)g_settings->getS32("screenshot_quality");
	quality = rangelim(quality, 0, 100) / 100.0f * 255;

	// Try to find a unique filename
	std::string filename;
	unsigned serial = 0;

	while (serial < SCREENSHOT_MAX_SERIAL_TRIES) {
		filename = filename_base + (serial > 0 ? ("_" + itos(serial)) : "").append(filename_ext);
		if (!fs::PathExists(filename))
			break;	// File did not apparently exist, we'll go with it
		serial++;
	}

	if (serial == SCREENSHOT_MAX_SERIAL_TRIES) {
		errorstream << "Could not find suitable filename for screenshot" << std::endl;
		raw_image->drop();
		return false;
	}

	video::IImage* const image =
			driver->createImage(video::ECF_R8G8B8, raw_image->getDimension());

	if (!image) {
		errorstream << "Could not create image for screenshot" << std::endl;
		raw_image->drop();
		return false;
	}

	raw_image->copyTo(image);

	bool success = driver->writeImageToFile(image, filename.c_str(), quality);

	if (success) {
		filename_out = filename;
		std::string msg = fmtgettext("Saved screenshot to \"%s\"", filename.c_str());
		infostream << msg << std::endl;
	} else {
		std::string msg = fmtgettext("Failed to save screenshot to \"%s\"", filename.c_str());
		errorstream << msg << std::endl;
	}

	image->drop();
	raw_image->drop();
	return success;
}

