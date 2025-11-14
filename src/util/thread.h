// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2010-2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <utility> // std::move
#include "irrlichttypes.h"
#include "threading/thread.h"
#include "threading/mutex_auto_lock.h"
#include "container.h"

template<typename T>
class MutexedVariable
{
public:
	// default initialization
	MutexedVariable() {}

	MutexedVariable(const T &value):
		m_value(value) {}
	MutexedVariable(T &&value):
		m_value(std::move(value)) {}

	T get()
	{
		MutexAutoLock lock(m_mutex);
		return m_value;
	}

	void set(const T &value)
	{
		MutexAutoLock lock(m_mutex);
		m_value = value;
	}

	void set(T &&value)
	{
		MutexAutoLock lock(m_mutex);
		m_value = std::move(value);
	}

private:
	T m_value;
	std::mutex m_mutex;
};

/*
	A single worker thread - multiple client threads queue framework.
*/
template<typename Key, typename T, typename Caller, typename CallerData>
class GetResult {
public:
	Key key;
	T item;
	std::pair<Caller, CallerData> caller;
};

template<typename Key, typename T, typename Caller, typename CallerData>
class ResultQueue : public MutexedQueue<GetResult<Key, T, Caller, CallerData> > {
};

template<typename Caller, typename Data, typename Key, typename T>
class CallerInfo {
public:
	Caller caller;
	Data data;
	ResultQueue<Key, T, Caller, Data> *dest;
};

template<typename Key, typename T, typename Caller, typename CallerData>
class GetRequest {
public:
	typedef CallerInfo<Caller, CallerData, Key, T> caller_info_type;

	GetRequest() = default;
	~GetRequest() = default;

	GetRequest(const Key &a_key): key(a_key)
	{
	}

	Key key;
	std::vector<caller_info_type> callers;
};

/**
 * Notes for RequestQueue usage
 * @param Key unique key to identify a request for a specific resource
 * @param T data passed back to caller
 * @param Caller unique id of calling thread
 * @param CallerData additional data provided by caller
 */
template<typename Key, typename T, typename Caller, typename CallerData>
class RequestQueue {
public:
	typedef GetRequest<Key, T, Caller, CallerData> request_type;
	typedef GetResult<Key, T, Caller, CallerData> result_type;
	typedef ResultQueue<Key, T, Caller, CallerData> result_queue_type;

	bool empty() const
	{
		return m_queue.empty();
	}

	void add(const Key &key, Caller caller, CallerData callerdata,
		result_queue_type *dest)
	{
		{
			MutexAutoLock lock(m_queue.getMutex());

			for (auto &request : m_queue.getQueue()) {
				if (request.key != key)
					continue;

				// If the caller is already on the list, only update CallerData
				for (auto &ca : request.callers) {
					if (ca.caller == caller) {
						ca.data = callerdata;
						return;
					}
				}

				// Or add this caller
				typename request_type::caller_info_type ca;
				ca.caller = caller;
				ca.data = callerdata;
				ca.dest = dest;
				request.callers.push_back(std::move(ca));
				return;
			}
		}

		// Else add a new request to the queue
		request_type request;
		request.key = key;
		typename request_type::caller_info_type ca;
		ca.caller = caller;
		ca.data = callerdata;
		ca.dest = dest;
		request.callers.push_back(std::move(ca));

		m_queue.push_back(std::move(request));
	}

	request_type pop(unsigned int timeout_ms)
	{
		return m_queue.pop_front(timeout_ms);
	}

	request_type pop()
	{
		return m_queue.pop_frontNoEx();
	}

	void pushResult(const request_type &req, const T &res)
	{
		for (auto &ca : req.callers) {
			result_type result;

			result.key = req.key;
			result.item = res;
			result.caller.first = ca.caller;
			result.caller.second = ca.data;

			ca.dest->push_back(std::move(result));
		}
	}

private:
	MutexedQueue<request_type> m_queue;
};

class UpdateThread : public Thread
{
public:
	UpdateThread(const std::string &name) : Thread(name + "Update") {}
	~UpdateThread() = default;

	void deferUpdate() { m_update_sem.post(); }

	void stop()
	{
		Thread::stop();

		// give us a nudge
		m_update_sem.post();
	}

	void *run()
	{
		BEGIN_DEBUG_EXCEPTION_HANDLER

		while (!stopRequested()) {
			m_update_sem.wait();
			// Set semaphore to 0
			while (m_update_sem.wait(0));

			if (stopRequested()) break;

			doUpdate();
		}

		END_DEBUG_EXCEPTION_HANDLER

		return NULL;
	}

protected:
	virtual void doUpdate() = 0;

private:
	Semaphore m_update_sem;
};
