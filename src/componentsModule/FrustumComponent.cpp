#include "FrustumComponent.h"

FrustumComponent::FrustumComponent() {
}

void FrustumComponent::updateFrustum(const ProjectionModule::Projection& projection, const glm::mat4& view) {
	mFrustum = FrustumModule::createFrustum(projection.getProjectionsMatrix() * view);
}

void FrustumComponent::updateFrustum(const glm::mat4& projView) {
	mFrustum = FrustumModule::createFrustum(projView);
}

void FrustumComponent::updateFrustum(const glm::mat4& projection, const glm::mat4& view) {
	mFrustum = FrustumModule::createFrustum(projection * view);
}

bool FrustumComponent::isOnFrustum(const FrustumModule::BoundingVolume& bVolume) const {
	return bVolume.isOnFrustum(mFrustum);
}

GameEngine::FrustumModule::Frustum* FrustumComponent::getFrustum() {
	return &mFrustum;
}

void FrustumComponent::getFrustumCorners(std::vector<glm::vec3>& corners, glm::mat4 projection) {
	corners.clear();

    // homogeneous corner coords
    glm::vec4 hcorners[8];
    // near
    hcorners[0] = glm::vec4(-1, 1, 1, 1);
    hcorners[1] = glm::vec4(1, 1, 1, 1);
    hcorners[2] = glm::vec4(1, -1, 1, 1);
    hcorners[3] = glm::vec4(-1, -1, 1, 1);
    // far
    hcorners[4] = glm::vec4(-1, 1, -1, 1);
    hcorners[5] = glm::vec4(1, 1, -1, 1);
    hcorners[6] = glm::vec4(1, -1, -1, 1);
    hcorners[7] = glm::vec4(-1, -1, -1, 1);

    glm::mat4 inverseProj = glm::inverse(projection);
    for (int i = 0; i < 8; i++) {
        hcorners[i] = hcorners[i] * inverseProj;
        hcorners[i] /= hcorners[i].w;

        corners.push_back(glm::vec3(hcorners[i]));
    }
}
