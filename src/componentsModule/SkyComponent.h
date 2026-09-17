#pragma once

#include <algorithm>
#include <cmath>

#include "mathModule/Forward.h"
#include "mathModule/Quaternion.h"

namespace SFE::ComponentsModule {
	// An outdoor sky: the atmosphere drawn behind the scene and baked into the ambient light, and the sun that lights
	// and shadows it. A scene without one keeps the studio environment and a black background.
	struct SkyComponent {
		// Degrees. Elevation above the horizon; azimuth turns the sun around the vertical axis.
		float sunElevation = 14.f;
		float sunAzimuth = 35.f;
		// 0 a clear sky, 1 overcast.
		float cloudCoverage = 0.45f;
		// Mist lying on the water: its density at the water line per metre, and the metres up over which it thins to
		// 1/e of that. 0 for none.
		float mist = 0.f;
		float mistHeight = 4.f;
		// Haze over any distance, per metre. 0 for none.
		float haze = 0.f;

		// The sun's path through a day: up in the east at 6, highest in the south at noon, down in the west at 18,
		// and under the horizon through the night.
		static constexpr float kNoonElevation = 58.f;
		void setTimeOfDay(const float hours) {
			const float angle = (hours - 6.f) / 12.f * 3.14159265f;
			sunElevation = kNoonElevation * std::sin(angle);
			sunAzimuth = 90.f + (hours - 6.f) * 15.f;
		}
		// The hour of the day this sun stands at; before or after noon by which side of south it is on.
		float timeOfDay() const {
			const float up = std::asin(std::clamp(sunElevation / kNoonElevation, -1.f, 1.f)) / 3.14159265f * 12.f;
			const float afternoon = std::fmod(sunAzimuth + 360.f, 360.f) > 180.f;
			return std::fmod(afternoon ? 18.f - up : 6.f + up + 24.f, 24.f);
		}

		// The rotation the sun's transform takes: its forward is the direction the light travels.
		Math::Vec3 sunRotation() const { return { -sunElevation, sunAzimuth, 0.f }; }

		// The moon stands opposite the sun: it rises as the sun sets, which is a full moon and the brightest night
		// there is. Its light is the sun's, so its transform and direction are the sun's turned round.
		Math::Vec3 moonRotation() const { return { sunElevation, sunAzimuth + 180.f, 0.f }; }

		// Towards the sun, the way the transform with sunRotation() computes its forward.
		Math::Vec3 toSun() const {
			const auto m = Math::Quaternion<float>::fromEulerDegrees(sunRotation()).toMat4();
			return Math::normalize(Math::Vec3{ m[2][0], m[2][1], m[2][2] });
		}
	};
}

using SFE::ComponentsModule::SkyComponent;
