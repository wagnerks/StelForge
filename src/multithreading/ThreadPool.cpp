#include "ThreadPool.h"

#include "core/Engine.h"


namespace SFE {
	ThreadPool::ThreadPool() {
		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // Create an invisible window
		mLoadingWindow = glfwCreateWindow(1, 1, "loading", nullptr, Engine::instance()->getMainWindow());
	}

	ThreadPool::~ThreadPool() {
		glfwDestroyWindow(mLoadingWindow);
	}

	void ThreadPool::syncUpdate() {
		while (!mSyncTasks.empty()) { //mainThreadTasksQueue can be popped only in main thread, and update calling only in main thread, so it safe to check empty without lock
			auto task = std::move(mSyncTasks.front());
			{
				std::lock_guard lock(mSyncMtx);
				mSyncTasks.pop();
			}
			
			if (task.valid()) {
				task();
			}
		}
	}

	std::shared_future<void> ThreadPool::addTaskToSynchronization(std::function<void()>&& task) {
		std::lock_guard lock(mSyncMtx); //if main thread update will be called while getting the future from back, and the queue size will be 1, it can try to get future from empty queue

		mSyncTasks.emplace(std::move(task));

		return mSyncTasks.back().get_future();
	}

	std::shared_future<void> ThreadPool::addLoadingTask(std::function<void()>&& task) {
		return mLoadingWorkers.addTask([this, task]() {
			glfwMakeContextCurrent(mLoadingWindow);
			task();
			glfwMakeContextCurrent(nullptr);
		});
	}
}
