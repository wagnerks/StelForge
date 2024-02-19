#pragma once
#include "Forward.h"
#include "imgui.h"
#include "Quaternion.h"

namespace ImGui {
	static bool DragFloat3(std::string_view id, SFE::Math::Vec3& vec, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0) {
		if (ImGui::DragFloat3(id.data(), vec.m, v_speed, v_min, v_max, format, flags)) {
			return true;
		}

		return false;
	}

	static bool DragFloat4(std::string_view id, SFE::Math::Vec4& vec, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0) {
		if (ImGui::DragFloat4(id.data(), vec.m, v_speed, v_min, v_max, format, flags)) {
			return true;
		}

		return false;
	}

	static bool DragFloat4(std::string_view id, SFE::Math::Quaternion<float>& vec, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0) {
		float vec4[] = { vec.w, vec.x, vec.y, vec.z };
		if (ImGui::DragFloat4(id.data(), vec4, v_speed, v_min, v_max, format, flags)) {
			vec.x = vec4[1];
			vec.y = vec4[2];
			vec.z = vec4[3];
			vec.w = vec4[0];

			return true;
		}

		return false;
	}

	static bool DragFloat2(std::string_view id, SFE::Math::Vec2& vec, float v_speed = 1.0f, float v_min = 0.0f, float v_max = 0.0f, const char* format = "%.3f", ImGuiSliderFlags flags = 0) {
		if (ImGui::DragFloat2(id.data(), vec.m, v_speed, v_min, v_max, format, flags)) {
			return true;
		}

		return false;
	}

}
