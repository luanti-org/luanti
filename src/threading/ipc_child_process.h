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
	// inherit_fd: if >= 0, dup2'd to fd 3 in the child via posix_spawn
	// file_actions (used by the Android SSCSM worker spawn path that
	// has to pass a memory fd rather than a shared name). -1 = no
	// inheritance.
	explicit IPCChildProcess(const std::vector<std::string> &args,
			int inherit_fd = -1);

	// Spawn a specific executable.
	IPCChildProcess(const std::string &exec_path,
			const std::vector<std::string> &args,
			int inherit_fd = -1);

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

	// Exit code if the child has exited normally, or -1 if not yet / killed
	// by signal / error. When -1, getTermSignal() may give the signal number.
	int getExitCode() const noexcept { return m_exit_code; }

	// Signal number that terminated the child (POSIX), or 0 if the child
	// exited normally / hasn't exited / wasn't seen. Use this to
	// distinguish "exited via SIGSEGV" from "exited via SIGSYS" etc.
	int getTermSignal() const noexcept { return m_term_signal; }

	// Set the grace period used by the destructor before terminate() is
	// called. The default is DEFAULT_WAIT_MS. Set to 0 to terminate
	// immediately in the destructor.
	void setDestructorTimeoutMs(int ms) noexcept { m_destructor_timeout_ms = ms; }

private:
	void spawnInternal(const std::string &exec_path,
			const std::vector<std::string> &args,
			int inherit_fd);

#if defined(_WIN32)
	void *m_process_handle = nullptr; // HANDLE
#else
	int m_pid = -1;
	bool m_reaped = false;
#endif
	int m_exit_code = -1;
	int m_term_signal = 0;
	int m_destructor_timeout_ms = DEFAULT_WAIT_MS;
};
