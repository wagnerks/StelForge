#pragma once
#include "ecss/Registry.h"
#include "nodeModule/TreeNode.h"
#include "renderModule/RenderPass.h"
#include "renderModule/TextRenderer.h"

namespace SFE::Render::RenderPasses {

	struct PosComponent {
		Math::Vec2 pos;
		Math::Vec2 size;


		Math::Vec2 pivot;
	};

	struct ColorComponent {
		Math::Vec4 color {1.f};
	};

	struct TextComponent {
		std::string text;
		float scale = 1.f;
	};

	struct FontComponent {
		Font* font = nullptr;
	};

	class GUIPass : public RenderPass {
	public:
		~GUIPass() override;
		void init() override;
		void render(Renderer* renderer, SystemsModule::RenderData& renderDataHandle, Batcher& batcher) override;

		static inline ecss::Registry registry;

		VertexArray VAO;
		Buffer VBO{ ARRAY_BUFFER };
	};
}

