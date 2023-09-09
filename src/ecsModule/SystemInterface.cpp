#include "SystemInterface.h"


using namespace ecsModule;

SystemInterface::SystemInterface() :
	mTimeSinceLastUpdate(0.f),
	mUpdateInterval(0.f),
	mPriority(0) { }