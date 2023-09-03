#pragma once

namespace Engine {
	template<class T>
	class Singleton {
	public:
		inline static T* instance() {
			if (!mInstance) {
				mInstance = new T();
				mInstance->init();
			}

			return mInstance;
		}

		inline static void terminate() {
			delete mInstance;
			mInstance = nullptr;
		}

		inline virtual void init() {};
		inline virtual ~Singleton() = default;
	private:
		inline static T* mInstance = nullptr;
	};
}