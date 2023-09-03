#pragma once

namespace Engine {
	namespace SystemsModule {
		struct RenderDataHandle;
		class RenderSystem;
	}
}

namespace Engine::RenderModule {
	class Renderer;

	class RenderPass {
		friend bool operator<(const RenderPass& lhs, const RenderPass& rhs) {
			return lhs.mPriority < rhs.mPriority;
		}

		friend bool operator<=(const RenderPass& lhs, const RenderPass& rhs) {
			return !(rhs < lhs);
		}

		friend bool operator>(const RenderPass& lhs, const RenderPass& rhs) {
			return rhs < lhs;
		}

		friend bool operator>=(const RenderPass& lhs, const RenderPass& rhs) {
			return !(lhs < rhs);
		}

		friend bool operator==(const RenderPass& lhs, const RenderPass& rhs) {
			return lhs.mPriority == rhs.mPriority;
		}

		friend bool operator!=(const RenderPass& lhs, const RenderPass& rhs) {
			return !(lhs == rhs);
		}

	public:
		virtual ~RenderPass() = default;
		virtual void render(Renderer* renderer, SystemsModule::RenderDataHandle& renderDataHandle) = 0;
		void setPriority(size_t priority);
		size_t getPriority() const;
	private:
		size_t mPriority = 0;
	};
}
