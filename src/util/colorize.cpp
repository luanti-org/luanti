/*
Part of Minetest
Copyright (C) 2024 rubenwardy

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "colorize.h"

#ifdef HAVE_COLORIZE_URL

#include <curl/urlapi.h>
#include "log.h"
#include "string.h"
#include <sstream>
#include <memory>

std::string colorize_url(const std::string &url)
{
	auto urlHandleRAII = std::unique_ptr<CURLU, decltype(&curl_url_cleanup)>(
			curl_url(), curl_url_cleanup);
	CURLU *urlHandle = urlHandleRAII.get();

	auto rc = curl_url_set(urlHandle, CURLUPART_URL, url.c_str(), 0);
	if (rc != CURLUE_OK) {
		throw std::runtime_error("URL is not valid");
	}

	auto url_get = [&] (CURLUPart what) -> std::string {
		char *tmp = nullptr;
		curl_url_get(urlHandle, what, &tmp, 0);
		std::string ret(tmp ? tmp : "");
		curl_free(tmp);
		return ret;
	};

	auto scheme = url_get(CURLUPART_SCHEME);
	auto user = url_get(CURLUPART_USER);
	auto password = url_get(CURLUPART_PASSWORD);
	auto host = url_get(CURLUPART_HOST);
	auto port = url_get(CURLUPART_PORT);
	auto path = url_get(CURLUPART_PATH);
	auto query = url_get(CURLUPART_QUERY);
	auto fragment = url_get(CURLUPART_FRAGMENT);
#if LIBCURL_VERSION_NUM >= 0x074100
	auto zoneid = url_get(CURLUPART_ZONEID);
#else
	std::string zoneid;
#endif

	std::ostringstream os;

	std::string_view red = COLOR_CODE("#faa");
	std::string_view white = COLOR_CODE("#fff");
	std::string_view grey = COLOR_CODE("#aaa");

	os << grey << scheme << "://";
	if (!user.empty())
		os << user;
	if (!password.empty())
		os << ":" << password;
	if (!(user.empty() && password.empty()))
		os << "@";

	// Print hostname, escaping unsafe characters
	os << white;
	bool was_alphanum = true;
	for (char c : host) {
		bool is_alphanum = isalnum(c) || ispunct(c);
		if (is_alphanum == was_alphanum) {
			// skip
		} else if (is_alphanum) {
			os << white;
		} else {
			os << red;
		}
		was_alphanum = is_alphanum;

		if (is_alphanum) {
			os << c;
		} else {
			os << "%" << std::setfill('0') << std::setw(2) << std::hex
				<< (static_cast<unsigned int>(c) & 0xff);
		}
	}

	os << grey;
	if (!zoneid.empty())
		os << "%" << zoneid;
	if (!port.empty())
		os << ":" << port;
	os << path;
	if (!query.empty())
		os << "?" << query;
	if (!fragment.empty())
		os << "#" << fragment;

	return os.str();
}

#endif
