#include "logger.h"

#include <cassert>
#include <REND.h>

using namespace Engine::LogsModule;

void Logger::logMessage(const eLogLevel level, const char* msg) {
	switch (level) {
	case eLogLevel::WARNING:
		OutputDebugString("WARNING::");
		break;
	case eLogLevel::ERROR_:
		OutputDebugString("ERROR::");
		break;
	case eLogLevel::INFO:
		break;
	case eLogLevel::FATAL:
		OutputDebugString("FATAL::");
	}
	OutputDebugString(msg);
	OutputDebugString("\n");

	if (level == eLogLevel::FATAL) {
		assert(false && msg);
	}
}
