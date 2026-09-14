#pragma once

#include <cstdio>
#include <functional>

namespace SFE::Render {
	enum class RhiLogLevel { Info, Warning, Error, Fatal };

	// Where the RHI's diagnostics go. Process-wide and set once by whoever owns the RHI, before it is
	// created; until then messages go to stderr. The engine routes them to its logger, the tests collect
	// them so a test can fail on a validation error.
	using RhiLogSink = std::function<void(RhiLogLevel level, const char* message)>;
	void setRhiLogSink(RhiLogSink sink);

	namespace RhiLog {
		void write(RhiLogLevel level, const char* message);

		template <typename... Args>
		void format(const RhiLogLevel level, const char* message, const Args&... args) {
			// No arguments: the text is written as-is, so a '%' in it is not read as a conversion.
			if (sizeof...(args) == 0) {
				write(level, message);
				return;
			}
			char buffer[2048];
			std::snprintf(buffer, sizeof(buffer), message, args...);
			write(level, buffer);
		}

		template <typename... Args>
		void info(const char* message, const Args&... args) { format(RhiLogLevel::Info, message, args...); }

		template <typename... Args>
		void warning(const char* message, const Args&... args) { format(RhiLogLevel::Warning, message, args...); }

		template <typename... Args>
		void error(const char* message, const Args&... args) { format(RhiLogLevel::Error, message, args...); }

		template <typename... Args>
		void fatal(const char* message, const Args&... args) { format(RhiLogLevel::Fatal, message, args...); }

		// Fatal when `ok` is false. The condition is always evaluated, so it may have side effects.
		template <typename... Args>
		void check(const bool ok, const char* message, const Args&... args) {
			if (!ok) {
				format(RhiLogLevel::Fatal, message, args...);
			}
		}
	}
}
