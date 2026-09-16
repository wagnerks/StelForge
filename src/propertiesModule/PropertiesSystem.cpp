#include "propertiesModule/PropertiesSystem.h"
#include "core/FileSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "assetsModule/AssetsManager.h"
#include "assetsModule/modelModule/MeshVaoRegistry.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "componentsModule/ArmatureComponent.h"
#include "componentsModule/CascadeShadowComponent.h"
#include "componentsModule/DebugDataComponent.h"
#include "componentsModule/IsDrawableComponent.h"
#include "componentsModule/LightSourceComponent.h"
#include "componentsModule/MaterialComponent.h"
#include "componentsModule/MeshComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/OcTreeComponent.h"
#include "componentsModule/TransformComponent.h"
#include "componentsModule/Hierarchy.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "rhi/GraphicsBackend.h"
#include "multithreading/ThreadPool.h"
#include "componentsModule/CameraComponent.h"
#include "mathModule/VectorOperations.h"
#include "systemsModule/systems/CameraSystem.h"
#include "systemsModule/TasksManager.h"

namespace SFE::PropertiesModule {
	namespace {
		AssetsModule::Model* getOrCreateUnitSphere() {
			constexpr const char* kPath = "builtin:sphere";
			if (auto* existing = AssetsModule::AssetsManager::instance()->getAsset<AssetsModule::Model>(kPath)) {
				return existing;
			}

			constexpr int stacks = 32;
			constexpr int slices = 48;
			constexpr float pi = 3.14159265f;

			auto makeVert = [](float u, float v, float x, float y, float z) {
				SFE::Vertex3D vert{};
				vert.position = { x, y, z };
				vert.normal = vert.position;
				vert.texCoords = { u, 1.f - v };
				vert.texCoords1 = vert.texCoords;
				const float lenXZ = std::sqrt(x * x + z * z);
				if (lenXZ > 1e-4f) {
					vert.tangent = { -z / lenXZ, 0.f, x / lenXZ };
				}
				else {
					vert.tangent = { 1.f, 0.f, 0.f };
				}
				vert.biTangent = Math::cross(vert.normal, vert.tangent);
				return vert;
			};

			SFE::MeshObject3D meshObj;
			meshObj.mesh.vertices.push_back(makeVert(0.5f, 0.f, 0.f, 1.f, 0.f));
			for (int i = 1; i < stacks; ++i) {
				const float v = static_cast<float>(i) / static_cast<float>(stacks);
				const float phi = v * pi;
				const float sp = std::sin(phi);
				const float cp = std::cos(phi);
				for (int j = 0; j <= slices; ++j) {
					const float u = static_cast<float>(j) / static_cast<float>(slices);
					const float theta = u * 2.f * pi;
					meshObj.mesh.vertices.push_back(makeVert(u, v, sp * std::cos(theta), cp, sp * std::sin(theta)));
				}
			}
			meshObj.mesh.vertices.push_back(makeVert(0.5f, 1.f, 0.f, -1.f, 0.f));

			const unsigned ring = static_cast<unsigned>(slices + 1);
			const unsigned south = static_cast<unsigned>(meshObj.mesh.vertices.size() - 1);
			for (int j = 0; j < slices; ++j) {
				const unsigned a = 1u + static_cast<unsigned>(j);
				meshObj.mesh.indices.push_back(0);
				meshObj.mesh.indices.push_back(a);
				meshObj.mesh.indices.push_back(a + 1);
			}
			const int rings = stacks - 1;
			for (int i = 0; i < rings - 1; ++i) {
				for (int j = 0; j < slices; ++j) {
					const unsigned a = 1u + static_cast<unsigned>(i) * ring + static_cast<unsigned>(j);
					const unsigned b = a + ring;
					meshObj.mesh.indices.push_back(a);
					meshObj.mesh.indices.push_back(b);
					meshObj.mesh.indices.push_back(a + 1);
					meshObj.mesh.indices.push_back(a + 1);
					meshObj.mesh.indices.push_back(b);
					meshObj.mesh.indices.push_back(b + 1);
				}
			}
			const unsigned lastRing = 1u + static_cast<unsigned>(rings - 1) * ring;
			for (int j = 0; j < slices; ++j) {
				meshObj.mesh.indices.push_back(south);
				meshObj.mesh.indices.push_back(lastRing + static_cast<unsigned>(j) + 1);
				meshObj.mesh.indices.push_back(lastRing + static_cast<unsigned>(j));
			}
			meshObj.aabb = SFE::FrustumModule::AABB({ -1.f, -1.f, -1.f }, { 1.f, 1.f, 1.f });

			SFE::Tree<SFE::MeshObject3D> tree;
			tree.value = std::move(meshObj);
			return AssetsModule::AssetsManager::instance()->createAsset<AssetsModule::Model>(
				kPath, std::move(tree), AssetsModule::Armature{}, std::vector<AssetsModule::Animation>{});
		}

		// A cube from -1 to 1, one quad per face so the normals are flat.
		AssetsModule::Model* getOrCreateUnitBox() {
			constexpr const char* kPath = "builtin:box";
			if (auto* existing = AssetsModule::AssetsManager::instance()->getAsset<AssetsModule::Model>(kPath)) {
				return existing;
			}

			const Math::Vec3 normals[6] = { {1.f,0.f,0.f}, {-1.f,0.f,0.f}, {0.f,1.f,0.f}, {0.f,-1.f,0.f}, {0.f,0.f,1.f}, {0.f,0.f,-1.f} };
			const Math::Vec3 tangents[6] = { {0.f,0.f,-1.f}, {0.f,0.f,1.f}, {1.f,0.f,0.f}, {1.f,0.f,0.f}, {1.f,0.f,0.f}, {-1.f,0.f,0.f} };

			SFE::MeshObject3D meshObj;
			for (int face = 0; face < 6; ++face) {
				const auto n = normals[face];
				const auto t = tangents[face];
				const auto b = Math::cross(n, t);
				const unsigned first = static_cast<unsigned>(meshObj.mesh.vertices.size());
				for (int corner = 0; corner < 4; ++corner) {
					const float u = (corner == 1 || corner == 2) ? 1.f : 0.f;
					const float v = (corner >= 2) ? 1.f : 0.f;
					SFE::Vertex3D vert{};
					vert.position = n + t * (u * 2.f - 1.f) + b * (v * 2.f - 1.f);
					vert.normal = n;
					vert.tangent = t;
					vert.biTangent = b;
					vert.texCoords = { u, v };
					vert.texCoords1 = vert.texCoords;
					meshObj.mesh.vertices.push_back(vert);
				}
				for (const unsigned idx : { 0u, 1u, 2u, 0u, 2u, 3u }) {
					meshObj.mesh.indices.push_back(first + idx);
				}
			}
			meshObj.aabb = SFE::FrustumModule::AABB({ -1.f, -1.f, -1.f }, { 1.f, 1.f, 1.f });

			SFE::Tree<SFE::MeshObject3D> tree;
			tree.value = std::move(meshObj);
			return AssetsModule::AssetsManager::instance()->createAsset<AssetsModule::Model>(
				kPath, std::move(tree), AssetsModule::Armature{}, std::vector<AssetsModule::Animation>{});
		}

		void markGlass(ecss::EntityId entity, float opacity, float ior, const Math::Vec3& tint);

		ecss::EntityId spawnModelEntity(AssetsModule::Model* model, const char* path, const Math::Vec3& pos, const Math::Vec3& scale, bool glass,
			const Math::Vec3& rotate = {}) {
			if (!SFE::Engine::instance()->isAlive() || !model) {
				return {};
			}

			auto entity = ECSHandler::registry().takeEntity();
			auto aabbCmp = ECSHandler::addComponent<SFE::ComponentsModule::AABBComponent>(entity);
			aabbCmp->aabbScale = 1.f;
			ECSHandler::addComponent<OcTreeComponent>(entity);
			auto modelComp = ECSHandler::addComponent<ModelComponent>(entity, entity);
			modelComp->mPath = path;
			modelComp->boneMatrices = model->getDefaultBoneMatrices();
			modelComp->armature = model->getArmature();
			modelComp->setModel(model->getLODs());

			auto transformComp = ECSHandler::addComponent<TransformComponent>(entity, entity);
			transformComp->setPos(pos);
			transformComp->setScale(scale);
			transformComp->setRotate(rotate);
			// ChunksSystem seeds before RenderSystem exists, so the render copy is filled here rather than left to the
			// notify. It is computed from the spawn values, the way reloadTransform does for an entity with no parent,
			// instead of reloading the component: the setters above have already woken TransformSystem, whose worker
			// may be reloading the same component right now.
			const auto rotation = Math::Quaternion<float>::fromEulerDegrees(rotate);
			const auto seed = Math::translate(Math::Mat4{ 1.f }, pos) * rotation.toMat4() * Math::scale(Math::Mat4{ 1.f }, scale);
			ECSHandler::registry().addComponent<ComponentsModule::TransformMatComp>(entity, seed);
			ECSHandler::addComponent<IsDrawableComponent>(entity);

			PropertiesSystem::bindRenderable(entity);
			if (glass) {
				markGlass(entity, 0.22f, 1.52f, { 1.f, 1.f, 1.f });
			}
			return entity;
		}

		void spawnModelEntity(const char* path, const Math::Vec3& pos, const Math::Vec3& scale, bool glass) {
			spawnModelEntity(AssetsModule::ModelLoader::instance()->load(path), path, pos, scale, glass);
		}

		const AssetsModule::Animation* pickPlaybackClip(const AssetsModule::Model* model) {
			if (!model) {
				return nullptr;
			}
			const auto& anims = model->getAnimations();
			if (anims.empty()) {
				return nullptr;
			}

			auto findSub = [&](const char* sub) -> const AssetsModule::Animation* {
				for (const auto& anim : anims) {
					if (anim.getName().find(sub) != std::string::npos) {
						return &anim;
					}
				}
				return nullptr;
			};

			if (const auto* clip = findSub("dance")) {
				return clip;
			}
			if (const auto* clip = findSub("Dance")) {
				return clip;
			}
			if (const auto* clip = findSub("walk")) {
				return clip;
			}
			if (const auto* clip = findSub("idle")) {
				return clip;
			}
			if (const auto* clip = findSub("run")) {
				return clip;
			}

			for (const auto& anim : anims) {
				const auto& name = anim.getName();
				if (name.find("Static") == std::string::npos && name.find("static") == std::string::npos) {
					return &anim;
				}
			}
			return &anims.front();
		}

		ecss::EntityId spawnDancingVampire(const Math::Vec3& pos, float yawDegrees, float animPhase = 0.f) {
			constexpr const char* kPath = "models/vampire.fbx";
			auto* model = AssetsModule::ModelLoader::instance()->load(kPath);
			if (!model) {
				return {};
			}

			const auto id = spawnModelEntity(model, kPath, pos, { 0.1f }, false, { 0.f, yawDegrees, 0.f });
			if (!id) {
				return {};
			}

			if (const auto* clip = pickPlaybackClip(model)) {
				auto* anim = ECSHandler::registry().addComponent<ComponentsModule::AnimationComponent>(id);
				anim->mCurrentAnimation = clip;
				anim->mLoop = true;
				anim->play(animPhase);
				SFE::SystemsModule::TasksManager::instance()->notify({ id, SFE::SystemsModule::TaskType::ANIMATION_UPDATED });
			}

			if (auto debug = ECSHandler::registry().pinComponent<DebugDataComponent>(id)) {
				debug->stringId = "dancer";
			}
			return id;
		}

		void hsvToRgb(float h, float s, float v, float& r, float& g, float& b) {
			h = h - std::floor(h);
			const float c = v * s;
			const float x = c * (1.f - std::fabs(std::fmod(h * 6.f, 2.f) - 1.f));
			const float m = v - c;
			float rf = 0.f, gf = 0.f, bf = 0.f;
			const int sector = static_cast<int>(h * 6.f) % 6;
			switch (sector) {
			case 0: rf = c; gf = x; break;
			case 1: rf = x; gf = c; break;
			case 2: gf = c; bf = x; break;
			case 3: gf = x; bf = c; break;
			case 4: rf = x; bf = c; break;
			default: rf = c; bf = x; break;
			}
			r = rf + m;
			g = gf + m;
			b = bf + m;
		}

		void markGlass(ecss::EntityId entity, float opacity, float ior, const Math::Vec3& tint) {
			if (auto mat = ECSHandler::registry().pinComponent<ComponentsModule::MaterialComponent>(entity)) {
				mat->transparent = true;
				mat->opacity = opacity;
				mat->ior = ior;
				mat->tint = tint;
			}
			if (auto mesh = ECSHandler::registry().pinComponent<MeshComponent>(entity)) {
				for (auto& mm : mesh->meshMaterials) {
					mm.transparent = true;
					mm.opacity = opacity;
					mm.ior = ior;
					mm.tint = tint;
				}
			}
			SFE::SystemsModule::TasksManager::instance()->notify({ entity, SFE::SystemsModule::TaskType::MATERIAL_UPDATED });
		}

		void applyGlassTint(ecss::EntityId entity, uint32_t hash) {
			if (!entity) {
				return;
			}
			const float hue = static_cast<float>(hash % 12u) / 12.f;
			const float sat = 0.55f + static_cast<float>((hash >> 4) & 3u) * 0.12f;
			const float val = 0.75f + static_cast<float>((hash >> 6) & 3u) * 0.08f;
			float r = 1.f, g = 1.f, b = 1.f;
			hsvToRgb(hue, sat, val, r, g, b);
			markGlass(entity, 0.22f, 1.52f, { r, g, b });
		}

		void makeSphereLamp(ecss::EntityId entity, float spacing) {
			if (!entity) {
				return;
			}
			auto light = ECSHandler::addComponent<LightSourceComponent>(entity, entity, ComponentsModule::eLightType::POINT);
			light->setLightColor({ 6.5f, 4.0f, 1.8f });
			light->setIntensity(1.f);
			light->mRadius = (std::max)(28.f, spacing * 5.5f);
			light->mNear = 0.2f;
			light->mLinear = 0.07f;
			light->mQuadratic = 0.017f;
			light->setSamples(8);
			light->setBias(0.0002f);
			light->setTexelSize({ 0.004f, 0.004f });
			light->mWithShadows = true;

			auto debug = ECSHandler::addComponent<DebugDataComponent>(entity);
			debug->stringId = "lamp";
		}
	}

	ecss::EntityId PropertiesSystem::loadScene(std::string_view path) {
		if (!FileSystem::isFileExists(path)) {
			return {};
		}

		auto scene = ECSHandler::registry().takeEntity();
		fillTree(scene, FileSystem::readJson(path));
		return scene;
	}

	void PropertiesSystem::applyProperties(ecss::EntityId entity, const Json::Value& properties) {
		if (entity == ecss::INVALID_ID || !properties.isMember("Properties")) {
			return;
		}

		ComponentRegistry::load(entity, properties["Properties"], LoadPhase::Direct);
	}

	void PropertiesSystem::bindRenderable(ecss::EntityId entity) {
		auto modelComp = ECSHandler::registry().pinComponent<ModelComponent>(entity);
		if (!modelComp) {
			return;
		}
		if (modelComp->getModel().meshes.empty() && !modelComp->mPath.empty()) {
			if (auto* model = AssetsModule::ModelLoader::instance()->load(modelComp->mPath)) {
				modelComp->boneMatrices = model->getDefaultBoneMatrices();
				modelComp->armature = model->getArmature();
				modelComp->setModel(model->getLODs());
			}
		}
		if (modelComp->getModel().meshes.empty()) {
			return;
		}

		AssetsModule::Model* model = nullptr;
		if (!modelComp->mPath.empty()) {
			model = AssetsModule::ModelLoader::instance()->load(modelComp->mPath);
		}

		auto meshComp = ECSHandler::registry().addComponent<MeshComponent>(entity);
		auto materialComp = ECSHandler::addComponent<MaterialComponent>(entity);
		const SFE::Material* srcMat = nullptr;
		bool allGlass = true;
		bool anyMesh = false;
		if (model) {
			meshComp->meshMaterials.clear();
			SFE::Graph<SFE::RenderMeshData> meshData;
			meshData.fill<SFE::MeshObject3D>(model->getMeshTree(), [&](const SFE::MeshObject3D& meshObj) {
				meshComp->meshMaterials.push_back(SFE::ComponentsModule::fromAssetMaterial(meshObj.material));
				if (!meshObj.mesh.vertices.empty()) {
					anyMesh = true;
					if (!meshObj.material.transparent) {
						allGlass = false;
					}
				}
				if (!srcMat && !meshObj.material.materialTextures.empty()) {
					srcMat = &meshObj.material;
				}
				return std::move(SFE::MeshVaoRegistry::instance()->get(const_cast<SFE::Mesh3D*>(&meshObj.mesh)));
			});
			meshComp->meshGraph = meshData;
			meshComp->meshModel = model;
			if (anyMesh && allGlass) {
				materialComp->transparent = true;
			}
		}
		else {
			meshComp->meshGraph.root().value = SFE::MeshVaoRegistry::instance()->get(&modelComp->getModel().meshes[0]->mesh);
			if (!modelComp->getModel().meshes.empty() && modelComp->getModel().meshes[0]) {
				meshComp->meshMaterials.push_back(
					SFE::ComponentsModule::fromAssetMaterial(modelComp->getModel().meshes[0]->material));
			}
		}
		SFE::SystemsModule::TasksManager::instance()->notify({ entity, SFE::SystemsModule::TaskType::MESH_UPDATED });
		if (srcMat) {
			materialComp->materials.clearMaterials();
			for (auto& mat : srcMat->materialTextures) {
				materialComp->materials.addMaterial({ mat.second.uniformSlot, mat.second.texture });
			}
		}
		SFE::SystemsModule::TasksManager::instance()->notify({ entity, SFE::SystemsModule::TaskType::MATERIAL_UPDATED });

		if (modelComp->armature.bones.size()) {
			auto armatureComp = ECSHandler::addComponent<SFE::ComponentsModule::ArmatureComponent>(entity);
			armatureComp->armature = modelComp->armature;
		}

		if (!modelComp->boneMatrices.empty()) {
			auto armatureBonesComp = ECSHandler::registry().addComponent<SFE::ComponentsModule::ArmatureBonesComponent>(entity);
			armatureBonesComp->boneMatrices = modelComp->boneMatrices;
			SFE::SystemsModule::TasksManager::instance()->notify({ entity, SFE::SystemsModule::TaskType::ARMATURE_UPDATED });
		}

		if (model && !model->getAnimations().empty()
			&& !ECSHandler::registry().pinComponent<const ComponentsModule::AnimationComponent>(entity)) {
			auto* anim = ECSHandler::registry().addComponent<ComponentsModule::AnimationComponent>(entity);
			anim->mCurrentAnimation = pickPlaybackClip(model);
			anim->mLoop = true;
			anim->play(0.f);
			SFE::SystemsModule::TasksManager::instance()->notify({ entity, SFE::SystemsModule::TaskType::ANIMATION_UPDATED });
		}
	}

	void PropertiesSystem::fillTree(ecss::EntityId entity, const Json::Value& properties) {
		if (!properties.isObject()) {
			return;
		}

		if (properties.isMember("id")) {
			auto debugData = ECSHandler::registry().addComponent<DebugDataComponent>(entity);
			debugData->stringId = properties["id"].asString();
		}

		ECSHandler::registry().addComponent<ComponentsModule::AABBComponent>(entity);
		ECSHandler::registry().addComponent<OcTreeComponent>(entity);
		applyProperties(entity, properties);
		ECSHandler::registry().addComponent<IsDrawableComponent>(entity);
		bindRenderable(entity);
		if (properties.isMember("Properties")) {
			ComponentRegistry::load(entity, properties["Properties"], LoadPhase::AfterBind);
		}

		if (properties.isMember("Children") && properties["Children"].isArray()) {
			for (auto element : properties["Children"]) {
				auto child = ECSHandler::registry().takeEntity();
				fillTree(child, element);
				Hierarchy::setParent(child, entity);
			}
		}
	}

	Json::Value PropertiesSystem::serializeEntity(ecss::EntityId entity) {
		Json::Value result = Json::objectValue;
		if (entity == ecss::INVALID_ID) {
			return result;
		}

		if (auto debugData = ECSHandler::registry().pinComponent<const DebugDataComponent>(entity)) {
			result["id"] = debugData->stringId;
		}

		ComponentRegistry::save(entity, result["Properties"]);

		const auto children = Hierarchy::children(entity);
		if (!children.empty()) {
			result["Children"] = Json::arrayValue;
			auto& childrenJson = result["Children"];
			for (auto node : children) {
				childrenJson.append(serializeEntity(node));
			}
		}

		return result;
	}

	bool isSnapshotEntity(ecss::EntityId id) {
		if (!ECSHandler::registry().pinComponent<const ModelComponent>(id)) {
			return false;
		}
		if (ECSHandler::registry().pinComponent<const CameraComponent>(id)) {
			return false;
		}
		if (ECSHandler::registry().pinComponent<const CascadeShadowComponent>(id)) {
			return false;
		}
		if (Hierarchy::parent(id) != ecss::INVALID_ID) {
			return false;
		}
		return true;
	}

	Json::Value PropertiesSystem::serializeWorldSnapshot(const Json::Value& request) {
		Json::Value root = Json::objectValue;

		const size_t maxEntities = (std::max)(1u, request.get("maxEntities", 512).asUInt());
		const uint32_t detailId = request.get("detailId", 0u).asUInt();

		Math::Vec3 camPos{};
		if (auto* camSys = ECSHandler::getSystem<SystemsModule::CameraSystem>()) {
			if (auto tc = ECSHandler::registry().pinComponent<const TransformComponent>(camSys->getCurrentCamera())) {
				camPos = tc->getPos(true);
			}
		}

		std::vector<std::pair<float, ecss::EntityId>> ranked;
		ranked.reserve(4096);
		for (const auto& [id, model, transform] : ECSHandler::registry().view<const ModelComponent, const TransformComponent>()) {
			if (!isSnapshotEntity(id)) {
				continue;
			}
			const float dist = Math::distanceSqr(camPos, transform->getPos(true));
			ranked.emplace_back(dist, id);
		}

		root["entityCount"] = static_cast<Json::UInt64>(ranked.size());
		root["fps"] = Engine::instance()->getFPS();
		root["backend"] = Render::graphicsBackendName(Render::activeGraphicsBackend());

		std::ranges::sort(ranked, [](const auto& a, const auto& b) { return a.first < b.first; });

		Json::Value entities = Json::arrayValue;
		const size_t count = (std::min)(maxEntities, ranked.size());
		for (size_t i = 0; i < count; ++i) {
			const ecss::EntityId id = ranked[i].second;
			auto transform = ECSHandler::registry().pinComponent<const TransformComponent>(id);
			auto modelComp = ECSHandler::registry().pinComponent<const ModelComponent>(id);
			if (!transform || !modelComp) {
				continue;
			}

			Json::Value entry = Json::objectValue;
			entry["id"] = static_cast<Json::UInt>(static_cast<uint32_t>(id));
			entry["model"] = modelComp->mPath;
			const auto pos = transform->getPos(true);
			entry["pos"] = Json::arrayValue;
			entry["pos"].append(pos.x);
			entry["pos"].append(pos.y);
			entry["pos"].append(pos.z);
			if (auto mat = ECSHandler::registry().pinComponent<const ComponentsModule::MaterialComponent>(id)) {
				entry["glass"] = mat->transparent;
				entry["opacity"] = mat->opacity;
			}
			entities.append(std::move(entry));
		}
		root["entities"] = std::move(entities);

		if (detailId != 0) {
			const ecss::EntityId detailEntity = static_cast<ecss::EntityId>(detailId);
			if (ECSHandler::registry().contains(detailEntity)) {
				root["detail"] = serializeEntity(detailEntity);
				root["detail"]["id"] = detailId;
			}
		}

		return root;
	}

	Json::Value PropertiesSystem::serializeEntities(const std::vector<ecss::EntityId>& entities) {
		Json::Value root = Json::objectValue;
		root["entities"] = Json::arrayValue;
		for (auto id : entities) {
			if (Hierarchy::parent(id) != ecss::INVALID_ID) {
				continue;
			}
			root["entities"].append(serializeEntity(id));
		}
		return root;
	}

	std::vector<ecss::EntityId> PropertiesSystem::loadEntities(const Json::Value& root) {
		std::vector<ecss::EntityId> ids;
		if (!root.isMember("entities") || !root["entities"].isArray()) {
			return ids;
		}
		for (const auto& element : root["entities"]) {
			auto entity = ECSHandler::registry().takeEntity();
			fillTree(entity, element);
			ids.push_back(entity);
		}
		return ids;
	}

	ecss::EntityId PropertiesSystem::spawnDefaultScene() {
		return {};
	}

	void PropertiesSystem::spawnVampireGrid(int chunkX, int chunkZ, float chunkSize, float spacing) {
		if (spacing < 1.f) {
			spacing = 1.f;
		}

		auto* sampleModel = AssetsModule::ModelLoader::instance()->load("models/vampire.fbx");
		if (!sampleModel) {
			return;
		}
		const AssetsModule::Animation* sampleClip = pickPlaybackClip(sampleModel);
		const float clipDuration = sampleClip ? sampleClip->getDuration() : 0.f;

		const float originX = static_cast<float>(chunkX) * chunkSize;
		const float originZ = static_cast<float>(chunkZ) * chunkSize;
		const float maxX = originX + chunkSize;
		const float maxZ = originZ + chunkSize;
		const int i0 = static_cast<int>(std::ceil(originX / spacing - 1e-4f));
		const int i1 = static_cast<int>(std::ceil(maxX / spacing - 1e-4f));
		const int j0 = static_cast<int>(std::ceil(originZ / spacing - 1e-4f));
		const int j1 = static_cast<int>(std::ceil(maxZ / spacing - 1e-4f));

		for (int i = i0; i < i1; ++i) {
			for (int j = j0; j < j1; ++j) {
				const float x = static_cast<float>(i) * spacing;
				const float z = static_cast<float>(j) * spacing;
				float phase = 0.f;
				if (clipDuration > 0.f) {
					const uint32_t h = static_cast<uint32_t>(i) * 374761393u + static_cast<uint32_t>(j) * 668265263u;
					phase = clipDuration * static_cast<float>(h & 1023u) / 1024.f;
				}
				spawnDancingVampire({ x, 0.f, z }, 180.f, phase);
			}
		}
	}

	// Rooms of solid walls with a doorway and a roof, a few dancing characters in each: what is inside one room is
	// hidden from the camera and from the sun by the room in front of it, which is what occlusion culling is for.
	void PropertiesSystem::spawnRoomGrid(const int chunkX, const int chunkZ, const float chunkSize, float roomSize) {
		if (roomSize < 48.f) {
			roomSize = 48.f;
		}
		auto* box = getOrCreateUnitBox();
		if (!box) {
			return;
		}
		auto* sampleModel = AssetsModule::ModelLoader::instance()->load("models/vampire.fbx");
		const AssetsModule::Animation* sampleClip = pickPlaybackClip(sampleModel);
		const float clipDuration = sampleClip ? sampleClip->getDuration() : 0.f;

		const int rooms = (std::max)(1, static_cast<int>(chunkSize / roomSize));
		const float originX = static_cast<float>(chunkX) * chunkSize;
		const float originZ = static_cast<float>(chunkZ) * chunkSize;

		// A character stands about 18 units tall, so the walls have to clear that to hide anyone behind them.
		constexpr float kWallHeight = 26.f;
		constexpr float kWallThickness = 1.5f;
		constexpr float kDoorWidth = 14.f;
		// The gap between rooms is a street, so the camera starts outside and looks along one.
		constexpr float kStreet = 16.f;

		const auto spawnBox = [&](const Math::Vec3& center, const Math::Vec3& halfSize) {
			spawnModelEntity(box, "builtin:box", center, halfSize, false);
		};

		for (int i = 0; i < rooms; ++i) {
			for (int j = 0; j < rooms; ++j) {
				const float cx = originX + (static_cast<float>(i) + 0.5f) * roomSize;
				const float cz = originZ + (static_cast<float>(j) + 0.5f) * roomSize;
				const float half = roomSize * 0.5f - kStreet;
				const float segment = (half - kDoorWidth * 0.5f) * 0.5f;
				const float segmentCenter = kDoorWidth * 0.5f + segment;

				// Three solid walls and a fourth with a doorway in the middle, so the inside is visible from one side
				// only -- which side varies, so some rooms open onto the street the camera stands in and some do not.
				uint32_t room = static_cast<uint32_t>(chunkX * rooms + i) * 2246822519u + static_cast<uint32_t>(chunkZ * rooms + j) * 3266489917u;
				room ^= room >> 15;
				const uint32_t doorSide = room & 3u;
				for (uint32_t side = 0; side < 4u; ++side) {
					// Sides 0 and 1 run along x at -z and +z; sides 2 and 3 along z at -x and +x.
					const float sign = (side & 1u) ? 1.f : -1.f;
					const bool alongX = side < 2u;
					const Math::Vec3 center = alongX ? Math::Vec3{ cx, kWallHeight * 0.5f, cz + sign * half }
						: Math::Vec3{ cx + sign * half, kWallHeight * 0.5f, cz };
					const Math::Vec3 full = alongX ? Math::Vec3{ half, kWallHeight * 0.5f, kWallThickness }
						: Math::Vec3{ kWallThickness, kWallHeight * 0.5f, half };
					if (side != doorSide) {
						spawnBox(center, full);
						continue;
					}
					const Math::Vec3 piece = alongX ? Math::Vec3{ segment, kWallHeight * 0.5f, kWallThickness }
						: Math::Vec3{ kWallThickness, kWallHeight * 0.5f, segment };
					const Math::Vec3 step = alongX ? Math::Vec3{ segmentCenter, 0.f, 0.f } : Math::Vec3{ 0.f, 0.f, segmentCenter };
					spawnBox(center - step, piece);
					spawnBox(center + step, piece);
				}
				// The roof hides the room from the sun as well, so the shadow cascades have something to cull too.
				spawnBox({ cx, kWallHeight, cz }, { half + kWallThickness, kWallThickness, half + kWallThickness });

				if (!sampleModel) {
					continue;
				}
				for (int k = 0; k < 3; ++k) {
					const uint32_t h = static_cast<uint32_t>(chunkX * rooms + i) * 374761393u
						+ static_cast<uint32_t>(chunkZ * rooms + j) * 668265263u + static_cast<uint32_t>(k) * 2654435761u;
					const float phase = clipDuration > 0.f ? clipDuration * static_cast<float>(h & 1023u) / 1024.f : 0.f;
					const float dx = (static_cast<float>(k) - 1.f) * (half * 0.5f);
					const float dz = static_cast<float>((h >> 10) & 7u) * 0.5f - 2.f;
					spawnDancingVampire({ cx + dx, 0.f, cz + dz }, 180.f, phase);
				}
			}
		}
	}

	void PropertiesSystem::spawnSphereGrid(int chunkX, int chunkZ, float chunkSize, float spacing) {
		if (spacing < 4.f) {
			spacing = 4.f;
		}
		if (spacing > 64.f) {
			spacing = 64.f;
		}

		auto* model = getOrCreateUnitSphere();
		if (!model) {
			return;
		}

		int n = static_cast<int>(chunkSize / spacing);
		if (n < 1) {
			n = 1;
		}

		const float originX = static_cast<float>(chunkX) * chunkSize;
		const float originZ = static_cast<float>(chunkZ) * chunkSize;
		const float scaleV = spacing * 0.45f;
		const Math::Vec3 scale{ scaleV };

		uint32_t ch = static_cast<uint32_t>(chunkX) * 374761393u + static_cast<uint32_t>(chunkZ) * 668265263u;
		ch = (ch ^ (ch >> 13)) * 1274126177u;
		const int lampI = static_cast<int>(ch % static_cast<uint32_t>(n));
		const int lampJ = static_cast<int>((ch >> 8) % static_cast<uint32_t>(n));

		for (int i = 0; i < n; ++i) {
			for (int j = 0; j < n; ++j) {
				const int gx = chunkX * n + i;
				const int gz = chunkZ * n + j;
				uint32_t h = static_cast<uint32_t>(gx) * 374761393u + static_cast<uint32_t>(gz) * 668265263u;
				h = (h ^ (h >> 13)) * 1274126177u;
				const float y = scaleV + static_cast<float>(h & 255u) * (spacing * 1.5f / 255.f);
				const float x = originX + (static_cast<float>(i) + 0.5f) * spacing;
				const float z = originZ + (static_cast<float>(j) + 0.5f) * spacing;
				const bool isLamp = (i == lampI && j == lampJ);
				const auto id = spawnModelEntity(model, "builtin:sphere", { x, y, z }, scale, !isLamp);
				if (isLamp) {
					makeSphereLamp(id, spacing);
				}
				else {
					applyGlassTint(id, h);
				}
			}
		}
	}
}
