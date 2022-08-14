#pragma once
#include <cmath>

namespace ecsModule {
	class SystemInterface {
		friend class SystemManager;
	protected:
		SystemInterface();
	public:
		virtual ~SystemInterface() = default;

		virtual inline const char* getSystemTypeName() const = 0;
		virtual size_t getStaticSystemTypeID() const = 0;

		virtual void preUpdate(float_t dt) = 0;
		virtual void update(float_t dt) = 0;
		virtual void postUpdate(float_t dt) = 0;
	private:
		//variables should be set through systemManager
		float_t mTimeSinceLastUpdate;
		float_t mUpdateInterval;

		uint16_t mPriority;

		bool mEnabled = true;
	};
}
