// SPDX-FileCopyrightText: 2026 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "ipc_child_process.h"

#include "log.h"
#include "porting.h"
#include <cstring>
#include <thread>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <spawn.h>
#include <unistd.h>
#include <errno.h>

extern char **environ;
#endif

static std::string getExecPath()
{
	char buf[4096];
	if (!porting::getCurrentExecPath(buf, sizeof(buf))) {
		errorstream << "IPCChildProcess: failed to resolve current exec path"
				<< std::endl;
		return {};
	}
	return buf;
}

IPCChildProcess::IPCChildProcess(const std::vector<std::string> &args)
	: IPCChildProcess(getExecPath(), args)
{
}

IPCChildProcess::IPCChildProcess(const std::string &exec_path,
		const std::vector<std::string> &args)
{
	if (!exec_path.empty())
		spawnInternal(exec_path, args);
}

IPCChildProcess::~IPCChildProcess()
{
	if (!isValid())
		return;

	if (!waitWithTimeout(m_destructor_timeout_ms)) {
		warningstream << "IPCChildProcess: child did not exit within "
				<< m_destructor_timeout_ms << " ms, terminating"
				<< std::endl;
		terminate();
		// Give it a brief moment to die before we give up
		if (!waitWithTimeout(1000)) {
#if !defined(_WIN32)
			// SIGTERM may have been caught (e.g. if the child installed
			// a graceful-shutdown handler). Escalate to SIGKILL.
			warningstream << "IPCChildProcess: child ignored SIGTERM, "
					"escalating to SIGKILL" << std::endl;
			if (m_pid > 0 && !m_reaped)
				kill(m_pid, SIGKILL);
			(void)waitWithTimeout(1000);
#endif
		}
	}

#if defined(_WIN32)
	if (m_process_handle) {
		CloseHandle(m_process_handle);
		m_process_handle = nullptr;
	}
#else
	if (!m_reaped && m_pid > 0) {
		// Reap anyway to avoid zombie
		int status = 0;
		(void)waitpid(m_pid, &status, WNOHANG);
	}
#endif
}

IPCChildProcess::IPCChildProcess(IPCChildProcess &&other) noexcept
{
	*this = std::move(other);
}

IPCChildProcess &IPCChildProcess::operator=(IPCChildProcess &&other) noexcept
{
	if (this == &other)
		return *this;

#if defined(_WIN32)
	m_process_handle = other.m_process_handle;
	other.m_process_handle = nullptr;
#else
	m_pid = other.m_pid;
	m_reaped = other.m_reaped;
	other.m_pid = -1;
	other.m_reaped = true;
#endif
	m_exit_code = other.m_exit_code;
	m_destructor_timeout_ms = other.m_destructor_timeout_ms;
	return *this;
}

bool IPCChildProcess::isValid() const noexcept
{
#if defined(_WIN32)
	return m_process_handle != nullptr;
#else
	return m_pid > 0 && !m_reaped;
#endif
}

#if defined(_WIN32)

void IPCChildProcess::spawnInternal(const std::string &exec_path,
		const std::vector<std::string> &args)
{
	// Build command line: "exec_path" arg1 arg2 ...
	std::string cmdline;
	auto append_quoted = [&](const std::string &s) {
		if (!cmdline.empty())
			cmdline += ' ';
		cmdline += '"';
		for (char c : s) {
			if (c == '"' || c == '\\')
				cmdline += '\\';
			cmdline += c;
		}
		cmdline += '"';
	};
	append_quoted(exec_path);
	for (const auto &a : args)
		append_quoted(a);

	STARTUPINFOA si{};
	si.cb = sizeof(si);
	PROCESS_INFORMATION pi{};

	std::vector<char> cmdline_buf(cmdline.begin(), cmdline.end());
	cmdline_buf.push_back('\0');

	if (!CreateProcessA(exec_path.c_str(), cmdline_buf.data(),
			nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
		errorstream << "IPCChildProcess: CreateProcessA failed: "
				<< GetLastError() << std::endl;
		return;
	}

	m_process_handle = pi.hProcess;
	CloseHandle(pi.hThread);
}

bool IPCChildProcess::waitWithTimeout(int timeout_ms) noexcept
{
	if (!m_process_handle)
		return true; // already done

	DWORD wait = timeout_ms < 0 ? INFINITE : static_cast<DWORD>(timeout_ms);
	DWORD r = WaitForSingleObject(m_process_handle, wait);
	if (r == WAIT_OBJECT_0) {
		DWORD code = 0;
		GetExitCodeProcess(m_process_handle, &code);
		m_exit_code = static_cast<int>(code);
		CloseHandle(m_process_handle);
		m_process_handle = nullptr;
		return true;
	}
	return false;
}

void IPCChildProcess::terminate() noexcept
{
	if (m_process_handle)
		TerminateProcess(m_process_handle, 1);
}

#else // POSIX

void IPCChildProcess::spawnInternal(const std::string &exec_path,
		const std::vector<std::string> &args)
{
	// Build argv: [exec_path, args..., nullptr]
	std::vector<std::string> storage;
	storage.reserve(args.size() + 1);
	storage.push_back(exec_path);
	for (const auto &a : args)
		storage.push_back(a);

	std::vector<char *> argv;
	argv.reserve(storage.size() + 1);
	for (auto &s : storage)
		argv.push_back(s.data());
	argv.push_back(nullptr);

	pid_t pid = 0;
	int rc = posix_spawn(&pid, exec_path.c_str(), nullptr, nullptr,
			argv.data(), environ);
	if (rc != 0) {
		errorstream << "IPCChildProcess: posix_spawn failed: "
				<< std::strerror(rc) << std::endl;
		return;
	}
	m_pid = pid;
}

bool IPCChildProcess::waitWithTimeout(int timeout_ms) noexcept
{
	if (m_pid <= 0 || m_reaped)
		return true;

	// waitpid doesn't have a native timeout. Poll with WNOHANG + sleep.
	// For infinite wait, block directly.
	if (timeout_ms < 0) {
		int status = 0;
		pid_t r;
		do {
			r = waitpid(m_pid, &status, 0);
		} while (r < 0 && errno == EINTR);
		if (r == m_pid) {
			m_reaped = true;
			if (WIFEXITED(status))
				m_exit_code = WEXITSTATUS(status);
			else
				m_exit_code = -1;
			return true;
		}
		return false;
	}

	// Polling loop with small sleep intervals.
	// Do at least one WNOHANG probe before sleeping, so timeout_ms=0
	// works as a non-blocking "is it dead right now?" check.
	const int step_ms = 10;
	int elapsed = 0;
	while (true) {
		int status = 0;
		pid_t r = waitpid(m_pid, &status, WNOHANG);
		if (r == m_pid) {
			m_reaped = true;
			if (WIFEXITED(status))
				m_exit_code = WEXITSTATUS(status);
			else
				m_exit_code = -1;
			return true;
		}
		if (r < 0 && errno != EINTR)
			return false;
		if (elapsed >= timeout_ms)
			return false;
		int sleep_for = std::min(step_ms, timeout_ms - elapsed);
		std::this_thread::sleep_for(std::chrono::milliseconds(sleep_for));
		elapsed += sleep_for;
	}
}

void IPCChildProcess::terminate() noexcept
{
	if (m_pid > 0 && !m_reaped)
		kill(m_pid, SIGTERM);
}

#endif
