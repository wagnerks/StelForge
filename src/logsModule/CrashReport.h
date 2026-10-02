#pragma once

namespace SFE::LogsModule {
	// Catches what kills the process -- an exception nothing caught (an access violation, a stack overflow, a throw
	// nobody catches), abort() (std::terminate, a pure virtual call, an ecss wait gone stuck), a bad argument to the C
	// runtime -- and leaves a report in <exe>/crashes: what happened, the crashed thread's stack with files and lines,
	// and the last lines logged. A fast fail (a stack buffer overrun, a corrupt heap) ends the process before anything
	// in it can run, and is left to Windows Error Reporting. Windows only: install() does nothing elsewhere.
	struct CrashReport {
		// Once, first thing in main.
		static void install();
		// A line just logged: the last ones go into the report.
		static void remember(const char* line);
	};
}
