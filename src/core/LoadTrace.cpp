#include "LoadTrace.h"

#include <algorithm>
#include <cstdio>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "FileSystem.h"
#include "logsModule/logger.h"

namespace SFE {
	namespace {
		struct Gathered {
			std::mutex guard;
			std::unordered_map<std::string, double> seconds;
			std::unordered_map<std::string, int> times;   // how many stages carried the name
			std::unordered_map<std::string, long long> counts;
			std::chrono::steady_clock::time_point began = std::chrono::steady_clock::now();
			bool on = true;
		};

		Gathered& gathered() {
			static Gathered one;
			return one;
		}

		// Times under this are noise beside a load of seconds, and a table of them hides what matters.
		constexpr double kWorthShowing = 0.002;
	}

	LoadTrace::Step::Step(std::string name) : mName(std::move(name)), mStart(std::chrono::steady_clock::now()) {}

	LoadTrace::Step::~Step() {
		const std::chrono::duration<double> spent = std::chrono::steady_clock::now() - mStart;
		add(mName, spent.count());
	}

	void LoadTrace::add(std::string_view name, double seconds) {
		auto& all = gathered();
		if (!all.on) {
			return;
		}
		std::lock_guard lock(all.guard);
		const std::string key(name);
		all.seconds[key] += seconds;
		++all.times[key];
	}

	void LoadTrace::count(std::string_view name, long long many) {
		auto& all = gathered();
		if (!all.on) {
			return;
		}
		std::lock_guard lock(all.guard);
		all.counts[std::string(name)] += many;
	}

	std::string LoadTrace::plainName(std::string_view typeName) {
		const auto last = typeName.rfind("::");
		if (last != std::string_view::npos) {
			return std::string(typeName.substr(last + 2));
		}
		const auto space = typeName.rfind(' ');   // msvc writes "class Foo", gcc writes a mangled name with no space
		return std::string(space == std::string_view::npos ? typeName : typeName.substr(space + 1));
	}

	void LoadTrace::report(std::string_view what) {
		auto& all = gathered();
		std::vector<std::pair<std::string, double>> rows;
		std::vector<std::pair<std::string, long long>> counted;
		double whole = 0.0;
		std::unordered_map<std::string, int> times;
		{
			std::lock_guard lock(all.guard);
			if (all.seconds.empty() && all.counts.empty()) {
				return;
			}
			rows.assign(all.seconds.begin(), all.seconds.end());
			counted.assign(all.counts.begin(), all.counts.end());
			times = all.times;
			const std::chrono::duration<double> since = std::chrono::steady_clock::now() - all.began;
			whole = since.count();
			all.seconds.clear();
			all.times.clear();
			all.counts.clear();
			all.began = std::chrono::steady_clock::now();
		}
		std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
		std::sort(counted.begin(), counted.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

		std::string table = "load of " + std::string(what) + ": " + std::to_string(static_cast<int>(whole * 1000.f)) + " ms\n";
		for (const auto& [name, seconds] : rows) {
			if (seconds < kWorthShowing) {
				continue;
			}
			char line[256];
			const int share = whole > 0.0 ? static_cast<int>(seconds / whole * 100.0 + 0.5) : 0;
			std::snprintf(line, sizeof(line), "  %-28s %8.0f ms  %3d%%  x%d\n",
				name.c_str(), seconds * 1000.0, share, times[name]);
			table += line;
		}
		for (const auto& [name, many] : counted) {
			char line[256];
			std::snprintf(line, sizeof(line), "  %-28s %8lld\n", name.c_str(), many);
			table += line;
		}
		LogsModule::Logger::LOG_INFO("%s", table.c_str());
		// Beside the executable, so two builds can be put side by side without digging the log out.
		if (FILE* file = std::fopen((FileSystem::executableDir() / "load_times.log").string().c_str(), "a")) {
			std::fputs(table.c_str(), file);
			std::fputc('\n', file);
			std::fclose(file);
		}
	}

	void LoadTrace::reset() {
		auto& all = gathered();
		std::lock_guard lock(all.guard);
		all.seconds.clear();
		all.times.clear();
		all.counts.clear();
		all.began = std::chrono::steady_clock::now();
	}

	void LoadTrace::listen(bool on) {
		auto& all = gathered();
		std::lock_guard lock(all.guard);
		all.on = on;
	}

	bool LoadTrace::listening() {
		auto& all = gathered();
		std::lock_guard lock(all.guard);
		return all.on;
	}
}
