#include "RenderComponent.h"


using namespace GameEngine::ComponentsModule;

bool RenderComponent::isDrawable() const {
	return mIsDrawable;
}
void RenderComponent::setDrawable(bool isDrawable) {
	mIsDrawable = isDrawable;
}
