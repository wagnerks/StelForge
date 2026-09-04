#pragma once
#include "systemsModule/SystemBase.h"
#include "mathModule/Forward.h"

#include <mutex>
#include <string>
#include <vector>

namespace SFE::SystemsModule {
	struct ChunksDebugData {
		bool mCreate = true;
		bool mClear = true;
		bool mChunksDraw = false;
		bool proceduralSpheres = false;
		float sphereSpacing = 8.f;
		int currentX = 0;
		int currentZ = 0;
		int loaded = 0;
	};

	class ChunksSystem : public ecss::System {
	public:
		struct Chunk {
			int x = 0;
			int z = 0;

			bool operator==(const Chunk& other) const { return x == other.x && z == other.z; }
			bool operator<(const Chunk& other) const {
				return x < other.x || (x == other.x && z < other.z);
			}
		};

		ChunksSystem();
		~ChunksSystem() override;

		void updateAsync(const std::vector<ecss::SectorId>& entitiesToProcess) override;
		void debugUpdate(float dt) override;

		void saveLoaded();
		void* getDebugData() override { return &debugData; }

		constexpr static inline float CHUNK_SIZE = 256.f;
		constexpr static inline int CHUNK_DEEP = 2;

	private:
		static Chunk chunkOf(const Math::Vec3& pos);
		static std::string chunkPath(const Chunk& chunk);
		static std::vector<Chunk> wantedChunks(const Chunk& center);

		void loadChunk(const Chunk& chunk);
		void unloadChunk(const Chunk& chunk);
		std::vector<ecss::EntityId> detachChunk(const Chunk& chunk);
		void seedIfEmpty();
		std::vector<ecss::EntityId> entitiesInChunk(const Chunk& chunk) const;
		bool isWorldEntity(ecss::EntityId id) const;

		ChunksDebugData debugData;
		std::mutex mMutex;
		std::vector<Chunk> mChunks;
		Chunk mCurrentChunk{ 0, 0 };
		bool mHasCurrent = false;
		std::atomic_bool mBusy{ false };
	};
}
