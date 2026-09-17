#include "Vegetation.h"

#include <algorithm>
#include <cmath>
#include <execution>
#include <numeric>
#include <random>
#include <unordered_map>
#include <vector>

#include "assetsModule/AssetsManager.h"
#include "assetsModule/TextureHandler.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "assetsModule/modelModule/Terrain.h"
#include "componentsModule/MeshComponent.h"
#include "componentsModule/TerrainComponent.h"
#include "componentsModule/TransformComponent.h"
#include "componentsModule/VegetationComponent.h"
#include "core/BinaryCache.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "propertiesModule/SceneComponents.h"
#include "systemsModule/TasksManager.h"
#include <json/writer.h>

namespace SFE::PropertiesModule {
	namespace {
		// Positions already taken by a layer, bucketed by `spacing`, so a new one only checks its neighbours.
		class SpacingGrid {
		public:
			explicit SpacingGrid(float spacing) : mSpacing(spacing) {}

			bool tryTake(float x, float z) {
				if (mSpacing <= 0.f) {
					return true;
				}
				const int cx = static_cast<int>(std::floor(x / mSpacing));
				const int cz = static_cast<int>(std::floor(z / mSpacing));
				for (int dx = -1; dx <= 1; ++dx) {
					for (int dz = -1; dz <= 1; ++dz) {
						const auto it = mCells.find(key(cx + dx, cz + dz));
						if (it == mCells.end()) {
							continue;
						}
						for (const auto& [px, pz] : it->second) {
							if ((px - x) * (px - x) + (pz - z) * (pz - z) < mSpacing * mSpacing) {
								return false;
							}
						}
					}
				}
				mCells[key(cx, cz)].push_back({ x, z });
				return true;
			}

		private:
			static int64_t key(int x, int z) { return (static_cast<int64_t>(x) << 32) ^ static_cast<uint32_t>(z); }

			float mSpacing = 0.f;
			std::unordered_map<int64_t, std::vector<std::pair<float, float>>> mCells;
		};

		// How deep in the forest each spot is, 0..1, on the terrain's own x/z (centred on it), kForestTexelMetres a
		// texel: every tree of a layer with a canopy shades a soft disc round it, and where the discs pile up is forest.
		struct ForestMap {
			int side = 0;
			std::vector<float> texels;

			float at(float x, float z) const {
				const float half = static_cast<float>(side) * 0.5f;
				const float gx = std::clamp(x / ComponentsModule::kForestTexelMetres + half - 0.5f, 0.f, static_cast<float>(side - 1));
				const float gz = std::clamp(z / ComponentsModule::kForestTexelMetres + half - 0.5f, 0.f, static_cast<float>(side - 1));
				const int x0 = (std::min)(static_cast<int>(gx), side - 2);
				const int z0 = (std::min)(static_cast<int>(gz), side - 2);
				const float fx = gx - static_cast<float>(x0);
				const float fz = gz - static_cast<float>(z0);
				const auto texel = [&](int i, int j) { return texels[static_cast<size_t>(j) * side + i]; };
				return std::lerp(std::lerp(texel(x0, z0), texel(x0 + 1, z0), fx), std::lerp(texel(x0, z0 + 1), texel(x0 + 1, z0 + 1), fx), fz);
			}
		};

		ForestMap buildForest(const std::vector<ComponentsModule::VegetationInstance>& instances, const ComponentsModule::VegetationComponent& vegetation,
			float terrainSize, const Math::Vec3& origin) {
			const float texel = ComponentsModule::kForestTexelMetres;
			ForestMap forest;
			forest.side = (std::max)(2, static_cast<int>(std::ceil(terrainSize / texel)));
			const size_t side = static_cast<size_t>(forest.side);
			std::vector<float> shade(side * side, 0.f);
			const float half = static_cast<float>(forest.side) * 0.5f;
			for (const auto& instance : instances) {
				const float canopy = vegetation.layers[instance.layer].canopy * instance.scale;
				if (canopy <= 0.f) {
					continue;
				}
				const float cx = (instance.pos.x - origin.x) / texel + half;
				const float cz = (instance.pos.z - origin.z) / texel + half;
				const float reach = canopy * 1.5f / texel;
				for (int j = (std::max)(0, static_cast<int>(cz - reach)); j <= (std::min)(forest.side - 1, static_cast<int>(cz + reach)); ++j) {
					for (int i = (std::max)(0, static_cast<int>(cx - reach)); i <= (std::min)(forest.side - 1, static_cast<int>(cx + reach)); ++i) {
						const float dx = static_cast<float>(i) + 0.5f - cx;
						const float dz = static_cast<float>(j) + 0.5f - cz;
						const float d2 = (dx * dx + dz * dz) / (reach * reach);
						if (d2 < 1.f) {
							shade[static_cast<size_t>(j) * side + i] += (1.f - d2) * (1.f - d2);
						}
					}
				}
			}
			// Softened a few texels each way, rows then columns, so the forest thins out towards its edge rather than
			// stopping at it.
			constexpr int kBlur = 3;
			std::vector<float> blurred(shade.size());
			for (const bool rows : { true, false }) {
				for (size_t line = 0; line < side; ++line) {
					for (size_t k = 0; k < side; ++k) {
						float sum = 0.f;
						for (int o = -kBlur; o <= kBlur; ++o) {
							const size_t at = static_cast<size_t>(std::clamp(static_cast<int>(k) + o, 0, forest.side - 1));
							sum += rows ? shade[line * side + at] : shade[at * side + line];
						}
						(rows ? blurred[line * side + k] : blurred[k * side + line]) = sum / static_cast<float>(2 * kBlur + 1);
					}
				}
				shade.swap(blurred);
			}
			forest.texels.resize(shade.size());
			std::transform(shade.begin(), shade.end(), forest.texels.begin(), [](float s) { return 1.f - std::exp(-1.5f * s); });
			return forest;
		}

		// The forest map as a texture, on the terrain's material in the SPECULAR slot, where the terrain shader reads
		// it; GrassPass takes it from the placements.
		Render::TextureHandle forestTexture(const ForestMap& forest, uint64_t key, ecss::EntityId terrainEntity) {
			const std::string name = "builtin:forest:" + std::to_string(key);
			auto* texture = AssetsModule::AssetsManager::instance()->getAsset<AssetsModule::Texture>(name);
			if (!texture) {
				std::vector<uint8_t> bytes(forest.texels.size());
				std::transform(forest.texels.begin(), forest.texels.end(), bytes.begin(),
					[](float f) { return static_cast<uint8_t>(std::lround(std::clamp(f, 0.f, 1.f) * 255.f)); });
				Render::TextureDesc desc;
				desc.width = static_cast<uint32_t>(forest.side);
				desc.height = desc.width;
				desc.format = Render::TextureFormat::R8;
				texture = AssetsModule::AssetsManager::instance()->createAsset<AssetsModule::Texture>(name);
				texture->texture = Engine::instance()->rhi()->createTexture(desc, bytes.data());
			}
			if (auto mesh = ECSHandler::registry().pinComponent<MeshComponent>(terrainEntity)) {
				for (auto& material : mesh->meshMaterials) {
					auto& materials = material.materials;
					const auto end = materials.material + materials.materialsCount;
					const auto slot = std::find_if(materials.material, end, [](const auto& m) { return m.slot == SFE::SPECULAR; });
					if (slot != end) {
						slot->texture = texture->texture;
					}
					else {
						materials.addMaterial({ SFE::SPECULAR, texture->texture, 0 });
					}
				}
			}
			SystemsModule::TasksManager::instance()->notify({ terrainEntity, SystemsModule::TaskType::MATERIAL_UPDATED });
			return texture->texture;
		}
	}

	namespace {
		// Bump when the placement rules change: the cache keys on this too.
		constexpr uint32_t kCacheVersion = 5;
	}

	void generateVegetation(const ecss::EntityId terrainEntity) {
		auto& registry = ECSHandler::registry();
		ComponentsModule::TerrainComponent terrain;
		ComponentsModule::VegetationComponent vegetation;
		Math::Vec3 origin{};
		{
			const auto terrainComp = registry.pinComponent<const ComponentsModule::TerrainComponent>(terrainEntity);
			const auto vegetationComp = registry.pinComponent<const ComponentsModule::VegetationComponent>(terrainEntity);
			const auto transform = registry.pinComponent<const TransformComponent>(terrainEntity);
			if (!terrainComp || !vegetationComp) {
				return;
			}
			terrain = *terrainComp;
			vegetation = *vegetationComp;
			origin = transform ? transform->getPos() : Math::Vec3{};
		}

		// Every model the layers use, together: loading them one by one as they are first spawned reads their files and
		// textures one after another.
		std::vector<std::string> models;
		for (const auto& layer : vegetation.layers) {
			models.insert(models.end(), layer.models.begin(), layer.models.end());
		}
		AssetsModule::ModelLoader::instance()->loadAll(models);

		ComponentsModule::VegetationPlacementsComponent placements;
		const float half = terrain.size * 0.5f;
		placements.cellsPerSide = (std::max)(1, static_cast<int>(std::ceil(terrain.size / placements.cellSize)));
		placements.origin = { origin.x - half, origin.z - half };

		// Keyed on the terrain, the component as the scene saves it and where it stands.
		Json::Value description;
		ComponentSerializer<ComponentsModule::VegetationComponent>::save(vegetation, description);
		const std::string keyText = std::to_string(kCacheVersion) + ":" + std::to_string(AssetsModule::terrainCacheKey(terrain)) + ":"
			+ std::to_string(origin.x) + ":" + std::to_string(origin.y) + ":" + std::to_string(origin.z) + ":" + Json::writeString(Json::StreamWriterBuilder{}, description);
		const auto key = BinaryCache::hash(keyText);
		const auto cacheFile = BinaryCache::path("vegetation", key);
		bool cached = false;
		if (BinaryCache::Reader reader(cacheFile); reader) {
			reader.vector(placements.cellStart);
			reader.vector(placements.instances);
			cached = reader && placements.cellStart.size() == static_cast<size_t>(placements.cellsPerSide) * placements.cellsPerSide + 1;
			if (!cached) {
				placements.cellStart.clear();
				placements.instances.clear();
			}
		}

		if (!cached) {
			const auto& heights = AssetsModule::terrainHeights(terrain);
			// Each layer on its own: its own random sequence and spacing, nothing shared but the terrain -- and, for the
			// layers that keep to the forest or away from it, the forest the layers with a canopy grew first.
			std::vector<std::vector<ComponentsModule::VegetationInstance>> layers(vegetation.layers.size());
			ForestMap forest;
			const auto grow = [&](const size_t layerIndex) {
				const auto& layer = vegetation.layers[layerIndex];
				if (layer.models.empty()) {
					return;
				}
				const bool byForest = layer.canopy <= 0.f && (layer.forestMin > 0.f || layer.forestMax < 1.f);
				auto& instances = layers[layerIndex];
				std::mt19937 random(static_cast<uint32_t>(vegetation.seed) * 2654435761u + static_cast<uint32_t>(layerIndex) * 40503u);
				std::uniform_real_distribution<float> unit(0.f, 1.f);
				const float maxSlopeTan = std::tan(layer.maxSlope * 0.0174533f);
				SpacingGrid taken(layer.spacing);

				for (int attempt = 0; attempt < layer.count; ++attempt) {
					// Every draw is made whether or not the placement survives, so changing one rule does not reshuffle
					// the rest of the layer.
					const float x = (unit(random) * 2.f - 1.f) * half * 0.97f;
					const float z = (unit(random) * 2.f - 1.f) * half * 0.97f;
					const float pick = unit(random);
					const float scaleRoll = unit(random);
					const float yaw = unit(random) * 360.f;

					float slopeX = 0.f;
					float slopeZ = 0.f;
					const float height = AssetsModule::terrainSurfaceHeight(terrain, heights, x, z, &slopeX, &slopeZ);
					if (height < layer.minHeight || height > layer.maxHeight) {
						continue;
					}
					if (slopeX * slopeX + slopeZ * slopeZ > maxSlopeTan * maxSlopeTan) {
						continue;
					}
					if (layer.noiseScale > 0.f
						&& AssetsModule::terrainNoise(x * layer.noiseScale, z * layer.noiseScale, vegetation.seed + 977 * static_cast<int>(layerIndex), 3) < layer.noiseThreshold) {
						continue;
					}
					// Nothing grows on the road; trees stand back far enough for their crowns to leave it open to the sky,
					// and rocks and logs keep off the verge.
					const float clearance = AssetsModule::kRoadHalfWidth + (layer.canopy > 0.f || layer.impostor ? 4.f : layer.colliderRadius > 0.f ? 1.5f : 0.3f);
					if (std::fabs(AssetsModule::terrainRoadDistance(terrain, x, z)) < clearance) {
						continue;
					}
					if (byForest) {
						const float depth = forest.at(x, z);
						if (depth < layer.forestMin || depth > layer.forestMax) {
							continue;
						}
					}
					if (!taken.tryTake(x, z)) {
						continue;
					}

					ComponentsModule::VegetationInstance instance;
					instance.model = static_cast<uint16_t>((std::min)(static_cast<size_t>(pick * static_cast<float>(layer.models.size())), layer.models.size() - 1));
					instance.layer = static_cast<uint16_t>(layerIndex);
					instance.scale = layer.minScale + (layer.maxScale - layer.minScale) * scaleRoll;
					instance.yaw = yaw;
					instance.pitch = layer.pitch;
					instance.pos = origin + Math::Vec3{ x, height - layer.sink * instance.scale, z };
					instances.push_back(instance);
				}
			};
			std::vector<size_t> trees;
			std::vector<size_t> rest;
			for (size_t i = 0; i < vegetation.layers.size(); ++i) {
				(vegetation.layers[i].canopy > 0.f ? trees : rest).push_back(i);
			}
			std::for_each(std::execution::par, trees.begin(), trees.end(), grow);
			std::vector<ComponentsModule::VegetationInstance> treeInstances;
			for (const size_t i : trees) {
				treeInstances.insert(treeInstances.end(), layers[i].begin(), layers[i].end());
			}
			forest = buildForest(treeInstances, vegetation, terrain.size, origin);
			std::for_each(std::execution::par, rest.begin(), rest.end(), grow);

			std::vector<ComponentsModule::VegetationInstance> instances;
			for (const auto& layer : layers) {
				instances.insert(instances.end(), layer.begin(), layer.end());
			}

			// Bucketed by cell, keeping the generation order inside each.
			const auto cellOf = [&](const ComponentsModule::VegetationInstance& instance) {
				const int cx = std::clamp(static_cast<int>((instance.pos.x - placements.origin.x) / placements.cellSize), 0, placements.cellsPerSide - 1);
				const int cz = std::clamp(static_cast<int>((instance.pos.z - placements.origin.y) / placements.cellSize), 0, placements.cellsPerSide - 1);
				return static_cast<size_t>(cz) * static_cast<size_t>(placements.cellsPerSide) + static_cast<size_t>(cx);
			};
			const size_t cellCount = static_cast<size_t>(placements.cellsPerSide) * static_cast<size_t>(placements.cellsPerSide);
			placements.cellStart.assign(cellCount + 1, 0);
			for (const auto& instance : instances) {
				++placements.cellStart[cellOf(instance) + 1];
			}
			for (size_t i = 0; i < cellCount; ++i) {
				placements.cellStart[i + 1] += placements.cellStart[i];
			}
			auto next = placements.cellStart;
			placements.instances.resize(instances.size());
			for (const auto& instance : instances) {
				placements.instances[next[cellOf(instance)]++] = instance;
			}
			BinaryCache::Writer writer(cacheFile);
			writer.vector(placements.cellStart);
			writer.vector(placements.instances);
			writer.commit();
		}

		placements.forestMap = forestTexture(buildForest(placements.instances, vegetation, terrain.size, origin), key, terrainEntity);
		registry.addComponent<ComponentsModule::VegetationPlacementsComponent>(terrainEntity, std::move(placements));
	}
}
