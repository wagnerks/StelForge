#include "logger.h"

#include <cassert>
#ifdef __APPLE__
#include <os/log.h>
#else
#include <REND.h>
#endif

#include "chrono"
#include <sstream>
#include <ctime>
#include <iomanip>

using namespace Engine::LogsModule;

void Logger::logMessage(const eLogLevel level, const char* msg) {
	auto t = time(nullptr);
	
	std::ostringstream oss;
	tm buf;
#ifdef __APPLE__
	localtime_r(&t ,&buf);
	uint8_t logType;
#else
	localtime_s(&buf, &t);
#endif

	oss << std::put_time(&buf, "%d/%m/%y %H:%M:%S ");

	switch (level) {
	case eLogLevel::WARNING:
#ifdef __APPLE__
		logType = OS_LOG_TYPE_ERROR;
#endif
		oss << "WARNING:  ";
		break;
	case eLogLevel::ERROR_:
#ifdef __APPLE__
		logType = OS_LOG_TYPE_ERROR;
#endif
		oss << "ERROR:  ";
		break;
	case eLogLevel::INFO:
#ifdef __APPLE__
		logType = OS_LOG_TYPE_INFO;
#endif
		break;
	case eLogLevel::FATAL:
#ifdef __APPLE__
		logType = OS_LOG_TYPE_FAULT;
#endif
		oss << "FATAL:  ";
	}
	oss << msg << "\n";
#ifdef __APPLE__
	os_log_with_type(OS_LOG_DEFAULT, os_log_type_t(logType), "%s", oss.str().c_str());
#else
	OutputDebugString(oss.str().c_str());
#endif
	if (level == eLogLevel::FATAL) {
		assert(false && msg);
	}
}
