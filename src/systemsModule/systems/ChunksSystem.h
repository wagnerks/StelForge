#pragma once
#include "systemsModule/SystemBase.h"
#include "mathModule/Forward.h"
#include "OcTreeSystem.h"

namespace SFE::SystemsModule {
	struct ChunksDebugData {
		bool mCreate = true;
		bool mClear = true;
		bool mChunksDraw = false;
	};

	class ChunksSystem : public ecss::System {
	public:
		using Chunk = Math::Vec3;

		ChunksSystem();
		void updateAsync(const std::vector<ecss::SectorId>& entitiesToProcess) override;
		void debugUpdate(float dt) override;

		void clearChunk(const Chunk& chunk);
		void loadChunk(const Chunk& chunk);

		void* getDebugData() override { return &debugData; }

	private:
		ChunksDebugData debugData;

		std::vector<Chunk> mChunks;
		Chunk mCurrentChunk;
		
		std::atomic_size_t mDeleted = 0;
		std::atomic_size_t mCreated = 0;
		
		std::queue<std::pair<std::vector<Chunk>, std::vector<Chunk>>> mUpdateQueue;

		FuturesBunch mFutures;
		constexpr static inline float CHUNK_SIZE = OcTreeSystem::OCTREE_SIZE * 2;//this size should be divided by octree size
		constexpr static inline uint8_t CHUNK_DEEP = 1;
	};
}
