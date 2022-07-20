#include "logger.h"

#include <REND.h>

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
	}
	OutputDebugString(msg);
	OutputDebugString("\n");
}
