#pragma once
#include <typeindex>
#include <utility>

#include "ECSHandler.h"
#include "SystemInterface.h"
#include "SystemManager.h"
#include "ecsModule/helper.h"

namespace ecsModule {
	template <class T>
	class System : public SystemInterface {
	public:

		inline static const size_t STATIC_SYSTEM_TYPE_ID = ecsModule::FamilySize<SystemInterface>::Get<T>();
		size_t getStaticSystemTypeID() const override {
			return STATIC_SYSTEM_TYPE_ID;
		}

		const char* getSystemTypeName() const override {
			static const char* SYSTEM_TYPE_NAME{std::type_index(typeid(T)).name()};
			return SYSTEM_TYPE_NAME;
		}

		template <class... Dependencies>
		void AddDependencies(Dependencies&&... dependencies) {
			ECSHandler::systemManagerInstance()->addSystemDependency(this, std::forward<Dependencies>(dependencies)...);
		}

		void preUpdate(float_t dt) override {}
		void update(float_t dt) override {}
		void postUpdate(float_t dt) override {}
	};
}
