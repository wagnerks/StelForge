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
	localtime_r(&t ,&buf);
	oss << std::put_time(&buf, "%d/%m/%y %H:%M:%S ");
    uint8_t logType;
	switch (level) {
	case eLogLevel::WARNING:
            logType = OS_LOG_TYPE_ERROR;
		oss << "WARNING:  ";
		break;
	case eLogLevel::ERROR_:
            logType = OS_LOG_TYPE_ERROR;
		oss << "ERROR:  ";
		break;
	case eLogLevel::INFO:
            logType = OS_LOG_TYPE_INFO;
		break;
	case eLogLevel::FATAL:
            logType = OS_LOG_TYPE_FAULT;
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
