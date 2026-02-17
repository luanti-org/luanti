// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti developers

#include <set>
#include <map>
#include <sstream>
#include "mod_errors.h"
#include "common/c_internal.h"
#include "log.h"
#include "exceptions.h"

struct SingleModErrors {
	std::set<std::string> messages;
	bool was_handled;
};

static std::map<std::string, SingleModErrors> mod_errors;

void ModErrors::logErrors() {
	auto error_handling_mode = get_mod_error_handling_mode();
	if (error_handling_mode == ModErrorHandlingMode::IgnoreModError) {
		return;
	}

	for (auto &pair : mod_errors) {
		const auto path = pair.first;
		SingleModErrors& err = pair.second;

		if (!err.was_handled && !err.messages.empty()) {
			err.was_handled = true;
			std::ostringstream os;
			os << "Mod at " << path << ":" << std::endl;
			for (const auto& msg : err.messages) {
				os << "\t" << msg << std::endl;
			}
			if (error_handling_mode == ModErrorHandlingMode::ThrowModError)
				throw ModError(os.str());
			else
				warningstream << os.str();
		}
	}
}


void ModErrors::setModError(const std::string &path, std::string error_message) {
	SingleModErrors &cur = mod_errors[path];
	auto result = cur.messages.emplace(error_message);
	if (result.second) {
		// New message was added, reset the was_handled flag
		cur.was_handled = false;
	}
}
