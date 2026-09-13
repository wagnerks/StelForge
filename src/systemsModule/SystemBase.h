#pragma once

#include <shared_mutex>

#include "ecss/Types.h"
#include <vector>

#include "TasksManager.h"
#include "containersModule/SparseSet.h"
#include "multithreading/ThreadPool.h"

namespace ecss {
	class SystemTypeCounter {
		using SystemType = uint16_t;
		inline static SystemType mCounter = 0;

	public:
		template <class U>
		static SystemType type() {
			static const SystemType STATIC_TYPE_ID{mCounter++};
			return STATIC_TYPE_ID;
		}
	};

	class System : public SFE::SystemsModule::TaskWorker {
		friend class SystemManager;

	public:
		virtual ~System() = default;
		virtual void update(float dt) {}
		virtual void debugUpdate(float dt) {}
		virtual void updateAsync(const std::vector<SectorId>& entitiesToProcess) {}

		virtual void* getDebugData() { return nullptr; }
	protected:
		System(std::initializer_list<SFE::SystemsModule::TaskType> types) : TaskWorker(std::move(types)) {}
		System() = default;

		void setTick(float ticks) {
			mTicks = ticks;
		}

		float getTicks() const {
			return mTicks;
		}

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

		virtual void onNotify() {
			// mIsWorking only changes under mMutex, together with the worker's last sync. A notification
			// either reaches the running worker's next sync or finds it finished and starts a new one.
			// Setting it after releasing the lock left a window where both sides assumed the other would
			// pick the entity up, and one was silently never processed -- a random object missing from
			// the octree for the rest of the run.
			std::unique_lock lock(mMutex);
			if (mIsWorking) {
				return;
			}
			sync();
			if (mEntitiesToProcess.empty()) {
				return;
			}
			mIsWorking = true;
			lock.unlock();

			SFE::ThreadPool::instance()->addTask([this] {
				for (;;) {
					updateAsync(mEntitiesToProcess);
					std::unique_lock workerLock(mMutex);
					sync();
					if (mEntitiesToProcess.empty()) {
						mIsWorking = false;
						return;
					}
				}
			});
		}

		void notify(SFE::SystemsModule::Task task) override {
			auto& entities = mUpdatedEntities[task.type];
			
			entities.mutex.lock();
			entities.entities.push_back(task.entity);
			entities.mutex.unlock();

			onNotify();
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

		SFE::SparseSet<EntitiesContainer, SFE::SystemsModule::TaskType> mUpdatedEntities;

		std::vector<SectorId> mEntitiesToProcess;

		float mTicks = -1.f;

	private:
		std::atomic_bool mIsWorking = false;
		std::shared_mutex mMutex;
	};
}
