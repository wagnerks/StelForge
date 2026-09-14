#include "rhi/RhiLog.h"

#include <utility>

namespace SFE::Render {
	namespace {
		RhiLogSink& sink() {
			static RhiLogSink instance;
			return instance;
		}
	}

	void setRhiLogSink(RhiLogSink newSink) {
		sink() = std::move(newSink);
	}

	void RhiLog::write(const RhiLogLevel level, const char* message) {
		if (const auto& current = sink()) {
			current(level, message);
			return;
		}
		static constexpr const char* kNames[] = { "INFO", "WARNING", "ERROR", "FATAL" };
		std::fprintf(stderr, "RHI %s: %s\n", kNames[static_cast<int>(level)], message);
	}
}
