#include "ImGuiRHI.h"

#include "imgui.h"
#include "logsModule/logger.h"
#include "mathModule/Forward.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace SFE::Render {
	namespace {
		ImTextureID packTex(const TextureHandle handle) {
			return reinterpret_cast<ImTextureID>(static_cast<uintptr_t>(handle.id) + 1u);
		}

		TextureHandle unpackTex(const ImTextureID id) {
			const auto value = reinterpret_cast<uintptr_t>(id);
			if (value == 0) {
				return {};
			}
			return TextureHandle{ static_cast<uint32_t>(value - 1u) };
		}

		IndexType imguiIndexType() {
			return sizeof(ImDrawIdx) == 2 ? IndexType::Uint16 : IndexType::Uint32;
		}
	}

	void ImGuiRHI::init(RenderHardwareInterface* rhi) {
		mRhi = rhi;
		if (!mRhi) {
			return;
		}

		SamplerDesc samplerDesc;
		samplerDesc.minFilter = FilterMode::Linear;
		samplerDesc.magFilter = FilterMode::Linear;
		samplerDesc.addressU = AddressMode::ClampToEdge;
		samplerDesc.addressV = AddressMode::ClampToEdge;
		mSampler = mRhi->createSampler(samplerDesc);

		DescriptorSetLayoutDesc layoutDesc;
		layoutDesc.bindings.emplace_back(0, DescriptorType::CombinedImageSampler, ShaderStage::Fragment);
		mLayout = mRhi->createDescriptorSetLayout(layoutDesc);

		RenderPassDesc rpDesc;
		rpDesc.colorAttachmentCount = 1;
		rpDesc.colorAttachments[0].loadOp = AttachmentLoadOp::Load;
		rpDesc.colorAttachments[0].storeOp = AttachmentStoreOp::Store;
		rpDesc.colorAttachments[0].format = TextureFormat::RGBA8;
		mRenderPass = mRhi->createRenderPass(rpDesc);

		GraphicsPipelineDesc pipelineDesc;
		pipelineDesc.layout.bindings.push_back({ 0, static_cast<uint32_t>(sizeof(ImDrawVert)), VertexInputRate::PerVertex });
		pipelineDesc.layout.attributes.push_back({ 0, 0, VertexFormat::Float2, static_cast<uint32_t>(offsetof(ImDrawVert, pos)), false });
		pipelineDesc.layout.attributes.push_back({ 1, 0, VertexFormat::Float2, static_cast<uint32_t>(offsetof(ImDrawVert, uv)), false });
		pipelineDesc.layout.attributes.push_back({ 2, 0, VertexFormat::UInt, static_cast<uint32_t>(offsetof(ImDrawVert, col)), false });
		pipelineDesc.topology = PrimitiveTopology::Triangles;
		pipelineDesc.cullMode = CullMode::None;
		pipelineDesc.frontFace = FrontFace::CounterClockWise;
		pipelineDesc.depthStencilState.depthTest = false;
		pipelineDesc.depthStencilState.depthWrite = false;
		pipelineDesc.blend = { true, BlendFactor::SrcAlpha, BlendFactor::OneMinusSrcAlpha };
		pipelineDesc.renderPass = mRenderPass;
		pipelineDesc.descriptorSetLayouts = { mLayout };
		pipelineDesc.pushConstantSize = sizeof(Math::Mat4);
		pipelineDesc.pushConstantStages = ShaderStage::Vertex;
		pipelineDesc.shaders.vertex = mRhi->loadShader(ShaderStage::Vertex, "shaders/imgui.vs");
		pipelineDesc.shaders.fragment = mRhi->loadShader(ShaderStage::Fragment, "shaders/imgui.fs");
		mPipeline = mRhi->createGraphicsPipeline(pipelineDesc);
		if (!mPipeline) {
			LogsModule::Logger::LOG_ERROR("ImGuiRHI: pipeline create failed");
		}
	}

	void ImGuiRHI::shutdown() {
		if (!mRhi) {
			return;
		}
		for (int i = 0; i < kFramesInFlight; ++i) {
			if (mVb[i]) {
				mRhi->destroyBuffer(mVb[i]);
				mVb[i] = {};
			}
			if (mIb[i]) {
				mRhi->destroyBuffer(mIb[i]);
				mIb[i] = {};
			}
			mVbCap[i] = 0;
			mIbCap[i] = 0;
		}
		for (auto& extra : mExtraSets) {
			if (extra.second) {
				mRhi->freeDescriptorSet(extra.second);
			}
		}
		mExtraSets.clear();
		if (mFontSet) {
			mRhi->freeDescriptorSet(mFontSet);
			mFontSet = {};
		}
		if (mFontTex) {
			mRhi->destroyTexture(mFontTex);
			mFontTex = {};
		}
		if (mSampler) {
			mRhi->destroySampler(mSampler);
			mSampler = {};
		}
		if (mPipeline) {
			mRhi->destroyGraphicsPipeline(mPipeline);
			mPipeline = {};
		}
		if (mLayout) {
			mRhi->destroyDescriptorSetLayout(mLayout);
			mLayout = {};
		}
		if (mRenderPass) {
			mRhi->destroyRenderPass(mRenderPass);
			mRenderPass = {};
		}
		mRhi = nullptr;
	}

	void ImGuiRHI::createFontsTexture() {
		if (!mRhi || !mLayout) {
			return;
		}
		unsigned char* pixels = nullptr;
		int width = 0;
		int height = 0;
		ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
		if (!pixels || width <= 0 || height <= 0) {
			LogsModule::Logger::LOG_ERROR("ImGuiRHI: font atlas is empty");
			return;
		}

		if (mFontTex) {
			mRhi->destroyTexture(mFontTex);
			mFontTex = {};
		}
		if (mFontSet) {
			mRhi->freeDescriptorSet(mFontSet);
			mFontSet = {};
		}

		TextureDesc texDesc;
		texDesc.width = static_cast<uint32_t>(width);
		texDesc.height = static_cast<uint32_t>(height);
		texDesc.format = TextureFormat::RGBA8;
		texDesc.usage = TextureUsage::Sampled;
		mFontTex = mRhi->createTexture(texDesc, pixels);
		mFontSet = mRhi->allocateDescriptorSet(mLayout);
		mRhi->updateDescriptorSets({
			{
				.set = mFontSet,
				.binding = 0,
				
				.images = { { mFontTex, mSampler } }
			}
		});
		ImGui::GetIO().Fonts->SetTexID(packTex(mFontTex));
		ImGui::GetIO().Fonts->ClearTexData();
	}

	void ImGuiRHI::ensureBuffers(const size_t vtxBytes, const size_t idxBytes) {
		auto grow = [this](BufferHandle& handle, size_t& cap, const size_t needed, const BufferUsage usage) {
			if (needed <= cap && handle) {
				return;
			}
			size_t next = cap == 0 ? needed : cap;
			while (next < needed) {
				next *= 2;
			}
			next = (std::max)(next, static_cast<size_t>(64 * 1024));
			if (handle) {
				mRhi->destroyBuffer(handle);
			}
			BufferDesc desc;
			desc.size = next;
			desc.usage = usage;
			desc.cpuAccess = CpuAccess::Write;
			handle = mRhi->createBuffer(desc);
			cap = next;
		};
		grow(mVb[mWriteFrame], mVbCap[mWriteFrame], vtxBytes, BufferUsage::Vertex);
		grow(mIb[mWriteFrame], mIbCap[mWriteFrame], idxBytes, BufferUsage::Index);
	}

	DescriptorSetHandle ImGuiRHI::descriptorFor(const TextureHandle texture) {
		if (!texture || texture.id == mFontTex.id) {
			return mFontSet;
		}
		for (const auto& extra : mExtraSets) {
			if (extra.first == texture.id) {
				return extra.second;
			}
		}
		auto set = mRhi->allocateDescriptorSet(mLayout);
		mRhi->updateDescriptorSets({
			{
				.set = set,
				.binding = 0,
				
				.images = { { texture, mSampler } }
			}
		});
		mExtraSets.push_back({ texture.id, set });
		return set;
	}

	void ImGuiRHI::bindDrawState(const CommandListHandle cmd) {
		mRhi->cmdBindPipeline(cmd, mPipeline);
		mRhi->cmdBindVertexBuffer(cmd, 0, mVb[mWriteFrame]);
		mRhi->cmdBindIndexBuffer(cmd, mIb[mWriteFrame], imguiIndexType());
		mRhi->cmdBindDescriptorSets(cmd, mPipeline, 0, &mFontSet, 1);
	}

	void ImGuiRHI::render(const CommandListHandle cmd, ImDrawData* drawData, const FramebufferHandle target, const int width, const int height) {
		if (!mRhi || !mPipeline || !mRenderPass || !target || !drawData || width <= 0 || height <= 0) {
			return;
		}
		if (drawData->CmdListsCount == 0 || drawData->TotalVtxCount == 0 || drawData->TotalIdxCount == 0) {
			return;
		}

		mWriteFrame = (mWriteFrame + 1) % kFramesInFlight;

		const size_t vtxBytes = static_cast<size_t>(drawData->TotalVtxCount) * sizeof(ImDrawVert);
		const size_t idxBytes = static_cast<size_t>(drawData->TotalIdxCount) * sizeof(ImDrawIdx);
		ensureBuffers(vtxBytes, idxBytes);
		if (!mVb[mWriteFrame] || !mIb[mWriteFrame]) {
			return;
		}

		size_t vtxOff = 0;
		size_t idxOff = 0;
		for (int n = 0; n < drawData->CmdListsCount; ++n) {
			const ImDrawList* list = drawData->CmdLists[n];
			const size_t vBytes = static_cast<size_t>(list->VtxBuffer.Size) * sizeof(ImDrawVert);
			const size_t iBytes = static_cast<size_t>(list->IdxBuffer.Size) * sizeof(ImDrawIdx);
			mRhi->updateBuffer(mVb[mWriteFrame], vtxOff, vBytes, list->VtxBuffer.Data);
			mRhi->updateBuffer(mIb[mWriteFrame], idxOff, iBytes, list->IdxBuffer.Data);
			vtxOff += vBytes;
			idxOff += iBytes;
		}

		const float L = drawData->DisplayPos.x;
		const float R = drawData->DisplayPos.x + drawData->DisplaySize.x;
		const float T = drawData->DisplayPos.y;
		const float B = drawData->DisplayPos.y + drawData->DisplaySize.y;
		if (R == L || B == T) {
			return;
		}

		Math::Mat4 proj{};
		std::memset(&proj, 0, sizeof(proj));
		proj[0][0] = 2.f / (R - L);
		proj[1][1] = 2.f / (T - B);
		proj[2][2] = -1.f;
		proj[3][0] = (R + L) / (L - R);
		proj[3][1] = (T + B) / (B - T);
		proj[3][3] = 1.f;

		RenderPassBeginInfo rpBegin;
		rpBegin.renderPass = mRenderPass;
		rpBegin.framebuffer = target;
		rpBegin.width = width;
		rpBegin.height = height;
		mRhi->cmdBeginRenderPass(cmd, rpBegin);
		bindDrawState(cmd);
		mRhi->cmdPushConstants(cmd, mPipeline, ShaderStage::Vertex, &proj, static_cast<uint32_t>(sizeof(proj)));

		const ImVec2 clipOff = drawData->DisplayPos;
		// Draw into an RHI target of `width` x `height`, not the OS framebuffer.
		// ClipRect / verts are in DisplaySize space — map them 1:1 onto the pass.
		const float dispW = drawData->DisplaySize.x;
		const float dispH = drawData->DisplaySize.y;
		const float scaleX = dispW > 0.f ? static_cast<float>(width) / dispW : 1.f;
		const float scaleY = dispH > 0.f ? static_cast<float>(height) / dispH : 1.f;
		const float fbWidth = static_cast<float>(width);
		const float fbHeight = static_cast<float>(height);

		int globalVtx = 0;
		int globalIdx = 0;
		DescriptorSetHandle boundSet = mFontSet;
		for (int n = 0; n < drawData->CmdListsCount; ++n) {
			const ImDrawList* list = drawData->CmdLists[n];
			for (int cmdI = 0; cmdI < list->CmdBuffer.Size; ++cmdI) {
				const ImDrawCmd& pcmd = list->CmdBuffer[cmdI];
				if (pcmd.UserCallback) {
					if (pcmd.UserCallback == ImDrawCallback_ResetRenderState) {
						bindDrawState(cmd);
						boundSet = mFontSet;
						mRhi->cmdPushConstants(cmd, mPipeline, ShaderStage::Vertex, &proj, static_cast<uint32_t>(sizeof(proj)));
					}
					continue;
				}
				if (pcmd.ElemCount == 0) {
					continue;
				}

				ImVec2 clipMin((pcmd.ClipRect.x - clipOff.x) * scaleX, (pcmd.ClipRect.y - clipOff.y) * scaleY);
				ImVec2 clipMax((pcmd.ClipRect.z - clipOff.x) * scaleX, (pcmd.ClipRect.w - clipOff.y) * scaleY);
				if (clipMin.x < 0.f) { clipMin.x = 0.f; }
				if (clipMin.y < 0.f) { clipMin.y = 0.f; }
				if (clipMax.x > fbWidth) { clipMax.x = fbWidth; }
				if (clipMax.y > fbHeight) { clipMax.y = fbHeight; }
				if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y) {
					continue;
				}

				mRhi->cmdSetScissor(
					cmd,
					static_cast<int32_t>(clipMin.x),
					static_cast<int32_t>(clipMin.y),
					static_cast<uint32_t>(clipMax.x - clipMin.x),
					static_cast<uint32_t>(clipMax.y - clipMin.y)
				);

				const TextureHandle tex = unpackTex(pcmd.GetTexID());
				const DescriptorSetHandle set = descriptorFor(tex);
				if (set.id != boundSet.id) {
					mRhi->cmdBindDescriptorSets(cmd, mPipeline, 0, &set, 1);
					boundSet = set;
				}

				mRhi->cmdDrawIndexed(
					cmd,
					pcmd.ElemCount,
					1,
					static_cast<uint32_t>(globalIdx + static_cast<int>(pcmd.IdxOffset)),
					globalVtx + static_cast<int>(pcmd.VtxOffset),
					0
				);
			}
			globalVtx += list->VtxBuffer.Size;
			globalIdx += list->IdxBuffer.Size;
		}

		mRhi->cmdEndRenderPass(cmd);
	}
}
