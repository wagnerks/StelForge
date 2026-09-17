#include "propertiesModule/SceneComponents.h"

#include <utility>

#include "assetsModule/modelModule/Model.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "assetsModule/modelModule/Terrain.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/MeshComponent.h"
#include "core/ECSHandler.h"
#include "propertiesModule/PropertiesSystem.h"
#include "propertiesModule/Vegetation.h"
#include "systemsModule/systems/CameraSystem.h"
#include "systemsModule/TasksManager.h"

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

	// ===== SkyComponent =======================================================

	void ComponentSerializer<SkyComponent>::save(const SkyComponent& component, Json::Value& data) {
		data["sun_elevation"] = component.sunElevation;
		data["sun_azimuth"] = component.sunAzimuth;
		data["cloud_coverage"] = component.cloudCoverage;
		data["mist"] = component.mist;
		data["mist_height"] = component.mistHeight;
		data["haze"] = component.haze;
	}

	void ComponentSerializer<SkyComponent>::load(ecss::EntityId entity, const Json::Value& data) {
		auto component = ECSHandler::registry().addComponent<SkyComponent>(entity);
		if (!component) {
			return;
		}
		component->sunElevation = data.get("sun_elevation", component->sunElevation).asFloat();
		component->sunAzimuth = data.get("sun_azimuth", component->sunAzimuth).asFloat();
		component->cloudCoverage = data.get("cloud_coverage", component->cloudCoverage).asFloat();
		component->mist = data.get("mist", component->mist).asFloat();
		component->mistHeight = data.get("mist_height", component->mistHeight).asFloat();
		component->haze = data.get("haze", component->haze).asFloat();
	}

	// ===== TerrainComponent ===================================================

	void ComponentSerializer<TerrainComponent>::save(const TerrainComponent& component, Json::Value& data) {
		data["size"] = component.size;
		data["resolution"] = component.resolution;
		data["lake_width"] = component.lakeWidth;
		data["lake_length"] = component.lakeLength;
		data["lake_depth"] = component.lakeDepth;
		data["hill_height"] = component.hillHeight;
		data["seed"] = component.seed;
		data["road"] = Json::arrayValue;
		for (const auto& point : component.road) {
			Json::Value item = Json::arrayValue;
			item.append(point.x);
			item.append(point.y);
			data["road"].append(item);
		}
	}

	void ComponentSerializer<TerrainComponent>::load(ecss::EntityId entity, const Json::Value& data) {
		auto component = ECSHandler::registry().addComponent<TerrainComponent>(entity);
		if (!component) {
			return;
		}
		component->size = data.get("size", component->size).asFloat();
		component->resolution = data.get("resolution", component->resolution).asInt();
		component->lakeWidth = data.get("lake_width", component->lakeWidth).asFloat();
		component->lakeLength = data.get("lake_length", component->lakeLength).asFloat();
		component->lakeDepth = data.get("lake_depth", component->lakeDepth).asFloat();
		component->hillHeight = data.get("hill_height", component->hillHeight).asFloat();
		component->seed = data.get("seed", component->seed).asInt();
		component->road.clear();
		for (const auto& point : data["road"]) {
			component->road.push_back({ point[0].asFloat(), point[1].asFloat() });
		}

		auto* model = AssetsModule::getOrCreateTerrain(*component);
		auto modelComp = ECSHandler::registry().addComponent<ModelComponent>(entity, entity);
		if (!model || !modelComp) {
			return;
		}
		modelComp->mPath = model->assetPath;
		modelComp->setModel(model->getLODs());
		PropertiesSystem::bindRenderable(entity);

		// Painted by its own shader from world position and height, with the ground materials as texture arrays.
		if (auto mesh = ECSHandler::registry().pinComponent<MeshComponent>(entity)) {
			for (auto& material : mesh->meshMaterials) {
				material.fragmentShader = AssetsModule::kTerrainFS;
			}
		}
		SFE::SystemsModule::TasksManager::instance()->notify({ entity, SFE::SystemsModule::TaskType::MATERIAL_UPDATED });
	}

	// ===== WaterComponent =====================================================

	void ComponentSerializer<WaterComponent>::save(const WaterComponent& component, Json::Value& data) {
		data["size"] = component.size;
		data["clarity"] = component.clarity;
	}

	void ComponentSerializer<WaterComponent>::load(ecss::EntityId entity, const Json::Value& data) {
		auto component = ECSHandler::registry().addComponent<WaterComponent>(entity);
		if (!component) {
			return;
		}
		component->size = data.get("size", component->size).asFloat();
		component->clarity = data.get("clarity", component->clarity).asFloat();
	}

	// ===== CharacterComponent =================================================

	void ComponentSerializer<CharacterComponent>::save(const CharacterComponent& component, Json::Value& data) {
		data["walk_speed"] = component.walkSpeed;
		data["run_speed"] = component.runSpeed;
		data["sprint_speed"] = component.sprintSpeed;
		data["jump_height"] = component.jumpHeight;
		if (!component.animations.empty()) {
			data["animations"] = component.animations;
		}
	}

	void ComponentSerializer<CharacterComponent>::load(ecss::EntityId entity, const Json::Value& data) {
		auto component = ECSHandler::registry().addComponent<CharacterComponent>(entity);
		if (!component) {
			return;
		}
		component->walkSpeed = data.get("walk_speed", component->walkSpeed).asFloat();
		component->runSpeed = data.get("run_speed", component->runSpeed).asFloat();
		component->sprintSpeed = data.get("sprint_speed", component->sprintSpeed).asFloat();
		component->jumpHeight = data.get("jump_height", component->jumpHeight).asFloat();
		component->animations = data.get("animations", component->animations).asString();
	}

	// ===== CharacterAppearanceComponent =======================================

	void ComponentSerializer<CharacterAppearanceComponent>::save(const CharacterAppearanceComponent& component, Json::Value& data) {
		const auto colour = [](const Math::Vec3& c) {
			Json::Value out(Json::arrayValue);
			out.append(c.x);
			out.append(c.y);
			out.append(c.z);
			return out;
		};
		data["skin_color"] = colour(component.skinColor);
		data["hair_color"] = colour(component.hairColor);
		data["hair_style"] = component.hairStyle;
	}

	void ComponentSerializer<CharacterAppearanceComponent>::load(ecss::EntityId entity, const Json::Value& data) {
		auto component = ECSHandler::registry().addComponent<CharacterAppearanceComponent>(entity);
		if (!component) {
			return;
		}
		const auto colour = [&](const char* key, Math::Vec3& out) {
			const auto& value = data[key];
			if (value.isArray() && value.size() == 3) {
				out = { value[0].asFloat(), value[1].asFloat(), value[2].asFloat() };
			}
		};
		colour("skin_color", component->skinColor);
		colour("hair_color", component->hairColor);
		component->hairStyle = data.get("hair_style", component->hairStyle).asString();
	}

	// ===== VegetationComponent ================================================

	namespace {
		// Every field of a layer, by the name it has in the scene file.
		template<class Visit>
		void forEachLayerField(Visit&& visit) {
			visit("count", &VegetationLayer::count);
			visit("min_scale", &VegetationLayer::minScale);
			visit("max_scale", &VegetationLayer::maxScale);
			visit("min_height", &VegetationLayer::minHeight);
			visit("max_height", &VegetationLayer::maxHeight);
			visit("max_slope", &VegetationLayer::maxSlope);
			visit("spacing", &VegetationLayer::spacing);
			visit("noise_scale", &VegetationLayer::noiseScale);
			visit("noise_threshold", &VegetationLayer::noiseThreshold);
			visit("sink", &VegetationLayer::sink);
			visit("collider_radius", &VegetationLayer::colliderRadius);
			visit("collider_height", &VegetationLayer::colliderHeight);
			visit("draw_distance", &VegetationLayer::drawDistance);
			visit("pitch", &VegetationLayer::pitch);
			visit("impostor", &VegetationLayer::impostor);
			visit("canopy", &VegetationLayer::canopy);
			visit("forest_min", &VegetationLayer::forestMin);
			visit("forest_max", &VegetationLayer::forestMax);
		}
	}

	namespace {
		constexpr std::pair<std::string_view, Sway> kSways[] = { { "none", Sway::None }, { "tree", Sway::Tree }, { "plant", Sway::Plant } };

		std::string_view swayName(Sway sway) {
			for (const auto& [name, value] : kSways) {
				if (value == sway) {
					return name;
				}
			}
			return kSways[0].first;
		}

		Sway swayFromName(const std::string& name) {
			for (const auto& [candidate, value] : kSways) {
				if (candidate == name) {
					return value;
				}
			}
			return Sway::None;
		}
	}

	void ComponentSerializer<VegetationComponent>::save(const VegetationComponent& component, Json::Value& data) {
		data["seed"] = component.seed;
		data["layers"] = Json::arrayValue;
		for (const auto& layer : component.layers) {
			Json::Value item = Json::objectValue;
			for (const auto& model : layer.models) {
				item["models"].append(model);
			}
			for (const auto& model : layer.rootModels) {
				item["root_models"].append(model);
			}
			forEachLayerField([&](const char* name, auto member) { item[name] = layer.*member; });
			item["sway"] = std::string(swayName(layer.sway));
			for (const auto& surface : layer.surfaces) {
				Json::Value value = Json::objectValue;
				value["on"] = surface.on;
				value["texture"] = surface.texture;
				for (const float bound : surface.uv) {
					value["uv"].append(bound);
				}
				value["metres"] = surface.metres;
				if (surface.rotate) {
					value["rotate"] = true;
				}
				item["surfaces"].append(value);
			}
			data["layers"].append(item);
		}
	}

	void ComponentSerializer<VegetationComponent>::load(ecss::EntityId entity, const Json::Value& data) {
		VegetationComponent component;
		component.seed = data.get("seed", component.seed).asInt();
		for (const auto& item : data["layers"]) {
			VegetationLayer layer;
			for (const auto& model : item["models"]) {
				layer.models.push_back(model.asString());
			}
			for (const auto& model : item["root_models"]) {
				layer.rootModels.push_back(model.asString());
			}
			forEachLayerField([&](const char* name, auto member) {
				if (!item.isMember(name)) {
					return;
				}
				using Field = std::remove_cvref_t<decltype(layer.*member)>;
				layer.*member = std::is_same_v<Field, int> ? static_cast<Field>(item[name].asInt()) : static_cast<Field>(item[name].asFloat());
			});
			if (item.isMember("sway")) {
				layer.sway = swayFromName(item["sway"].asString());
			}
			for (const auto& value : item["surfaces"]) {
				VegetationSurface surface;
				surface.on = value.get("on", "").asString();
				surface.texture = value.get("texture", "").asString();
				for (const auto& bound : value["uv"]) {
					surface.uv.push_back(bound.asFloat());
				}
				surface.metres = value.get("metres", surface.metres).asFloat();
				surface.rotate = value.get("rotate", false).asBool();
				layer.surfaces.push_back(std::move(surface));
			}
			component.layers.push_back(std::move(layer));
		}
		if (!ECSHandler::registry().addComponent<VegetationComponent>(entity, std::move(component))) {
			return;
		}
		generateVegetation(entity);
	}

	// ===== WindComponent ======================================================

	void ComponentSerializer<WindComponent>::save(const WindComponent& component, Json::Value& data) {
		data["direction"] = component.direction;
		data["strength"] = component.strength;
	}

	void ComponentSerializer<WindComponent>::load(ecss::EntityId entity, const Json::Value& data) {
		auto component = ECSHandler::registry().addComponent<WindComponent>(entity);
		if (!component) {
			return;
		}
		component->direction = data.get("direction", component->direction).asFloat();
		component->strength = data.get("strength", component->strength).asFloat();
	}

	// ===== AudioSourceComponent ===============================================

	void ComponentSerializer<AudioSourceComponent>::save(const AudioSourceComponent& component, Json::Value& data) {
		data["path"] = component.mPath;
		data["volume"] = component.mVolume;
		data["pitch"] = component.mPitch;
		data["min_distance"] = component.mMinDistance;
		data["max_distance"] = component.mMaxDistance;
		data["loop"] = component.mLoop;
		data["spatial"] = component.mSpatial;
		data["stream"] = component.mStream;
		data["playing"] = component.mPlaying;
	}

	void ComponentSerializer<AudioSourceComponent>::load(ecss::EntityId entity, const Json::Value& data) {
		auto component = ECSHandler::registry().addComponent<AudioSourceComponent>(entity);
		if (!component) {
			return;
		}
		component->mPath = data.get("path", component->mPath).asString();
		component->mVolume = data.get("volume", component->mVolume).asFloat();
		component->mPitch = data.get("pitch", component->mPitch).asFloat();
		component->mMinDistance = data.get("min_distance", component->mMinDistance).asFloat();
		component->mMaxDistance = data.get("max_distance", component->mMaxDistance).asFloat();
		component->mLoop = data.get("loop", component->mLoop).asBool();
		component->mSpatial = data.get("spatial", component->mSpatial).asBool();
		component->mStream = data.get("stream", component->mStream).asBool();
		component->mPlaying = data.get("playing", component->mPlaying).asBool();
	}

	// ===== SoundscapeComponent ================================================

	void ComponentSerializer<SoundscapeComponent>::save(const SoundscapeComponent& component, Json::Value& data) {
		data["wind"] = component.wind;
		data["wind_volume"] = component.windVolume;
		data["lake"] = component.lake;
		data["lake_volume"] = component.lakeVolume;
		data["birds"] = Json::Value(Json::arrayValue);
		for (const auto& bird : component.birds) {
			data["birds"].append(bird);
		}
		data["bird_volume"] = component.birdVolume;
		data["frogs"] = Json::Value(Json::arrayValue);
		for (const auto& frog : component.frogs) {
			data["frogs"].append(frog);
		}
		data["frog_volume"] = component.frogVolume;
		data["seed"] = component.seed;
		data["footsteps"] = Json::Value(Json::arrayValue);
		for (const auto& surface : component.footsteps) {
			Json::Value entry;
			entry["surface"] = surface.surface;
			for (const auto& clip : surface.clips) {
				entry["clips"].append(clip);
			}
			entry["volume"] = surface.volume;
			entry["max_length"] = surface.maxLength;
			data["footsteps"].append(entry);
		}
	}

	void ComponentSerializer<SoundscapeComponent>::load(ecss::EntityId entity, const Json::Value& data) {
		auto component = ECSHandler::registry().addComponent<SoundscapeComponent>(entity);
		if (!component) {
			return;
		}
		component->wind = data.get("wind", "").asString();
		component->windVolume = data.get("wind_volume", component->windVolume).asFloat();
		component->lake = data.get("lake", "").asString();
		component->lakeVolume = data.get("lake_volume", component->lakeVolume).asFloat();
		component->birds.clear();
		for (const auto& bird : data["birds"]) {
			component->birds.push_back(bird.asString());
		}
		component->birdVolume = data.get("bird_volume", component->birdVolume).asFloat();
		component->frogs.clear();
		for (const auto& frog : data["frogs"]) {
			component->frogs.push_back(frog.asString());
		}
		component->frogVolume = data.get("frog_volume", component->frogVolume).asFloat();
		component->seed = data.get("seed", component->seed).asUInt();
		component->footsteps.clear();
		for (const auto& entry : data["footsteps"]) {
			ComponentsModule::FootstepSurface surface;
			surface.surface = entry.get("surface", "").asString();
			for (const auto& clip : entry["clips"]) {
				surface.clips.push_back(clip.asString());
			}
			surface.volume = entry.get("volume", surface.volume).asFloat();
			surface.maxLength = entry.get("max_length", surface.maxLength).asFloat();
			component->footsteps.push_back(std::move(surface));
		}
	}

	// ===== GarbageComponent ===================================================

	void ComponentSerializer<GarbageComponent>::save(const GarbageComponent& component, Json::Value& data) {
		data["seed"] = component.seed;
		data["shore_count"] = component.shoreCount;
		data["shore_below"] = component.shoreBelow;
		data["shore_above"] = component.shoreAbove;
		data["float_count"] = component.floatCount;
		data["float_depth"] = component.floatDepth;
		auto& items = data["items"];
		items = Json::Value(Json::arrayValue);
		for (const auto& item : component.items) {
			Json::Value entry;
			entry["model"] = item.model;
			entry["size"] = item.size;
			entry["mass"] = item.mass;
			entry["buoyancy"] = item.buoyancy;
			entry["weight"] = item.weight;
			items.append(entry);
		}
	}

	void ComponentSerializer<GarbageComponent>::load(ecss::EntityId entity, const Json::Value& data) {
		auto component = ECSHandler::registry().addComponent<GarbageComponent>(entity);
		if (!component) {
			return;
		}
		component->seed = data.get("seed", component->seed).asUInt();
		component->shoreCount = data.get("shore_count", component->shoreCount).asUInt();
		component->shoreBelow = data.get("shore_below", component->shoreBelow).asFloat();
		component->shoreAbove = data.get("shore_above", component->shoreAbove).asFloat();
		component->floatCount = data.get("float_count", component->floatCount).asUInt();
		component->floatDepth = data.get("float_depth", component->floatDepth).asFloat();
		component->items.clear();
		for (const auto& entry : data["items"]) {
			ComponentsModule::GarbageItem item;
			item.model = entry.get("model", item.model).asString();
			item.size = entry.get("size", item.size).asFloat();
			item.mass = entry.get("mass", item.mass).asFloat();
			item.buoyancy = entry.get("buoyancy", item.buoyancy).asFloat();
			item.weight = entry.get("weight", item.weight).asFloat();
			component->items.push_back(std::move(item));
		}
	}

	// ===== GrassComponent =====================================================

	void ComponentSerializer<GrassComponent>::save(const GrassComponent& component, Json::Value& data) {
		data["density"] = component.density;
		data["height"] = component.height;
		data["radius"] = component.radius;
	}

	void ComponentSerializer<GrassComponent>::load(ecss::EntityId entity, const Json::Value& data) {
		auto component = ECSHandler::registry().addComponent<GrassComponent>(entity);
		if (!component) {
			return;
		}
		component->density = data.get("density", component->density).asFloat();
		component->height = data.get("height", component->height).asFloat();
		component->radius = data.get("radius", component->radius).asFloat();
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
			entryFor<SkyComponent>(LoadPhase::Direct),
			// After bind: it builds its own mesh and binds it.
			entryFor<TerrainComponent>(LoadPhase::AfterBind),
			entryFor<WaterComponent>(LoadPhase::Direct),
			entryFor<CharacterComponent>(LoadPhase::Direct),
			// After the terrain it stands on.
			entryFor<VegetationComponent>(LoadPhase::AfterBind),
			entryFor<WindComponent>(LoadPhase::Direct),
			entryFor<GrassComponent>(LoadPhase::Direct),
			entryFor<AudioSourceComponent>(LoadPhase::Direct),
			entryFor<GarbageComponent>(LoadPhase::Direct),
			entryFor<SoundscapeComponent>(LoadPhase::Direct),
			entryFor<CharacterAppearanceComponent>(LoadPhase::Direct),
		};

		return table;
	}
}
