// SPDX-FileCopyrightText: 2026 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "util/basic_macros.h"
#include <string>
#include <vector>

/*
	Cross-platform child process handle for spawning worker processes
	(such as the SSCSM worker). Uses posix_spawn on POSIX and
	CreateProcessA on Windows.

	The executable path defaults to the current process's executable
	(via porting::getCurrentExecPath). Args are passed as given, with
	argv[0] set to the executable path.

	Destructor behavior:
	1. If the process is still running, wait for it for up to
	   wait_ms_abs milliseconds (default: 120000 = 2 minutes).
	2. If it's still running after the wait, send SIGTERM (POSIX) /
	   TerminateProcess (Win32) and wait briefly.
	3. Reap the exit status (waitpid / WaitForSingleObject+CloseHandle)
	   so there's no zombie left behind.
*/
class IPCChildProcess
{
public:
	// Default grace period before we kill a stuck child in the destructor.
	static constexpr int DEFAULT_WAIT_MS = 2 * 60 * 1000;

	// Spawn a child process using the current executable and the given args.
	// On failure, isValid() returns false.
	// `args` should not include argv[0]; it is set automatically.
	explicit IPCChildProcess(const std::vector<std::string> &args);

	// Spawn a specific executable.
	IPCChildProcess(const std::string &exec_path,
			const std::vector<std::string> &args);

	~IPCChildProcess();

	DISABLE_CLASS_COPY(IPCChildProcess)

	IPCChildProcess(IPCChildProcess &&other) noexcept;
	IPCChildProcess &operator=(IPCChildProcess &&other) noexcept;

	bool isValid() const noexcept;

	// Wait for the child to exit, up to timeout_ms (-1 = forever).
	// Returns true if the child has exited (and the exit code is available).
	// Returns false on timeout.
	[[nodiscard]]
	bool waitWithTimeout(int timeout_ms) noexcept;

	// If the child is still running, send termination signal.
	// Safe to call multiple times.
	void terminate() noexcept;

	// Exit code if the child has exited, or -1 if not yet / killed / error.
	int getExitCode() const noexcept { return m_exit_code; }

	// Set the grace period used by the destructor before terminate() is
	// called. The default is DEFAULT_WAIT_MS. Set to 0 to terminate
	// immediately in the destructor.
	void setDestructorTimeoutMs(int ms) noexcept { m_destructor_timeout_ms = ms; }

private:
	void spawnInternal(const std::string &exec_path,
			const std::vector<std::string> &args);

#if defined(_WIN32)
	void *m_process_handle = nullptr; // HANDLE
#else
	int m_pid = -1;
	bool m_reaped = false;
#endif
	int m_exit_code = -1;
	int m_destructor_timeout_ms = DEFAULT_WAIT_MS;
};
