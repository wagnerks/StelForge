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
		auto match = [&](const VkMemoryPropertyFlags required) -> uint32_t {
			for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
				if ((typeFilter & (1u << i)) && (memProperties.memoryTypes[i].propertyFlags & required) == required) {
					return i;
				}
			}
			return UINT32_MAX;
		};
		// Prefer ReBAR (DEVICE_LOCAL | HOST_VISIBLE) so GPU-read SSBOs/UBOs stay in VRAM.
		if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
			if (const uint32_t id = match(properties | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT); id != UINT32_MAX) {
				return id;
			}
		}
		if (const uint32_t id = match(properties); id != UINT32_MAX) {
			return id;
		}
		LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: failed to find suitable memory type");
		return 0;
	}

	VkBuffer VulkanRHI::createStagingBuffer(const size_t size, VmaAllocation& allocation, const void* data) {
		allocation = VK_NULL_HANDLE;
		VkBuffer stagingBuffer = VK_NULL_HANDLE;
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size < 16 ? 16 : size;
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		VmaAllocationCreateInfo allocInfo{};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		VmaAllocationInfo mapped{};
		if (vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo, &stagingBuffer, &allocation, &mapped) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: failed to create staging buffer");
		}
		if (data && mapped.pMappedData) {
			std::memcpy(mapped.pMappedData, data, size);
			vmaFlushAllocation(m_allocator, allocation, 0, size);
		}
		return stagingBuffer;
	}

	void VulkanRHI::destroyStagingBuffer(VkBuffer buffer, VmaAllocation allocation) {
		if (buffer != VK_NULL_HANDLE) {
			vmaDestroyBuffer(m_allocator, buffer, allocation);
		}
	}

	void VulkanRHI::createUploadRings() {
		for (int i = 0; i < kMaxFramesInFlight; ++i) {
			VkBufferCreateInfo bufferInfo{};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = kUploadRingSize;
			bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			VmaAllocationCreateInfo allocInfo{};
			allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
			allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
			VmaAllocationInfo mapped{};
			if (vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo, &m_uploadRing[i], &m_uploadRingAlloc[i], &mapped) != VK_SUCCESS) {
				LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: failed to create upload ring");
			}
			m_uploadRingMapped[i] = mapped.pMappedData;
		}
	}

	void VulkanRHI::destroyUploadRings() {
		for (int i = 0; i < kMaxFramesInFlight; ++i) {
			if (m_uploadRing[i] != VK_NULL_HANDLE) {
				vmaDestroyBuffer(m_allocator, m_uploadRing[i], m_uploadRingAlloc[i]);
				m_uploadRing[i] = VK_NULL_HANDLE;
				m_uploadRingAlloc[i] = VK_NULL_HANDLE;
				m_uploadRingMapped[i] = nullptr;
			}
		}
		m_uploadOps.clear();
		m_uploadRingUsed = 0;
	}

	void VulkanRHI::waitForInFlightFrame() {
		if (m_frameWaited) {
			return;
		}
		if (m_currentFrame < m_inFlightFences.size() && m_inFlightFences[m_currentFrame] != VK_NULL_HANDLE) {
			vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
		}
		recycleSceneList(m_inflightSceneCmd[m_currentFrame]);
		m_inflightSceneCmd[m_currentFrame] = {};
		collectGpuTimestamp();
		if (m_timestampPool != VK_NULL_HANDLE) {
			vkResetQueryPool(m_device, m_timestampPool, m_currentFrame * kTimestampsPerFrame, kTimestampsPerFrame);
		}
		m_uploadRingUsed = 0;
		m_uploadOps.clear();
		m_uploadsFlushed = false;
		m_frameWaited = true;
	}

	void VulkanRHI::queueBufferUpload(const uint32_t bufferId, size_t offset, size_t size, const void* data) {
		auto* buffer = getLive(m_buffers, bufferId);
		if (!buffer || !buffer->buffer || !data || size == 0) {
			return;
		}
		waitForInFlightFrame();
		buffer = getLive(m_buffers, bufferId);
		if (!buffer || !buffer->buffer) {
			return;
		}
		if (m_uploadsFlushed) {
			VmaAllocation stagingMemory = VK_NULL_HANDLE;
			const VkBuffer staging = createStagingBuffer(size, stagingMemory, data);
			const VkBuffer dst = buffer->buffer;
			executeImmediate([&](VkCommandBuffer cmd) {
				VkBufferCopy copyRegion{};
				copyRegion.srcOffset = 0;
				copyRegion.dstOffset = offset;
				copyRegion.size = size;
				vkCmdCopyBuffer(cmd, staging, dst, 1, &copyRegion);
			});
			destroyStagingBuffer(staging, stagingMemory);
			return;
		}
		const VkDeviceSize aligned = (static_cast<VkDeviceSize>(size) + 3u) & ~VkDeviceSize{ 3 };
		if (!m_uploadRingMapped[m_currentFrame] || m_uploadRingUsed + aligned > kUploadRingSize) {
			VmaAllocation stagingMemory = VK_NULL_HANDLE;
			const VkBuffer staging = createStagingBuffer(size, stagingMemory, data);
			const VkBuffer dst = buffer->buffer;
			executeImmediate([&](VkCommandBuffer cmd) {
				VkBufferCopy copyRegion{};
				copyRegion.srcOffset = 0;
				copyRegion.dstOffset = offset;
				copyRegion.size = size;
				vkCmdCopyBuffer(cmd, staging, dst, 1, &copyRegion);
			});
			destroyStagingBuffer(staging, stagingMemory);
			return;
		}
		std::memcpy(static_cast<uint8_t*>(m_uploadRingMapped[m_currentFrame]) + m_uploadRingUsed, data, size);
		m_uploadOps.push_back({
			bufferId,
			static_cast<VkDeviceSize>(offset),
			m_uploadRingUsed,
			static_cast<VkDeviceSize>(size)
		});
		m_uploadRingUsed += aligned;
	}

	void VulkanRHI::flushBufferUploads(const VkCommandBuffer cmd) {
		if (cmd == VK_NULL_HANDLE) {
			return;
		}
		if (m_uploadOps.empty()) {
			m_uploadsFlushed = true;
			return;
		}
		if (m_uploadRingAlloc[m_currentFrame] != VK_NULL_HANDLE && m_uploadRingUsed > 0) {
			vmaFlushAllocation(m_allocator, m_uploadRingAlloc[m_currentFrame], 0, m_uploadRingUsed);
		}
		for (const auto& op : m_uploadOps) {
			const auto* dst = getLive(m_buffers, op.dstId);
			if (!dst || !dst->buffer || op.size == 0) {
				continue;
			}
			VkBufferCopy copyRegion{};
			copyRegion.srcOffset = op.srcOffset;
			copyRegion.dstOffset = op.dstOffset;
			copyRegion.size = op.size;
			vkCmdCopyBuffer(cmd, m_uploadRing[m_currentFrame], dst->buffer, 1, &copyRegion);
		}
		VkMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
			| VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			0, 1, &barrier, 0, nullptr, 0, nullptr);
		m_uploadOps.clear();
		m_uploadsFlushed = true;
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
		if (m_uploadFence != VK_NULL_HANDLE) {
			vkResetFences(m_device, 1, &m_uploadFence);
			{
				std::lock_guard lock(m_queueMutex);
				const VkResult submitResult = vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_uploadFence);
				if (submitResult != VK_SUCCESS) {
					LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: upload submit failed (%d)", static_cast<int>(submitResult));
				}
			}
			vkWaitForFences(m_device, 1, &m_uploadFence, VK_TRUE, UINT64_MAX);
		}
		else {
			{
				std::lock_guard lock(m_queueMutex);
				vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
			}
			vkQueueWaitIdle(m_graphicsQueue);
		}

		vkFreeCommandBuffers(m_device, m_uploadCommandPool, 1, &commandBuffer);
	}

	void VulkanRHI::createDescriptorPool() {
		const std::array<VkDescriptorPoolSize, 3> poolSizes{ {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2048 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2048 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2048 },
		} };

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		poolInfo.maxSets = 2048;
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
		if (buffer.buffer != VK_NULL_HANDLE) {
			const uint32_t id = static_cast<uint32_t>(&buffer - m_buffers.data());
			if (id < m_buffers.size() && &m_buffers[id] == &buffer) {
				m_uploadOps.erase(
					std::remove_if(m_uploadOps.begin(), m_uploadOps.end(),
						[id](const PendingBufferUpload& op) { return op.dstId == id; }),
					m_uploadOps.end());
			}
			vmaDestroyBuffer(m_allocator, buffer.buffer, buffer.allocation);
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
		const bool gpuStorage = desc.usage == BufferUsage::Storage && desc.cpuAccess != CpuAccess::None
			&& desc.size >= (1u << 20);
		if (desc.cpuAccess != CpuAccess::None || initialData || gpuStorage) {
			bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		}
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo allocInfo{};
		const bool hostMapped = !gpuStorage && (desc.cpuAccess == CpuAccess::Write || desc.cpuAccess == CpuAccess::ReadWrite);
		if (gpuStorage) {
			allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
			resource.deviceLocal = true;
		}
		else if (hostMapped) {
			allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
			allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
		}
		else {
			allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
			resource.deviceLocal = true;
		}

		VmaAllocationInfo mapped{};
		VkResult allocResult = vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo, &resource.buffer, &resource.allocation, &mapped);
		if (allocResult != VK_SUCCESS && hostMapped) {
			allocInfo = {};
			allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
			resource.deviceLocal = true;
			allocResult = vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo, &resource.buffer, &resource.allocation, &mapped);
		}
		if (allocResult != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: vmaCreateBuffer failed (%d)", static_cast<int>(allocResult));
			return {};
		}
		if (hostMapped && mapped.pMappedData) {
			resource.mapped = mapped.pMappedData;
			resource.persistentMapped = true;
			resource.deviceLocal = false;
		}

		BufferHandle handle{};
		handle.id = storeResource(m_buffers, m_freeBuffers, std::move(resource));

		if (initialData) {
			auto* stored = getLive(m_buffers, handle.id);
			if (stored && stored->mapped) {
				std::memcpy(stored->mapped, initialData, desc.size);
				if (stored->allocation != VK_NULL_HANDLE) {
					vmaFlushAllocation(m_allocator, stored->allocation, 0, desc.size);
				}
			}
			else if (stored && stored->buffer) {
				VmaAllocation stagingMemory = VK_NULL_HANDLE;
				const VkBuffer staging = createStagingBuffer(desc.size, stagingMemory, initialData);
				const VkBuffer dst = stored->buffer;
				executeImmediate([&](VkCommandBuffer cmd) {
					VkBufferCopy copyRegion{};
					copyRegion.srcOffset = 0;
					copyRegion.dstOffset = 0;
					copyRegion.size = desc.size;
					vkCmdCopyBuffer(cmd, staging, dst, 1, &copyRegion);
				});
				destroyStagingBuffer(staging, stagingMemory);
			}
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
			if (offset + size > buffer->desc.size) {
				LogsModule::Logger::LOG_ERROR("VulkanRHI: updateBuffer OOB offset=%zu size=%zu cap=%zu",
					offset, size, static_cast<size_t>(buffer->desc.size));
				return;
			}
			std::memcpy(static_cast<uint8_t*>(buffer->mapped) + offset, data, size);
			if (buffer->allocation != VK_NULL_HANDLE) {
				vmaFlushAllocation(m_allocator, buffer->allocation, offset, size);
			}
			return;
		}
		if (buffer->deviceLocal) {
			queueBufferUpload(handle.id, offset, size, data);
			return;
		}

		VmaAllocation stagingMemory = VK_NULL_HANDLE;
		const VkBuffer staging = createStagingBuffer(size, stagingMemory, data);
		executeImmediate([&](VkCommandBuffer cmd) {
			VkBufferCopy copyRegion{};
			copyRegion.srcOffset = 0;
			copyRegion.dstOffset = offset;
			copyRegion.size = size;
			vkCmdCopyBuffer(cmd, staging, buffer->buffer, 1, &copyRegion);
		});
		destroyStagingBuffer(staging, stagingMemory);
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
		vmaMapMemory(m_allocator, buffer->allocation, &mapped);
		buffer->mapped = mapped;
		return mapped;
	}

	void VulkanRHI::unmapBuffer(const BufferHandle handle) {
		auto* buffer = getLive(m_buffers, handle.id);
		if (!buffer || !buffer->mapped) {
			return;
		}
		if (buffer->allocation != VK_NULL_HANDLE) {
			vmaFlushAllocation(m_allocator, buffer->allocation, 0, VK_WHOLE_SIZE);
		}
		if (buffer->persistentMapped) {
			return;
		}
		vmaUnmapMemory(m_allocator, buffer->allocation);
		buffer->mapped = nullptr;
	}

	void VulkanRHI::transitionImageLayout(const VkCommandBuffer cmd, const VkImage image, const VkFormat format,
		const VkImageLayout oldLayout, const VkImageLayout newLayout, const uint32_t layerCount, const uint32_t mipLevels)
	{
		if (oldLayout == newLayout) {
			return;
		}

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = image;
		barrier.subresourceRange.aspectMask = aspectMaskForFormat(format);
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = (std::max)(1u, mipLevels);
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = (std::max)(1u, layerCount);

		auto accessForLayout = [](VkImageLayout layout) -> VkAccessFlags {
			switch (layout) {
			case VK_IMAGE_LAYOUT_UNDEFINED:
			case VK_IMAGE_LAYOUT_PREINITIALIZED:
				return 0;
			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				return VK_ACCESS_TRANSFER_WRITE_BIT;
			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				return VK_ACCESS_TRANSFER_READ_BIT;
			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				return VK_ACCESS_SHADER_READ_BIT;
			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
				return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			case VK_IMAGE_LAYOUT_GENERAL:
				return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
			case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
				return 0;
			default:
				return VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
			}
		};

		auto stagesForLayout = [](VkImageLayout layout, bool) -> VkPipelineStageFlags {
			switch (layout) {
			case VK_IMAGE_LAYOUT_UNDEFINED:
			case VK_IMAGE_LAYOUT_PREINITIALIZED:
				return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				return VK_PIPELINE_STAGE_TRANSFER_BIT;
			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
				return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			case VK_IMAGE_LAYOUT_GENERAL:
				return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;
			case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
				return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			default:
				return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			}
		};

		barrier.srcAccessMask = accessForLayout(oldLayout);
		barrier.dstAccessMask = accessForLayout(newLayout);
		vkCmdPipelineBarrier(cmd, stagesForLayout(oldLayout, true), stagesForLayout(newLayout, false),
			0, 0, nullptr, 0, nullptr, 1, &barrier);
	}

	void VulkanRHI::ensureTextureLayout(VkCommandBuffer cmd, VkTextureResource& texture, const VkImageLayout newLayout) {
		if (!texture.image || texture.layout == newLayout) {
			return;
		}
		const uint32_t layers = (texture.desc.dimension == TextureDimension::Cube) ? 6u
			: (texture.desc.dimension == TextureDimension::Array)
			? static_cast<uint32_t>((std::max)(1u, static_cast<uint32_t>(texture.desc.depth)))
			: 1u;
		const uint32_t mips = (std::max)(1u, texture.desc.mipLevels);
		transitionImageLayout(cmd, texture.image, texture.format, texture.layout, newLayout, layers, mips);
		texture.layout = newLayout;
	}

	void VulkanRHI::generateMipmaps(VkCommandBuffer cmd, VkTextureResource& texture) {
		const uint32_t mipLevels = (std::max)(1u, texture.desc.mipLevels);
		if (mipLevels <= 1 || !texture.image) {
			return;
		}
		const uint32_t layers = (texture.desc.dimension == TextureDimension::Cube) ? 6u
			: (texture.desc.dimension == TextureDimension::Array)
			? static_cast<uint32_t>((std::max)(1u, static_cast<uint32_t>(texture.desc.depth)))
			: 1u;

		ensureTextureLayout(cmd, texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		int32_t mipWidth = static_cast<int32_t>(texture.desc.width);
		int32_t mipHeight = static_cast<int32_t>(texture.desc.height);
		for (uint32_t i = 1; i < mipLevels; ++i) {
			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.image = texture.image;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.subresourceRange.aspectMask = aspectMaskForFormat(texture.format);
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = layers;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &barrier);

			VkImageBlit blit{};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
			blit.srcSubresource.aspectMask = aspectMaskForFormat(texture.format);
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = layers;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
			blit.dstSubresource.aspectMask = aspectMaskForFormat(texture.format);
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = layers;
			vkCmdBlitImage(cmd, texture.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				0, 0, nullptr, 0, nullptr, 1, &barrier);

			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		}

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = texture.image;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = aspectMaskForFormat(texture.format);
		barrier.subresourceRange.baseMipLevel = mipLevels - 1;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = layers;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &barrier);
		texture.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	void VulkanRHI::copyBufferToImage(const VkCommandBuffer cmd, const VkBuffer buffer, const VkImage image,
		const uint32_t width, const uint32_t height, const uint32_t layer, const uint32_t mip)
	{
		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = mip;
		region.imageSubresource.baseArrayLayer = layer;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { width, height, 1 };
		vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	}

	void VulkanRHI::createImage(VkTextureResource& out, const TextureDesc& desc, const void* data) {
		out.desc = desc;
		out.format = toVkFormat(desc.format);

		const uint32_t mipLevels = desc.sampleCount > 1 ? 1u : (std::max)(1u, desc.mipLevels);
		uint32_t arrayLayers = 1;
		VkImageCreateFlags flags = 0;
		VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
		if (desc.dimension == TextureDimension::Cube) {
			arrayLayers = 6;
			flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
			viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		}
		else if (desc.dimension == TextureDimension::Array) {
			arrayLayers = (std::max)(1u, static_cast<uint32_t>(desc.depth));
			viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		}

		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.flags = flags;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent = { (std::max)(1u, desc.width), (std::max)(1u, desc.height), 1 };
		imageInfo.mipLevels = mipLevels;
		imageInfo.arrayLayers = arrayLayers;
		imageInfo.format = out.format;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = toVkImageUsage(desc.usage);
		imageInfo.samples = toVkSampleCount(desc.sampleCount);
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo allocInfo{};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
		if (vmaCreateImage(m_allocator, &imageInfo, &allocInfo, &out.image, &out.allocation, nullptr) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: vmaCreateImage failed");
		}

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = out.image;
		viewInfo.viewType = viewType;
		viewInfo.format = out.format;
		viewInfo.subresourceRange.aspectMask = aspectMaskForFormat(out.format);
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = mipLevels;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = arrayLayers;
		if (vkCreateImageView(m_device, &viewInfo, nullptr, &out.view) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: vkCreateImageView failed");
		}

		out.layout = VK_IMAGE_LAYOUT_UNDEFINED;
		if (data && desc.dimension == TextureDimension::Tex2D && desc.sampleCount <= 1) {
			const size_t pixelSize = bytesPerPixel(desc.format);
			const size_t imageSize = static_cast<size_t>(desc.width) * desc.height * pixelSize;
			VmaAllocation stagingMemory = VK_NULL_HANDLE;
			const VkBuffer staging = createStagingBuffer(imageSize, stagingMemory, data);
			executeImmediate([&](VkCommandBuffer cmd) {
				transitionImageLayout(cmd, out.image, out.format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, arrayLayers, mipLevels);
				out.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				copyBufferToImage(cmd, staging, out.image, desc.width, desc.height);
				if (mipLevels > 1) {
					generateMipmaps(cmd, out);
				}
				else {
					transitionImageLayout(cmd, out.image, out.format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, arrayLayers, mipLevels);
					out.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				}
			});
			destroyStagingBuffer(staging, stagingMemory);
		}
		else {
			const VkImageLayout targetLayout = (desc.usage & TextureUsage::DepthStencilAttachment)
				? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
				: ((desc.usage & TextureUsage::ColorAttachment)
					? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
					: VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			executeImmediate([&](VkCommandBuffer cmd) {
				transitionImageLayout(cmd, out.image, out.format, VK_IMAGE_LAYOUT_UNDEFINED, targetLayout, arrayLayers, mipLevels);
			});
			out.layout = targetLayout;
		}
	}

	void VulkanRHI::destroyTextureInternal(VkTextureResource& texture) {
		if (texture.view != VK_NULL_HANDLE) {
			vkDestroyImageView(m_device, texture.view, nullptr);
		}
		if (texture.image != VK_NULL_HANDLE) {
			vmaDestroyImage(m_allocator, texture.image, texture.allocation);
		}
		texture = {};
	}

	TextureHandle VulkanRHI::createTexture(const TextureDesc& desc, const void* data) {
		if (desc.dimension == TextureDimension::Tex3D) {
			LogsModule::Logger::LOG_ERROR("VulkanRHI: Tex3D textures not supported yet");
			return {};
		}

		TextureDesc fixed = desc;
		if (data || (fixed.usage & TextureUsage::Sampled)) {
			fixed.usage = fixed.usage | TextureUsage::TransferDst;
		}
		if (fixed.dimension == TextureDimension::Cube) {
			// Cubes are filled via updateCubeFace; always allow transfer + sampling.
			fixed.usage = fixed.usage | TextureUsage::TransferDst | TextureUsage::Sampled;
		}
		if (fixed.mipLevels > 1) {
			fixed.usage = fixed.usage | TextureUsage::TransferSrc | TextureUsage::TransferDst;
		}
		// Blit / layout transitions between passes need transfer usage on RT textures.
		if ((fixed.usage & TextureUsage::ColorAttachment) || (fixed.usage & TextureUsage::DepthStencilAttachment)) {
			fixed.usage = fixed.usage | TextureUsage::TransferSrc | TextureUsage::TransferDst;
		}
		if (!(fixed.usage & TextureUsage::Sampled) && !(fixed.usage & TextureUsage::ColorAttachment)
			&& !(fixed.usage & TextureUsage::DepthStencilAttachment)) {
			fixed.usage = fixed.usage | TextureUsage::Sampled;
		}

		VkTextureResource resource{};
		resource.live = true;
		createImage(resource, fixed, data);

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
		if (!texture || !data || w == 0 || h == 0) {
			return;
		}
		const size_t imageSize = static_cast<size_t>(w) * h * bytesPerPixel(texture->desc.format);
		VmaAllocation stagingMemory = VK_NULL_HANDLE;
		const VkBuffer staging = createStagingBuffer(imageSize, stagingMemory, data);
		executeImmediate([&](VkCommandBuffer cmd) {
			ensureTextureLayout(cmd, *texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			VkBufferImageCopy region{};
			region.bufferOffset = 0;
			region.imageSubresource.aspectMask = aspectMaskForFormat(texture->format);
			region.imageSubresource.mipLevel = 0;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.layerCount = 1;
			region.imageOffset = { static_cast<int32_t>(x), static_cast<int32_t>(y), 0 };
			region.imageExtent = { w, h, 1 };
			vkCmdCopyBufferToImage(cmd, staging, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
			if (texture->desc.mipLevels > 1) {
				generateMipmaps(cmd, *texture);
			}
			else {
				ensureTextureLayout(cmd, *texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			}
		});
		destroyStagingBuffer(staging, stagingMemory);
	}

	void VulkanRHI::updateCubeFace(const TextureHandle tex, const uint32_t face, const uint32_t mip, const void* data) {
		auto* texture = getLive(m_textures, tex.id);
		if (!texture || !data || texture->desc.dimension != TextureDimension::Cube || face > 5) {
			return;
		}
		uint32_t w = texture->desc.width >> mip;
		uint32_t h = texture->desc.height >> mip;
		if (w < 1) w = 1;
		if (h < 1) h = 1;

		const size_t imageSize = static_cast<size_t>(w) * h * bytesPerPixel(texture->desc.format);
		VmaAllocation stagingMemory = VK_NULL_HANDLE;
		const VkBuffer staging = createStagingBuffer(imageSize, stagingMemory, data);
		executeImmediate([&](VkCommandBuffer cmd) {
			ensureTextureLayout(cmd, *texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			copyBufferToImage(cmd, staging, texture->image, w, h, face, mip);
			ensureTextureLayout(cmd, *texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		});
		destroyStagingBuffer(staging, stagingMemory);
	}

	void VulkanRHI::updateCubeMip(const TextureHandle tex, const uint32_t mip, const void* packed6Faces, const size_t faceBytes) {
		auto* texture = getLive(m_textures, tex.id);
		if (!texture || !packed6Faces || faceBytes == 0 || texture->desc.dimension != TextureDimension::Cube) {
			return;
		}
		uint32_t w = texture->desc.width >> mip;
		uint32_t h = texture->desc.height >> mip;
		if (w < 1) w = 1;
		if (h < 1) h = 1;

		const size_t total = faceBytes * 6;
		VmaAllocation stagingMemory = VK_NULL_HANDLE;
		const VkBuffer staging = createStagingBuffer(total, stagingMemory, packed6Faces);
		executeImmediate([&](VkCommandBuffer cmd) {
			ensureTextureLayout(cmd, *texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			for (uint32_t face = 0; face < 6; ++face) {
				VkBufferImageCopy region{};
				region.bufferOffset = static_cast<VkDeviceSize>(face) * faceBytes;
				region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				region.imageSubresource.mipLevel = mip;
				region.imageSubresource.baseArrayLayer = face;
				region.imageSubresource.layerCount = 1;
				region.imageExtent = { w, h, 1 };
				vkCmdCopyBufferToImage(cmd, staging, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
			}
			ensureTextureLayout(cmd, *texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		});
		destroyStagingBuffer(staging, stagingMemory);
	}

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
		const bool wantMips = desc.minFilter == FilterMode::LinearMipmapLinear;
		samplerInfo.mipmapMode = wantMips ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
		samplerInfo.minLod = 0.f;
		samplerInfo.maxLod = wantMips ? VK_LOD_CLAMP_NONE : 0.f;
		samplerInfo.addressModeU = toVkAddressMode(desc.addressU);
		samplerInfo.addressModeV = toVkAddressMode(desc.addressV);
		samplerInfo.addressModeW = toVkAddressMode(desc.addressW);
		const bool aniso = m_samplerAnisotropy && desc.maxAnisotropy > 1.f;
		samplerInfo.anisotropyEnable = aniso ? VK_TRUE : VK_FALSE;
		samplerInfo.maxAnisotropy = aniso ? desc.maxAnisotropy : 1.f;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = desc.compareOp != CompareOp::None ? VK_TRUE : VK_FALSE;
		samplerInfo.compareOp = toVkCompareOp(desc.compareOp);

		VkSamplerResource resource{};
		resource.desc = desc;
		if (vkCreateSampler(m_device, &samplerInfo, nullptr, &resource.sampler) != VK_SUCCESS) {
			LogsModule::Logger::LOG_ERROR("VulkanRHI: vkCreateSampler failed");
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
		auto makeVkLayout = [&](const std::vector<DescriptorSetLayoutBinding>& src, VkDescriptorSetLayout& out) -> bool {
			std::vector<VkDescriptorSetLayoutBinding> bindings;
			bindings.reserve(src.size());
			for (const auto& binding : src) {
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
			layoutInfo.pBindings = bindings.empty() ? nullptr : bindings.data();
			return vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &out) == VK_SUCCESS;
		};

		VkDescriptorSetLayoutResource resource{};
		resource.desc = desc;
		resource.split = descriptorLayoutNeedsSplit(desc);

		if (resource.split) {
			std::vector<DescriptorSetLayoutBinding> images;
			std::vector<DescriptorSetLayoutBinding> buffers;
			images.reserve(desc.bindings.size());
			buffers.reserve(desc.bindings.size());
			for (const auto& binding : desc.bindings) {
				if (isImageDescriptor(binding.type)) {
					images.push_back(binding);
				}
				else {
					buffers.push_back(binding);
				}
			}
			if (!makeVkLayout(images, resource.layout) || !makeVkLayout(buffers, resource.bufferLayout)) {
				if (resource.layout != VK_NULL_HANDLE) {
					vkDestroyDescriptorSetLayout(m_device, resource.layout, nullptr);
				}
				if (resource.bufferLayout != VK_NULL_HANDLE) {
					vkDestroyDescriptorSetLayout(m_device, resource.bufferLayout, nullptr);
				}
				return {};
			}
		}
		else if (!makeVkLayout(desc.bindings, resource.layout)) {
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
			if (layoutObj->bufferLayout != VK_NULL_HANDLE) {
				vkDestroyDescriptorSetLayout(m_device, layoutObj->bufferLayout, nullptr);
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

		VkDescriptorSetLayout layouts[2] = { layoutObj->layout, layoutObj->bufferLayout };
		const uint32_t setCount = layoutObj->split ? 2u : 1u;
		allocInfo.descriptorSetCount = setCount;
		allocInfo.pSetLayouts = layouts;

		VkDescriptorSet allocated[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
		VkDescriptorSetResource resource{};
		resource.layoutHandle = layout;
		if (vkAllocateDescriptorSets(m_device, &allocInfo, allocated) != VK_SUCCESS) {
			return {};
		}
		resource.set = allocated[0];
		resource.bufferSet = layoutObj->split ? allocated[1] : VK_NULL_HANDLE;
		resource.live = true;
		DescriptorSetHandle handle{};
		handle.id = storeResource(m_descriptorSets, m_freeDescriptorSets, std::move(resource));
		return handle;
	}

	void VulkanRHI::freeDescriptorSet(const DescriptorSetHandle set) {
		if (auto* setObj = getLive(m_descriptorSets, set.id)) {
			VkDescriptorSet toFree[2] = { setObj->set, setObj->bufferSet };
			uint32_t count = 0;
			if (toFree[0] != VK_NULL_HANDLE) {
				++count;
			}
			if (toFree[1] != VK_NULL_HANDLE) {
				if (count == 0) {
					toFree[0] = toFree[1];
				}
				++count;
			}
			if (count > 0) {
				vkFreeDescriptorSets(m_device, m_descriptorPool, count, toFree);
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

			if (const auto* layoutObj = getLive(m_descriptorSetLayouts, setObj->layoutHandle.id)) {
				if (layoutObj->split && !isImageDescriptor(write.type) && setObj->bufferSet != VK_NULL_HANDLE) {
					vkWrite.dstSet = setObj->bufferSet;
				}
			}

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
		prepareInlinePresent();

		VkRecordedCommandList list{};
		list.type = type;
		list.recording = true;
		list.live = true;
		if (m_sceneCmds[m_currentFrame] != VK_NULL_HANDLE) {
			list.cmd = m_sceneCmds[m_currentFrame];
			vkResetCommandBuffer(list.cmd, 0);
		}
		else {
			VkCommandBufferAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.commandPool = m_sceneCommandPool;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandBufferCount = 1;
			if (vkAllocateCommandBuffers(m_device, &allocInfo, &list.cmd) != VK_SUCCESS) {
				LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: failed to allocate command buffer");
				return {};
			}
		}

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		if (vkBeginCommandBuffer(list.cmd, &beginInfo) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: vkBeginCommandBuffer failed");
		}
		flushBufferUploads(list.cmd);
		if (m_timestampPool != VK_NULL_HANDLE) {
			vkCmdWriteTimestamp(list.cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				m_timestampPool, m_currentFrame * kTimestampsPerFrame);
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
				if (list->activeFramebuffer) {
					vkCmdEndRenderPass(list->cmd);
					if (auto* fb = getLive(m_framebuffers, list->activeFramebuffer.id)) {
						for (const auto& color : fb->desc.colorAttachments) {
							if (auto* tex = getLive(m_textures, color.texture.id)) {
								tex->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
							}
						}
						if (fb->desc.depthStencilAttachment.has_value()) {
							if (auto* tex = getLive(m_textures, fb->desc.depthStencilAttachment->texture.id)) {
								tex->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
							}
						}
					}
					list->activeFramebuffer = {};
				}
				if (Engine::instance()->publishEditorDisplay() && m_presentBlitTexture) {
					if (auto* srcTex = getLive(m_textures, m_presentBlitTexture.id)) {
						const uint32_t w = (std::max)(1u, srcTex->desc.width);
						const uint32_t h = (std::max)(1u, srcTex->desc.height);
						ensureDisplayTextures(w, h);
						const int slot = m_displayWrite;
						if (auto* dstTex = getLive(m_textures, m_displayTex[slot].id)) {
							blitTextureToTexture(list->cmd, *srcTex, *dstTex);
							m_displayPublished.store(slot, std::memory_order_release);
							m_displayWrite = 1 - slot;
							m_inlineDisplayBlit = true;
						}
					}
				}
				if (m_swapchainAcquired) {
					if (!m_swapchainBlitted) {
						recordPresentBlit(list->cmd, m_acquiredImageIndex);
						if (m_acquiredImageIndex < m_swapchainLayouts.size()) {
							m_swapchainLayouts[m_acquiredImageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
						}
						m_swapchainBlitted = true;
					}
					if (m_swapchainBlitted && m_acquiredImageIndex < m_swapchainImages.size()
						&& m_acquiredImageIndex < m_swapchainLayouts.size()) {
						auto& layout = m_swapchainLayouts[m_acquiredImageIndex];
						if (layout != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
							transitionImageLayout(list->cmd, m_swapchainImages[m_acquiredImageIndex],
								m_swapchainFormat, layout, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
							layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
						}
					}
					m_inlinePresent = true;
				}
				if (m_timestampPool != VK_NULL_HANDLE) {
					vkCmdWriteTimestamp(list->cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
						m_timestampPool, m_currentFrame * kTimestampsPerFrame + 1);
					m_timestampPending[m_currentFrame] = true;
				}
				if (vkEndCommandBuffer(list->cmd) != VK_SUCCESS) {
					LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: vkEndCommandBuffer failed");
				}
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

		if (m_inlinePresent) {
			m_inflightSceneCmd[m_currentFrame] = lists[0];
			submitInlinePresent(list->cmd);
			list->submitted = true;
			return;
		}

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &list->cmd;
		{
			std::lock_guard lock(m_queueMutex);
			const VkResult submitResult = vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_sceneFence);
			if (submitResult != VK_SUCCESS) {
				LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: scene submit failed (%d)", static_cast<int>(submitResult));
			}
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
		if (!list || !buffer || !data || size == 0) {
			return;
		}
		if (buffer->mapped) {
			updateBuffer(handle, offset, size, data);
			return;
		}
		if (list->activeFramebuffer) {
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
