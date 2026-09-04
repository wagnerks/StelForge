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
			// Delete first so instance() still returns this during ~T. Nulling
			// beforehand made Engine::instance() construct a second Engine (or
			// return null rhi) from TextureHandler / pass teardown.
			delete mInstance;
			mInstance = nullptr;
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