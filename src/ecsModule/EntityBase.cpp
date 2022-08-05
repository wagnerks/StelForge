#include "EntityBase.h"

using namespace ecsModule;

size_t EntityInterface::getEntityID() const {
	return mId;
}

void EntityInterface::setId(size_t id) {
	mId = id;
}

EntityInterface::~EntityInterface() {
	releaseComponents();
}