#ifdef _WIN32
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "core/FileSystem.h"
#include "logsModule/CrashReport.h"
#include "logsModule/logger.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// A crash ends the process it happens in, so each way is tried in a child of this suite, started with --crash <way>.
// Its report has to be there, say what happened, name the crashing function with its file, and carry the line logged
// just before.
namespace SFE::EngineTests {
	namespace {
		__declspec(noinline) void crashByNull() {
			volatile int* nothing = nullptr;
			*nothing = 1;
		}

		__declspec(noinline) void crashByOverflow(volatile int depth) {
			volatile char room[1024];
			room[0] = static_cast<char>(depth);
			if (depth >= 0) {
				crashByOverflow(depth + 1);
			}
			room[1] = room[0];
		}

		__declspec(noinline) void crashByAbort() {
			std::abort();
		}

		// + 1: not a tail call, so this frame stays on the stack.
		__declspec(noinline) int crashByArgument() {
			return std::fclose(nullptr) + 1;
		}
	}

	int crashForTest(std::string_view way) {
		_set_abort_behavior(0, _CALL_REPORTFAULT);   // after the report, out without Windows Error Reporting and its dump
		LogsModule::CrashReport::install();
		LogsModule::Logger::LOG_INFO("crashing by %s", std::string(way).c_str());
		if (way == "null") {
			crashByNull();
		}
		else if (way == "overflow") {
			crashByOverflow(0);
		}
		else if (way == "abort") {
			crashByAbort();
		}
		else if (way == "argument") {
			crashByArgument();
		}
		return 0;
	}

	TEST(CrashReport, ReportsEachWay) {
		struct Way {
			const char* name;
			const char* says;
			const char* in;
			PROCESS_INFORMATION child{};
		};
		Way ways[] = {
			{ "null", "access violation", "crashByNull" },
			{ "overflow", "stack overflow", "crashByOverflow" },
			{ "abort", "abort() was called", "crashByAbort" },
			{ "argument", "a bad argument to a C runtime function", "crashByArgument" },
		};
		char self[MAX_PATH];
		GetModuleFileNameA(nullptr, self, MAX_PATH);
		// All at once: each spends seconds on symbols.
		for (auto& way : ways) {
			std::string command = std::string("\"") + self + "\" --crash " + way.name;
			STARTUPINFOA startup{};
			startup.cb = sizeof(startup);
			ASSERT_TRUE(CreateProcessA(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &way.child)) << way.name;
		}
		const auto folder = FileSystem::executableDir() / "crashes";
		for (auto& way : ways) {
			EXPECT_EQ(WaitForSingleObject(way.child.hProcess, 120000), WAIT_OBJECT_0) << way.name;
			const auto ending = "_" + std::to_string(way.child.dwProcessId) + ".txt";
			std::filesystem::path found;
			std::error_code ec;
			for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
				if (entry.path().filename().string().ends_with(ending)) {
					found = entry.path();
				}
			}
			std::string report;
			if (!found.empty()) {
				std::ifstream file(found);
				report.assign(std::istreambuf_iterator<char>(file), {});
				file.close();
				std::filesystem::remove(found, ec);
			}
			EXPECT_NE(report.find(way.says), std::string::npos) << way.name << ":\n" << report;
			EXPECT_NE(report.find(way.in), std::string::npos) << way.name << ":\n" << report;
			EXPECT_NE(report.find("CrashReportTests.cpp"), std::string::npos) << way.name << ":\n" << report;
			EXPECT_NE(report.find(std::string("crashing by ") + way.name), std::string::npos) << way.name << ":\n" << report;
			CloseHandle(way.child.hThread);
			CloseHandle(way.child.hProcess);
		}
	}
}
#endif
