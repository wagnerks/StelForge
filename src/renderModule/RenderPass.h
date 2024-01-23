#pragma once
#include <future>
#include <vector>

#include "Batcher.h"

namespace SFE {
	namespace SystemsModule {
		struct RenderData;
	}
}

namespace SFE::RenderModule {
	class Renderer;

	enum class RenderPreparingStatus {
		NONE,
		PREPARING,
		READY,
	};

	class RenderPassData {
	public:
		virtual ~RenderPassData() = default;
		Batcher& getBatcher() { return mBatcher; }
	private:
		Batcher mBatcher;
		
	};

	class RenderPass {
	public:
		virtual ~RenderPass() = default;
		virtual void render(Renderer* renderer, SystemsModule::RenderData& renderDataHandle, Batcher& batcher) = 0;
		virtual void init() {}
		void setPriority(size_t priority);
		size_t getPriority() const;

	private:
		size_t mPriority = 0;
	};

	class RenderPassWithData : public RenderPass {
	public:
		std::shared_future<void> currentLock;

		~RenderPassWithData() override {
			for (auto& data : passData) {
				delete data;
			}
			passData.clear();
		}

		virtual void prepare() {}
		RenderPreparingStatus getStatus() { return mStatus; }
		void rotate() {
			if (cur == passData.size() - 1) {
				cur = 0;
			}
			else {
				cur++;
			}
		}

		RenderPassData* getCurrentPassData() const {
			return passData[cur];
		}

	protected:
		RenderPreparingStatus mStatus = RenderPreparingStatus::READY; //default status is ready for passes without render data
		std::vector<RenderPassData*> passData;
		int cur = 0;
	};

}
