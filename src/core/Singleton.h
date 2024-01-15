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
			auto instance = mInstance;
			mInstance = nullptr;
			delete instance;
		}

		inline static bool isAlive() {
			return mInstance;
		}

		inline virtual void init() {}
	protected:
		//if need to override destructor - you need to declare Singleton class as friend in inherited class
		inline virtual ~Singleton() = default;
	private:
		inline static T* mInstance = nullptr;
	};
}