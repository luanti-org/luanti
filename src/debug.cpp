// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>


#include "porting.h"
#include "debug.h"
#include "exceptions.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <thread>
#include "threading/mutex_auto_lock.h"
#include "config.h"

#ifdef _MSC_VER
	#include <dbghelp.h>
	#include <windows.h>
	#include <eh.h>
	#include "version.h"
	#include "filesys.h"
#endif

#if USE_CURSES
	#include "terminal_chat_console.h"
#endif

/*
	Assert
*/

void sanity_check_fn(const char *assertion, const char *file,
		unsigned int line, const char *function)
{
#if USE_CURSES
	g_term_console.stopAndWaitforThread();
#endif

	errorstream << std::endl << "In thread " << std::hex
		<< std::this_thread::get_id() << ":\n" << std::dec;
	errorstream << file << ":" << line << ": " << function
		<< ": An engine assumption '" << assertion << "' failed." << std::endl;

	abort();
}

void fatal_error_fn(const char *msg, const char *file,
		unsigned int line, const char *function)
{
#if USE_CURSES
	g_term_console.stopAndWaitforThread();
#endif

	errorstream << std::endl << "In thread " << std::hex
		<< std::this_thread::get_id() << ":\n" << std::dec;
	errorstream << file << ":" << line << ": " << function
		<< ": A fatal error occurred: " << msg << std::endl;

	abort();
}

std::string debug_describe_exc(const std::exception &e)
{
	if (dynamic_cast<const std::bad_alloc*>(&e))
		return "C++ out of memory";
	return std::string("\"").append(e.what()).append("\"");
}

#ifdef _MSC_VER

const char *Win32ExceptionCodeToString(DWORD exception_code)
{
	switch (exception_code) {
	case EXCEPTION_ACCESS_VIOLATION:
		return "Access violation";
	case EXCEPTION_DATATYPE_MISALIGNMENT:
		return "Misaligned data access";
	case EXCEPTION_BREAKPOINT:
		return "Breakpoint reached";
	case EXCEPTION_SINGLE_STEP:
		return "Single debug step";
	case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
		return "Array access out of bounds";
	case EXCEPTION_FLT_DENORMAL_OPERAND:
		return "Denormal floating point operand";
	case EXCEPTION_FLT_DIVIDE_BY_ZERO:
		return "Floating point division by zero";
	case EXCEPTION_FLT_INEXACT_RESULT:
		return "Inaccurate floating point result";
	case EXCEPTION_FLT_INVALID_OPERATION:
		return "Invalid floating point operation";
	case EXCEPTION_FLT_OVERFLOW:
		return "Floating point exponent overflow";
	case EXCEPTION_FLT_STACK_CHECK:
		return "Floating point stack overflow or underflow";
	case EXCEPTION_FLT_UNDERFLOW:
		return "Floating point exponent underflow";
	case EXCEPTION_INT_DIVIDE_BY_ZERO:
		return "Integer division by zero";
	case EXCEPTION_INT_OVERFLOW:
		return "Integer overflow";
	case EXCEPTION_PRIV_INSTRUCTION:
		return "Privileged instruction executed";
	case EXCEPTION_IN_PAGE_ERROR:
		return "Could not access or load page";
	case EXCEPTION_ILLEGAL_INSTRUCTION:
		return "Illegal instruction encountered";
	case EXCEPTION_NONCONTINUABLE_EXCEPTION:
		return "Attempted to continue after fatal exception";
	case EXCEPTION_STACK_OVERFLOW:
		return "Stack overflow";
	case EXCEPTION_INVALID_DISPOSITION:
		return "Invalid disposition returned to the exception dispatcher";
	case EXCEPTION_GUARD_PAGE:
		return "Attempted guard page access";
	case EXCEPTION_INVALID_HANDLE:
		return "Invalid handle";
	}

	return "Unknown exception";
}

long WINAPI Win32ExceptionHandler(struct _EXCEPTION_POINTERS *pExceptInfo)
{
	char buf[512];
	MINIDUMP_EXCEPTION_INFORMATION mdei;
	MINIDUMP_USER_STREAM_INFORMATION mdusi;
	MINIDUMP_USER_STREAM mdus;
	bool minidump_created = false;

	std::string dumpfile = porting::path_user + DIR_DELIM PROJECT_NAME ".dmp";

	std::string version_str(PROJECT_NAME " ");
	version_str += g_version_hash;

	HANDLE hFile = CreateFileA(dumpfile.c_str(), GENERIC_WRITE,
		FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		goto minidump_failed;

	if (SetEndOfFile(hFile) == FALSE)
		goto minidump_failed;

	mdei.ClientPointers	   = NULL;
	mdei.ExceptionPointers = pExceptInfo;
	mdei.ThreadId		   = GetCurrentThreadId();

	mdus.Type       = CommentStreamA;
	mdus.BufferSize = version_str.size();
	mdus.Buffer     = (PVOID)version_str.c_str();

	mdusi.UserStreamArray = &mdus;
	mdusi.UserStreamCount = 1;

	if (MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
			MiniDumpNormal, &mdei, &mdusi, NULL) == FALSE)
		goto minidump_failed;

	minidump_created = true;

minidump_failed:

	CloseHandle(hFile);

	DWORD excode = pExceptInfo->ExceptionRecord->ExceptionCode;
	_snprintf(buf, sizeof(buf),
		" >> === FATAL ERROR ===\n"
		" >> %s (Exception 0x%08X) at 0x%p\n",
		Win32ExceptionCodeToString(excode), excode,
		pExceptInfo->ExceptionRecord->ExceptionAddress);
	dstream << buf;

	if (minidump_created)
		dstream << " >> Saved dump to " << dumpfile << std::endl;
	else
		dstream << " >> Failed to save dump" << std::endl;

	return EXCEPTION_EXECUTE_HANDLER;
}

#endif

void debug_set_exception_handler()
{
#ifdef _MSC_VER
	SetUnhandledExceptionFilter(Win32ExceptionHandler);
#endif
}

