#include "ChunksSystem.h"

#include <random>

#include "CameraSystem.h"
#include "imgui.h"
#include "OcTreeSystem.h"
#include "assetsModule/modelModule/ModelLoader.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/IsDrawableComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/OcTreeComponent.h"
#include "componentsModule/TransformComponent.h"
#include "core/ECSHandler.h"
#include "debugModule/Benchmark.h"

namespace SFE::SystemsModule {
	ChunksSystem::ChunksSystem() {
		//ChunksSystem::update({ ECSHandler::getSystem<CameraSystem>()->getCurrentCamera() });
	}

	void ChunksSystem::update(const std::vector<ecss::SectorId>& entitiesToProcess) {
		auto curCam = entitiesToProcess.front();
		auto curChunkCoords = ECSHandler::registry().getComponent<TransformComponent>(curCam)->getPos(true);

		auto calcMinus = [size = static_cast<int>(CHUNK_SIZE)](float val) {
			auto remainder = [size](float val) {
				return static_cast<int>(std::fabs(val)) % size;
			};
			val = std::round(val);
			if (val < 0.f) {
				return val + remainder(val) - size;
			}
			else if (val > 0.f) {
				return val - remainder(val);
			}

			return 0.f;
		};

		curChunkCoords.x = calcMinus(curChunkCoords.x);
		curChunkCoords.y = calcMinus(curChunkCoords.y);
		curChunkCoords.z = calcMinus(curChunkCoords.z);

		if (mCurrentChunk == curChunkCoords) {
			return;
		}

		mCurrentChunk = curChunkCoords;
		mDeleted = 0;
		mCreated = 0;

		std::vector<Chunk> chunkClearTasksQueue = mChunks;
		std::vector<Chunk> chunkLoadTasksQueue;

		mChunks.clear();
		for (float x = -CHUNK_DEEP; x <= CHUNK_DEEP; x++) {
			for (float y = -CHUNK_DEEP; y <= CHUNK_DEEP; y++) {
				for (float z = -CHUNK_DEEP; z <= CHUNK_DEEP; z++) {
					mChunks.emplace_back(mCurrentChunk + Math::Vec3{ x, y, z } * CHUNK_SIZE);
					auto it = std::find(chunkClearTasksQueue.begin(), chunkClearTasksQueue.end(), mChunks.back());
					if (it == chunkClearTasksQueue.end()) {
						chunkLoadTasksQueue.emplace_back(mChunks.back());
					}
					else {
						chunkClearTasksQueue.erase(it);
					}
				}
			}
		}

		if (!mUpdateQueue.empty()) {
			mUpdateQueue.push({ std::move(chunkLoadTasksQueue), std::move(chunkClearTasksQueue) });
			return;
		}

		mUpdateQueue.push({ std::move(chunkLoadTasksQueue), std::move(chunkClearTasksQueue) });
		ThreadPool::instance()->addTask([this] {
			while (!mUpdateQueue.empty()) {
				auto taskContainers = mUpdateQueue.front();
				if (mClear) {
					std::vector<ecss::SectorId> entitiesToDelete;
					for (auto& chunk : taskContainers.second) {
						clearChunk(chunk);
						ECSHandler::getSystem<OcTreeSystem>()->forEachOctreeInAABB(FrustumModule::AABB{{chunk + CHUNK_SIZE * 0.5f}, CHUNK_SIZE * 0.5f, CHUNK_SIZE * 0.5f, CHUNK_SIZE * 0.5f}, [&entitiesToDelete](OcTreeSystem::SysOcTree& octree) mutable {
							auto lock = octree.readLock();
							octree.forEach([octree, &entitiesToDelete](auto& obj) mutable {
								auto octreeComp = ECSHandler::registry().getComponent<OcTreeComponent>(obj.data.getID());
								assert(octreeComp);
								if (!octreeComp->mParentOcTrees.empty()) {
									auto it = std::find(octreeComp->mParentOcTrees.begin(), octreeComp->mParentOcTrees.end(), octree.mPos);
									if (it != octreeComp->mParentOcTrees.end()) {
										octreeComp->mParentOcTrees.erase(it);
									}
									
								}
								
								if (octreeComp->mParentOcTrees.empty()) {
									entitiesToDelete.push_back(obj.data.getID());
								}
							});
							lock.unlock();
							octree.clear();
							/*auto wLock = octree.writeLock();
							ECSHandler::getSystem<OcTreeSystem>()->deleteOctree(octree.mPos);
							wLock.unlock();*/
						});
					}

					mDeleted += entitiesToDelete.size();
					ECSHandler::registry().destroyEntities(entitiesToDelete);
				}

				for (auto& chunk : taskContainers.first) {
					loadChunk(chunk);
				}
				mFutures.waitAll();
				mUpdateQueue.pop();
			}
		});

	}

	void ChunksSystem::debugUpdate(float dt) {
		{
			if (ImGui::BeginMainMenuBar()) {
				if (ImGui::BeginMenu("Debug")) {
					if (ImGui::BeginMenu("Systems debug")) {
						ImGui::Checkbox("chunks debug", &mChunksDebugOpened);
						ImGui::EndMenu();
					}
					
					ImGui::EndMenu();
				}
			}
			ImGui::EndMainMenuBar();
		}
		if (mChunksDebugOpened) {
			if (ImGui::Begin("chunks debug", &mChunksDebugOpened)) {
				ImGui::Text("cur chunk %f, %f, %f", mCurrentChunk.x, mCurrentChunk.y, mCurrentChunk.z);
				ImGui::Text("deleted %zu", mDeleted.load());
				ImGui::Text("created %zu", mCreated.load());
				ImGui::Checkbox("create", &mCreate);
				ImGui::Checkbox("clear", &mClear);
				ImGui::Checkbox("draw chunks borders", &mChunksDraw);

				if (ImGui::Button("clearEmptyEnts")) {
					ECSHandler::registry().removeEmptySectors();
				}

				if (mChunksDraw) {
					constexpr static Math::Mat4 rotate = {
							{1.f,0.f,0.f,0.f},
							{0.f,1.f,0.f,0.f},
							{0.f,0.f,1.f,0.f},
							{0.f,0.f,0.f,1.f}
					};

					for (auto& chunkPos : mChunks) {

						constexpr static auto notEmptyColor = Math::Vec4(0.5f, 0.5f, 0.5f, 0.08f);

						RenderModule::Utils::renderCubeMesh(
							Math::Vec3(0.f, 0.f, 0.f),
							Math::Vec3(CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE),
							rotate, Math::Vec3(chunkPos), notEmptyColor
						);
					}
				}
			}
			ImGui::End();
		}
		
	}

	void ChunksSystem::clearChunk(const Chunk& chunk) {
		if (!mClear) {
			return;
		}		
	}

	void ChunksSystem::loadChunk(const Chunk& chunk) {
		if (!mCreate) {
			return;
		}
		return;
		mFutures.add(ThreadPool::instance()->addTask([this, chunk]() {
			auto cubeModel = AssetsModule::ModelLoader::instance()->load("models/cube.fbx");
			float step = 250.f;

			std::random_device rd;
			std::mt19937 generator(rd());

			// Define the distribution for the range -5.0f to 15.0f
			std::uniform_real_distribution<float> distribution(-5.0f, 15.0f);
			std::uniform_real_distribution<float> distribution1(1.f, 3.0f);
			std::uniform_int_distribution<int> distribution2(0, 1);

			//ECSHandler::registry().reserve<SFE::ComponentsModule::AABBComponent, OcTreeComponent, ModelComponent, TransformComponent>(24*24*24);

			for (auto i = 1; i < 33; i+=1) {
				for (auto y = 1; y < 33; y+=1) {
					for (auto z = 1; z < 33 ; z+=1) {
						if (distribution2(generator)) {
							continue;
						}
						auto cube = ECSHandler::registry().takeEntity();
						++mCreated;

						float randomValue = distribution(generator);
						float randomValue2 = distribution1(generator);
						ECSHandler::registry().addComponent<SFE::ComponentsModule::AABBComponent>(cube);
						ECSHandler::registry().addComponent<OcTreeComponent>(cube);
						ECSHandler::registry().addComponentWithInit<ModelComponent>(cube, [cubeModel](ModelComponent* comp) { comp->init(cubeModel); }, cube.getID());
						ECSHandler::registry().addComponentWithInit<TransformComponent>(cube, [chunk, i ,step,y,z, randomValue, randomValue2](TransformComponent* comp) {
							comp->setPos(chunk + Math::Vec3{ i* step, y* step + randomValue, z* step });
							comp->setScale({ randomValue2 * 0.1f, randomValue2 * 0.1f, randomValue2 * 0.1f });
						}, cube.getID());
					}
				}
			}
		}));
	}
}

