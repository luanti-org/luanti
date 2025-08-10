// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "httpfetch.h"
#include "porting.h" // for sleep_ms(), get_sysinfo(), secure_rand_fill_buf()
#include <list>
#include <unordered_map>
#include <cerrno>
#include <mutex>
#include "threading/event.h"
#include "config.h"
#include "exceptions.h"
#include "debug.h"
#include "log.h"
#include "porting.h"
#include "util/container.h"
#include "util/thread.h"
#include "util/numeric.h"
#include "version.h"
#include "settings.h"

static std::mutex g_httpfetch_mutex;
static std::unordered_map<u64, std::queue<HTTPFetchResult>>
	g_httpfetch_results;

static std::string default_user_agent()
{
	std::string ret(PROJECT_NAME_C "/");
	ret.append(g_version_string).append(" (").append(porting::get_sysinfo()).append(")");
	return ret;
}

HTTPFetchRequest::HTTPFetchRequest() :
	timeout(g_settings->getS32("curl_timeout")),
	connect_timeout(10 * 1000),
	useragent(default_user_agent())
{
	timeout = std::max(timeout, MIN_HTTPFETCH_TIMEOUT_INTERACTIVE);
}


static void httpfetch_deliver_result(const HTTPFetchResult &fetch_result)
{
	u64 caller = fetch_result.caller;
	if (caller != HTTPFETCH_DISCARD) {
		MutexAutoLock lock(g_httpfetch_mutex);
		g_httpfetch_results[caller].emplace(fetch_result);
	}
}

static void httpfetch_request_clear(u64 caller);

u64 httpfetch_caller_alloc()
{
	MutexAutoLock lock(g_httpfetch_mutex);

	// Check each caller ID except reserved ones
	for (u64 caller = HTTPFETCH_CID_START; caller != 0; ++caller) {
		auto it = g_httpfetch_results.find(caller);
		if (it == g_httpfetch_results.end()) {
			verbosestream << "httpfetch_caller_alloc: allocating "
					<< caller << std::endl;
			// Access element to create it
			g_httpfetch_results[caller];
			return caller;
		}
	}

	FATAL_ERROR("httpfetch_caller_alloc: ran out of caller IDs");
}

u64 httpfetch_caller_alloc_secure()
{
	MutexAutoLock lock(g_httpfetch_mutex);

	// Generate random caller IDs and make sure they're not
	// already used or reserved.
	// Give up after 100 tries to prevent infinite loop
	int tries = 100;
	u64 caller;

	do {
		// Global RNG is seeded securely, so we can use it.
		myrand_bytes(&caller, sizeof(caller));

		if (--tries < 1) {
			FATAL_ERROR("httpfetch_caller_alloc_secure: ran out of caller IDs");
			return HTTPFETCH_DISCARD;
		}
	} while (caller < HTTPFETCH_CID_START ||
		g_httpfetch_results.find(caller) != g_httpfetch_results.end());

	verbosestream << "httpfetch_caller_alloc_secure: allocating "
		<< caller << std::endl;

	// Access element to create it
	g_httpfetch_results[caller];
	return caller;
}

void httpfetch_caller_free(u64 caller)
{
	verbosestream<<"httpfetch_caller_free: freeing "
			<<caller<<std::endl;

	httpfetch_request_clear(caller);
	if (caller != HTTPFETCH_DISCARD) {
		MutexAutoLock lock(g_httpfetch_mutex);
		g_httpfetch_results.erase(caller);
	}
}

bool httpfetch_async_get(u64 caller, HTTPFetchResult &fetch_result)
{
	MutexAutoLock lock(g_httpfetch_mutex);

	// Check that caller exists
	auto it = g_httpfetch_results.find(caller);
	if (it == g_httpfetch_results.end())
		return false;

	// Check that result queue is nonempty
	std::queue<HTTPFetchResult> &caller_results = it->second;
	if (caller_results.empty())
		return false;

	// Pop first result
	fetch_result = std::move(caller_results.front());
	caller_results.pop();
	return true;
}

#if USE_CURL
#include <curl/curl.h>

/*
	USE_CURL is on: use cURL based httpfetch implementation
*/

static size_t httpfetch_writefunction(
		char *ptr, size_t size, size_t nmemb, void *userdata)
{
	auto *dest = reinterpret_cast<std::string*>(userdata);
	size_t count = size * nmemb;
	dest->append(ptr, count);
	return count;
}

static size_t httpfetch_discardfunction(
		char *ptr, size_t size, size_t nmemb, void *userdata)
{
	return size * nmemb;
}

class CurlHandlePool
{
	std::vector<CURL*> handles;

public:
	CurlHandlePool() = default;

	~CurlHandlePool()
	{
		for (CURL *it : handles) {
			curl_easy_cleanup(it);
		}
	}
	CURL * alloc()
	{
		CURL *curl;
		if (handles.empty()) {
			curl = curl_easy_init();
			if (!curl)
				throw std::bad_alloc();
		} else {
			curl = handles.back();
			handles.pop_back();
		}
		return curl;
	}
	void free(CURL *handle)
	{
		if (handle)
			handles.push_back(handle);
	}
};

class HTTPFetchOngoing
{
public:
	HTTPFetchOngoing(const HTTPFetchRequest &request, CurlHandlePool *pool);
	~HTTPFetchOngoing();

	CURLcode start(CURLM *multi);
	const HTTPFetchResult * complete(CURLcode res);

	const HTTPFetchRequest &getRequest()    const { return request; };
	const CURL             *getEasyHandle() const { return curl; };

private:
	CurlHandlePool *pool;
	CURL *curl = nullptr;
	CURLM *multi = nullptr;
	HTTPFetchRequest request;
	HTTPFetchResult result;
	struct curl_slist *http_header = nullptr;
	curl_mime *multipart_mime = nullptr;
};


HTTPFetchOngoing::HTTPFetchOngoing(const HTTPFetchRequest &request_,
		CurlHandlePool *pool_):
	pool(pool_),
	request(request_),
	result(request_)
{
	curl = pool->alloc();
	if (!curl)
		return;

	// Set static cURL options
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3);
	curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, ""); // = all supported ones

	std::string bind_address = g_settings->get("bind_address");
	if (!bind_address.empty()) {
		curl_easy_setopt(curl, CURLOPT_INTERFACE, bind_address.c_str());
	}

	if (!g_settings->getBool("enable_ipv6")) {
		curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
	}

	// Restrict protocols so that curl vulnerabilities in
	// other protocols don't affect us.
#if LIBCURL_VERSION_NUM >= 0x075500
	// These settings were introduced in curl 7.85.0.
	const char *protocols = "HTTP,HTTPS,FTP,FTPS";
	curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, protocols);
	curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, protocols);
#elif LIBCURL_VERSION_NUM >= 0x071304
	// These settings were introduced in curl 7.19.4, and later deprecated.
	long protocols =
		CURLPROTO_HTTP |
		CURLPROTO_HTTPS |
		CURLPROTO_FTP |
		CURLPROTO_FTPS;
	curl_easy_setopt(curl, CURLOPT_PROTOCOLS, protocols);
	curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, protocols);
#endif

	// Set cURL options based on HTTPFetchRequest
	curl_easy_setopt(curl, CURLOPT_URL,
			request.url.c_str());
	curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS,
			request.timeout);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS,
			request.connect_timeout);

	if (!request.useragent.empty())
		curl_easy_setopt(curl, CURLOPT_USERAGENT, request.useragent.c_str());

	// Set up a write callback that writes to the
	// result struct, unless the data is to be discarded
	if (request.caller == HTTPFETCH_DISCARD) {
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
				httpfetch_discardfunction);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, nullptr);
	} else {
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
				httpfetch_writefunction);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result.data);
	}

	// Configure the method
	switch (request.method) {
	default:
		assert(false);
	case HTTP_GET:
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1);
		break;
	case HTTP_HEAD:
		// This is kinda pointless right now, since we don't return response headers (TODO?)
		curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
		break;
	case HTTP_POST:
		curl_easy_setopt(curl, CURLOPT_POST, 1);
		break;
	case HTTP_PUT:
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
		break;
	case HTTP_PATCH:
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
		break;
	case HTTP_DELETE:
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
		break;
	}
	const bool has_request_body = request.method != HTTP_GET && request.method != HTTP_HEAD;

	// Set data from fields or raw_data
	if (request.multipart) {
		assert(has_request_body);
		multipart_mime = curl_mime_init(curl);
		for (auto &it : request.fields) {
			curl_mimepart *part = curl_mime_addpart(multipart_mime);
			curl_mime_name(part, it.first.c_str());
			curl_mime_data(part, it.second.c_str(), it.second.size());
		}
		curl_easy_setopt(curl, CURLOPT_MIMEPOST, multipart_mime);
	} else if (has_request_body) {
		if (request.fields.empty()) {
			// Note that we need to set this to an empty buffer (not NULL)
			// even if no data is to be sent.
			curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
					request.raw_data.size());
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS,
					request.raw_data.c_str());
		} else {
			std::string str;
			for (auto &field : request.fields) {
				if (!str.empty())
					str += "&";
				str += urlencode(field.first);
				str += "=";
				str += urlencode(field.second);
			}
			curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, str.size());
			curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, str.c_str());
		}
	}

	// Set additional HTTP headers
	for (const auto &s : request.extra_headers) {
		http_header = curl_slist_append(http_header, s.c_str());
	}
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http_header);

	if (!g_settings->getBool("curl_verify_cert")) {
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
	}
}

CURLcode HTTPFetchOngoing::start(CURLM *multi_)
{
	if (!curl)
		return CURLE_FAILED_INIT;

	if (!multi_) {
		// Easy interface (sync)
		return curl_easy_perform(curl);
	}

	// Multi interface (async)
	CURLMcode mres = curl_multi_add_handle(multi_, curl);
	if (mres != CURLM_OK) {
		errorstream << "curl_multi_add_handle"
			<< " returned error code " << mres
			<< std::endl;
		return CURLE_FAILED_INIT;
	}
	multi = multi_; // store for curl_multi_remove_handle
	return CURLE_OK;
}

const HTTPFetchResult * HTTPFetchOngoing::complete(CURLcode res)
{
	result.succeeded = (res == CURLE_OK);
	result.timeout = (res == CURLE_OPERATION_TIMEDOUT);

	// Get HTTP/FTP response code
	result.response_code = 0;
	if (curl && (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE,
				&result.response_code) != CURLE_OK)) {
		// We failed to get a return code, make sure it is still 0
		result.response_code = 0;
	}

	if (res != CURLE_OK) {
		errorstream << "HTTPFetch for " << request.url << " failed: "
			<< curl_easy_strerror(res);
		if (result.timeout)
			errorstream << " (timeout = " << request.timeout << "ms)" << std::endl;
		errorstream << std::endl;
	} else if (result.response_code >= 400) {
		errorstream << "HTTPFetch for " << request.url
			<< " returned response code " << result.response_code
			<< std::endl;
		if (result.caller == HTTPFETCH_PRINT_ERR && !result.data.empty()) {
			errorstream << "Response body:" << std::endl;
			safe_print_string(errorstream, result.data);
			errorstream << std::endl;
		}
	}

	return &result;
}

HTTPFetchOngoing::~HTTPFetchOngoing()
{
	if (multi) {
		// Note: this can block if curl is stuck waiting for DNS, see
		// <https://github.com/luanti-org/luanti/issues/16272>
		CURLMcode mres = curl_multi_remove_handle(multi, curl);
		if (mres != CURLM_OK) {
			errorstream << "curl_multi_remove_handle"
				<< " returned error code " << mres
				<< std::endl;
		}
	}

	// Set safe options for the reusable cURL handle
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
			httpfetch_discardfunction);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, nullptr);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, nullptr);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, nullptr);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, nullptr);
	if (http_header) {
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, nullptr);
		curl_slist_free_all(http_header);
	}
	if (multipart_mime) {
		curl_easy_setopt(curl, CURLOPT_MIMEPOST, nullptr);
		curl_mime_free(multipart_mime);
	}

	// Store the cURL handle for reuse
	pool->free(curl);
}


#if LIBCURL_VERSION_NUM >= 0x074200
#define HAVE_CURL_MULTI_POLL
#else
#undef HAVE_CURL_MULTI_POLL
#endif

class CurlFetchThread : public Thread
{
protected:
	enum RequestType {
		RT_FETCH,
		RT_CLEAR,
		RT_WAKEUP,
	};

	struct Request {
		RequestType type;
		HTTPFetchRequest fetch_request;
		Event *event = nullptr;
	};

	CURLM *m_multi;
	MutexedQueue<Request> m_requests;
	size_t m_parallel_limit;

	// Variables exclusively used within thread
	std::vector<std::unique_ptr<HTTPFetchOngoing>> m_all_ongoing;
	std::list<HTTPFetchRequest> m_queued_fetches;

public:
	CurlFetchThread(int parallel_limit) :
		Thread("CurlFetch")
	{
		if (parallel_limit >= 1)
			m_parallel_limit = parallel_limit;
		else
			m_parallel_limit = 1;
	}

	void requestFetch(const HTTPFetchRequest &fetch_request)
	{
		Request req;
		req.type = RT_FETCH;
		req.fetch_request = fetch_request;
		m_requests.push_back(std::move(req));
	}

	void requestClear(u64 caller, Event *event)
	{
		Request req;
		req.type = RT_CLEAR;
		req.fetch_request.caller = caller;
		req.event = event;
		m_requests.push_back(std::move(req));
	}

	void requestWakeUp()
	{
		Request req;
		req.type = RT_WAKEUP;
		m_requests.push_back(std::move(req));
	}

protected:
	// Handle a request from some other thread
	// E.g. new fetch; clear fetches for one caller; wake up
	void processRequest(Request &req)
	{
		if (req.type == RT_FETCH) {
			// New fetch, queue until there are less
			// than m_parallel_limit ongoing fetches
			m_queued_fetches.push_back(std::move(req.fetch_request));

			// see processQueued() for what happens next

		} else if (req.type == RT_CLEAR) {
			u64 caller = req.fetch_request.caller;

			// Abort all ongoing fetches for the caller
			for (auto it = m_all_ongoing.begin(); it != m_all_ongoing.end();) {
				if ((*it)->getRequest().caller == caller) {
					it = m_all_ongoing.erase(it);
				} else {
					++it;
				}
			}

			// Also abort all queued fetches for the caller
			for (auto it = m_queued_fetches.begin();
					it != m_queued_fetches.end();) {
				if ((*it).caller == caller)
					it = m_queued_fetches.erase(it);
				else
					++it;
			}
		} else if (req.type == RT_WAKEUP) {
			// Wakeup: Nothing to do, thread is awake at this point
		}

		if (req.event)
			req.event->signal();
	}

	// Start new ongoing fetches if m_parallel_limit allows
	void processQueued(CurlHandlePool *pool)
	{
		while (m_all_ongoing.size() < m_parallel_limit &&
				!m_queued_fetches.empty()) {
			HTTPFetchRequest request = std::move(m_queued_fetches.front());
			m_queued_fetches.pop_front();

			// Create ongoing fetch data and make a cURL handle
			// Set cURL options based on HTTPFetchRequest
			auto ongoing = std::make_unique<HTTPFetchOngoing>(request, pool);

			// Initiate the connection (curl_multi_add_handle)
			CURLcode res = ongoing->start(m_multi);
			if (res == CURLE_OK) {
				m_all_ongoing.push_back(std::move(ongoing));
			} else {
				httpfetch_deliver_result(*ongoing->complete(res));
			}
		}
	}

	// Process CURLMsg (indicates completion of a fetch)
	void processCurlMessage(CURLMsg *msg)
	{
		if (msg->msg != CURLMSG_DONE)
			return;
		// Determine which ongoing fetch the message pertains to
		for (auto it = m_all_ongoing.begin(); it != m_all_ongoing.end(); ++it) {
			auto &ongoing = **it;
			if (ongoing.getEasyHandle() != msg->easy_handle)
				continue;
			httpfetch_deliver_result(*ongoing.complete(msg->data.result));
			m_all_ongoing.erase(it);
			return;
		}
	}

	// Wait for a request from another thread, or timeout elapses
	void waitForRequest(long timeout)
	{
		if (m_queued_fetches.empty()) {
			try {
				Request req = m_requests.pop_front(timeout);
				processRequest(req);
			}
			catch (ItemNotFoundException &e) {}
		}
	}

	// Wait until some IO happens, or timeout elapses
	void waitForIO(long timeout)
	{
		CURLMcode mres;

#ifdef HAVE_CURL_MULTI_POLL
		mres = curl_multi_poll(m_multi, nullptr, 0, timeout, nullptr);

		if (mres != CURLM_OK) {
			errorstream << "curl_multi_poll returned error code "
				<< mres << std::endl;
		}
#else
		// If there's nothing to do curl_multi_wait() will immediately return
		// so we have to emulate the sleeping.

		fd_set dummy;
		int max_fd;
		mres = curl_multi_fdset(m_multi, &dummy, &dummy, &dummy, &max_fd);
		if (mres != CURLM_OK) {
			errorstream << "curl_multi_fdset returned error code "
				<< mres << std::endl;
			max_fd = -1;
		}

		if (max_fd == -1) { // curl has nothing to wait for
			if (timeout > 0)
				sleep_ms(timeout);
		} else {
			mres = curl_multi_wait(m_multi, nullptr, 0, timeout, nullptr);

			if (mres != CURLM_OK) {
				errorstream << "curl_multi_wait returned error code "
					<< mres << std::endl;
			}
		}
#endif
	}

	void *run()
	{
		CurlHandlePool pool;

		m_multi = curl_multi_init();
		FATAL_ERROR_IF(!m_multi, "curl_multi_init returned NULL");

		FATAL_ERROR_IF(!m_all_ongoing.empty(), "Expected empty");

		while (!stopRequested()) {
			BEGIN_DEBUG_EXCEPTION_HANDLER

			const u64 t0 = porting::getTimeMs();

			/*
				Handle new async requests
			*/

			while (!m_requests.empty()) {
				Request req = m_requests.pop_frontNoEx();
				processRequest(req);
			}
			processQueued(&pool);

			/*
				Handle ongoing async requests
			*/

			int still_ongoing = 0;
			while (curl_multi_perform(m_multi, &still_ongoing) ==
					CURLM_CALL_MULTI_PERFORM)
				/* noop */;

			/*
				Handle completed async requests
			*/
			if (still_ongoing < (int) m_all_ongoing.size()) {
				CURLMsg *msg;
				int msgs_in_queue;
				msg = curl_multi_info_read(m_multi, &msgs_in_queue);
				while (msg != NULL) {
					processCurlMessage(msg);
					msg = curl_multi_info_read(m_multi, &msgs_in_queue);
				}
			}

			/*
				If we took suspiciously long, warn.
			*/
			const u64 tdelta = porting::getTimeMs() - t0;
			if (tdelta > 300) {
				warningstream << "CurlFetchThread blocked for " << tdelta << "ms"
					<< std::endl;
			}


			/*
				If there are ongoing requests, wait for data
				(with a timeout of 100ms so that new requests
				can be processed).

				If no ongoing requests, wait for a new request.
				(Possibly an empty request that signals
				that the thread should be stopped.)
			*/
			if (m_all_ongoing.empty())
				waitForRequest(100000000);
			else
				waitForIO(100);

			END_DEBUG_EXCEPTION_HANDLER
		}

		// Call curl_multi_remove_handle and cleanup easy handles
		m_all_ongoing.clear();

		m_queued_fetches.clear();

		CURLMcode mres = curl_multi_cleanup(m_multi);
		if (mres != CURLM_OK) {
			errorstream<<"curl_multi_cleanup"
				<<" returned error code "<<mres
				<<std::endl;
		}

		return NULL;
	}
};

static std::unique_ptr<CurlFetchThread> g_httpfetch_thread;

void httpfetch_init(int parallel_limit)
{
	FATAL_ERROR_IF(g_httpfetch_thread, "httpfetch_init called twice");

	verbosestream<<"httpfetch_init: parallel_limit="<<parallel_limit
			<<std::endl;

	CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
	FATAL_ERROR_IF(res != CURLE_OK, "cURL init failed");

	g_httpfetch_thread = std::make_unique<CurlFetchThread>(parallel_limit);
}

void httpfetch_cleanup()
{
	verbosestream<<"httpfetch_cleanup: cleaning up"<<std::endl;

	if (g_httpfetch_thread) {
		g_httpfetch_thread->stop();
		g_httpfetch_thread->requestWakeUp();
		g_httpfetch_thread->wait();
		g_httpfetch_thread.reset();
	}

	curl_global_cleanup();
}

void httpfetch_async(const HTTPFetchRequest &fetch_request)
{
	g_httpfetch_thread->requestFetch(fetch_request);
	if (!g_httpfetch_thread->isRunning())
		g_httpfetch_thread->start();
}

static void httpfetch_request_clear(u64 caller)
{
	g_httpfetch_thread->requestClear(caller, nullptr);
}

bool httpfetch_sync_interruptible(const HTTPFetchRequest &fetch_request,
		HTTPFetchResult &fetch_result, long interval)
{
	if (const Thread *thread = Thread::getCurrentThread()) {
		HTTPFetchRequest req = fetch_request;
		req.caller = httpfetch_caller_alloc_secure();
		httpfetch_async(req);
		do {
			if (thread->stopRequested()) {
				httpfetch_caller_free(req.caller);
				fetch_result = HTTPFetchResult(fetch_request);
				return false;
			}
			sleep_ms(interval);
		} while (!httpfetch_async_get(req.caller, fetch_result));
		httpfetch_caller_free(req.caller);
	} else {
		throw ModError(std::string("You have tried to execute a synchronous HTTP request on the main thread! "
				"This offense shall be punished. (").append(fetch_request.url).append(")"));
	}
	return true;
}

#else  // USE_CURL

/*
	USE_CURL is off:

	Dummy httpfetch implementation that always returns an error.
*/

void httpfetch_init(int parallel_limit)
{
}

void httpfetch_cleanup()
{
}

void httpfetch_async(const HTTPFetchRequest &fetch_request)
{
	errorstream << "httpfetch_async: unable to fetch " << fetch_request.url
			<< " because USE_CURL=0" << std::endl;

	HTTPFetchResult fetch_result(fetch_request); // sets succeeded = false etc.
	httpfetch_deliver_result(fetch_result);
}

static void httpfetch_request_clear(u64 caller)
{
}

bool httpfetch_sync_interruptible(const HTTPFetchRequest &fetch_request,
		HTTPFetchResult &fetch_result, long interval)
{
	errorstream << "httpfetch_sync_interruptible: unable to fetch " << fetch_request.url
			<< " because USE_CURL=0" << std::endl;

	fetch_result = HTTPFetchResult(fetch_request); // sets succeeded = false etc.
	return false;
}

#endif  // USE_CURL
