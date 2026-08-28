#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include <vulkan/vulkan.h>

#include "renderModule/RenderHardwareInterface.h"

namespace SFE::Render
{
	struct VkBufferResource {
		VkBuffer buffer = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		BufferDesc desc{};
		void* mapped = nullptr;
		bool live = false;
	};

	struct VkTextureResource {
		VkImage image = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkImageView view = VK_NULL_HANDLE;
		VkFormat format = VK_FORMAT_UNDEFINED;
		VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
		TextureDesc desc{};
		bool live = false;
	};

	struct VkSamplerResource {
		VkSampler sampler = VK_NULL_HANDLE;
		SamplerDesc desc{};
		bool live = false;
	};

	struct VkDescriptorSetLayoutResource {
		VkDescriptorSetLayout layout = VK_NULL_HANDLE;
		DescriptorSetLayoutDesc desc{};
		bool live = false;
	};

	struct VkDescriptorSetResource {
		VkDescriptorSet set = VK_NULL_HANDLE;
		DescriptorSetLayoutHandle layoutHandle{};
		bool live = false;
	};

	struct VkRecordedCommandList {
		VkCommandBuffer cmd = VK_NULL_HANDLE;
		VkPipelineLayout boundLayout = VK_NULL_HANDLE;
		VkPipelineBindPoint boundBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		CommandListType type = CommandListType::Graphics;
		bool recording = false;
		bool live = false;
		bool submitted = false;
		std::vector<PipelineBarrier> barriers;
	};

	struct VkShaderModuleResource {
		VkShaderModule module = VK_NULL_HANDLE;
		ShaderModuleDesc desc{};
		bool live = false;
	};

	struct VkGraphicsPipelineResource {
		VkPipeline pipeline = VK_NULL_HANDLE;
		VkPipelineLayout layout = VK_NULL_HANDLE;
		GraphicsPipelineDesc desc{};
		ComputePipelineDesc computeDesc{};
		bool isCompute = false;
		bool live = false;
	};

	struct VkRenderPassResource {
		VkRenderPass renderPass = VK_NULL_HANDLE;
		RenderPassDesc desc{};
		bool live = false;
	};

	struct VkFramebufferResource {
		VkFramebuffer framebuffer = VK_NULL_HANDLE;
		FramebufferDesc desc{};
		bool live = false;
	};

	class VulkanRHI final : public RenderHardwareInterface
	{
	public:
		VulkanRHI();
		~VulkanRHI() override;

		GraphicsBackend backend() const override { return GraphicsBackend::Vulkan; }
		bool supportsFullPipeline() const override { return false; }
		bool supportsGpuResources() const override { return true; }
		bool supportsGeometryPass() const override { return true; }
		void setPresentBlitSource(TextureHandle texture, SamplerHandle sampler) override;

		void init() override;
		void cleanup() override;

		PipelineHandle createGraphicsPipeline(const GraphicsPipelineDesc& desc) override;
		PipelineHandle createComputePipeline(const ComputePipelineDesc& desc) override;
		void destroyGraphicsPipeline(PipelineHandle pipeline) override;
		TextureHandle createTexture(const TextureDesc& desc, const void* data = nullptr) override;
		void destroyTexture(TextureHandle texture) override;
		SamplerHandle createSampler(const SamplerDesc& desc) override;
		void destroySampler(SamplerHandle sampler) override;
		ShaderModuleHandle createShaderModule(const ShaderModuleDesc& desc) override;
		void destroyShaderModule(ShaderModuleHandle handle) override;
		FramebufferHandle createFramebuffer(const FramebufferDesc& desc) override;
		void destroyFramebuffer(FramebufferHandle framebuffer) override;
		FramebufferHandle getBackbufferFramebuffer() override;
		BufferHandle createBuffer(const BufferDesc& desc, const void* initialData = nullptr) override;
		void destroyBuffer(BufferHandle handle) override;
		void updateBuffer(BufferHandle handle, size_t offset = 0, size_t size = 0, const void* data = nullptr) override;
		void* mapBuffer(BufferHandle handle, MapMode mode) override;
		void unmapBuffer(BufferHandle handle) override;
		RenderPassHandle createRenderPass(const RenderPassDesc& desc) override;
		void destroyRenderPass(RenderPassHandle renderPass) override;
		DescriptorSetLayoutHandle createDescriptorSetLayout(const DescriptorSetLayoutDesc& desc) override;
		void destroyDescriptorSetLayout(DescriptorSetLayoutHandle layout) override;
		DescriptorSetHandle allocateDescriptorSet(DescriptorSetLayoutHandle layout) override;
		void freeDescriptorSet(DescriptorSetHandle set) override;
		void updateDescriptorSets(const std::vector<WriteDescriptor>& writes) override;
		CommandListHandle beginCommandList(CommandListType type) override;
		void endCommandList(CommandListHandle cmd) override;
		void cmdBeginRenderPass(CommandListHandle cmd, const RenderPassBeginInfo& beginInfo) override;
		void cmdEndRenderPass(CommandListHandle cmd) override;
		void cmdBindPipeline(CommandListHandle cmd, PipelineHandle pipeline) override;
		void cmdBindVertexBuffer(CommandListHandle cmd, uint32_t slot, BufferHandle handle, uint32_t offset = 0) override;
		void cmdBindIndexBuffer(CommandListHandle cmd, BufferHandle handle, IndexType type, uint32_t offset = 0) override;
		void cmdBindDescriptorSets(CommandListHandle cmd, PipelineHandle layout, uint32_t firstSet, const DescriptorSetHandle* sets, uint32_t setCount) override;
		void cmdPushConstants(CommandListHandle cmd, PipelineHandle pipeline, ShaderStage stages, const void* data, uint32_t size) override;
		void cmdDraw(CommandListHandle cmd, uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0) override;
		void cmdDrawIndexed(CommandListHandle cmd, uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t vertexOffset = 0, uint32_t firstInstance = 0) override;
		void cmdDispatch(CommandListHandle cmd, uint32_t groupCountX, uint32_t groupCountY = 1, uint32_t groupCountZ = 1) override;
		void cmdBlit(CommandListHandle cmd, FramebufferHandle src, FramebufferHandle dst, BlitMask mask = BlitMask::Color, BlitFilter filter = BlitFilter::Nearest) override;
		void cmdPipelineBarrier(CommandListHandle cmd, const PipelineBarrier& barrier) override;
		void cmdUpdateBuffer(CommandListHandle cmd, BufferHandle handle, size_t offset, size_t size, const void* data) override;
		void submit(const CommandListHandle* lists, uint32_t count) override;
		void present() override;
		void updateTexture(TextureHandle tex, uint32_t x, uint32_t y, uint32_t w, uint32_t h, TextureFormat fmt, const void* data) override;
		void updateCubeFace(TextureHandle tex, uint32_t face, uint32_t mip, const void* data) override;
		uint64_t getNativeTextureId(TextureHandle tex) const override;

	private:
		template<typename T>
		static uint32_t storeResource(std::vector<T>& pool, std::vector<uint32_t>& freeList, T value) {
			if (!freeList.empty()) {
				const uint32_t id = freeList.back();
				freeList.pop_back();
				pool[id] = std::move(value);
				return id;
			}
			pool.push_back(std::move(value));
			return static_cast<uint32_t>(pool.size() - 1);
		}

		template<typename T>
		static T* getLive(std::vector<T>& pool, uint32_t id) {
			if (id >= pool.size() || !pool[id].live) {
				return nullptr;
			}
			return &pool[id];
		}

		template<typename T>
		static const T* getLive(const std::vector<T>& pool, uint32_t id) {
			if (id >= pool.size() || !pool[id].live) {
				return nullptr;
			}
			return &pool[id];
		}

		template<typename T>
		static bool recycleResource(std::vector<T>& pool, std::vector<uint32_t>& freeList, uint32_t id) {
			if (id >= pool.size() || !pool[id].live) {
				return false;
			}
			pool[id] = T{};
			freeList.push_back(id);
			return true;
		}

		uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
		void destroyBufferInternal(VkBufferResource& buffer);
		void createImage2D(VkTextureResource& out, const TextureDesc& desc, const void* data);
		void destroyTextureInternal(VkTextureResource& texture);
		void transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
		void copyBufferToImage(VkCommandBuffer cmd, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
		void executeImmediate(const std::function<void(VkCommandBuffer)>& fn);
		void createDescriptorPool();
		void destroyDescriptorPool();
		VkBuffer createStagingBuffer(size_t size, VkDeviceMemory& memory, const void* data);

		void createInstance();
		void createSurface();
		void pickPhysicalDevice();
		void createDevice();
		void createSwapchain();
		void createImageViews();
		void createSwapchainRenderPass();
		void createFramebuffers();
		void createCommandPool();
		void createSyncObjects();
		void createSwapchainSyncObjects();
		void destroySwapchainSyncObjects();
		void createCommandBuffers();
		void createSceneCommandPool();
		void createBlitPipeline();
		void destroyBlitPipeline();
		void recordPresentBlit(VkCommandBuffer cmd, uint32_t imageIndex);
		void cleanupSwapchain();
		void recreateSwapchain();
		void recordClearCommandBuffer(uint32_t imageIndex);
		void drawFrame();

		bool m_inited = false;
		bool m_framebufferResized = false;
		uint32_t m_currentFrame = 0;

		VkInstance m_instance = VK_NULL_HANDLE;
		VkSurfaceKHR m_surface = VK_NULL_HANDLE;
		VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
		VkDevice m_device = VK_NULL_HANDLE;
		VkQueue m_graphicsQueue = VK_NULL_HANDLE;
		VkQueue m_presentQueue = VK_NULL_HANDLE;
		uint32_t m_graphicsQueueFamily = 0;
		uint32_t m_presentQueueFamily = 0;

		VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
		VkFormat m_swapchainFormat = VK_FORMAT_B8G8R8A8_SRGB;
		VkExtent2D m_swapchainExtent{};
		std::vector<VkImage> m_swapchainImages;
		std::vector<VkImageView> m_swapchainImageViews;
		std::vector<VkFramebuffer> m_swapchainFramebuffers;

		VkRenderPass m_swapchainRenderPass = VK_NULL_HANDLE;
		VkCommandPool m_commandPool = VK_NULL_HANDLE;
		VkCommandPool m_uploadCommandPool = VK_NULL_HANDLE;
		VkCommandPool m_sceneCommandPool = VK_NULL_HANDLE;
		VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
		std::vector<VkCommandBuffer> m_commandBuffers;
		VkFence m_sceneFence = VK_NULL_HANDLE;

		VkPipeline m_blitPipeline = VK_NULL_HANDLE;
		VkPipelineLayout m_blitPipelineLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout m_blitSetLayout = VK_NULL_HANDLE;
		VkDescriptorSet m_blitDescriptorSet = VK_NULL_HANDLE;
		VkSampler m_blitDefaultSampler = VK_NULL_HANDLE;
		TextureHandle m_presentBlitTexture{};
		SamplerHandle m_presentBlitSampler{};
		bool m_hasPendingScene = false;
		CommandListHandle m_pendingSceneCmd{};

		static constexpr int kMaxFramesInFlight = 2;
		std::vector<VkSemaphore> m_imageAvailableSemaphores;
		std::vector<VkSemaphore> m_renderFinishedSemaphores;
		std::vector<VkFence> m_inFlightFences;
		std::vector<VkFence> m_imagesInFlight;

		std::vector<VkBufferResource> m_buffers;
		std::vector<uint32_t> m_freeBuffers;
		std::vector<VkTextureResource> m_textures;
		std::vector<uint32_t> m_freeTextures;
		std::vector<VkSamplerResource> m_samplers;
		std::vector<uint32_t> m_freeSamplers;
		std::vector<VkDescriptorSetLayoutResource> m_descriptorSetLayouts;
		std::vector<uint32_t> m_freeDescriptorSetLayouts;
		std::vector<VkDescriptorSetResource> m_descriptorSets;
		std::vector<uint32_t> m_freeDescriptorSets;
		std::vector<VkRecordedCommandList> m_commandLists;
		std::vector<uint32_t> m_freeCommandLists;
		std::vector<VkShaderModuleResource> m_shaders;
		std::vector<uint32_t> m_freeShaders;
		std::vector<VkGraphicsPipelineResource> m_pipelines;
		std::vector<uint32_t> m_freePipelines;
		std::vector<VkRenderPassResource> m_renderPasses;
		std::vector<uint32_t> m_freeRenderPasses;
		std::vector<VkFramebufferResource> m_framebuffers;
		std::vector<uint32_t> m_freeFramebuffers;
	};
}
