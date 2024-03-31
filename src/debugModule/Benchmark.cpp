#include "Benchmark.h"

namespace SFE::Debug {
	BenchmarkFunc::BenchmarkFunc(const std::string& name, int line, const std::string& customName) : name(name), customName(customName), line(line) {
		{
			std::unique_lock lock(BenchmarkSystem::instance()->mtx);
			BenchmarkSystem::instance()->measurements[name];

			if (!customName.empty()) {
				BenchmarkSystem::instance()->measurements[name].first[customName];
			}
		}
		Benchmark::start(name + customName);
	}

	BenchmarkFunc::~BenchmarkFunc() {
		auto stopRes = Benchmark::stop(name + customName, false);

		std::unique_lock lock(BenchmarkSystem::instance()->mtx);
		{
			auto& timings = BenchmarkSystem::instance()->measurements[name];
			if (!customName.empty()) {
				auto& childTimings = timings.first[customName];
				childTimings.measurements.push_back(stopRes);
				childTimings.plotData.push_back(static_cast<float>(childTimings.measurements.back().delta));
				if (childTimings.measurements.size() > 100) {
					childTimings.measurements.erase(childTimings.measurements.begin());
					childTimings.plotData.erase(childTimings.plotData.begin());
				}
			}
			else {
				timings.second.measurements.push_back(stopRes);
				timings.second.plotData.push_back(static_cast<float>(timings.second.measurements.back().delta));
				if (timings.second.measurements.size() > 100) {
					timings.second.measurements.erase(timings.second.measurements.begin());
					timings.second.plotData.erase(timings.second.plotData.begin());
				}
			}
		}
	}
}
