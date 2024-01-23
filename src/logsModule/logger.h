#pragma once
#include <cstddef>
#include <cstdio>
#include <cwchar>
#include <stdio.h>
#include <wchar.h>

namespace SFE::LogsModule {
	enum class eLogLevel {
		INFO,
		WARNING,
		ERROR_,
		FATAL
	};

	class Logger {
		inline static char msgBuf[2048];

		template <typename... Args>
		static const char* format_internal(const char* msg,const Args&... args) {
			snprintf(msgBuf, 2048, msg, args...);
			return msgBuf;
		}

		template <typename... Args>
		static void log(eLogLevel level, const char* msg,const Args&... args) {
			constexpr size_t count = sizeof...(args);
			if (count > 0) {
				msg = format_internal(msg, args...);
			}
			
			logMessage(level, msg);
		}

		static void logMessage(eLogLevel level, const char* msg);

	public:
		template <typename... Args>
		static void LOG_INFO(const char* msg,const Args&... args) {
			log(eLogLevel::INFO, msg, args...);
		}

		template <typename... Args>
		static void LOG_ERROR(const char* msg,const Args&... args) {
			log(eLogLevel::ERROR_, msg, args...);
		}

		template <typename... Args>
		static void LOG_WARNING(const char* msg,const Args&... args) {
			log(eLogLevel::WARNING, msg, args...);
		}

		template <typename... Args>
		static void LOG_FATAL(bool check, const char* msg,const Args&... args) {
			if (check) {
				return;
			}

			log(eLogLevel::FATAL, msg, args...);
		}
	};
}
