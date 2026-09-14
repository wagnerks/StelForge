#include "TasksManager.h"

#include <cassert>

namespace SFE::SystemsModule {
	void TasksManager::notify(Task task) const {
		const auto& list = workers[task.type];
		const auto count = list.count.load(std::memory_order_acquire);
		for (size_t i = 0; i < count; ++i) {
			list.slots[i].load(std::memory_order_acquire)->notify(task);//probably this should batch tasks into system, and after some accumulation should be called notify to wake up the worker
		}
	}

	void TasksManager::addWorker(TaskWorker* worker, TaskType type) {
		// Subscribers are added from the main thread only, so the append itself needs no lock.
		auto& list = workers[type];
		const auto count = list.count.load(std::memory_order_acquire);
		assert(count < kMaxWorkersPerType);
		if (count >= kMaxWorkersPerType) {
			return;
		}
		list.slots[count].store(worker, std::memory_order_release);
		list.count.store(count + 1, std::memory_order_release);
	}
}

