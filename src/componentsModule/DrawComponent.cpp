#include "DrawComponent.h"

#include "ComponentHolder.h"
#include "MeshComponent.h"
#include "ModelComponent.h"

using namespace GameEngine::ComponentsModule;

void DrawComponent::draw() {
	if (auto meshComp = owner->getComponent<MeshComponent>(false)) {
		meshComp->draw();
	}
	else if (auto modelComp = owner->getComponent<ModelComponent>(false)) {
		modelComp->draw();
	}
}
