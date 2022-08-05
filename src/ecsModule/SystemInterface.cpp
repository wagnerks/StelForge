#include "SystemInterface.h"

ecsModule::SystemInterface::SystemInterface(uint16_t priority, float_t updateInterval_ms) : mTimeSinceLastUpdate(0),
	mUpdateInterval(updateInterval_ms),
	mPriority(priority) { }
