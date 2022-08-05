#include "ComponentBase.h"

using namespace ecsModule;

size_t ComponentInterface::getOwnerId() const {
	return mOwnerId;
}

size_t ComponentInterface::getComponentId() const {
	return mComponentID;
}

void ComponentInterface::setOwnerId(size_t id) {
	mOwnerId = id;
}

void ComponentInterface::setId(size_t id) {
	mComponentID = id;
}
