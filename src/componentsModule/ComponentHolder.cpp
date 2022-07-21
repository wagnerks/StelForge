#include "ComponentHolder.h"

#include <ranges>

#include "Component.h"


using namespace GameEngine;
using namespace GameEngine::ComponentsModule;

ComponentHolder::~ComponentHolder() {
	for (auto& component : components | std::views::values) {
		delete component;
	}
	components.clear();
}

void ComponentHolder::updateComponents() const {
	for (const auto component : components | std::views::values) {
		component->updateComponent();
	}
}