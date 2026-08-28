#include "VulkanRHI.h"

#include <algorithm>
#include <cstring>

#include "VulkanRHIVkUtil.h"
#include "core/Engine.h"
#include "logsModule/logger.h"

using namespace SFE::Render::VulkanVkUtil;

namespace SFE::Render {
	uint32_t VulkanRHI::findMemoryType(const uint32_t typeFilter, const VkMemoryPropertyFlags properties) const {
		VkPhysicalDeviceMemoryProperties memProperties{};
		vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);
		for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
			if ((typeFilter & (1u << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}
		LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: failed to find suitable memory type");
		return 0;
	}

	VkBuffer VulkanRHI::createStagingBuffer(const size_t size, VkDeviceMemory& memory, const void* data) {
		VkBuffer stagingBuffer = VK_NULL_HANDLE;
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size < 16 ? 16 : size;
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: failed to create staging buffer");
		}

		VkMemoryRequirements memRequirements{};
		vkGetBufferMemoryRequirements(m_device, stagingBuffer, &memRequirements);
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		if (vkAllocateMemory(m_device, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: failed to allocate staging memory");
		}
		vkBindBufferMemory(m_device, stagingBuffer, memory, 0);

		if (data) {
			void* mapped = nullptr;
			vkMapMemory(m_device, memory, 0, bufferInfo.size, 0, &mapped);
			std::memcpy(mapped, data, size);
			vkUnmapMemory(m_device, memory);
		}
		return stagingBuffer;
	}

	void VulkanRHI::executeImmediate(const std::function<void(VkCommandBuffer)>& fn) {
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = m_uploadCommandPool;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
		vkAllocateCommandBuffers(m_device, &allocInfo, &commandBuffer);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(commandBuffer, &beginInfo);
		fn(commandBuffer);
		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;
		vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(m_graphicsQueue);

		vkFreeCommandBuffers(m_device, m_uploadCommandPool, 1, &commandBuffer);
	}

	void VulkanRHI::createDescriptorPool() {
		const std::array<VkDescriptorPoolSize, 3> poolSizes{ {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 512 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 512 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 512 },
		} };

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		poolInfo.maxSets = 512;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();

		if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: failed to create descriptor pool");
		}
	}

	void VulkanRHI::destroyDescriptorPool() {
		if (m_descriptorPool != VK_NULL_HANDLE) {
			vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
			m_descriptorPool = VK_NULL_HANDLE;
		}
	}

	void VulkanRHI::destroyBufferInternal(VkBufferResource& buffer) {
		if (buffer.mapped) {
			vkUnmapMemory(m_device, buffer.memory);
			buffer.mapped = nullptr;
		}
		if (buffer.buffer != VK_NULL_HANDLE) {
			vkDestroyBuffer(m_device, buffer.buffer, nullptr);
		}
		if (buffer.memory != VK_NULL_HANDLE) {
			vkFreeMemory(m_device, buffer.memory, nullptr);
		}
		buffer = {};
	}

	BufferHandle VulkanRHI::createBuffer(const BufferDesc& desc, const void* initialData) {
		VkBufferResource resource{};
		resource.desc = desc;
		resource.live = true;

		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = desc.size < 16 ? 16 : desc.size;
		bufferInfo.usage = toVkBufferUsage(desc.usage);
		if (desc.cpuAccess != CpuAccess::None || initialData) {
			bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		}
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &resource.buffer) != VK_SUCCESS) {
			return {};
		}

		VkMemoryRequirements memRequirements{};
		vkGetBufferMemoryRequirements(m_device, resource.buffer, &memRequirements);
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, toVkMemoryProperties(desc.cpuAccess));
		if (vkAllocateMemory(m_device, &allocInfo, nullptr, &resource.memory) != VK_SUCCESS) {
			vkDestroyBuffer(m_device, resource.buffer, nullptr);
			return {};
		}
		vkBindBufferMemory(m_device, resource.buffer, resource.memory, 0);

		if (desc.cpuAccess == CpuAccess::Write || desc.cpuAccess == CpuAccess::ReadWrite) {
			vkMapMemory(m_device, resource.memory, 0, bufferInfo.size, 0, &resource.mapped);
		}

		BufferHandle handle{};
		handle.id = storeResource(m_buffers, m_freeBuffers, std::move(resource));

		if (initialData) {
			updateBuffer(handle, 0, desc.size, initialData);
		}
		return handle;
	}

	void VulkanRHI::destroyBuffer(const BufferHandle handle) {
		if (auto* buffer = getLive(m_buffers, handle.id)) {
			destroyBufferInternal(*buffer);
			recycleResource(m_buffers, m_freeBuffers, handle.id);
		}
	}

	void VulkanRHI::updateBuffer(const BufferHandle handle, size_t offset, size_t size, const void* data) {
		auto* buffer = getLive(m_buffers, handle.id);
		if (!buffer || !data) {
			return;
		}
		if (size == 0) {
			size = buffer->desc.size;
		}

		if (buffer->mapped) {
			std::memcpy(static_cast<uint8_t*>(buffer->mapped) + offset, data, size);
			return;
		}

		VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
		const VkBuffer staging = createStagingBuffer(size, stagingMemory, data);
		executeImmediate([&](VkCommandBuffer cmd) {
			VkBufferCopy copyRegion{};
			copyRegion.srcOffset = 0;
			copyRegion.dstOffset = offset;
			copyRegion.size = size;
			vkCmdCopyBuffer(cmd, staging, buffer->buffer, 1, &copyRegion);
		});
		vkDestroyBuffer(m_device, staging, nullptr);
		vkFreeMemory(m_device, stagingMemory, nullptr);
	}

	void* VulkanRHI::mapBuffer(const BufferHandle handle, MapMode) {
		auto* buffer = getLive(m_buffers, handle.id);
		if (!buffer) {
			return nullptr;
		}
		if (buffer->mapped) {
			return buffer->mapped;
		}
		void* mapped = nullptr;
		vkMapMemory(m_device, buffer->memory, 0, VK_WHOLE_SIZE, 0, &mapped);
		buffer->mapped = mapped;
		return mapped;
	}

	void VulkanRHI::unmapBuffer(const BufferHandle handle) {
		auto* buffer = getLive(m_buffers, handle.id);
		if (!buffer || !buffer->mapped) {
			return;
		}
		vkUnmapMemory(m_device, buffer->memory);
		buffer->mapped = nullptr;
	}

	void VulkanRHI::transitionImageLayout(const VkCommandBuffer cmd, const VkImage image, const VkFormat format,
		const VkImageLayout oldLayout, const VkImageLayout newLayout)
	{
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = aspectMaskForFormat(format);
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}

		vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
	}

	void VulkanRHI::copyBufferToImage(const VkCommandBuffer cmd, const VkBuffer buffer, const VkImage image,
		const uint32_t width, const uint32_t height)
	{
		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { width, height, 1 };
		vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	}

	void VulkanRHI::createImage2D(VkTextureResource& out, const TextureDesc& desc, const void* data) {
		out.desc = desc;
		out.format = toVkFormat(desc.format);

		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent = { std::max(1u, desc.width), std::max(1u, desc.height), 1 };
		imageInfo.mipLevels = std::max(1u, desc.mipLevels);
		imageInfo.arrayLayers = 1;
		imageInfo.format = out.format;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = toVkImageUsage(desc.usage);
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateImage(m_device, &imageInfo, nullptr, &out.image) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: vkCreateImage failed");
		}

		VkMemoryRequirements memRequirements{};
		vkGetImageMemoryRequirements(m_device, out.image, &memRequirements);
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		if (vkAllocateMemory(m_device, &allocInfo, nullptr, &out.memory) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: image memory allocation failed");
		}
		vkBindImageMemory(m_device, out.image, out.memory, 0);

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = out.image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = out.format;
		viewInfo.subresourceRange.aspectMask = aspectMaskForFormat(out.format);
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = imageInfo.mipLevels;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		if (vkCreateImageView(m_device, &viewInfo, nullptr, &out.view) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: vkCreateImageView failed");
		}

		if (data) {
			const size_t pixelSize = 4;
			const size_t imageSize = static_cast<size_t>(desc.width) * desc.height * pixelSize;
			VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
			const VkBuffer staging = createStagingBuffer(imageSize, stagingMemory, data);
			executeImmediate([&](VkCommandBuffer cmd) {
				transitionImageLayout(cmd, out.image, out.format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
				copyBufferToImage(cmd, staging, out.image, desc.width, desc.height);
				transitionImageLayout(cmd, out.image, out.format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			});
			vkDestroyBuffer(m_device, staging, nullptr);
			vkFreeMemory(m_device, stagingMemory, nullptr);
		}
		else {
			const VkImageLayout targetLayout = (desc.usage & TextureUsage::DepthStencilAttachment)
				? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
				: VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			executeImmediate([&](VkCommandBuffer cmd) {
				transitionImageLayout(cmd, out.image, out.format, VK_IMAGE_LAYOUT_UNDEFINED, targetLayout);
			});
			out.layout = targetLayout;
		}
	}

	void VulkanRHI::destroyTextureInternal(VkTextureResource& texture) {
		if (texture.view != VK_NULL_HANDLE) {
			vkDestroyImageView(m_device, texture.view, nullptr);
		}
		if (texture.image != VK_NULL_HANDLE) {
			vkDestroyImage(m_device, texture.image, nullptr);
		}
		if (texture.memory != VK_NULL_HANDLE) {
			vkFreeMemory(m_device, texture.memory, nullptr);
		}
		texture = {};
	}

	TextureHandle VulkanRHI::createTexture(const TextureDesc& desc, const void* data) {
		if (desc.dimension != TextureDimension::Tex2D) {
			LogsModule::Logger::LOG_ERROR("VulkanRHI: only Tex2D supported in Phase 2");
			return {};
		}

		VkTextureResource resource{};
		resource.live = true;
		createImage2D(resource, desc, data);

		TextureHandle handle{};
		handle.id = storeResource(m_textures, m_freeTextures, std::move(resource));
		return handle;
	}

	void VulkanRHI::destroyTexture(const TextureHandle texture) {
		if (auto* tex = getLive(m_textures, texture.id)) {
			destroyTextureInternal(*tex);
			recycleResource(m_textures, m_freeTextures, texture.id);
		}
	}

	void VulkanRHI::updateTexture(const TextureHandle tex, const uint32_t x, const uint32_t y,
		const uint32_t w, const uint32_t h, const TextureFormat, const void* data)
	{
		auto* texture = getLive(m_textures, tex.id);
		if (!texture || !data) {
			return;
		}
		const size_t imageSize = static_cast<size_t>(w) * h * 4;
		VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
		const VkBuffer staging = createStagingBuffer(imageSize, stagingMemory, data);
		executeImmediate([&](VkCommandBuffer cmd) {
			transitionImageLayout(cmd, texture->image, texture->format,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			VkBufferImageCopy region{};
			region.bufferOffset = 0;
			region.imageSubresource.aspectMask = aspectMaskForFormat(texture->format);
			region.imageSubresource.mipLevel = 0;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.layerCount = 1;
			region.imageOffset = { static_cast<int32_t>(x), static_cast<int32_t>(y), 0 };
			region.imageExtent = { w, h, 1 };
			vkCmdCopyBufferToImage(cmd, staging, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
			transitionImageLayout(cmd, texture->image, texture->format,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		});
		vkDestroyBuffer(m_device, staging, nullptr);
		vkFreeMemory(m_device, stagingMemory, nullptr);
	}

	void VulkanRHI::updateCubeFace(TextureHandle, uint32_t, uint32_t, const void*) {}

	uint64_t VulkanRHI::getNativeTextureId(const TextureHandle tex) const {
		if (const auto* texture = getLive(m_textures, tex.id)) {
			return reinterpret_cast<uint64_t>(texture->view);
		}
		return 0;
	}

	SamplerHandle VulkanRHI::createSampler(const SamplerDesc& desc) {
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = toVkFilter(desc.magFilter);
		samplerInfo.minFilter = toVkFilter(desc.minFilter);
		samplerInfo.addressModeU = toVkAddressMode(desc.addressU);
		samplerInfo.addressModeV = toVkAddressMode(desc.addressV);
		samplerInfo.addressModeW = toVkAddressMode(desc.addressW);
		samplerInfo.anisotropyEnable = desc.maxAnisotropy > 1.f ? VK_TRUE : VK_FALSE;
		samplerInfo.maxAnisotropy = desc.maxAnisotropy;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = desc.compareOp != CompareOp::None ? VK_TRUE : VK_FALSE;
		samplerInfo.compareOp = toVkCompareOp(desc.compareOp);
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.minLod = 0.f;
		samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

		VkSamplerResource resource{};
		resource.desc = desc;
		if (vkCreateSampler(m_device, &samplerInfo, nullptr, &resource.sampler) != VK_SUCCESS) {
			return {};
		}
		resource.live = true;
		SamplerHandle handle{};
		handle.id = storeResource(m_samplers, m_freeSamplers, std::move(resource));
		return handle;
	}

	void VulkanRHI::destroySampler(const SamplerHandle sampler) {
		if (auto* samp = getLive(m_samplers, sampler.id)) {
			if (samp->sampler != VK_NULL_HANDLE) {
				vkDestroySampler(m_device, samp->sampler, nullptr);
			}
			recycleResource(m_samplers, m_freeSamplers, sampler.id);
		}
	}

	DescriptorSetLayoutHandle VulkanRHI::createDescriptorSetLayout(const DescriptorSetLayoutDesc& desc) {
		std::vector<VkDescriptorSetLayoutBinding> bindings;
		bindings.reserve(desc.bindings.size());
		for (const auto& binding : desc.bindings) {
			VkDescriptorSetLayoutBinding vkBinding{};
			vkBinding.binding = binding.binding;
			vkBinding.descriptorType = toVkDescriptorType(binding.type);
			vkBinding.descriptorCount = binding.count;
			vkBinding.stageFlags = toVkShaderStage(binding.stageMask);
			bindings.push_back(vkBinding);
		}

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		layoutInfo.pBindings = bindings.data();

		VkDescriptorSetLayoutResource resource{};
		resource.desc = desc;
		if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &resource.layout) != VK_SUCCESS) {
			return {};
		}
		resource.live = true;
		DescriptorSetLayoutHandle handle{};
		handle.id = storeResource(m_descriptorSetLayouts, m_freeDescriptorSetLayouts, std::move(resource));
		return handle;
	}

	void VulkanRHI::destroyDescriptorSetLayout(const DescriptorSetLayoutHandle layout) {
		if (auto* layoutObj = getLive(m_descriptorSetLayouts, layout.id)) {
			if (layoutObj->layout != VK_NULL_HANDLE) {
				vkDestroyDescriptorSetLayout(m_device, layoutObj->layout, nullptr);
			}
			recycleResource(m_descriptorSetLayouts, m_freeDescriptorSetLayouts, layout.id);
		}
	}

	DescriptorSetHandle VulkanRHI::allocateDescriptorSet(const DescriptorSetLayoutHandle layout) {
		const auto* layoutObj = getLive(m_descriptorSetLayouts, layout.id);
		if (!layoutObj) {
			return {};
		}

		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = m_descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &layoutObj->layout;

		VkDescriptorSetResource resource{};
		resource.layoutHandle = layout;
		if (vkAllocateDescriptorSets(m_device, &allocInfo, &resource.set) != VK_SUCCESS) {
			return {};
		}
		resource.live = true;
		DescriptorSetHandle handle{};
		handle.id = storeResource(m_descriptorSets, m_freeDescriptorSets, std::move(resource));
		return handle;
	}

	void VulkanRHI::freeDescriptorSet(const DescriptorSetHandle set) {
		if (auto* setObj = getLive(m_descriptorSets, set.id)) {
			if (setObj->set != VK_NULL_HANDLE) {
				vkFreeDescriptorSets(m_device, m_descriptorPool, 1, &setObj->set);
			}
			recycleResource(m_descriptorSets, m_freeDescriptorSets, set.id);
		}
	}

	void VulkanRHI::updateDescriptorSets(const std::vector<WriteDescriptor>& writes) {
		std::vector<VkWriteDescriptorSet> vkWrites;
		std::vector<VkDescriptorBufferInfo> bufferInfos;
		std::vector<VkDescriptorImageInfo> imageInfos;
		vkWrites.reserve(writes.size());
		bufferInfos.reserve(writes.size() * 2);
		imageInfos.reserve(writes.size());

		for (const auto& write : writes) {
			const auto* setObj = getLive(m_descriptorSets, write.set.id);
			if (!setObj) {
				continue;
			}

			VkWriteDescriptorSet vkWrite{};
			vkWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			vkWrite.dstSet = setObj->set;
			vkWrite.dstBinding = write.binding;
			vkWrite.descriptorType = toVkDescriptorType(write.type);
			vkWrite.descriptorCount = 1;

			if (write.type == DescriptorType::CombinedImageSampler) {
				if (write.images.empty()) {
					continue;
				}
				const auto& image = write.images.front();
				const auto* tex = getLive(m_textures, image.texture.id);
				const auto* samp = getLive(m_samplers, image.sampler.id);
				if (!tex) {
					continue;
				}
				VkDescriptorImageInfo info{};
				info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				info.imageView = tex->view;
				info.sampler = samp ? samp->sampler : VK_NULL_HANDLE;
				imageInfos.push_back(info);
				vkWrite.pImageInfo = &imageInfos.back();
			}
			else {
				if (write.buffers.empty()) {
					continue;
				}
				const auto& buf = write.buffers.front();
				const auto* buffer = getLive(m_buffers, buf.buffer.id);
				if (!buffer) {
					continue;
				}
				VkDescriptorBufferInfo info{};
				info.buffer = buffer->buffer;
				info.offset = buf.offset;
				info.range = buf.range > 0 ? buf.range : buffer->desc.size;
				bufferInfos.push_back(info);
				vkWrite.pBufferInfo = &bufferInfos.back();
			}
			vkWrites.push_back(vkWrite);
		}

		if (!vkWrites.empty()) {
			vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(vkWrites.size()), vkWrites.data(), 0, nullptr);
		}
	}

	CommandListHandle VulkanRHI::beginCommandList(const CommandListType type) {
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = m_sceneCommandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;

		VkRecordedCommandList list{};
		list.type = type;
		list.recording = true;
		list.live = true;
		if (vkAllocateCommandBuffers(m_device, &allocInfo, &list.cmd) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: failed to allocate command buffer");
			return {};
		}

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		if (vkBeginCommandBuffer(list.cmd, &beginInfo) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: vkBeginCommandBuffer failed");
		}

		CommandListHandle handle{};
		handle.id = storeResource(m_commandLists, m_freeCommandLists, std::move(list));
		return handle;
	}

	void VulkanRHI::endCommandList(const CommandListHandle cmd) {
		if (auto* list = getLive(m_commandLists, cmd.id)) {
			if (list->recording && list->cmd != VK_NULL_HANDLE) {
				for (const auto& barrier : list->barriers) {
					VkMemoryBarrier memoryBarrier{};
					memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
					memoryBarrier.srcAccessMask = toVkAccess(barrier.srcAccess);
					memoryBarrier.dstAccessMask = toVkAccess(barrier.dstAccess);
					vkCmdPipelineBarrier(list->cmd,
						toVkPipelineStage(barrier.srcStage),
						toVkPipelineStage(barrier.dstStage),
						0,
						1, &memoryBarrier,
						0, nullptr,
						0, nullptr);
				}
				list->barriers.clear();
				vkEndCommandBuffer(list->cmd);
			}
			list->recording = false;
		}
	}

	void VulkanRHI::cmdPipelineBarrier(const CommandListHandle cmd, const PipelineBarrier& barrier) {
		auto* list = getLive(m_commandLists, cmd.id);
		if (!list || !list->recording || list->cmd == VK_NULL_HANDLE) {
			return;
		}

		VkMemoryBarrier memoryBarrier{};
		memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		memoryBarrier.srcAccessMask = toVkAccess(barrier.srcAccess);
		memoryBarrier.dstAccessMask = toVkAccess(barrier.dstAccess);
		vkCmdPipelineBarrier(list->cmd,
			toVkPipelineStage(barrier.srcStage),
			toVkPipelineStage(barrier.dstStage),
			0,
			1, &memoryBarrier,
			0, nullptr,
			0, nullptr);
	}

	void VulkanRHI::submit(const CommandListHandle* lists, const uint32_t count) {
		if (!lists || count == 0) {
			return;
		}
		auto* list = getLive(m_commandLists, lists[0].id);
		if (!list || list->cmd == VK_NULL_HANDLE) {
			return;
		}

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &list->cmd;
		if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_sceneFence) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: scene submit failed");
		}

		m_hasPendingScene = true;
		m_pendingSceneCmd = lists[0];
		list->submitted = true;
	}

	void VulkanRHI::cmdUpdateBuffer(const CommandListHandle cmd, const BufferHandle handle,
		const size_t offset, const size_t size, const void* data)
	{
		auto* list = getLive(m_commandLists, cmd.id);
		const auto* buffer = getLive(m_buffers, handle.id);
		if (!list || !buffer || !data) {
			return;
		}
		if (size <= 65536) {
			vkCmdUpdateBuffer(list->cmd, buffer->buffer, offset, size, data);
		}
		else {
			updateBuffer(handle, offset, size, data);
		}
	}
}
