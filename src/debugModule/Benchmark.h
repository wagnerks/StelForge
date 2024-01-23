#pragma once
#include <cassert>
#include <chrono>
#include <map>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "core/Singleton.h"
#include "logsModule/logger.h"

#define BENCHMARK_ENABLED 0

#if !BENCHMARK_ENABLED
#define FUNCTION_BENCHMARK 
#define FUNCTION_BENCHMARK_NAMED(name) 
#else
#define FUNCTION_BENCHMARK SFE::Debug::BenchmarkFunc scopeObj##__LINE__ = SFE::Debug::BenchmarkFunc(std::string(__FUNCTION__) + std::to_string(__LINE__));
#define FUNCTION_BENCHMARK_NAMED(name) SFE::Debug::BenchmarkFunc scopeObj##__LINE__ = SFE::Debug::BenchmarkFunc(std::string(__FUNCTION__) + std::to_string(__LINE__) + #name);
#endif


namespace SFE::Debug {
	

	class Benchmark {
	public:
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

	class BenchmarkGUI : public Singleton<BenchmarkGUI> {
	public:
		void onGui();
		struct MeasureData {
			std::vector<Benchmark::Time> measurements;
			std::vector<float> plotData;


		};
		std::map<std::string, MeasureData> measurements;
		std::string filter;
		std::shared_mutex mtx;

		bool windowOpened = false;
	};

	class BenchmarkFunc final {
	public:
		BenchmarkFunc(const std::string& name) : name(name) {
			{
				std::unique_lock lock(BenchmarkGUI::instance()->mtx);
				BenchmarkGUI::instance()->measurements[name];
			}
			Benchmark::start(name);
		}

		~BenchmarkFunc() {
			std::unique_lock lock(BenchmarkGUI::instance()->mtx);
			auto& timings = BenchmarkGUI::instance()->measurements[name];
			timings.measurements.push_back(Benchmark::stop(name, false));
			timings.plotData.push_back(static_cast<float>(timings.measurements.back().delta));
			if (timings.measurements.size() > 100) {
				timings.measurements.erase(timings.measurements.begin());
				timings.plotData.erase(timings.plotData.begin());
			}
		}

		std::string name;
	};
}
