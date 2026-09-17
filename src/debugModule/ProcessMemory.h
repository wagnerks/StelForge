#pragma once

#include <cstdint>

namespace SFE::Debug {
	struct ProcessMemory {
		uint64_t inRam = 0;     // private pages actually in RAM: what the task manager shows
		uint64_t committed = 0; // private memory promised to the process, touched or not: drivers reserve a lot of it
	};
	// Zeros where the platform does not say.
	ProcessMemory processMemory();
}
