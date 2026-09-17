#include "ProcessMemory.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#endif

namespace SFE::Debug {
	ProcessMemory processMemory() {
		ProcessMemory memory;
#ifdef _WIN32
		PROCESS_MEMORY_COUNTERS_EX2 counters{};
		if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters))) {
			memory.inRam = counters.PrivateWorkingSetSize;
			memory.committed = counters.PrivateUsage;
		}
#endif
		return memory;
	}
}
