#pragma once
#include <cassert>
#include <chrono>
#include <string>
#include <unordered_map>

#include "logsModule/logger.h"

namespace Engine::Debug {
	class Benchmark {
	public:
		inline static void start(std::string id) {
			mCounters[id] = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
		}

		inline static void stop(std::string id) {
			if (mCounters.contains(id)) {
				const auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
				const auto delta = nanoseconds - mCounters[id];
				mCounters.erase(id);

				const auto seconds = delta / 1'000'000'000;
				const auto millisecond = delta / 1'000'000 % 1'000;
				const auto microsecond = delta / 1'000 % 1'000;
				const auto nanosecond = delta % 1'000;
				
				LogsModule::Logger::LOG_INFO("%s : delta - < %d s: %d: ms: %d mu: %d ns >", id.c_str(), seconds, millisecond, microsecond, nanosecond);
			}
			else {
				LogsModule::Logger::LOG_ERROR("\"%s\" benchmark was not started", id.c_str());
			}
		}
	private:
		inline static std::unordered_map<std::string, long long> mCounters; //id, startTime ns
	};
}
