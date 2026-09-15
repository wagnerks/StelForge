#include "TasksManager.h"

namespace SFE::SystemsModule {
	void TasksManager::notify(Task task) const {
		const auto list = workers[task.type].load(std::memory_order_acquire);
		if (!list) {
			return;
		}
		for (const auto worker : *list) {
			worker->notify(task);//probably this should batch tasks into system, and after some accumulation should be called notify to wake up the worker
		}
	}

	void TasksManager::addWorker(TaskWorker* worker, TaskType type) {
		auto& slot = workers[type];
		auto current = slot.load(std::memory_order_acquire);
		std::shared_ptr<const Workers> next;
		do {
			auto copy = current ? std::make_shared<Workers>(*current) : std::make_shared<Workers>();
			copy->push_back(worker);
			next = std::move(copy);
		} while (!slot.compare_exchange_weak(current, next, std::memory_order_acq_rel));
	}
}

