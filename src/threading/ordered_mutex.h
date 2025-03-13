// Minetest
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <cstdint>
#include <condition_variable>

/*
	Fair mutex based on ticketing approach.
	Satisfies `Mutex` C++11 requirements.
*/
class ordered_mutex {
public:
	ordered_mutex() : next_ticket(0), counter(0) {}

	void lock()
	{
		std::unique_lock autolock(cv_lock);
		const auto ticket = next_ticket++;
		cv.wait(autolock, [&] { return counter == ticket; });
	}

	bool try_lock()
	{
		std::lock_guard autolock(cv_lock);
		if (counter != next_ticket)
			return false;
		next_ticket++;
		return true;
	}

	void unlock()
	{
		{
			std::lock_guard autolock(cv_lock);
			counter++;
		}
		cv.notify_all(); // intentionally outside lock
	}

private:
	std::condition_variable cv;
	std::mutex cv_lock;
	uint_fast32_t next_ticket, counter;
};
