#include "ThreadPool.h"

#include "core/Engine.h"
#include "logsModule/logger.h"


namespace SFE {
	ThreadPool::ThreadPool() {}

	ThreadPool::~ThreadPool() {
		shutdown();
	}

	void ThreadPool::ensureLoadingContext() {
		if (mShuttingDown.load(std::memory_order_acquire) || mLoadingWindow) {
			return;
		}
		auto* main = Engine::instance()->getMainWindow();
		if (!main) {
			return;
		}
		glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
		mLoadingWindow = glfwCreateWindow(1, 1, "loading", nullptr, main);
		glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
		glfwMakeContextCurrent(main);
		if (!mLoadingWindow) {
			SFE::LogsModule::Logger::LOG_ERROR("ThreadPool:: failed to create shared loading GL context");
		}
	}

	void ThreadPool::drainSync() {
		while (true) {
			std::packaged_task<void()> task;
			{
				std::lock_guard lock(mSyncMtx);
				if (mSyncTasks.empty()) {
					break;
				}
				task = std::move(mSyncTasks.front());
				mSyncTasks.pop();
			}
			if (task.valid()) {
				task();
			}
		}
	}

	void ThreadPool::shutdown() {
		if (mShuttingDown.exchange(true, std::memory_order_acq_rel)) {
			return;
		}

		mCommonWorkers.requestStop();
		mRenderWorkers.requestStop();
		mLoadingWorkers.requestStop();

		drainSync();
		mCommonWorkers.join();
		mRenderWorkers.join();
		mLoadingWorkers.join();
		drainSync();

		if (mLoadingWindow) {
			glfwMakeContextCurrent(nullptr);
			glfwDestroyWindow(mLoadingWindow);
			mLoadingWindow = nullptr;
		}
	}

	void ThreadPool::syncUpdate() {
		if (mShuttingDown.load(std::memory_order_acquire)) {
			return;
		}
		ensureLoadingContext();
		drainSync();
	}

	std::shared_future<void> ThreadPool::addTaskToSynchronization(std::function<void()>&& task) {
		if (mShuttingDown.load(std::memory_order_acquire)) {
			return {};
		}
		std::lock_guard lock(mSyncMtx);
		if (mShuttingDown.load(std::memory_order_acquire)) {
			return {};
		}

		mSyncTasks.emplace(std::move(task));
		return mSyncTasks.back().get_future();
	}

	std::shared_future<void> ThreadPool::addLoadingTask(std::function<void()>&& task) {
		return mLoadingWorkers.addTask([this, task]() {
			std::lock_guard lock(mLoadingGlMtx);
			if (!mLoadingWindow) {
				SFE::LogsModule::Logger::LOG_ERROR("ThreadPool:: loading GL context is null");
				task();
				return;
			}
			glfwMakeContextCurrent(mLoadingWindow);
			task();
			glfwMakeContextCurrent(nullptr);
		});
	}
}
