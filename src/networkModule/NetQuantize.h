#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "NetBuffer.h"
#include "mathModule/Forward.h"
#include "mathModule/Quaternion.h"

namespace SFE::Network {
	// Smaller forms for what is sent many times a second.

	// A position near `anchor`, in steps of 1/kPositionSteps metres: six bytes, for within kPositionReach of it.
	constexpr float kPositionSteps = 256.f;
	constexpr float kPositionReach = 32767.f / kPositionSteps; // 128 m

	inline void writeNear(NetWriter& writer, const Math::Vec3& position, const Math::Vec3& anchor) {
		for (int axis = 0; axis < 3; ++axis) {
			const float steps = std::round((position[axis] - anchor[axis]) * kPositionSteps);
			writer.write(static_cast<int16_t>(std::clamp(steps, -32767.f, 32767.f)));
		}
	}

	inline Math::Vec3 readNear(NetReader& reader, const Math::Vec3& anchor) {
		Math::Vec3 position;
		for (int axis = 0; axis < 3; ++axis) {
			position[axis] = anchor[axis] + static_cast<float>(reader.read<int16_t>()) / kPositionSteps;
		}
		return position;
	}

	// A rotation in seven bytes: which component is largest, and the other three (each within +-1/sqrt(2)) in sixteen
	// bits; the largest is what makes it unit length again. Good to a few hundredths of a degree.
	inline void writeRotation(NetWriter& writer, const Math::Quaternion<float>& rotation) {
		float q[4] = { rotation.x, rotation.y, rotation.z, rotation.w };
		uint8_t largest = 0;
		for (uint8_t i = 1; i < 4; ++i) {
			if (std::fabs(q[i]) > std::fabs(q[largest])) {
				largest = i;
			}
		}
		// q and -q are the same turn: the largest is sent as positive.
		const float sign = q[largest] < 0.f ? -1.f : 1.f;
		writer.write(largest);
		constexpr float kScale = 32767.f / 0.70710678f;
		for (uint8_t i = 0; i < 4; ++i) {
			if (i != largest) {
				writer.write(static_cast<int16_t>(std::clamp(std::round(q[i] * sign * kScale), -32767.f, 32767.f)));
			}
		}
	}

	inline Math::Quaternion<float> readRotation(NetReader& reader) {
		const auto largest = static_cast<uint8_t>(reader.read<uint8_t>() & 3u);
		float q[4]{};
		float sum = 0.f;
		for (uint8_t i = 0; i < 4; ++i) {
			if (i != largest) {
				q[i] = static_cast<float>(reader.read<int16_t>()) * (0.70710678f / 32767.f);
				sum += q[i] * q[i];
			}
		}
		q[largest] = std::sqrt((std::max)(0.f, 1.f - sum));
		Math::Quaternion<float> rotation;
		rotation.x = q[0];
		rotation.y = q[1];
		rotation.z = q[2];
		rotation.w = q[3];
		return rotation;
	}
}
