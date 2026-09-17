#include "AppearanceSystem.h"

#include <cmath>
#include <functional>

#include "RenderSystem.h"
#include "componentsModule/CharacterComponent.h"
#include "componentsModule/MeshComponent.h"
#include "core/ECSHandler.h"

namespace SFE::SystemsModule {
	namespace {
		// The names of a model's parts in the order MeshComponent keeps their materials: the root, then depth first.
		std::vector<std::string> partNames(const AssetsModule::Model& model) {
			std::vector<std::string> names;
			std::function<void(const Tree<MeshObject3D>&)> visit = [&](const Tree<MeshObject3D>& node) {
				for (const auto& child : node.children) {
					names.push_back(child.value.nodeName);
					visit(child);
				}
			};
			names.push_back(model.getMeshTree().value.nodeName);
			visit(model.getMeshTree());
			return names;
		}

		bool isHair(const std::string& name) {
			return name.rfind("Hair_", 0) == 0;
		}

		// The tint that shows an sRGB colour over the albedo the texture already has.
		Math::Vec3 linear(const Math::Vec3& srgb) {
			return { std::pow(srgb.x, 2.2f), std::pow(srgb.y, 2.2f), std::pow(srgb.z, 2.2f) };
		}

		uint64_t hashOf(const ComponentsModule::CharacterAppearanceComponent& look) {
			uint64_t hash = std::hash<std::string>{}(look.hairStyle);
			for (const float v : { look.skinColor.x, look.skinColor.y, look.skinColor.z, look.hairColor.x, look.hairColor.y, look.hairColor.z }) {
				hash = hash * 1099511628211ull ^ std::hash<float>{}(v);
			}
			return hash;
		}
	}

	std::vector<std::string> AppearanceSystem::hairStyles(const ecss::EntityId entity) {
		std::vector<std::string> styles;
		const auto mesh = ECSHandler::registry().pinComponent<const MeshComponent>(entity);
		if (!mesh || !mesh->meshModel) {
			return styles;
		}
		for (const auto& name : partNames(*mesh->meshModel)) {
			if (isHair(name) && std::find(styles.begin(), styles.end(), name) == styles.end()) {
				styles.push_back(name);
			}
		}
		return styles;
	}

	void AppearanceSystem::update(float) {
		auto& registry = ECSHandler::registry();
		auto* renderSystem = ECSHandler::getSystem<RenderSystem>();
		for (auto [entity, look, mesh] : registry.view<const ComponentsModule::CharacterAppearanceComponent, MeshComponent>()) {
			if (!look || !mesh || !mesh->meshModel) {
				continue;
			}
			const auto hash = hashOf(*look);
			auto [it, added] = mApplied.try_emplace(entity, ~hash);
			if (it->second == hash) {
				continue;
			}
			it->second = hash;

			const auto names = partNames(*mesh->meshModel);
			const auto skin = linear(look->skinColor);
			const auto hair = linear(look->hairColor);
			for (size_t i = 0; i < names.size() && i < mesh->meshMaterials.size(); ++i) {
				auto& material = mesh->meshMaterials[i];
				if (names[i] == "Body") {
					material.tint = skin;
				}
				else if (names[i] == "Eyebrows" || isHair(names[i])) {
					material.tint = hair;
					material.hidden = isHair(names[i]) && names[i] != look->hairStyle;
				}
			}
			if (renderSystem) {
				renderSystem->scene().markEntity(entity);
			}
		}
	}
}
