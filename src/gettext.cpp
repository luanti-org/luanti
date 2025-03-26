// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include <string>
#include <cstring>
#include <iostream>
#include "gettext.h"
#include "util/string.h"
#include "porting.h"
#include "log.h"

#if USE_GETTEXT && defined(_MSC_VER)
#include <windows.h>
#include <map>
#include <direct.h>
#include "filesys.h"

#define setlocale(category, localename) \
	setlocale(category, MSVC_LocaleLookup(localename))

static std::map<std::wstring, std::wstring> glb_supported_locales;

/******************************************************************************/
static BOOL CALLBACK UpdateLocaleCallback(LPTSTR pStr)
{
	char* endptr = 0;
	int LOCALEID = strtol(pStr, &endptr,16);

	wchar_t buffer[LOCALE_NAME_MAX_LENGTH];
	memset(buffer, 0, sizeof(buffer));
	if (GetLocaleInfoW(
		LOCALEID,
		LOCALE_SISO639LANGNAME,
		buffer,
		LOCALE_NAME_MAX_LENGTH)) {

		std::wstring name = buffer;

		memset(buffer, 0, sizeof(buffer));
		GetLocaleInfoW(
		LOCALEID,
		LOCALE_SISO3166CTRYNAME,
		buffer,
		LOCALE_NAME_MAX_LENGTH);

		std::wstring country = buffer;

		memset(buffer, 0, sizeof(buffer));
		GetLocaleInfoW(
		LOCALEID,
		LOCALE_SENGLISHLANGUAGENAME,
		buffer,
		LOCALE_NAME_MAX_LENGTH);

		std::wstring languagename = buffer;

		/* set both short and long variant */
		glb_supported_locales[name] = languagename;
		glb_supported_locales[name + L"_" + country] = languagename;
	}
	return true;
}

/******************************************************************************/
static const char* MSVC_LocaleLookup(const char* raw_shortname)
{

	/* NULL is used to read locale only so we need to return it too */
	if (raw_shortname == NULL) return NULL;

	std::string shortname(raw_shortname);
	if (shortname == "C") return "C";
	if (shortname == "") return "";

	static std::string last_raw_value = "";
	static std::string last_full_name = "";
	static bool first_use = true;

	if (last_raw_value == shortname) {
		return last_full_name.c_str();
	}

	if (first_use) {
		EnumSystemLocalesA(UpdateLocaleCallback, LCID_SUPPORTED | LCID_ALTERNATE_SORTS);
		first_use = false;
	}

	last_raw_value = shortname;

	auto key = utf8_to_wide(shortname);
	if (glb_supported_locales.find(key) != glb_supported_locales.end()) {
		last_full_name = wide_to_utf8(glb_supported_locales[key]);
		return last_full_name.c_str();
	}

	/* empty string is system default */
	errorstream << "MSVC_LocaleLookup: unsupported locale: \"" << shortname
				<< "\" switching to system default!" << std::endl;
	return "";
}

static void MSVC_LocaleWorkaround(int argc, char* argv[])
{
	errorstream << "MSVC localization workaround active.  "
		"Restarting " PROJECT_NAME_C " in a new environment!" << std::endl;

	std::string parameters;
	for (int i = 1; i < argc; i++) {
		if (i > 1)
			parameters += ' ';
		parameters += porting::QuoteArgv(argv[i]);
	}

	char *ptr_parameters = nullptr;
	if (!parameters.empty())
		ptr_parameters = &parameters[0];

	// Allow calling without an extension
	std::string app_name = argv[0];
	if (app_name.compare(app_name.size() - 4, 4, ".exe") != 0)
		app_name += ".exe";

	STARTUPINFO startup_info = {};
	PROCESS_INFORMATION process_info = {};

	bool success = CreateProcess(app_name.c_str(), ptr_parameters,
		NULL, NULL, false, DETACHED_PROCESS | CREATE_UNICODE_ENVIRONMENT,
		NULL, NULL, &startup_info, &process_info);

	if (success) {
		exit(0);
		// NOTREACHED
	} else {
		auto e = GetLastError();

		errorstream
			<< "*******************************************************" << '\n'
			<< "CMD: " << app_name << '\n'
			<< "Failed to restart with current locale: "
			<< porting::ConvertError(e) << '\n'
			<< "Expect language to be broken!" << '\n'
			<< "*******************************************************" << std::endl;
	}
}

#endif

/******************************************************************************/
void init_gettext(const char *path, const std::string &configured_language,
	int argc, char *argv[])
{
#if USE_GETTEXT
	// First, try to set user override environment
	if (!configured_language.empty()) {
		// Set LANGUAGE which overrides all others, see
		// <https://www.gnu.org/software/gettext/manual/html_node/Locale-Environment-Variables.html>
#ifndef _MSC_VER
		setenv("LANGUAGE", configured_language.c_str(), 1);

		// Reload locale with changed environment
		setlocale(LC_ALL, "");
#else
		std::string current_language;
		const char *env_lang = getenv("LANGUAGE");
		if (env_lang)
			current_language = env_lang;

		setenv("LANGUAGE", configured_language.c_str(), 1);
		SetEnvironmentVariableA("LANGUAGE", configured_language.c_str());

#if CHECK_CLIENT_BUILD()
		// Hack to force gettext to see the right environment
		if (current_language != configured_language) {
			// Disabled when debugger is present as it can break debugging
			if (!IsDebuggerPresent())
				MSVC_LocaleWorkaround(argc, argv);
			else
				actionstream << "Debugger detected. Skipping MSVC_LocaleWorkaround." << std::endl;
		}
#else
		errorstream << "*******************************************************" << std::endl;
		errorstream << "Can't apply locale workaround for server!" << std::endl;
		errorstream << "Expect language to be broken!" << std::endl;
		errorstream << "*******************************************************" << std::endl;
#endif

		setlocale(LC_ALL, configured_language.c_str());
#endif // ifdef _MSC_VER
	} else {
		/* set current system default locale */
		setlocale(LC_ALL, "");
	}

#if defined(_WIN32)
	if (getenv("LANGUAGE") != 0) {
		setlocale(LC_ALL, getenv("LANGUAGE"));
	}
#ifdef _MSC_VER
	else if (getenv("LANG") != 0) {
		setlocale(LC_ALL, getenv("LANG"));
	}
#endif
#endif

	std::string name = lowercase(PROJECT_NAME);
	infostream << "Gettext: domainname=\"" << name
		<< "\" path=\"" << path << "\"" << std::endl;

	bindtextdomain(name.c_str(), path);
	textdomain(name.c_str());

#ifdef _WIN32
	// set character encoding
	char *tdomain = textdomain(nullptr);
	assert(tdomain);
	if (tdomain)
		bind_textdomain_codeset(tdomain, "UTF-8");
#endif

#else
	/* set current system default locale */
	setlocale(LC_ALL, "");
#endif // if USE_GETTEXT

	/* no matter what locale is used we need number format to be "C" */
	/* to ensure formspec parameters are evaluated correctly!        */

	setlocale(LC_NUMERIC, "C");
	infostream << "Message locale is now set to: "
			<< setlocale(LC_ALL, 0) << std::endl;
}
