// SPDX-FileCopyrightText: 2026 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "sscsm_worker_entry.h"

#include "log.h"
#include "sscsm_environment.h"
#include "threading/ipc_channel.h"

#if !defined(_WIN32)
#include <unistd.h>
#include <dirent.h>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <sys/syscall.h>
#endif

// Close every inherited file descriptor except stdio (0, 1, 2).
//
// On POSIX, posix_spawn inherits all fds that don't have FD_CLOEXEC set.
// Luanti's sockets and log file aren't currently opened with CLOEXEC, so
// the SSCSM worker inherits them — which defeats process isolation. We
// close them here as a belt-and-suspenders measure on top of any future
// CLOEXEC hygiene fix in the broader codebase.
//
// Windows uses explicit handle inheritance via CreateProcess, not fd-based
// inheritance, so there's nothing to do.
static void close_inherited_fds() noexcept
{
#if defined(_WIN32)
	// Nothing to do — handle inheritance is opt-in on Windows.
#else
	// Try the close_range syscall first (Linux 5.9+ / FreeBSD).
	// This is the fastest path and avoids walking /proc.
#if defined(__linux__) && defined(SYS_close_range)
	if (syscall(SYS_close_range, 3U, ~0U, 0U) == 0)
		return;
#endif

	// Fall back to walking /proc/self/fd (Linux) or /dev/fd (others).
	const char *fd_dir_paths[] = {
		"/proc/self/fd",
		"/dev/fd",
		nullptr,
	};
	for (const char **p = fd_dir_paths; *p; ++p) {
		DIR *dir = opendir(*p);
		if (!dir)
			continue;
		int dir_fd = dirfd(dir);
		std::vector<int> to_close;
		struct dirent *ent;
		while ((ent = readdir(dir)) != nullptr) {
			if (ent->d_name[0] == '.')
				continue;
			int fd = std::atoi(ent->d_name);
			if (fd >= 3 && fd != dir_fd)
				to_close.push_back(fd);
		}
		closedir(dir);
		for (int fd : to_close)
			close(fd);
		return;
	}

	// Last resort: brute-force close every fd up to the process limit.
	long max_fd = sysconf(_SC_OPEN_MAX);
	if (max_fd <= 0 || max_fd > 1048576)
		max_fd = 1024;
	for (int fd = 3; fd < max_fd; ++fd)
		close(fd);
#endif
}

int run_sscsm_worker(const std::string &shm_name)
{
	// Drop any fds the parent passed us — before we do anything else.
	close_inherited_fds();

	auto resources = IPCChannelResourcesShm::makeSecond(shm_name);
	if (!resources) {
		errorstream << "sscsm-worker: failed to attach to shm '"
				<< shm_name << "'" << std::endl;
		return 1;
	}

	auto end_b = IPCChannelEnd::makeB(std::move(resources));

	try {
		SSCSMEnvironment env(std::move(end_b));
		env.run();
	} catch (std::exception &e) {
		errorstream << "sscsm-worker: unhandled exception: " << e.what()
				<< std::endl;
		return 2;
	}

	return 0;
}
