#pragma once
#include <cassert>
#include <chrono>
#include <functional>
#include <map>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "containersModule/Singleton.h"
#include "logsModule/logger.h"

#define BENCHMARK_ENABLED 1

#define CONCATENATE_DETAIL(x, y) x##y
#define CONCATENATE(x, y) CONCATENATE_DETAIL(x, y)
#define MAKE_UNIQUE(x) CONCATENATE(x, __LINE__)

#if !BENCHMARK_ENABLED
#define FUNCTION_BENCHMARK 
#define FUNCTION_BENCHMARK_NAMED(name) 
#else
#define FUNCTION_BENCHMARK SFE::Debug::BenchmarkFunc MAKE_UNIQUE(scopeObj) = SFE::Debug::BenchmarkFunc(std::string(__FUNCTION__), __LINE__);
#define FUNCTION_BENCHMARK_NAMED(name) SFE::Debug::BenchmarkFunc MAKE_UNIQUE(scopeObj) = SFE::Debug::BenchmarkFunc(std::string(__FUNCTION__), __LINE__, "[" + std::string(#name) + "]");
#define FUNCTION_BENCHMARK_NAMED_STR(name) SFE::Debug::BenchmarkFunc MAKE_UNIQUE(scopeObj) = SFE::Debug::BenchmarkFunc(std::string(__FUNCTION__), __LINE__, "[" + std::string(name) + "]");
#endif

namespace SFE::Debug {
	struct Benchmark {
		struct Time {
			enum TimeUnits : int {
				SECONDS = 1 << 0,
				MILLI = 1 << 1,
				MICRO = 1 << 2,
				NANO = 1 << 3
			};
			long long seconds;
			long long millisecond;
			long long microsecond;
			long long nanosecond;
			long long delta;

			std::string getTimeString(int flag = TimeUnits::SECONDS | TimeUnits::MILLI | TimeUnits::MICRO | TimeUnits::NANO) const {
				std::string res;
				if (flag & TimeUnits::SECONDS) {
					res += std::to_string(seconds) + "s ";
				}
				if (flag & TimeUnits::MILLI) {
					res += std::to_string(millisecond) + "ms ";
				}
				if (flag & TimeUnits::MICRO) {
					res += std::to_string(microsecond) + "mus ";
				}
				if (flag & TimeUnits::NANO) {
					res += std::to_string(nanosecond) + "ns";
				}

				return res;
			}
		};

		inline static void start(const std::string& id) {
			std::unique_lock lock(mtx);
			mCounters[id].push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count());
		}

		inline static Time stop(const std::string& id, bool log = true) {
			std::unique_lock lock(mtx);
			if (mCounters.contains(id)) {
				const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
				const auto delta = nanoseconds - mCounters[id].back();
				if (mCounters[id].size() == 1) {
					mCounters.erase(id);
				}
				else {
					mCounters[id].pop_back();
				}
				Time time;
				time.seconds = delta / 1'000'000'000;
				time.millisecond = delta / 1'000'000 % 1'000;
				time.microsecond = delta / 1'000 % 1'000;
				time.nanosecond = delta % 1'000;
				time.delta = delta;

				if (log) {
					LogsModule::Logger::LOG_INFO("%s : delta - < %d s: %d: ms: %d mu: %d ns >", id.c_str(), time.seconds, time.millisecond, time.microsecond, time.nanosecond);
				}

				return time;
			}
			
			LogsModule::Logger::LOG_ERROR("\"%s\" benchmark was not started", id.c_str());

			return {};
		}
	private:
		inline static std::shared_mutex mtx;
		inline static std::unordered_map<std::string, std::vector<long long>> mCounters; //id, startTime ns
	};

	class BenchmarkSystem: public Singleton<BenchmarkSystem> {
	public:
		struct MeasureData {
			std::vector<Benchmark::Time> measurements;
			std::vector<float> plotData;
		};

		std::map<std::string, std::pair<std::map<std::string, MeasureData>, MeasureData>> measurements;

		std::shared_mutex mtx;
	};

	struct BenchmarkFunc final {
		BenchmarkFunc(const std::string& name, int line = -1, const std::string& customName = "");
		~BenchmarkFunc();

		std::string name;
		std::string customName;
		int line = -1;
	};
}
