#pragma once
#include <atomic>
#include <cassert>
#include <functional>
#include <future>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <shared_mutex>
#include <thread>
#include <vector>
#include <core/Engine.h>

#include "containersModule/Singleton.h"

namespace SFE {

	class ThreadSynchronizer {
	public:
		ThreadSynchronizer() = default;

		ThreadSynchronizer(const ThreadSynchronizer& other) {}
		ThreadSynchronizer(ThreadSynchronizer&& other) noexcept {}
		ThreadSynchronizer& operator=(const ThreadSynchronizer& other) { return *this; }
		ThreadSynchronizer& operator=(ThreadSynchronizer&& other) noexcept { return *this;}

		inline std::unique_lock<std::shared_mutex> writeLock() { return std::unique_lock(mMutex); }
		inline std::shared_lock<std::shared_mutex> readLock() { return std::shared_lock(mMutex); }

		inline void writeLock(std::function<void()> task) { if (task) { auto lock = std::unique_lock(mMutex); task(); } }
		inline void readLock(std::function<void()> task) { if (task) { auto lock = std::shared_lock(mMutex); task(); } }
	private:
		std::shared_mutex mMutex{};
	};

	struct FuturesBunch {
		mutable std::vector<std::shared_future<void>> futures;

		void reserve(size_t capacity) const {
			futures.reserve(capacity);
		}

		void add(const std::shared_future<void>& future) const {
			futures.push_back(future);
		}

		void waitAll() const {
			for (auto& future : futures) {
				if (future.valid()) {
					future.wait();
				}
			}
			futures.clear();
		}
	};

	template<size_t Size>
	struct WorkersPool {
		WorkersPool() {
			mWorkers.reserve(Size);

			for (auto i = 0u; i < Size; i++) {
				mWorkers.emplace_back(std::thread([this]() {
					while (true) {
						std::packaged_task<void()> task;
						{
							std::unique_lock lock(mMutex);
							mCondition.wait(lock, [this] {
								return mTerminating.load(std::memory_order_acquire) || !mTasksQueue.empty();
							});
							if (mTasksQueue.empty()) {
								if (mTerminating.load(std::memory_order_acquire)) {
									break;
								}
								continue;
							}
							task = std::move(mTasksQueue.front());
							mTasksQueue.pop();
						}
						if (task.valid()) {
							task();
						}
					}
				}));
			}
		}

		~WorkersPool() {
			shutdown();
		}

		void requestStop() {
			mTerminating.store(true, std::memory_order_release);
			mCondition.notify_all();
		}

		void join() {
			for (auto& worker : mWorkers) {
				if (worker.joinable()) {
					worker.join();
				}
			}
			mWorkers.clear();
		}

		void shutdown() {
			requestStop();
			join();
		}

		std::shared_future<void> addTask(std::function<void()>&& task) {
			if (mTerminating.load(std::memory_order_acquire)) {
				return {};
			}
			std::lock_guard lock(mMutex);
			if (mTerminating.load(std::memory_order_acquire)) {
				return {};
			}
			mTasksQueue.emplace(std::packaged_task(std::move(task)));
			auto future = mTasksQueue.back().get_future();
			mCondition.notify_one();

			return future;
		}

	private:
		std::vector<std::thread> mWorkers;
		std::queue<std::packaged_task<void()>> mTasksQueue;

		std::mutex mMutex;
		std::condition_variable mCondition;

		std::atomic<bool> mTerminating{ false };
	};

	enum class WorkerType {
		COMMON,
		RENDER,
		SYNC,
		RESOURCE_LOADING
	};

	class ThreadPool : public Singleton<ThreadPool> {
	public:
		ThreadPool();
		~ThreadPool();
		
		template<WorkerType Type = WorkerType::COMMON>
		FuturesBunch addBatchTasks(size_t size, size_t batchSize, std::function<void(size_t)> task) {
			FuturesBunch futures;

			const size_t count = size / batchSize + static_cast<size_t>((size % batchSize) > 0);
			futures.reserve(count);

			for (auto i = 0u; i < count; i++) {
				futures.add(addTask<Type>([task, it = i * batchSize, last = (i + 1) * batchSize > size ? size : (i + 1) * batchSize]() mutable {
					for (; it < last; it++) {
						task(it);
					}
				}));
			}
			
			return futures;
		}

		template<WorkerType Type = WorkerType::COMMON>
		std::shared_future<void> addTask(std::function<void()>&& task) {
			switch (Type) {
			case WorkerType::COMMON:
				return mCommonWorkers.addTask(std::move(task));
			case WorkerType::RENDER:
				return mRenderWorkers.addTask(std::move(task));
			case WorkerType::SYNC:
				return addTaskToSynchronization(std::move(task));
			case WorkerType::RESOURCE_LOADING:
				return addLoadingTask(std::move(task));
			}

			assert(false);
			return {};
		}

		void syncUpdate();
		void ensureLoadingContext();
		void shutdown();
		bool isShuttingDown() const { return mShuttingDown.load(std::memory_order_acquire); }

	private:
		std::shared_future<void> addTaskToSynchronization(std::function<void()>&& task);
		std::shared_future<void> addLoadingTask(std::function<void()>&& task);
		void drainSync();

		constexpr static inline uint8_t MAX_WORKERS = 128;
		constexpr static inline uint8_t RENDER_WORKERS = 16;
		constexpr static inline uint8_t LOADING_WORKERS = 8;

		WorkersPool<MAX_WORKERS> mCommonWorkers;
		WorkersPool<RENDER_WORKERS> mRenderWorkers;
		WorkersPool<LOADING_WORKERS> mLoadingWorkers;

		std::queue<std::packaged_task<void()>> mSyncTasks;
		std::mutex mSyncMtx;
		std::mutex mLoadingGlMtx;
		std::atomic<bool> mShuttingDown{ false };

		GLFWwindow* mLoadingWindow = nullptr;
	};
}