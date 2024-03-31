#include "TasksManager.h"
namespace SFE::SystemsModule {
	void TasksManager::notify(Task task) const {
		for (auto worker : workers[task.type]) {
			worker->notify(task);//probably this should batch tasks into system, and after some accumulation should be called notify to wake up the worker
		}
	}

	void TasksManager::addWorker(TaskWorker* worker, TaskType type) {
		workers[type].push_back(worker);
	}
}

