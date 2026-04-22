// SPDX-FileCopyrightText: 2026 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "sscsm_worker_entry.h"

#include "log.h"
#include "log_internal.h"
#include "sscsm_environment.h"
#include "threading/ipc_channel.h"

#include <csignal>

#if !defined(_WIN32)
#include <unistd.h>
#include <dirent.h>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <sys/socket.h>
#include <sys/syscall.h>
#endif

#if defined(__linux__)
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <stddef.h>
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

// Install a minimal seccomp-bpf sandbox on the worker process.
//
// This is a *denylist*: everything is allowed except a small set of
// syscalls that the SSCSM worker should never need and that would be
// obvious footholds for a sandbox escape:
//
//   execve / execveat  — no executing new programs
//   socket / socketpair — no network or unix sockets
//   fork / vfork / clone / clone3 — no spawning processes or threads
//   ptrace             — no attaching to other processes
//
// We use a denylist rather than an allowlist because LuaJIT and glibc
// touch a moving target of syscalls (allocator internals, JIT page
// management, etc.) and an allowlist would be brittle across kernel /
// libc versions. The denylist covers the real threats (RCE pivot,
// lateral movement, network exfiltration) without second-guessing
// what the runtime needs to function.
//
// Violations terminate the worker with SIGSYS. The kernel logs the
// offending syscall number via audit so regressions are diagnosable.
//
// Non-Linux platforms get a no-op for now; each needs its own
// equivalent (macOS sandboxd profile, Windows AppContainer, etc.).
static void install_sandbox() noexcept
{
#if defined(__linux__)
	// Determine this architecture's audit constant. If we don't have
	// a mapping for the current arch, we can't safely install a filter
	// (the numbers in <sys/syscall.h> would be relative to our arch
	// but an emulated syscall from a different ABI would slip past).
	// In that case we skip and log.
#if defined(__x86_64__)
	constexpr unsigned int expected_arch = AUDIT_ARCH_X86_64;
#elif defined(__aarch64__)
	constexpr unsigned int expected_arch = AUDIT_ARCH_AARCH64;
#elif defined(__i386__)
	constexpr unsigned int expected_arch = AUDIT_ARCH_I386;
#elif defined(__arm__)
	constexpr unsigned int expected_arch = AUDIT_ARCH_ARM;
#else
	warningstream << "sscsm-worker: no seccomp arch mapping for this "
			"platform, skipping sandbox" << std::endl;
	return;
#endif

	// Required before installing a seccomp filter without CAP_SYS_ADMIN.
	if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
		warningstream << "sscsm-worker: PR_SET_NO_NEW_PRIVS failed: "
				<< std::strerror(errno) << std::endl;
		return;
	}

#define DENY(nr) \
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, (nr), 0, 1), \
		BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS)

	struct sock_filter filter[] = {
		// Reject calls from unexpected ABIs (e.g. x32 on x86_64)
		BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
				offsetof(struct seccomp_data, arch)),
		BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, expected_arch, 1, 0),
		BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),

		// Load syscall number for the checks below
		BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
				offsetof(struct seccomp_data, nr)),

#ifdef __NR_execve
		DENY(__NR_execve),
#endif
#ifdef __NR_execveat
		DENY(__NR_execveat),
#endif
#ifdef __NR_socket
		DENY(__NR_socket),
#endif
#ifdef __NR_socketpair
		DENY(__NR_socketpair),
#endif
#ifdef __NR_fork
		DENY(__NR_fork),
#endif
#ifdef __NR_vfork
		DENY(__NR_vfork),
#endif
#ifdef __NR_clone
		DENY(__NR_clone),
#endif
#ifdef __NR_clone3
		DENY(__NR_clone3),
#endif
#ifdef __NR_ptrace
		DENY(__NR_ptrace),
#endif

		// Default: allow
		BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
	};
#undef DENY

	struct sock_fprog prog = {
		.len = static_cast<unsigned short>(
				sizeof(filter) / sizeof(filter[0])),
		.filter = filter,
	};

	if (syscall(SYS_seccomp, SECCOMP_SET_MODE_FILTER, 0, &prog) != 0) {
		// Fall back to prctl interface on older kernels.
		if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog) != 0) {
			warningstream << "sscsm-worker: seccomp install failed: "
					<< std::strerror(errno) << std::endl;
			return;
		}
	}
	verbosestream << "sscsm-worker: seccomp denylist sandbox installed"
			<< std::endl;
#endif
}

int run_sscsm_worker(const std::string &shm_name)
{
	// main() registered this thread as "Main" before the worker shortcut;
	// rename so worker log lines are distinguishable from the parent's.
	g_logger.registerThread("SSCSMWorker");

	// main()'s signal_handler_init installed a graceful-shutdown handler
	// for SIGINT/SIGTERM that just sets a flag. The worker doesn't poll
	// that flag, so it would ignore the parent's SIGTERM on shutdown —
	// an orphan'd worker would survive the parent's exit. Restore the
	// default disposition so SIGTERM just kills us.
	std::signal(SIGINT, SIG_DFL);
	std::signal(SIGTERM, SIG_DFL);

	// main() wired stderr at LL_ACTION before forking into the worker.
	// Bump to LL_INFO + LL_VERBOSE so the worker's load messages, Lua
	// errors, and sandbox notices are visible on the parent's terminal.
	// The worker has no debug.txt; stderr is our only channel.
	g_logger.addOutput(&stderr_output, LL_INFO);
	g_logger.addOutput(&stderr_output, LL_VERBOSE);

	// Drop any fds the parent passed us — before we do anything else.
	close_inherited_fds();

	// Lock down syscall surface before allocating anything or running
	// Lua code. If a bug in LuaJIT or our own code escapes the sandbox
	// in-process, it still can't exec, connect to the network, or
	// spawn children.
	install_sandbox();

#if !defined(_WIN32)
	// Sanity check: LUANTI_SSCSM_SECCOMP_TEST=1 attempts a blocked syscall
	// (socket(2)) to verify the filter is live. If seccomp is working the
	// kernel delivers SIGSYS and the worker dies before the next line; if
	// we reach the errorstream below, the sandbox is broken.
	if (std::getenv("LUANTI_SSCSM_SECCOMP_TEST")) {
		warningstream << "sscsm-worker: seccomp self-test — "
				"attempting blocked socket(AF_INET, ...)" << std::endl;
		int s = socket(AF_INET, SOCK_STREAM, 0);
		errorstream << "sscsm-worker: seccomp SELF-TEST FAILED — socket() "
				"returned " << s << " (errno=" << errno << "); "
				"sandbox is NOT enforcing denylist" << std::endl;
		return 3;
	}
#endif

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
