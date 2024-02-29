#pragma once

#include <string>

#include "glWrapper/Shader.h"
#include "glWrapper/Texture.h"
#include "mathModule/Forward.h"

namespace SFE::ShaderModule {
	class ShaderBase {
		friend class ShaderController;
	public:
		ShaderBase(const ShaderBase& other) = delete;
		ShaderBase(ShaderBase&& other) noexcept = delete;
		ShaderBase& operator=(const ShaderBase& other) = delete;
		ShaderBase& operator=(ShaderBase&& other) noexcept = delete;
	public:

		static std::string loadShaderCode(std::string_view path);

		virtual bool compile() = 0;
		void use() const;
		unsigned int getID() const;

		template <typename T>
		void setUniform(std::string_view, const T&) const { static_assert(sizeof(T) == 0, "setUniform is not implemented for this type."); }

		std::string vertexCode;
		std::string fragmentCode;

		inline size_t getHash() const { return mHash; }
	protected:
		virtual ~ShaderBase();
		ShaderBase() = default;
		ShaderBase(size_t hash) : mHash(hash) {};

		unsigned int id = 0;
	private:
		size_t mHash = 0;
	};

	template <>
	inline void ShaderBase::setUniform<bool>(std::string_view name, const bool& val) const {
		GLW::setUniformValue(id, name.data(), val);
	}

	template <>
	inline void ShaderBase::setUniform<int>(std::string_view name, const int& val) const {
		GLW::setUniformValue(id, name.data(), val);
	}

	template <>
	inline void ShaderBase::setUniform<float>(std::string_view name, const float& val) const {
		GLW::setUniformValue(id, name.data(), val);
	}

	template <>
	inline void ShaderBase::setUniform<Math::Mat4>(std::string_view name, const Math::Mat4& val) const {
		GLW::setUniformMat4Value(id, name.data(), val.data());
	}

	template <>
	inline void ShaderBase::setUniform<Math::Mat3>(std::string_view name, const Math::Mat3& val) const {
		GLW::setUniformMat3Value(id, name.data(), val.data());
	}

	template <>
	inline void ShaderBase::setUniform<Math::Vec4>(std::string_view name, const Math::Vec4& val) const {
		GLW::setUniform4Value(id, name.data(), val.data());
	}

	template <>
	inline void ShaderBase::setUniform<Math::Vec3>(std::string_view name, const Math::Vec3& val) const {
		GLW::setUniform3Value(id, name.data(), val.data());
	}

	template <>
	inline void ShaderBase::setUniform<Math::Vec2>(std::string_view name, const Math::Vec2& val) const {
		GLW::setUniform2Value(id, name.data(), val.data());
	}

	template <>
	inline void ShaderBase::setUniform<GLW::Texture>(std::string_view name, const GLW::Texture& val) const {
		GLW::setUniformValue(id, name.data(), val.mId);
	}
}
