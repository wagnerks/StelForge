#pragma once
#include <array>
#include <queue>
#include <unordered_map>

#include "containersModule/Singleton.h"
#include "ecss/Types.h"

namespace SFE::SystemsModule {

	enum TaskType : size_t {
		TRANSFORM_UPDATE,
		TRAHSFORM_RELOADED,
		AABB_UPDATED,
		CAMERA_UPDATED,
		ARMATURE_UPDATED,
		MESH_UPDATED,
		MATERIAL_UPDATED,

		COUNT
	};
	
	struct Task {
		ecss::EntityId entity;
		TaskType type;
		void* customData = nullptr;
	};

	struct TaskWorker;

	class TasksManager : public Singleton<TasksManager>{
	public:

		void notify(Task task) const;
		void addWorker(TaskWorker* worker, TaskType type);

	private:
		std::array<std::vector<TaskWorker*>, TaskType::COUNT> workers{};
	};

	struct TaskWorker {
		TaskWorker(std::initializer_list<TaskType> types = {}) {
			for (auto type : types) {
				TasksManager::instance()->addWorker(this, type);
			}
		}

		virtual ~TaskWorker() = default;
		virtual void notify(Task task) = 0;
	};
}
