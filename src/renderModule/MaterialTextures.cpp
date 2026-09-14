#include "MaterialTextures.h"

#include "assetsModule/TextureHandler.h"
#include "core/Engine.h"

namespace SFE::Render {
	namespace {
		TextureHandle solidTexture(const uint8_t r, const uint8_t g, const uint8_t b) {
			TextureDesc desc;
			desc.width = 1;
			desc.height = 1;
			desc.format = TextureFormat::RGBA8;
			const uint8_t px[4] = { r, g, b, 255 };
			return Engine::instance()->rhi()->createTexture(desc, px);
		}
	}

	std::vector<WriteDescriptor> materialTextureWrites(const DescriptorSetHandle set, const ComponentsModule::Materials& materials) {
		static const TextureHandle black = solidTexture(0, 0, 0);
		static const TextureHandle gray = solidTexture(128, 128, 128);
		const auto* white = AssetsModule::TextureHandler::instance()->loadTexture("white.png");
		const auto* normal = AssetsModule::TextureHandler::instance()->loadTexture("defaultNormal.png");
		const auto sampler = white->sampler;

		std::vector<WriteDescriptor> writes{
			{ .set = set, .binding = SFE::DIFFUSE, .images{ { white->texture, white->sampler } } },
			{ .set = set, .binding = SFE::NORMALS, .images{ { normal->texture, normal->sampler } } },
			{ .set = set, .binding = SFE::SPECULAR, .images{ { black, sampler } } },
			{ .set = set, .binding = SFE::ROUGHNESS, .images{ { gray, sampler } } },
			{ .set = set, .binding = SFE::METALLIC, .images{ { black, sampler } } },
			{ .set = set, .binding = SFE::SUBSURFACE, .images{ { black, sampler } } },
		};
		for (uint8_t i = 0; i < materials.materialsCount; ++i) {
			const auto& slot = materials.material[i];
			if (slot.texture) {
				writes.push_back({ .set = set, .binding = static_cast<uint32_t>(slot.slot), .images{ { slot.texture, sampler } } });
			}
		}
		return writes;
	}

	DescriptorSetHandle SingleTextureSets::get(const DescriptorSetLayoutHandle layout, const uint32_t binding, const TextureHandle texture) {
		auto& set = mSets[texture.id];
		if (!set) {
			const auto* white = AssetsModule::TextureHandler::instance()->loadTexture("white.png");
			auto* rhi = Engine::instance()->rhi();
			set = rhi->allocateDescriptorSet(layout);
			rhi->updateDescriptorSets({ { .set = set, .binding = binding, .images{ { texture ? texture : white->texture, white->sampler } } } });
		}
		return set;
	}
}
