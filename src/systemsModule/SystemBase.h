#pragma once

#include <shared_mutex>

#include "ecss/Types.h"
#include <vector>
#include "multithreading/ThreadPool.h"

namespace ecss {
	using SystemType = uint16_t;

	class SystemTypeCounter {
		inline static SystemType mCounter = 0;

	public:
		template <class U>
		static SystemType type() {
			static const SystemType STATIC_TYPE_ID{mCounter++};
			return STATIC_TYPE_ID;
		}
	};

	template<typename T, typename KeyType = size_t>
	struct SparseSet {
		constexpr static inline KeyType INVALID_IDX = std::numeric_limits<KeyType>::max();

		T& operator[](KeyType key) {
			auto idx = getIndex(key);
			if (idx == INVALID_IDX) {
				mDense.emplace_back(T{});
				if (mSparce.size() <= key) {
					mSparce.resize(key + 1, INVALID_IDX);
				}
				idx = mDense.size() - 1;
				mSparce[key] = static_cast<KeyType>(idx);
			}

			return mDense[idx];
		}

		KeyType getKey(size_t index) const {
			for (auto i = 0u; i < mSparce.size(); i++) {
				if (mSparce[i] == index) {
					return i;
				}
			}

			return INVALID_IDX;
		}

		void insert(KeyType key, const T& obj) {
			auto idx = getIndex(key);
			if (idx != INVALID_IDX) {
				return;
			}

			mDense.emplace_back(obj);
			if (mSparce.size() <= key) {
				mSparce.resize(key + 1, INVALID_IDX);
			}
			mSparce[key] = static_cast<KeyType>(mDense.size() - 1);
		}

		typename std::vector<T>::iterator begin() {
			return mDense.begin();
		}
		typename std::vector<T>::iterator end() {
			return mDense.end();
		}

	private:

		inline size_t getIndex(KeyType key) const {
			return static_cast<size_t>(key >= mSparce.size() ? INVALID_IDX : mSparce[key]);
		}
		
		std::vector<KeyType> mSparce;//indexes
		std::vector<T> mDense;//entities
	};

	class System {
		friend class SystemManager;

	public:
		virtual void update(float dt) {}
		virtual void debugUpdate(float dt) {}
		virtual void update(const std::vector<SectorId>& entitiesToProcess) {}

	protected:
		SystemType mType;

		System() = default;
		virtual ~System() = default;

		virtual void sync() {
			std::vector<ecss::SectorId> newEntities;
			for (auto& [mutex, container] : mUpdatedEntities) {
				mutex.lock();
				for (auto entity : container) {
					newEntities.push_back(entity);
				}
				container.clear();
				mutex.unlock();
			}

			mEntitiesToProcess.clear();
			if (!newEntities.empty()) {
				std::sort(newEntities.begin(), newEntities.end());
				mEntitiesToProcess.reserve(newEntities.size());
				mEntitiesToProcess.push_back(newEntities[0]);

				for (auto i = 1; i < newEntities.size(); i++) {
					if (newEntities[i - 1] != newEntities[i]) {
						mEntitiesToProcess.push_back(newEntities[i]);
					}
				}
			}
			mEntitiesToProcess.shrink_to_fit();
		}

		virtual void onDependentUpdate() {
			std::shared_lock lock(mMutex);
			if (mIsWorking) {
				return;
			}
			mIsWorking = true;

			sync();
			if (mEntitiesToProcess.empty()) {
				mIsWorking = false;
				return;
			}

			SFE::ThreadPool::instance()->addTask([this] {
				while (!mEntitiesToProcess.empty()) {
					update(mEntitiesToProcess);
					std::unique_lock lock(mMutex);
					sync();//synced
					//separate thread added something which can be synced
					//mEntitiesToProcess empty but here is entities to be synced
				}
				//it go here to set isWorking false
				//at this time onDependentUpdate called and mIsWorking was true
				//it was returned, no sync was called, isworking set false, but 
				mIsWorking = false;
			});
		}

		virtual void onDependentParentUpdate(SystemType systemType, const std::vector<SectorId>& updatedEntities) {
			auto& entities = mUpdatedEntities[systemType];

			entities.mutex.lock();
			entities.entities.insert(entities.entities.end(), updatedEntities.begin(), updatedEntities.end());
			entities.mutex.unlock();

			onDependentUpdate();
		}

		virtual void onDependentParentUpdate(SystemType systemType, const SectorId updatedEntities) {
			auto& entities = mUpdatedEntities[systemType];
			
			entities.mutex.lock();
			entities.entities.push_back(updatedEntities);
			entities.mutex.unlock();

			onDependentUpdate();
		}
		
		virtual void updateDependents(const std::vector<SectorId>& updatedEntities) const {
			for (const auto system : mDependentSystems) {
				system->onDependentParentUpdate(mType, updatedEntities);
			}
		}

		virtual void updateDependents(const SectorId updatedEntities) const {
			for (const auto system : mDependentSystems) {
				system->onDependentParentUpdate(mType, updatedEntities);
			}
		}

	protected:
		struct EntitiesContainer {
			EntitiesContainer() = default;
			EntitiesContainer(const EntitiesContainer& other) {}
			EntitiesContainer(EntitiesContainer&& other) noexcept {}

			EntitiesContainer& operator=(const EntitiesContainer& other) {
				if (this == &other)
					return *this;
				return *this;
			}

			EntitiesContainer& operator=(EntitiesContainer&& other) noexcept {
				if (this == &other)
					return *this;
				return *this;
			}

			std::shared_mutex mutex;
			std::vector<SectorId> entities;
		};

		SparseSet<EntitiesContainer, SystemType> mUpdatedEntities;

		std::vector<SectorId> mEntitiesToProcess;

	private:
		template<typename SystemT>
		void addDependency(System* system) {
			mDependentSystems.emplace_back(system);
			mUpdatedEntities[ecss::SystemTypeCounter::type<SystemT>()];
		}

		std::vector<System*> mDependentSystems;

	private:
		std::atomic_bool mIsWorking = false;
		std::shared_mutex mMutex;
		//variables should be set through systemManager
		float mTimeFromLastUpdate = 0.f;
		float mUpdateInterval = 0.f;

		bool  mEnabled = true;		
	};
}
