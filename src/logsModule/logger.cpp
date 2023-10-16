#include "logger.h"

#include <cassert>
#include <REND.h>
#include "chrono"

using namespace Engine::LogsModule;

void Logger::logMessage(const eLogLevel level, const char* msg) {
	auto t = time(nullptr);
	
	std::ostringstream oss;
	tm buf;
	localtime_s(&buf ,&t);
	oss << std::put_time(&buf, "%d/%m/%y %H:%M:%S ");

	switch (level) {
	case eLogLevel::WARNING:
		oss << "WARNING::";
		break;
	case eLogLevel::ERROR_:
		oss << "ERROR::";
		break;
	case eLogLevel::INFO:
		break;
	case eLogLevel::FATAL:
		oss << "FATAL::";
	}
	oss << msg << "\n";

	OutputDebugString(oss.str().c_str());

	if (level == eLogLevel::FATAL) {
		assert(false && msg);
	}
}
