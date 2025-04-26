// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <vector>
#include "util/string.h"
#include "config.h"

// These can be used in place of "caller" in to specify special handling.
// Discard result (used as default value of "caller").
#define HTTPFETCH_DISCARD 0
// Indicates that the result should not be discarded when performing a
// synchronous request (since a real caller ID is not needed for synchronous
// requests because the result does not have to be retrieved later).
#define HTTPFETCH_SYNC 1
// Print response body to console if the server returns an error code.
#define HTTPFETCH_PRINT_ERR 2
// Start of regular allocated caller IDs.
#define HTTPFETCH_CID_START 3

namespace {
	// lower bound for curl_timeout (see also settingtypes.txt)
	constexpr long MIN_HTTPFETCH_TIMEOUT_INTERACTIVE = 1000;
	// lower bound for curl_file_download_timeout
	constexpr long MIN_HTTPFETCH_TIMEOUT = 5000;
}

//  Methods
enum HttpMethod : u8
{
	HTTP_GET,
	HTTP_HEAD,
	HTTP_POST,
	HTTP_PUT,
	HTTP_PATCH,
	HTTP_DELETE,
};

struct HTTPFetchRequest
{
	std::string url = "";

	// Identifies the caller (for asynchronous requests)
	// Ignored by httpfetch_sync
	u64 caller = HTTPFETCH_DISCARD;

	// Some number that identifies the request
	// (when the same caller issues multiple httpfetch_async calls)
	u64 request_id = 0;

	// Timeout for the whole transfer, in milliseconds
	long timeout;

	// Timeout for the connection phase, in milliseconds
	long connect_timeout;

	// Indicates if this is multipart/form-data or
	// application/x-www-form-urlencoded. Not allowed for GET.
	bool multipart = false;

	// Method to use
	HttpMethod method = HTTP_GET;

	// Fields of the request
	StringMap fields;

	// Raw data of the request (instead of fields, ignored if multipart)
	std::string raw_data;

	// If not empty, should contain entries such as "Accept: text/html"
	std::vector<std::string> extra_headers;

	// User agent to send
	std::string useragent;

	HTTPFetchRequest();
};

struct HTTPFetchResult
{
	bool succeeded = false;
	bool timeout = false;
	long response_code = 0;
	std::string data = "";
	// The caller and request_id from the corresponding HTTPFetchRequest.
	u64 caller = HTTPFETCH_DISCARD;
	u64 request_id = 0;

	HTTPFetchResult() = default;

	HTTPFetchResult(const HTTPFetchRequest &fetch_request) :
			caller(fetch_request.caller), request_id(fetch_request.request_id)
	{
	}
};

// Initializes the httpfetch module
void httpfetch_init(int parallel_limit);

// Stops the httpfetch thread and cleans up resources
void httpfetch_cleanup();

// Starts an asynchronous HTTP fetch request
void httpfetch_async(const HTTPFetchRequest &fetch_request);

// If any fetch for the given caller ID is complete, removes it from the
// result queue, sets the fetch result and returns true. Otherwise returns false.
bool httpfetch_async_get(u64 caller, HTTPFetchResult &fetch_result);

// Allocates a caller ID for httpfetch_async
// Not required if you want to set caller = HTTPFETCH_DISCARD
u64 httpfetch_caller_alloc();

// Allocates a non-predictable caller ID for httpfetch_async
u64 httpfetch_caller_alloc_secure();

// Frees a caller ID allocated with httpfetch_caller_alloc
// Note: This can be expensive, because the httpfetch thread is told
// to stop any ongoing fetches for the given caller.
void httpfetch_caller_free(u64 caller);

// Performs a synchronous HTTP request that is interruptible if the current
// thread is a Thread object. interval is the completion check interval in ms.
// This blocks and therefore should only be used from background threads.
// Returned is whether the request completed without interruption.
bool httpfetch_sync_interruptible(const HTTPFetchRequest &fetch_request,
		HTTPFetchResult &fetch_result, long interval = 100);
