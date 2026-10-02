#include "CrashReport.h"

#include <algorithm>
#include <atomic>
#include <cstdio>

#ifdef _WIN32
#include "core/FileSystem.h"

#include <csignal>
#include <cstdarg>
#include <cstdlib>
#include <cwchar>
#include <iterator>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif

namespace SFE::LogsModule {
	namespace {
		// The last lines logged, each in a slot of its own taken by a counter, so no logger waits on another. The report
		// reads them without a lock: a line being written meanwhile comes out torn at worst.
		constexpr size_t kLinesKept = 200;
		char gLines[kLinesKept][512];
		std::atomic<size_t> gLinesNext{ 0 };
	}

	void CrashReport::remember(const char* line) {
		auto& slot = gLines[gLinesNext.fetch_add(1) % kLinesKept];
		if (std::snprintf(slot, sizeof(slot), "%s", line) >= static_cast<int>(sizeof(slot))) {
			slot[sizeof(slot) - 2] = '\n';   // cut short, it still ends its line
		}
	}

#ifdef _WIN32
	namespace {
		// The report is written by a thread of its own, started with the process: the crashed thread may have no stack
		// left (an overflow) to write it with. It stands still meanwhile, so its stack is walked from the context it
		// hands over.
		struct Crash {
			const char* reason = nullptr;
			const EXCEPTION_RECORD* record = nullptr;
			CONTEXT context{};
			DWORD thread = 0;
		};
		Crash gCrash;
		// Made at install: once crashed, nothing is allocated for a path.
		std::wstring gExeDir;
		std::wstring gFolder;
		HANDLE gBegin = nullptr;
		HANDLE gDone = nullptr;
		DWORD gWriter = 0;
		std::atomic<bool> gReporting{ false };
		HANDLE gFile = INVALID_HANDLE_VALUE;

		constexpr DWORD kWaitMost = 30000;   // ms the crashed thread waits for its report: symbols out of a big PDB take seconds
		constexpr int kFramesMost = 64;

		void put(const char* format, ...) {
			char text[2048];
			va_list args;
			va_start(args, format);
			const int length = std::vsnprintf(text, sizeof(text), format, args);
			va_end(args);
			DWORD written = 0;
			if (length > 0) {
				WriteFile(gFile, text, static_cast<DWORD>((std::min)(static_cast<size_t>(length), sizeof(text) - 1)), &written, nullptr);
			}
		}

		const char* named(DWORD code) {
			switch (code) {
			case EXCEPTION_ACCESS_VIOLATION: return "access violation";
			case EXCEPTION_STACK_OVERFLOW: return "stack overflow";
			case EXCEPTION_ILLEGAL_INSTRUCTION: return "illegal instruction";
			case EXCEPTION_INT_DIVIDE_BY_ZERO: return "integer division by zero";
			case EXCEPTION_IN_PAGE_ERROR: return "in-page error";
			case 0xE06D7363: return "C++ exception nobody caught";
			default: return "unhandled exception";
			}
		}

		void putWhat() {
			const auto* record = gCrash.record;
			if (!record) {
				put("%s\n", gCrash.reason);
				return;
			}
			const DWORD code = record->ExceptionCode;
			put("%s: exception 0x%08lX at %p", named(code), code, record->ExceptionAddress);
			if (code == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2) {
				const auto how = record->ExceptionInformation[0];
				put(", %s 0x%llX", how == 0 ? "reading" : how == 1 ? "writing" : "running",
					static_cast<unsigned long long>(record->ExceptionInformation[1]));
			}
			put("\n");
		}

		void putStack(CONTEXT context, HANDLE thread) {
			const HANDLE process = GetCurrentProcess();
			SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS);
			if (!SymInitializeW(process, gExeDir.c_str(), TRUE)) {
				put("  (no symbols: SymInitialize failed, error %lu)\n", GetLastError());
			}
			STACKFRAME_EX frame{};
			frame.StackFrameSize = sizeof(frame);
			frame.AddrPC = { context.Rip, 0, AddrModeFlat };
			frame.AddrFrame = { context.Rbp, 0, AddrModeFlat };
			frame.AddrStack = { context.Rsp, 0, AddrModeFlat };
			const DWORD64 top = context.Rip;
			for (int depth = 0; depth < kFramesMost && StackWalkEx(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &context,
				nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr, SYM_STKWALK_DEFAULT); ++depth) {
				// A caller's address is where its call returns to, maybe a line on already: a byte back is the call.
				const DWORD64 at = frame.AddrPC.Offset == top ? top : frame.AddrPC.Offset - 1;
				IMAGEHLP_MODULE64 module{};
				module.SizeOfStruct = sizeof(module);
				const char* in = SymGetModuleInfo64(process, at, &module) ? module.ModuleName : "?";
				alignas(SYMBOL_INFO) char space[sizeof(SYMBOL_INFO) + 1024];
				auto* symbol = reinterpret_cast<SYMBOL_INFO*>(space);
				symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
				symbol->MaxNameLen = 1024;
				DWORD64 offset = 0;
				IMAGEHLP_LINE64 line{};
				line.SizeOfStruct = sizeof(line);
				DWORD shift = 0;
				if (!SymFromInlineContext(process, at, frame.InlineFrameContext, &offset, symbol)) {
					put("  %s+0x%llX\n", in, at - module.BaseOfImage);
				}
				else if (SymGetLineFromInlineContext(process, at, frame.InlineFrameContext, 0, &shift, &line)) {
					put("  %s!%s  %s:%lu\n", in, symbol->Name, line.FileName, line.LineNumber);
				}
				else {
					put("  %s!%s+0x%llX\n", in, symbol->Name, offset);
				}
			}
		}

		void putLines() {
			const size_t next = gLinesNext.load();
			for (size_t i = next - (std::min)(next, kLinesKept); i < next; ++i) {
				put("%s", gLines[i % kLinesKept]);
			}
		}

		DWORD WINAPI write(void*) {
			WaitForSingleObject(gBegin, INFINITE);
			SYSTEMTIME now;
			GetLocalTime(&now);
			CreateDirectoryW(gFolder.c_str(), nullptr);
			wchar_t path[1024];
			std::swprintf(path, std::size(path), L"%ls\\%04u-%02u-%02u_%02u-%02u-%02u_%lu.txt", gFolder.c_str(), now.wYear, now.wMonth,
				now.wDay, now.wHour, now.wMinute, now.wSecond, GetCurrentProcessId());
			gFile = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (gFile != INVALID_HANDLE_VALUE) {
				char exe[MAX_PATH] = "?";
				GetModuleFileNameA(nullptr, exe, MAX_PATH);
				put("%s crashed, %04u-%02u-%02u %02u:%02u:%02u\n", exe, now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
				put("command line: %s\n", GetCommandLineA());
				putWhat();
				put("\nstack of thread %lu:\n", gCrash.thread);
				const HANDLE thread = OpenThread(THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, gCrash.thread);
				putStack(gCrash.context, thread ? thread : GetCurrentThread());
				if (thread) {
					CloseHandle(thread);
				}
				put("\nlast lines logged:\n");
				putLines();
				CloseHandle(gFile);
			}
			SetEvent(gDone);
			return 0;
		}

		// One crash is reported. Another thread crashing meanwhile waits for the process to end with the first one; the
		// writer, or the reporting thread crashing again, ends it at once.
		void report(const char* reason, const EXCEPTION_RECORD* record, const CONTEXT& context) {
			const DWORD self = GetCurrentThreadId();
			if (gReporting.exchange(true)) {
				if (self != gWriter && self != gCrash.thread) {
					Sleep(INFINITE);
				}
				return;
			}
			gCrash.reason = reason;
			gCrash.record = record;
			gCrash.context = context;
			gCrash.thread = self;
			SetEvent(gBegin);
			WaitForSingleObject(gDone, kWaitMost);
		}

		LONG WINAPI onException(EXCEPTION_POINTERS* info) {
			report(nullptr, info->ExceptionRecord, *info->ContextRecord);
			return EXCEPTION_EXECUTE_HANDLER;
		}

		// abort() goes on after this, to Windows Error Reporting as before.
		void onAbort(int) {
			CONTEXT context;
			RtlCaptureContext(&context);
			report("abort() was called", nullptr, context);
		}

		// Left alone, the runtime would fast fail here, past any handler; after the report it aborts instead.
		void onBadArgument(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t) {
			CONTEXT context;
			RtlCaptureContext(&context);
			report("a bad argument to a C runtime function", nullptr, context);
			std::abort();
		}
	}
#endif

	void CrashReport::install() {
#ifdef _WIN32
		if (gBegin) {
			return;
		}
		gExeDir = FileSystem::executableDir().wstring();
		gFolder = gExeDir + L"\\crashes";
		gBegin = CreateEventW(nullptr, TRUE, FALSE, nullptr);
		gDone = CreateEventW(nullptr, TRUE, FALSE, nullptr);
		const HANDLE writer = CreateThread(nullptr, 0, write, nullptr, 0, &gWriter);
		if (!gBegin || !gDone || !writer) {
			return;
		}
		SetThreadDescription(writer, L"crash report");
		CloseHandle(writer);
		SetUnhandledExceptionFilter(onException);
		std::signal(SIGABRT, onAbort);
		_set_invalid_parameter_handler(onBadArgument);
#endif
	}
}
