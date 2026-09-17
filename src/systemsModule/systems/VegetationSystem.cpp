#include "VegetationSystem.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "CameraSystem.h"
#include "OcTreeSystem.h"
#include "assetsModule/TextureHandler.h"
#include "componentsModule/MeshComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "debugModule/Benchmark.h"
#include "propertiesModule/PropertiesSystem.h"
#include "renderModule/MaterialRules.h"

namespace SFE::SystemsModule {
	namespace {
		// Metres past a layer's draw distance before its entities go, so one step back and forth does not churn them.
		constexpr float kHysteresis = 8.f;
		// Frames between an entity appearing or going and its impostor following: its transform reaches the GPU a
		// frame or two after the spawn, and for that long both draw rather than neither.
		constexpr uint64_t kSwapFrames = 4;
		// Spawning is spread over frames when the camera outruns it; the first frame fills everything in at once.
		constexpr double kSpawnBudgetMs = 2.0;
		// The least roughness of a material a layer lays a surface over.
		constexpr float kSurfaceRoughness = 0.9f;
	}

	bool VegetationSystem::init() {
		auto& registry = ECSHandler::registry();
		for (const auto& [id, vegetation, placements] : registry.view<const ComponentsModule::VegetationComponent, const ComponentsModule::VegetationPlacementsComponent>()) {
			if (!vegetation || !placements) {
				continue;
			}
			mVegetation = *vegetation;
			mPlacements = *placements;
			break;
		}
		mSurfaces.assign(mVegetation.layers.size(), {});
		for (size_t i = 0; i < mVegetation.layers.size(); ++i) {
			for (const auto& surface : mVegetation.layers[i].surfaces) {
				if (surface.uv.size() != 4) {
					continue;
				}
				const auto* on = AssetsModule::TextureHandler::loadTexture(surface.on);
				const auto* albedo = AssetsModule::TextureHandler::loadTexture(surface.texture + "_albedo.png");
				const auto* normals = AssetsModule::TextureHandler::loadTexture(surface.texture + "_normal.png");
				if (on && albedo && normals) {
					mSurfaces[i].push_back({ on->texture, albedo->texture, normals->texture, &surface });
				}
			}
		}
		mEntities.assign(mPlacements.instances.size(), ecss::INVALID_ID);
		mRoots.assign(mPlacements.instances.size(), ecss::INVALID_ID);
		mBits.assign((mPlacements.instances.size() + 31) / 32, 0u);
		for (const auto& layer : mVegetation.layers) {
			mFarthest = (std::max)(mFarthest, layer.drawDistance);
		}
		mReach = mFarthest * mDetail;
		// Layers without a draw distance are everywhere, for good.
		for (uint32_t i = 0; i < mPlacements.instances.size(); ++i) {
			if (mVegetation.layers[mPlacements.instances[i].layer].drawDistance <= 0.f) {
				spawn(i);
			}
		}
		return true;
	}

	void VegetationSystem::spawn(const uint32_t instance) {
		const auto& placement = mPlacements.instances[instance];
		const auto& layer = mVegetation.layers[placement.layer];
		const auto id = PropertiesModule::PropertiesSystem::spawnModel(layer.models[placement.model], placement.pos, Math::Vec3{ placement.scale },
			{ placement.pitch, placement.yaw, 0.f });
		if (id == ecss::INVALID_ID) {
			return;
		}
		// The roots stand still with the trunk and take its bark: the same layer's surfaces, no sway.
		ecss::EntityId roots = ecss::INVALID_ID;
		if (placement.model < layer.rootModels.size() && !layer.rootModels[placement.model].empty()) {
			roots = PropertiesModule::PropertiesSystem::spawnModel(layer.rootModels[placement.model], placement.pos, Math::Vec3{ placement.scale },
				{ placement.pitch, placement.yaw, 0.f });
		}
		mRoots[instance] = roots;
		const auto& surfaces = mSurfaces[placement.layer];
		for (const auto entity : { id, roots }) {
			if (entity == ecss::INVALID_ID || (layer.sway == ComponentsModule::Sway::None && surfaces.empty())) {
				continue;
			}
			auto mesh = ECSHandler::registry().pinComponent<MeshComponent>(entity);
			if (!mesh) {
				continue;
			}
			for (auto& material : mesh->meshMaterials) {
				material.sway = entity == roots ? ComponentsModule::Sway::None : layer.sway;
				const auto diffuse = Render::MaterialRules::diffuseTexture(material.materials);
				size_t used = 0;
				for (const auto& surface : surfaces) {
					if (surface.on != diffuse || used == MaterialComponent::kTriplanarLayers) {
						continue;
					}
					const bool first = used == 0;
					material.materials.addMaterial({ first ? SFE::TRIPLANAR_ALBEDO : SFE::TRIPLANAR2_ALBEDO, surface.albedo, 0 });
					material.materials.addMaterial({ first ? SFE::TRIPLANAR_NORMALS : SFE::TRIPLANAR2_NORMALS, surface.normals, 0 });
					std::copy(surface.layer->uv.begin(), surface.layer->uv.end(), material.triplanar[used].uv);
					material.triplanar[used].metres = surface.layer->metres;
					material.triplanar[used].rotated = surface.layer->rotate;
					// Bark, stone and moss have no gloss to speak of.
					material.roughness = (std::max)(material.roughness, kSurfaceRoughness);
					++used;
				}
			}
		}
		mEntities[instance] = id;
		if (layer.drawDistance > 0.f) {
			mSpawned.push_back(instance);
		}
		if (layer.impostor && layer.pitch == 0.f) {
			mShowing.push_back({ instance, id, mFrame + kSwapFrames });
		}
	}

	void VegetationSystem::setBit(const uint32_t instance, const bool set) {
		std::lock_guard lock(mBitsMutex);
		auto& word = mBits[instance / 32];
		const uint32_t bit = 1u << (instance % 32);
		word = set ? (word | bit) : (word & ~bit);
		++mBitsVersion;
	}

	bool VegetationSystem::copyEntityBits(std::vector<uint32_t>& bits, uint64_t& version) {
		std::lock_guard lock(mBitsMutex);
		if (version == mBitsVersion) {
			return false;
		}
		bits = mBits;
		version = mBitsVersion;
		return true;
	}

	void VegetationSystem::update(float) {
		FUNCTION_BENCHMARK;
		const bool first = !mInited;
		if (first) {
			mInited = init();
		}
		++mFrame;
		if (mPlacements.instances.empty()) {
			return;
		}

		if (auto* render = ECSHandler::getSystem<RenderSystem>()) {
			mDetail = (std::max)(render->getRenderData().graphics.lodDetail, 0.1f);
			mReach = mFarthest * mDetail;
		}
		auto* cameras = ECSHandler::getSystem<CameraSystem>();
		if (!cameras) {
			return;
		}
		Math::Vec3 camera;
		{
			const auto transform = ECSHandler::registry().pinComponent<const TransformComponent>(cameras->getCurrentCamera());
			if (!transform) {
				return;
			}
			camera = transform->getPos(true);
		}
		const auto distanceSq = [&](const ComponentsModule::VegetationInstance& placement) {
			const float dx = placement.pos.x - camera.x;
			const float dz = placement.pos.z - camera.z;
			return dx * dx + dz * dz;
		};

		// Gone out of reach: the impostor takes over now, the entity goes a few frames later.
		for (size_t i = 0; i < mSpawned.size();) {
			const uint32_t instance = mSpawned[i];
			const auto& placement = mPlacements.instances[instance];
			const auto& layer = mVegetation.layers[placement.layer];
			const float keep = reachOf(layer) + kHysteresis;
			if (distanceSq(placement) <= keep * keep) {
				++i;
				continue;
			}
			if (layer.impostor && layer.pitch == 0.f) {
				setBit(instance, false);
			}
			mDestroying.push_back({ instance, mEntities[instance], mFrame + kSwapFrames });
			mEntities[instance] = ecss::INVALID_ID;
			if (mRoots[instance] != ecss::INVALID_ID) {
				mDestroying.push_back({ instance, mRoots[instance], mFrame + kSwapFrames });
				mRoots[instance] = ecss::INVALID_ID;
			}
			mSpawned[i] = mSpawned.back();
			mSpawned.pop_back();
		}

		// Come into reach.
		const auto start = std::chrono::steady_clock::now();
		const float cell = mPlacements.cellSize;
		const int side = mPlacements.cellsPerSide;
		const int x0 = (std::max)(0, static_cast<int>(std::floor((camera.x - mReach - mPlacements.origin.x) / cell)));
		const int x1 = (std::min)(side - 1, static_cast<int>(std::floor((camera.x + mReach - mPlacements.origin.x) / cell)));
		const int z0 = (std::max)(0, static_cast<int>(std::floor((camera.z - mReach - mPlacements.origin.y) / cell)));
		const int z1 = (std::min)(side - 1, static_cast<int>(std::floor((camera.z + mReach - mPlacements.origin.y) / cell)));
		bool outOfTime = false;
		for (int cz = z0; cz <= z1 && !outOfTime; ++cz) {
			for (int cx = x0; cx <= x1 && !outOfTime; ++cx) {
				const size_t index = static_cast<size_t>(cz) * side + cx;
				for (uint32_t i = mPlacements.cellStart[index]; i < mPlacements.cellStart[index + 1]; ++i) {
					const auto& placement = mPlacements.instances[i];
					const float reach = reachOf(mVegetation.layers[placement.layer]);
					if (mEntities[i] != ecss::INVALID_ID || reach <= 0.f || distanceSq(placement) > reach * reach) {
						continue;
					}
					spawn(i);
				}
				if (!first) {
					outOfTime = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() > kSpawnBudgetMs;
				}
			}
		}

		for (size_t i = 0; i < mShowing.size();) {
			const auto& pending = mShowing[i];
			if (pending.frame > mFrame) {
				++i;
				continue;
			}
			// Unless it was already sent away again.
			if (mEntities[pending.instance] == pending.entity) {
				setBit(pending.instance, true);
			}
			mShowing[i] = mShowing.back();
			mShowing.pop_back();
		}

		std::vector<ecss::EntityId> doomed;
		for (size_t i = 0; i < mDestroying.size();) {
			if (mDestroying[i].frame > mFrame) {
				++i;
				continue;
			}
			doomed.push_back(mDestroying[i].entity);
			mDestroying[i] = mDestroying.back();
			mDestroying.pop_back();
		}
		if (!doomed.empty()) {
			if (auto* octree = ECSHandler::getSystem<OcTreeSystem>()) {
				octree->detach(doomed);
			}
			ECSHandler::destroyEntities(doomed);
		}
	}
}
