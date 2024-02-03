#include "ThreadPool.h"


namespace SFE {
	ThreadPool::ThreadPool() {}

	void ThreadPool::syncUpdate() {
		while (!mSyncTasks.empty()) { //mainThreadTasksQueue can be popped only in main thread, and update calling only in main thread, so it safe to check empty without lock
			auto task = std::move(mSyncTasks.front());
			mSyncTasks.pop();

			task();
		}
	}

	std::shared_future<void> ThreadPool::addTaskToSynchronization(std::function<void()> task) {
		std::lock_guard lock(mSyncMtx); //if main thread update will be called while getting the future from back, and the queue size will be 1, it can try to get future from empty queue

		mSyncTasks.emplace(std::move(task));

		return mSyncTasks.back().get_future();
	}
}
