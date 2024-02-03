#include "RenderPass.h"

using namespace SFE::RenderModule;

void RenderPass::setPriority(size_t priority) {
	mPriority = priority;
}

size_t RenderPass::getPriority() const {
	return mPriority;
}
