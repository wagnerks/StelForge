#include "CascadeShadowComponent.h"

std::vector<glm::vec4> CascadeShadowsHolderComponent::getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view) {
	return getFrustumCornersWorldSpace(proj * view);
}

std::vector<glm::vec4> CascadeShadowsHolderComponent::getFrustumCornersWorldSpace(const glm::mat4& projView) {
	const auto inv = glm::inverse(projView);

	std::vector<glm::vec4> frustumCorners;
	for (unsigned int x = 0; x < 2; ++x) {
		for (unsigned int y = 0; y < 2; ++y) {
			for (unsigned int z = 0; z < 2; ++z) {
				const glm::vec4 pt = inv * glm::vec4(
					2.0f * static_cast<float>(x) - 1.0f,
					2.0f * static_cast<float>(y) - 1.0f,
					2.0f * static_cast<float>(z) - 1.0f,
					1.0f
				);

				frustumCorners.push_back(pt / pt.w);
			}
		}
	}

	return frustumCorners;
}