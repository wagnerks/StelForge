#pragma once
#include <map>

#include "renderModule/RenderHardwareInterface.h"
#include "renderModule/renderPasses/RenderPass.h"

namespace SFE::Render::RenderPasses {
	class DebugPass : public RenderPass {
	public:
		DebugPass();
		~DebugPass();
		void init(SystemsModule::RenderData& renderDataHandle) override;
		void render(SystemsModule::RenderData& renderDataHandle, Render::CommandListHandle cmd) override;
	private:

		PipelineHandle getTrianglePipeline(bool blend, bool depthTest, bool cull, std::pair<BlendFactor, BlendFactor> blendFactors);
		struct PipelineDesc
		{
			friend bool operator<(const PipelineDesc& lhs, const PipelineDesc& rhs)
			{
				if (lhs.blend < rhs.blend)
					return true;
				if (rhs.blend < lhs.blend)
					return false;
				if (lhs.depthTest < rhs.depthTest)
					return true;
				if (rhs.depthTest < lhs.depthTest)
					return false;
				if (lhs.cull < rhs.cull)
					return true;
				if (rhs.cull < lhs.cull)
					return false;
				return lhs.blendFactors < rhs.blendFactors;
			}

			friend bool operator<=(const PipelineDesc& lhs, const PipelineDesc& rhs)
			{
				return !(rhs < lhs);
			}

			friend bool operator>(const PipelineDesc& lhs, const PipelineDesc& rhs)
			{
				return rhs < lhs;
			}

			friend bool operator>=(const PipelineDesc& lhs, const PipelineDesc& rhs)
			{
				return !(lhs < rhs);
			}

			bool blend;
			bool depthTest;
			bool cull;
			std::pair<BlendFactor, BlendFactor> blendFactors;

			friend bool operator==(const PipelineDesc& lhs, const PipelineDesc& rhs)
			{
				return lhs.blend == rhs.blend
					&& lhs.depthTest == rhs.depthTest
					&& lhs.cull == rhs.cull
					&& lhs.blendFactors == rhs.blendFactors;
			}

			friend bool operator!=(const PipelineDesc& lhs, const PipelineDesc& rhs)
			{
				return !(lhs == rhs);
			}
		};
		std::map<PipelineDesc, PipelineHandle> trianglesPipelinesCache;

		struct DrawDataTriangles
		{
			Math::Vec4 viewPos;
			Math::Mat4 pv;
		};
		DrawDataTriangles drawDataTriangles;

		struct DrawDataLines
		{
			Math::Vec4 color;
			Math::Mat4 pv;
		};
		DrawDataLines drawDataLines;

		BufferHandle trianglesVBO;
		BufferHandle linesVBO;
		size_t mLinesCapacity = 0;
		size_t mTrisCapacity = 0;

		PipelineHandle trianglesPipeline;
		PipelineHandle linesPipeline;

		RenderPassHandle debugRenderPass;
	};
}
