#pragma once

#include <string>
#include <unordered_map>

#include "mathModule/Forward.h"

namespace Engine::ShaderModule {
	class ShaderBase {
		friend class ShaderController;
	public:
		static std::string loadShaderCode(std::string_view path);

		virtual bool compile() = 0;
		void use() const;
		unsigned int getID() const;

		template <typename T>
		void setValue(std::string_view, const T&) {}

		void setInt(const char* name, int val);
		void setBool(const char* name, bool val);

		void setMat3(const char* name, const Math::Mat3& val);
		void setMat4(const char* name, const Math::Mat4& val);
		void setVec2(const char* name, const Engine::Math::Vec2& val);
		void setVec3(const char* name, const Engine::Math::Vec3& val);
		void setVec4(const char* name, const Engine::Math::Vec4& val);

		void setFloat(const char* name, float val);
		void setUniformBlockIdx(const char* name, unsigned val);

		ShaderBase(const ShaderBase& other) = delete;
		ShaderBase(ShaderBase&& other) noexcept = delete;
		ShaderBase& operator=(const ShaderBase& other) = delete;
		ShaderBase& operator=(ShaderBase&& other) noexcept = delete;

		std::string vertexCode;
		std::string fragmentCode;

		bool compileShader(const char* shaderCode, unsigned type);
		inline size_t getHash() const { return mHash; }
	protected:
		virtual ~ShaderBase();
		ShaderBase() = default;
		ShaderBase(size_t hash) : mHash(hash) {};
		unsigned int ID = 0;
		std::unordered_map<std::string, int> cachedUniforms;


	private:
		size_t mHash = 0;

		static bool checkCompileErrors(unsigned int shader, std::string_view type);
		int getUniformLocation(const std::string& name);

	};



	template <>
	inline void ShaderBase::setValue<int>(std::string_view name, const int& val) {
		setInt(name.data(), val);
	}

	template <>
	inline void ShaderBase::setValue<float>(std::string_view name, const float& val) {
		setFloat(name.data(), val);
	}

	template <>
	inline void ShaderBase::setValue<Math::Mat4>(std::string_view name, const Math::Mat4& val) {
		setMat4(name.data(), val);
	}

	template <>
	inline void ShaderBase::setValue<Math::Mat3>(std::string_view name, const Math::Mat3& val) {
		setMat3(name.data(), val);
	}

	template <>
	inline void ShaderBase::setValue<Math::Vec4>(std::string_view name, const Math::Vec4& val) {
		setVec4(name.data(), val);
	}

	template <>
	inline void ShaderBase::setValue<Math::Vec3>(std::string_view name, const Math::Vec3& val) {
		setVec3(name.data(), val);
	}

	template <>
	inline void ShaderBase::setValue<Math::Vec2>(std::string_view name, const Math::Vec2& val) {
		setVec2(name.data(), val);
	}
}
