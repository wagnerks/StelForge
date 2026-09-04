#include "propertiesModule/SceneComponents.h"

#include <utility>

#include "assetsModule/modelModule/Model.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/MeshComponent.h"
#include "core/ECSHandler.h"
#include "propertiesModule/PropertiesSystem.h"
#include "systemsModule/systems/CameraSystem.h"

namespace SFE::PropertiesModule {
	using namespace SFE::ComponentsModule;

	namespace {
		// Written by name rather than by ordinal, so inserting a light type into the enum does
		// not silently turn every saved point light into something else.
		constexpr std::pair<std::string_view, eLightType> kLightTypes[] = {
			{ "none",		 eLightType::NONE },
			{ "directional", eLightType::DIRECTIONAL },
			{ "point",		 eLightType::POINT },
			{ "perspective", eLightType::PERSPECTIVE },
			{ "world",		 eLightType::WORLD },
		};

		std::string lightTypeName(eLightType type) {
			for (const auto& [name, value] : kLightTypes) {
				if (value == type) {
					return std::string(name);
				}
			}
			return std::string(kLightTypes[0].first);
		}

		eLightType lightTypeFromName(const Json::Value& data) {
			const auto name = data.asString();
			for (const auto& [candidate, value] : kLightTypes) {
				if (candidate == name) {
					return value;
				}
			}
			return eLightType::NONE;
		}
	}

	// ===== TransformComponent =================================================

	void ComponentSerializer<TransformComponent>::save(const TransformComponent& component, Json::Value& data) {
		const auto& scale = component.getScale();
		data["Scale"].append(scale.x);
		data["Scale"].append(scale.y);
		data["Scale"].append(scale.z);

		const auto& pos = component.getPos();
		data["Pos"].append(pos.x);
		data["Pos"].append(pos.y);
		data["Pos"].append(pos.z);

		const auto& rotate = component.getRotate();
		data["Rotate"].append(rotate.x);
		data["Rotate"].append(rotate.y);
		data["Rotate"].append(rotate.z);
	}

	void ComponentSerializer<TransformComponent>::load(ecss::EntityId entity, const Json::Value& data) {
		auto component = ECSHandler::registry().addComponent<TransformComponent>(entity, entity);
		if (!component) {
			return;
		}

		if (auto val = JsonUtils::getValueArray(data, "Scale")) {
			component->setScale(JsonUtils::getVec3(*val));
		}

		if (auto val = JsonUtils::getValueArray(data, "Pos")) {
			component->setPos(JsonUtils::getVec3(*val));
		}

		if (auto val = JsonUtils::getValueArray(data, "Rotate")) {
			component->setRotate(JsonUtils::getVec3(*val));
		}
	}

	// ===== ModelComponent =====================================================

	void ComponentSerializer<ModelComponent>::save(const ModelComponent& component, Json::Value& data) {
		data["ModelPath"] = component.mPath;
	}

	void ComponentSerializer<ModelComponent>::load(ecss::EntityId entity, const Json::Value& data) {
		auto component = ECSHandler::registry().addComponent<ModelComponent>(entity, entity);
		if (!component) {
			return;
		}

		AssetsModule::Model* model = nullptr;
		if (data.isMember("ModelPath")) {
			component->mPath = data["ModelPath"].asString();
			model = AssetsModule::ModelLoader::instance()->load(component->mPath);
		}

		if (model) {
			component->boneMatrices = model->getDefaultBoneMatrices();
			component->armature = model->getArmature();
			component->addMeshData(model->getLODs());
		}
	}

	// ===== LightSourceComponent ===============================================

	void ComponentSerializer<LightSourceComponent>::save(const LightSourceComponent& component, Json::Value& data) {
		data["type"] = lightTypeName(component.getType());
		data["intensity"] = component.getIntensity();

		const auto& color = component.getLightColor();
		data["color"] = Json::arrayValue;
		data["color"].append(color.x);
		data["color"].append(color.y);
		data["color"].append(color.z);

		data["radius"] = component.mRadius;
		data["near"] = component.mNear;
		data["linear"] = component.mLinear;
		data["quadratic"] = component.mQuadratic;
		data["with_shadows"] = component.mWithShadows;

		data["bias"] = component.getBias();
		data["samples"] = component.getSamples();

		const auto& texelSize = component.getTexelSize();
		data["texel_size"] = Json::arrayValue;
		data["texel_size"].append(texelSize.x);
		data["texel_size"].append(texelSize.y);
	}

	void ComponentSerializer<LightSourceComponent>::load(ecss::EntityId entity, const Json::Value& data) {
		// The light type is a constructor argument, so it is read before the component exists.
		auto component = ECSHandler::addComponent<LightSourceComponent>(entity, entity, lightTypeFromName(data["type"]));
		if (!component) {
			return;
		}

		if (data.isMember("intensity")) {
			component->setIntensity(data["intensity"].asFloat());
		}
		if (auto val = JsonUtils::getValueArray(data, "color")) {
			component->setLightColor(JsonUtils::getVec3(*val));
		}
		if (data.isMember("radius")) {
			component->mRadius = data["radius"].asFloat();
		}
		if (data.isMember("near")) {
			component->mNear = data["near"].asFloat();
		}
		if (data.isMember("linear")) {
			component->mLinear = data["linear"].asFloat();
		}
		if (data.isMember("quadratic")) {
			component->mQuadratic = data["quadratic"].asFloat();
		}
		if (data.isMember("with_shadows")) {
			component->mWithShadows = data["with_shadows"].asBool();
		}
		if (data.isMember("bias")) {
			component->setBias(data["bias"].asFloat());
		}
		if (data.isMember("samples")) {
			component->setSamples(data["samples"].asInt());
		}
		if (auto val = JsonUtils::getValueArray(data, "texel_size")) {
			component->setTexelSize(JsonUtils::getVec2(*val));
		}
	}

	// ===== CascadeShadowComponent =============================================

	void ComponentSerializer<CascadeShadowComponent>::save(const CascadeShadowComponent& component, Json::Value& data) {
		data["cascaded_levels"] = Json::arrayValue;
		for (auto level : component.shadowCascadeLevels) {
			data["cascaded_levels"].append(level);
		}

		data["resolution"] = Json::arrayValue;
		data["resolution"].append(component.resolution.x);
		data["resolution"].append(component.resolution.y);

		data["cascades"] = Json::arrayValue;
		for (auto& cascade : component.cascades) {
			Json::Value cascadeData = Json::objectValue;
			cascadeData["bias"] = cascade.bias;
			cascadeData["samples"] = cascade.samples;

			cascadeData["texel_size"].append(cascade.texelSize.x);
			cascadeData["texel_size"].append(cascade.texelSize.y);

			cascadeData["z_mult"].append(cascade.zMult.x);
			cascadeData["z_mult"].append(cascade.zMult.y);

			data["cascades"].append(cascadeData);
		}

		data["shadow_intensity"] = component.shadowIntensity;
	}

	void ComponentSerializer<CascadeShadowComponent>::load(ecss::EntityId entity, const Json::Value& data) {
		auto component = ECSHandler::registry().addComponent<CascadeShadowComponent>(entity, entity);
		if (!component) {
			return;
		}

		// The cascade split points are derived from the camera the shadows are rendered for,
		// so the file only carries the per-cascade tuning laid over them.
		auto cameraSystem = ECSHandler::getSystem<SFE::SystemsModule::CameraSystem>();
		auto camera = cameraSystem ? ECSHandler::registry().pinComponent<const CameraComponent>(cameraSystem->getCurrentCamera()) : ecss::PinnedComponent<const CameraComponent>{};
		if (!camera) {
			return;
		}

		component->shadowCascadeLevels.clear();
		for (auto level : data["cascaded_levels"]) {
			component->shadowCascadeLevels.emplace_back(level.asFloat());
		}

		component->resolution = { data["resolution"][0].asFloat(), data["resolution"][1].asFloat() };

		component->updateCascades(camera->getProjection());

		int i = 0;
		for (auto& cascade : component->cascades) {
			auto& cascadeData = data["cascades"][i++];
			cascade.bias = cascadeData["bias"].asFloat();
			cascade.samples = cascadeData["samples"].asInt();

			cascade.texelSize = { cascadeData["texel_size"][0].asFloat(), cascadeData["texel_size"][1].asFloat() };
			cascade.zMult = { cascadeData["z_mult"][0].asFloat(), cascadeData["z_mult"][1].asFloat() };
		}

		component->shadowIntensity = data["shadow_intensity"].asFloat();
	}

	// ===== MaterialComponent ==================================================

	void ComponentSerializer<MaterialComponent>::save(const MaterialComponent& component, Json::Value& data) {
		data["transparent"] = component.transparent;
		data["opacity"] = component.opacity;
		data["ior"] = component.ior;
		data["tint"] = Json::arrayValue;
		data["tint"].append(component.tint.x);
		data["tint"].append(component.tint.y);
		data["tint"].append(component.tint.z);
	}

	void ComponentSerializer<MaterialComponent>::load(ecss::EntityId entity, const Json::Value& data) {
		auto pinned = ECSHandler::registry().pinComponent<MaterialComponent>(entity);
		MaterialComponent* material = pinned.get();
		if (!material) {
			material = ECSHandler::addComponent<MaterialComponent>(entity);
		}
		if (!material) {
			return;
		}

		if (data.isMember("transparent")) {
			material->transparent = data["transparent"].asBool();
		}
		if (data.isMember("opacity")) {
			material->opacity = data["opacity"].asFloat();
		}
		if (data.isMember("ior")) {
			material->ior = data["ior"].asFloat();
		}
		if (data.isMember("tint") && data["tint"].isArray() && data["tint"].size() >= 3) {
			material->tint = {
				data["tint"][0].asFloat(),
				data["tint"][1].asFloat(),
				data["tint"][2].asFloat()
			};
		}

		// The mesh carries its own copy of the material values, built from the asset by
		// bindRenderable(); the scene's overrides have to reach it too.
		if (auto mesh = ECSHandler::registry().pinComponent<MeshComponent>(entity)) {
			for (auto& meshMaterial : mesh->meshMaterials) {
				if (data.isMember("transparent") && material->transparent) {
					meshMaterial.transparent = true;
				}
				if (material->transparent) {
					if (data.isMember("opacity")) {
						meshMaterial.opacity = material->opacity;
					}
					if (data.isMember("ior")) {
						meshMaterial.ior = material->ior;
					}
					if (data.isMember("tint")) {
						meshMaterial.tint = material->tint;
					}
				}
			}
		}
	}

	// ===== The table ==========================================================

	const std::vector<ComponentRegistry::Entry>& ComponentRegistry::entries() {
		static const std::vector<Entry> table = {
			entryFor<ModelComponent>(LoadPhase::Direct),
			entryFor<TransformComponent>(LoadPhase::Direct),
			entryFor<LightSourceComponent>(LoadPhase::Direct),
			// Saved for inspection only: restoring it needs the camera projection the cascades
			// are split against, which a scene load does not have yet. CascadedShadowPass
			// restores it from its own file, through deserializeProperty().
			entryFor<CascadeShadowComponent>(LoadPhase::Never),
			// After bindRenderable(): the values are written through into the mesh materials
			// it just built.
			entryFor<MaterialComponent>(LoadPhase::AfterBind),
		};

		return table;
	}
}
