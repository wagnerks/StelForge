#include "ThreadPool.h"


namespace Engine {
	ThreadPool::ThreadPool() {
		workers.reserve(MAX_WORKERS + RENDER_WORKERS);

		for (auto i = 0u; i < MAX_WORKERS; i++) {
			workers.emplace_back(std::thread([this, task = std::packaged_task<void(std::mutex&)>()]()mutable {
				while (!terminating) {
					poolMutex.lock();
					if (tasksQueue.size()) {
						task = std::move(tasksQueue.front());
						tasksQueue.pop();
						poolMutex.unlock();

						task(poolMutex);
					}
					else {
						poolMutex.unlock();
						auto mutex = std::unique_lock(poolMutex);
						cond.wait(mutex);
					}
				}
			}));
		}
		for (auto i = 0u; i < RENDER_WORKERS; i++) {
			workers.emplace_back(std::thread([this, task = std::packaged_task<void(std::mutex&)>()]()mutable {
				while (!terminating) {
					poolMutex.lock();
					if (renderTasksQueue.size()) {
						task = std::move(renderTasksQueue.front());
						renderTasksQueue.pop();
						poolMutex.unlock();

						task(poolMutex);
					}
					else {
						poolMutex.unlock();
						auto mutex = std::unique_lock(poolMutex);
						renderCond.wait(mutex);
					}
				}
			}));
		}

		for (auto i = 0u; i < ECS_WORKERS; i++) {
			workers.emplace_back(std::thread([this, task = std::packaged_task<void(std::mutex&)>()]()mutable {
				while (!terminating) {
					poolMutex.lock();
					if (ecsTasksQueue.size()) {
						task = std::move(ecsTasksQueue.front());
						ecsTasksQueue.pop();
						poolMutex.unlock();

						task(poolMutex);
					}
					else {
						poolMutex.unlock();
						auto mutex = std::unique_lock(poolMutex);
						ecsLocker.wait(mutex);
					}
				}
			}));
		}
	}

	void ThreadPool::synchroUpdate() {
		while (!synchroTasks.empty()) { //mainThreadTasksQueue can be popped only in main thread, and update calling only in main thread, so it safe to check empty without lock
			auto task = std::move(synchroTasks.front());
			synchroTasks.pop();

			task();
		}
	}

	ThreadPool::~ThreadPool() {
		terminating = true;
		{
			std::lock_guard lock(poolMutex);
			cond.notify_all();
			renderCond.notify_all();
			ecsLocker.notify_all();
		}

		for (auto& worker : workers) {
			worker.join();
		}
	}

	std::shared_future<void> ThreadPool::addTask(std::function<void(std::mutex& threadMutex)> task) {
		std::lock_guard lock(poolMutex);
		tasksQueue.emplace(std::packaged_task(task));
		auto future = tasksQueue.back().get_future();
		cond.notify_one();

		return future;
	}

	std::shared_future<void> ThreadPool::addRenderTask(std::function<void(std::mutex& threadMutex)> task) {
		std::lock_guard lock(poolMutex);
		renderTasksQueue.emplace(std::packaged_task(task));
		auto future = renderTasksQueue.back().get_future();
		renderCond.notify_one();

		return future;
	}

	std::shared_future<void>  ThreadPool::addTaskToSynchronization(std::function<void()> task) {
		std::lock_guard lock(poolMutex); //if main thread update will be called while getting the future from back, and the queue size will be 1, it can try to get future from empty queue

		synchroTasks.emplace(std::move(task));

		return synchroTasks.back().get_future();
	}

	std::shared_future<void> ThreadPool::addTaskToEcsSynchronization(std::function<void(std::mutex& threadMutex)> task) {
		std::lock_guard lock(poolMutex);
		ecsTasksQueue.emplace(std::packaged_task(task));
		auto future = ecsTasksQueue.back().get_future();
		ecsLocker.notify_one();

		return future;
	}
}
