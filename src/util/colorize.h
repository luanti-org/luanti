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

#pragma once

#include <string>
#include "config.h"

#define COLOR_CODE(color) "\x1b(c@" color ")"

#if USE_CURL
#include <curl/curlver.h>
// curl_url functions since 7.62.0
#if LIBCURL_VERSION_NUM >= 0x073e00
#define HAVE_COLORIZE_URL
#endif
#endif

#ifdef HAVE_COLORIZE_URL

/**
 * Colorize URL to highlight the hostname and any unsafe characters.
 *
 * Throws an exception if the url is invalid.
 */
[[nodiscard]] std::string colorize_url(const std::string &url);

#endif
