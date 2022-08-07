#include "SystemInterface.h"

ecsModule::SystemInterface::SystemInterface(uint16_t priority, float_t updateInterval) : mTimeSinceLastUpdate(0),
	mUpdateInterval(updateInterval),
	mPriority(priority) { }
