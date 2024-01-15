#pragma once
#include <functional>
#include <string>
#include <vector>

#include "assetsModule/TextureHandler.h"
#include "assetsModule/shaderModule/Shader.h"
#include "assetsModule/shaderModule/ShaderBase.h"
#include "componentsModule/ComponentBase.h"

namespace Engine::ComponentsModule {

	template<typename T>
	struct ShaderUniformVariable {
		ShaderUniformVariable() = default;
		ShaderUniformVariable(std::string_view name, std::vector<T> value) :name(name.data()), value(std::move(value)) {}

		std::string name = {};
		std::vector<T> value;
	};

	struct ShaderUniformVariablesI {
		virtual ~ShaderUniformVariablesI() = default;
		virtual void apply(ShaderModule::ShaderBase* shader, std::string prefix = "") = 0;

		bool dirty = true;
	};

	template<typename T>
	struct ShaderUniformVariables : ShaderUniformVariablesI {
		std::vector<ShaderUniformVariable<T>> data;

		std::pair<std::string, T>& operator[](int idx) {
			return data[idx];
		}

		ShaderUniformVariable<T>& operator[](std::string_view uniformName) {
			for (auto& dataIt : data) {
				if (dataIt.name == uniformName) {
					return dataIt;
				}
			}

			dirty = true;
			data.emplace_back(uniformName, std::vector<T>{T{}});
			return data.back();
		}

		void apply(ShaderModule::ShaderBase* shader, std::string prefix = "") override;
	};


	struct ShaderVariablesStruct {
		template<typename T>
		static std::string toString(const T& value) { return std::to_string(value); }

		template<typename T>
		static T fromString(const std::string& value) { return T{}; }

		ShaderUniformVariables<int> integerUniforms;
		ShaderUniformVariables<float> floatUniforms;
		ShaderUniformVariables<Math::Mat4> mat4Uniforms;
		ShaderUniformVariables<Math::Mat3> mat3Uniforms;
		ShaderUniformVariables<Math::Vec2> vec2Uniforms;
		ShaderUniformVariables<Math::Vec3> vec3Uniforms;
		ShaderUniformVariables<Math::Vec4> vec4Uniforms;

		ShaderUniformVariables<ShaderVariablesStruct> structUniforms;

		void apply(ShaderModule::ShaderBase* shader, std::string prefix = "");
	};

	template<>
	inline std::string ShaderVariablesStruct::toString(const int& value) { return std::to_string(value); }
	template<>
	inline int ShaderVariablesStruct::fromString(const std::string& value) { return std::atoi(value.c_str()); }

	template<>
	inline std::string ShaderVariablesStruct::toString(const float& value) { return std::to_string(value); }

	template<>
	inline double ShaderVariablesStruct::fromString(const std::string& value) { return std::atof(value.c_str()); }

	template<>
	inline std::string ShaderVariablesStruct::toString(const Math::Vec2& value) {
		return "{" + toString(value.x) + "," + toString(value.y) + "}";
	}

	template<>
	inline std::string ShaderVariablesStruct::toString(const Math::Vec3& value) {
		return "{" + toString(value.x) + "," + toString(value.y) + "," + toString(value.z) + "}";
	}

	template<>
	inline std::string ShaderVariablesStruct::toString(const Math::Vec4& value) {
		return "{" + toString(value.x) + "," + toString(value.y) + "," + toString(value.z) + "," + toString(value.w) + "}";
	}

	template<>
	inline std::string ShaderVariablesStruct::toString(const Math::Mat3& value) {
		return "{[" + toString(value[0]) + "]" + "[" + toString(value[1]) + "]" + "[" + toString(value[2]) + "]}";
	}

	template<>
	inline std::string ShaderVariablesStruct::toString(const Math::Mat4& value) {
		return "{[" + toString(value[0]) + "]" + "[" + toString(value[1]) + "]" + "[" + toString(value[2]) + "]" + "[" + toString(value[3]) + "]}";
	}

	inline void ShaderVariablesStruct::apply(ShaderModule::ShaderBase* shader, std::string prefix) {
		integerUniforms.apply(shader, prefix);
		floatUniforms.apply(shader, prefix);
		mat4Uniforms.apply(shader, prefix);
		mat3Uniforms.apply(shader, prefix);
		vec2Uniforms.apply(shader, prefix);
		vec3Uniforms.apply(shader, prefix);
		vec4Uniforms.apply(shader, prefix);
		structUniforms.apply(shader, prefix);
	}

	class ShaderComponent {
	public:
		ShaderComponent() = default;

		ShaderVariablesStruct variables;

		ShaderUniformVariables<AssetsModule::Texture*> texturesUniforms;

		std::vector<ShaderUniformVariablesI*> uniforms;

		size_t shaderId = std::numeric_limits<size_t>::max();
		std::string shaderPathVertex = "";
		std::string shaderPathFragment = "";
		std::function<void(ShaderComponent*)> updateFunction = [](ShaderComponent*) {};
	};

	template <typename T>
	void ShaderUniformVariables<T>::apply(ShaderModule::ShaderBase* shader, std::string prefix) {
		if (!dirty) {
			return;
		}

		if (!shader) {
			return;
		}
		dirty = false;
		shader->use();
		for (auto uniform : data) {
			if (uniform.value.size() == 1) {
				auto& variable = uniform.value.front();
				if (typeid(variable) == typeid(ComponentsModule::ShaderVariablesStruct)) {
					auto& var = reinterpret_cast<ComponentsModule::ShaderVariablesStruct&>(variable);
					var.apply(shader, uniform.name + ".");
				}
				else {
					shader->setValue<T>(prefix + uniform.name, uniform.value.front());
				}

			}
			else {
				for (size_t idx = 0; idx < uniform.value.size(); idx++) {
					auto& variable = uniform.value[idx];
					if (typeid(variable) == typeid(ComponentsModule::ShaderVariablesStruct)) {
						auto& var = reinterpret_cast<ComponentsModule::ShaderVariablesStruct&>(variable);
						var.apply(shader, uniform.name + "[" + std::to_string(idx) + "]" + ".");
					}
					else {
						shader->setValue<T>(prefix + uniform.name + "[" + std::to_string(idx) + "]", uniform.value[idx]);
					}
				}
			}
		}
	}

}

using Engine::ComponentsModule::ShaderComponent;