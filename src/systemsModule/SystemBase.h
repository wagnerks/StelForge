#pragma once

#include "ecss/Types.h"
#include <type_traits>

namespace ecss {

	template <class T>
	class StaticTypeCounter {
	private:
		inline static ECSType mCounter = 0;
		inline static size_t mSize = 0;
	public:
		template <class U>
		static ECSType get() {
			static const ECSType STATIC_TYPE_ID{mCounter++};
			return STATIC_TYPE_ID;
		}

		template <class U>
		static size_t getSize() {
			if (std::is_const<U>::value) {
				return 0;
			}
			static const size_t STATIC_TYPE_SIZE{sizeof(U) + alignof(U)};
			mSize += STATIC_TYPE_SIZE;
			return STATIC_TYPE_SIZE;
		}

		static ECSType getCount() {
			return mCounter;
		}

		static size_t getSize() {
			return mSize;
		}
	};

	class EntityComponentSystem;

	class SystemInterface {
		friend class SystemManager;
	protected:
		SystemInterface() = default;
		virtual ~SystemInterface() = default;

	public:
		virtual void preUpdate(float dt) = 0;
		virtual void update(float dt) = 0;
		virtual void postUpdate(float dt) = 0;

	private:
		//variables should be set through systemManager
		float  mTimeSinceLastUpdate = 0.f;
		float  mUpdateInterval = 0.f;

		uint16_t mPriority = 0;

		bool	 mEnabled = true;
	};

	template <class T>
	class System : public SystemInterface {
	public:
		inline static const auto STATIC_SYSTEM_TYPE_ID = StaticTypeCounter<SystemInterface>::get<T>();
		inline static const auto STATIC_SYSTEM_SIZE = StaticTypeCounter<SystemInterface>::getSize<T>();

		void preUpdate(float dt) override {}
		void update(float dt) override {}
		void postUpdate(float dt) override {}
	};
}
