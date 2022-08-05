#pragma once
#include <cmath>

#include "helper.h"

namespace ecsModule {
	class SystemInterface {
		friend class SystemManager;
	public:

		float_t mTimeSinceLastUpdate;
		float_t mUpdateInterval;

		uint16_t mPriority;

		bool mEnabled = true;
		bool mNeedsUpdate  = true;
	protected:
		SystemInterface(uint16_t priority = ecsModule::NORMAL_SYSTEM_PRIORITY, float_t updateInterval_ms = -1.0f);

	public:
		virtual ~SystemInterface() = default;

		virtual inline const char* getSystemTypeName() const = 0;
		virtual size_t getStaticSystemTypeID() const = 0;

		virtual void preUpdate(float_t dt) = 0;
		virtual void update(float_t dt) = 0;
		virtual void postUpdate(float_t dt) = 0;
	};
}
