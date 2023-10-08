#pragma once
#include <functional>
#include <future>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <thread>
#include <vector>

#include "Singleton.h"

namespace Engine {
	class ThreadPool : public Singleton<ThreadPool> {
		friend class Singleton;
	public:
		ThreadPool();

		void synchroUpdate();

		~ThreadPool() override;
		std::shared_future<void> addTask(std::function<void(std::mutex& threadMutex)> task);
		std::shared_future<void> addRenderTask(std::function<void(std::mutex& threadMutex)> task);
		std::shared_future<void> addTaskToSynchronization(std::function<void()> task);
		std::shared_future<void> addTaskToEcsSynchronization(std::function<void(std::mutex& threadMutex)> task);
		std::queue<std::packaged_task<void(std::mutex& threadMutex)>> ecsTasksQueue;
		std::condition_variable ecsLocker;
	private:
		const uint8_t MAX_WORKERS = 32;
		const uint8_t RENDER_WORKERS = 4;

		const uint8_t ECS_WORKERS = 4;

		std::vector<std::thread> workers;
		std::queue<std::packaged_task<void(std::mutex& threadMutex)>> tasksQueue;
		std::queue<std::packaged_task<void(std::mutex& threadMutex)>> renderTasksQueue;



		std::queue<std::packaged_task<void()>> synchroTasks;

		std::mutex poolMutex;
		std::condition_variable cond;
		std::condition_variable renderCond;




		bool terminating = false;
	};
}