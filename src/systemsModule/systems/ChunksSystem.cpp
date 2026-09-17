#include "ChunksSystem.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>

#include "CameraSystem.h"
#include "imgui.h"
#include "componentsModule/CameraComponent.h"
#include "componentsModule/CascadeShadowComponent.h"
#include "componentsModule/ModelComponent.h"
#include "componentsModule/OcTreeComponent.h"
#include "componentsModule/TransformComponent.h"
#include "componentsModule/Hierarchy.h"
#include "core/ECSHandler.h"
#include "core/Engine.h"
#include "core/FileSystem.h"
#include "debugModule/Benchmark.h"
#include "logsModule/logger.h"
#include "propertiesModule/PropertiesSystem.h"
#include "renderModule/Utils.h"
#include "systemsModule/systems/OcTreeSystem.h"
#include "systemsModule/systems/RenderSystem.h"

namespace {
	constexpr const char* kSceneVersionPath = "world/scene_version";
	constexpr int kDefaultSceneVersion = 20;
	constexpr float kVampireSpacing = 40.f;

	int readSceneVersion() {
		std::string text;
		if (!SFE::FileSystem::isFileExists(kSceneVersionPath) || !SFE::FileSystem::readFile(kSceneVersionPath, text)) {
			return 0;
		}
		try {
			return std::stoi(text);
		}
		catch (...) {
			return 0;
		}
	}

	void writeSceneVersion(int version) {
		SFE::FileSystem::createDirectories("world");
		std::string text = std::to_string(version);
		SFE::FileSystem::writeFile(kSceneVersionPath, text);
	}

	void clearSavedChunks() {
		namespace fs = std::filesystem;
		std::error_code ec;
		const fs::path dir = SFE::FileSystem::resolve("world/chunks");
		if (!fs::exists(dir, ec)) {
			return;
		}
		for (const auto& entry : fs::directory_iterator(dir, ec)) {
			if (entry.path().extension() == ".json") {
				fs::remove(entry.path(), ec);
			}
		}
	}
}

namespace SFE::SystemsModule {
	ChunksSystem::ChunksSystem() : System({ SFE::SystemsModule::TaskType::CAMERA_UPDATED }) {
		FileSystem::createDirectories("world/chunks");
		seedIfEmpty();
		debugData.proceduralSpheres = Engine::instance()->proceduralSpheres();
		debugData.proceduralRooms = Engine::instance()->proceduralRooms();

		// A scene file replaces the streamed chunks: it is loaded whole, and nothing is spawned or dropped around the camera.
		if (const auto& scene = Engine::instance()->scenePath(); !scene.empty()) {
			debugData.mCreate = false;
			debugData.mClear = false;
			if (!PropertiesModule::PropertiesSystem::loadScene(scene)) {
				LogsModule::Logger::LOG_ERROR("scene not loaded: %s", scene.c_str());
			}
			return;
		}

		if (!Engine::instance()->rhi()) {
			return;
		}

		Math::Vec3 camPos{};
		if (auto* cams = ECSHandler::getSystem<CameraSystem>()) {
			if (auto transform = ECSHandler::registry().pinComponent<const TransformComponent>(cams->getCurrentCamera())) {
				camPos = transform->getPos(true);
			}
		}
		const auto center = chunkOf(camPos);
		auto wanted = wantedChunks(center);
		for (const auto& c : wanted) {
			loadChunk(c);
		}
		std::lock_guard lock(mMutex);
		mCurrentChunk = center;
		mHasCurrent = true;
		mChunks = std::move(wanted);
		debugData.loaded = static_cast<int>(mChunks.size());
	}

	ChunksSystem::~ChunksSystem() {
		saveLoaded();
	}

	ChunksSystem::Chunk ChunksSystem::chunkOf(const Math::Vec3& pos) {
		return {
			static_cast<int>(std::floor(pos.x / CHUNK_SIZE)),
			static_cast<int>(std::floor(pos.z / CHUNK_SIZE))
		};
	}

	std::string ChunksSystem::chunkPath(const Chunk& chunk) {
		return "world/chunks/" + std::to_string(chunk.x) + "_" + std::to_string(chunk.z) + ".json";
	}

	std::vector<ChunksSystem::Chunk> ChunksSystem::wantedChunks(const Chunk& center) {
		std::vector<Chunk> wanted;
		wanted.reserve(static_cast<size_t>((CHUNK_DEEP * 2 + 1) * (CHUNK_DEEP * 2 + 1)));
		for (int x = -CHUNK_DEEP; x <= CHUNK_DEEP; ++x) {
			for (int z = -CHUNK_DEEP; z <= CHUNK_DEEP; ++z) {
				wanted.push_back({ center.x + x, center.z + z });
			}
		}
		return wanted;
	}

	bool ChunksSystem::isWorldEntity(ecss::EntityId id) const {
		auto& reg = ECSHandler::registry();
		if (!reg.hasComponent<ModelComponent>(id)) {
			return false;
		}
		if (reg.hasComponent<CameraComponent>(id)) {
			return false;
		}
		if (reg.hasComponent<CascadeShadowComponent>(id)) {
			return false;
		}
		if (Hierarchy::parent(id) != ecss::INVALID_ID) {
			return false;
		}
		return true;
	}

	std::vector<ecss::EntityId> ChunksSystem::entitiesInChunk(const Chunk& chunk) const {
		std::vector<std::pair<ecss::EntityId, Math::Vec3>> candidates;
		for (const auto& [id, model, transform] : ECSHandler::registry().view<const ModelComponent, const TransformComponent>()) {
			if (!transform) {
				continue;
			}
			candidates.emplace_back(id, transform->getPos(true));
		}
		std::vector<ecss::EntityId> ids;
		for (const auto& [id, pos] : candidates) {
			if (!isWorldEntity(id)) {
				continue;
			}
			if (chunkOf(pos) == chunk) {
				ids.push_back(id);
			}
		}
		std::vector<ecss::EntityId> withChildren = ids;
		for (auto id : ids) {
			const auto nodes = Hierarchy::descendants(id);
			withChildren.insert(withChildren.end(), nodes.begin(), nodes.end());
		}
		return withChildren;
	}

	void ChunksSystem::seedIfEmpty() {
		if (readSceneVersion() == kDefaultSceneVersion) {
			return;
		}
		clearSavedChunks();
		writeSceneVersion(kDefaultSceneVersion);
	}

	void ChunksSystem::loadChunk(const Chunk& chunk) {
		if (!debugData.mCreate) {
			return;
		}
		if (debugData.proceduralRooms) {
			PropertiesModule::PropertiesSystem::spawnRoomGrid(
				chunk.x, chunk.z, CHUNK_SIZE, debugData.roomSize);
			return;
		}
		if (debugData.proceduralSpheres) {
			PropertiesModule::PropertiesSystem::spawnSphereGrid(
				chunk.x, chunk.z, CHUNK_SIZE, debugData.sphereSpacing);
			return;
		}
		PropertiesModule::PropertiesSystem::spawnVampireGrid(
			chunk.x, chunk.z, CHUNK_SIZE, kVampireSpacing);
	}

	void ChunksSystem::unloadChunk(const Chunk& chunk) {
		auto ids = detachChunk(chunk);
		if (!ids.empty()) {
			ECSHandler::destroyEntities(ids);
		}
	}

	std::vector<ecss::EntityId> ChunksSystem::detachChunk(const Chunk& chunk) {
		if (!debugData.mClear) {
			return {};
		}
		auto ids = entitiesInChunk(chunk);

		if (auto* octreeSys = ECSHandler::getSystem<OcTreeSystem>()) {
			octreeSys->detach(ids);
		}
		return ids;
	}

	void ChunksSystem::saveLoaded() {
	}

	void ChunksSystem::updateAsync(const std::vector<ecss::SectorId>& entitiesToProcess) {
		if (entitiesToProcess.empty()) {
			return;
		}
		if (mBusy.exchange(true)) {
			return;
		}

		auto curCam = entitiesToProcess.front();
		Math::Vec3 camPos;
		{
			auto transform = ECSHandler::registry().pinComponent<const TransformComponent>(curCam);
			if (!transform) {
				mBusy = false;
				return;
			}
			camPos = transform->getPos(true);
		}

		const auto camChunk = chunkOf(camPos);
		std::vector<Chunk> loaded;
		{
			std::lock_guard lock(mMutex);
			if (mHasCurrent && camChunk == mCurrentChunk) {
				mBusy = false;
				return;
			}
			mCurrentChunk = camChunk;
			mHasCurrent = true;
			debugData.currentX = camChunk.x;
			debugData.currentZ = camChunk.z;
			loaded = mChunks;
		}

		auto wanted = wantedChunks(camChunk);
		std::vector<Chunk> toLoad;
		std::vector<Chunk> toUnload;
		for (const auto& c : wanted) {
			if (std::find(loaded.begin(), loaded.end(), c) == loaded.end()) {
				toLoad.push_back(c);
			}
		}
		for (const auto& c : loaded) {
			if (std::find(wanted.begin(), wanted.end(), c) == wanted.end()) {
				toUnload.push_back(c);
			}
		}

		if (toLoad.empty() && toUnload.empty()) {
			mBusy = false;
			return;
		}

		RenderSystem::WorldMutateGuard worldLock;
		if (!Engine::isMainThread()) {
			if (auto renderSys = ECSHandler::getSystem<RenderSystem>()) {
				worldLock = renderSys->lockWorldMutate();
			}
		}

		for (const auto& c : toUnload) {
			unloadChunk(c);
		}
		for (const auto& c : toLoad) {
			loadChunk(c);
		}
		{
			std::lock_guard lock(mMutex);
			mChunks = std::move(wanted);
			debugData.loaded = static_cast<int>(mChunks.size());
		}
		mBusy = false;
	}

	void ChunksSystem::debugUpdate(float dt) {
		if (!debugData.mChunksDraw) {
			return;
		}

		std::vector<Chunk> loaded;
		{
			std::lock_guard lock(mMutex);
			loaded = mChunks;
		}

		constexpr static Math::Mat4 rotate = {
			{1.f, 0.f, 0.f, 0.f},
			{0.f, 1.f, 0.f, 0.f},
			{0.f, 0.f, 1.f, 0.f},
			{0.f, 0.f, 0.f, 1.f}
		};
		constexpr static auto color = Math::Vec4(0.5f, 0.5f, 0.5f, 0.08f);
		for (const auto& chunk : loaded) {
			const Math::Vec3 pos{
				static_cast<float>(chunk.x) * CHUNK_SIZE,
				0.f,
				static_cast<float>(chunk.z) * CHUNK_SIZE
			};
			Render::Utils::renderCubeMesh(
				Math::Vec3(0.f, 0.f, 0.f),
				Math::Vec3(CHUNK_SIZE, 2.f, CHUNK_SIZE),
				rotate, pos, color
			);
		}
	}
}
