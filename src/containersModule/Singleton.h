#pragma once

namespace SFE {
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
		Singleton() = default;
		inline virtual ~Singleton() = default;
	private:
		inline static T* mInstance = nullptr;
	};
}