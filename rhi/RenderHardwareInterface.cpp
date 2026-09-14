#include "rhi/RenderHardwareInterface.h"

#include <cstddef>

namespace SFE::Render {
	ShaderModuleHandle RenderHardwareInterface::loadShader(const ShaderStage stage, const std::string_view path) {
		ShaderModuleDesc desc;
		desc.stage = stage;
		desc.name = std::string(path);
		if (m_host) {
			desc.source = m_host->loadShaderSource(path);
		}
		return createShaderModule(desc);
	}

	void RenderHardwareInterface::updateCubeMip(const TextureHandle tex, const uint32_t mip, const void* packed6Faces, const size_t faceBytes) {
		if (!packed6Faces || faceBytes == 0) {
			return;
		}
		const auto* bytes = static_cast<const std::byte*>(packed6Faces);
		for (uint32_t face = 0; face < 6; ++face) {
			updateCubeFace(tex, face, mip, bytes + face * faceBytes);
		}
	}
}
