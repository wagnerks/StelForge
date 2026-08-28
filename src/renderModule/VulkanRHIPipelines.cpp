#include "VulkanRHI.h"

#include <array>
#include <cstring>
#include <fstream>
#include <vector>

#include "VulkanRHIVkUtil.h"
#include "core/FileSystem.h"
#include "logsModule/logger.h"

using namespace SFE::Render::VulkanVkUtil;

namespace SFE::Render {
	namespace {
		std::vector<uint32_t> loadSpirvBytes(const std::string& name) {
			const std::string path = "shaders/vulkan/" + name + ".spv";
			std::ifstream file(path, std::ios::binary | std::ios::ate);
			if (!file) {
				LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: SPIR-V not found: %s", path.c_str());
				return {};
			}
			const auto size = file.tellg();
			if (size <= 0 || (size % 4) != 0) {
				LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: invalid SPIR-V size for %s", path.c_str());
				return {};
			}
			std::vector<uint32_t> code(static_cast<size_t>(size / 4));
			file.seekg(0);
			file.read(reinterpret_cast<char*>(code.data()), size);
			return code;
		}

		VkShaderModule createVkShaderModule(VkDevice device, const std::vector<uint32_t>& code) {
			VkShaderModuleCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			createInfo.codeSize = code.size() * sizeof(uint32_t);
			createInfo.pCode = code.data();
			VkShaderModule module = VK_NULL_HANDLE;
			if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS) {
				LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: vkCreateShaderModule failed");
			}
			return module;
		}
	}

	void VulkanRHI::setPresentBlitSource(const TextureHandle texture, const SamplerHandle sampler) {
		m_presentBlitTexture = texture;
		m_presentBlitSampler = sampler;
	}

	void VulkanRHI::createSceneCommandPool() {
		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = m_graphicsQueueFamily;
		if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_sceneCommandPool) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: failed to create scene command pool");
		}

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		if (vkCreateFence(m_device, &fenceInfo, nullptr, &m_sceneFence) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: failed to create scene fence");
		}
	}

	void VulkanRHI::createBlitPipeline() {
		const auto vertCode = loadSpirvBytes("blit.vs");
		const auto fragCode = loadSpirvBytes("blit.fs");
		const VkShaderModule vertModule = createVkShaderModule(m_device, vertCode);
		const VkShaderModule fragModule = createVkShaderModule(m_device, fragCode);

		VkDescriptorSetLayoutBinding samplerBinding{};
		samplerBinding.binding = 0;
		samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerBinding.descriptorCount = 1;
		samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &samplerBinding;
		if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_blitSetLayout) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: blit descriptor set layout failed");
		}

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &m_blitSetLayout;
		if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_blitPipelineLayout) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: blit pipeline layout failed");
		}

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		if (vkCreateSampler(m_device, &samplerInfo, nullptr, &m_blitDefaultSampler) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: blit sampler failed");
		}

		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = m_descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &m_blitSetLayout;
		if (vkAllocateDescriptorSets(m_device, &allocInfo, &m_blitDescriptorSet) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: blit descriptor set allocation failed");
		}

		VkPipelineShaderStageCreateInfo stages[2]{};
		stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = vertModule;
		stages[0].pName = "main";
		stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = fragModule;
		stages[1].pName = "main";

		VkPipelineVertexInputStateCreateInfo vertexInput{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;

		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.cullMode = VK_CULL_MODE_NONE;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterizer.lineWidth = 1.f;

		VkPipelineMultisampleStateCreateInfo multisampling{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineColorBlendAttachmentState blendAttachment{};
		blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &blendAttachment;

		const std::array dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicState.pDynamicStates = dynamicStates.data();

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = stages;
		pipelineInfo.pVertexInputState = &vertexInput;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = m_blitPipelineLayout;
		pipelineInfo.renderPass = m_swapchainRenderPass;
		pipelineInfo.subpass = 0;

		if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_blitPipeline) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: blit pipeline creation failed");
		}

		vkDestroyShaderModule(m_device, vertModule, nullptr);
		vkDestroyShaderModule(m_device, fragModule, nullptr);
	}

	void VulkanRHI::destroyBlitPipeline() {
		if (m_blitPipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(m_device, m_blitPipeline, nullptr);
			m_blitPipeline = VK_NULL_HANDLE;
		}
		if (m_blitPipelineLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(m_device, m_blitPipelineLayout, nullptr);
			m_blitPipelineLayout = VK_NULL_HANDLE;
		}
		if (m_blitSetLayout != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(m_device, m_blitSetLayout, nullptr);
			m_blitSetLayout = VK_NULL_HANDLE;
		}
		if (m_blitDefaultSampler != VK_NULL_HANDLE) {
			vkDestroySampler(m_device, m_blitDefaultSampler, nullptr);
			m_blitDefaultSampler = VK_NULL_HANDLE;
		}
		m_blitDescriptorSet = VK_NULL_HANDLE;
	}

	void VulkanRHI::recordPresentBlit(const VkCommandBuffer cmd, const uint32_t imageIndex) {
		auto* tex = getLive(m_textures, m_presentBlitTexture.id);
		if (!tex || !tex->view) {
			VkRenderPassBeginInfo renderPassInfo{};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = m_swapchainRenderPass;
			renderPassInfo.framebuffer = m_swapchainFramebuffers[imageIndex];
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = m_swapchainExtent;
			VkClearValue clearValue{};
			clearValue.color = { { 0.05f, 0.08f, 0.22f, 1.f } };
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearValue;
			vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdEndRenderPass(cmd);
			return;
		}

		auto* samplerRes = getLive(m_samplers, m_presentBlitSampler.id);
		VkSampler sampler = samplerRes ? samplerRes->sampler : m_blitDefaultSampler;

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = tex->view;
		imageInfo.sampler = sampler;

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = m_blitDescriptorSet;
		write.dstBinding = 0;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.descriptorCount = 1;
		write.pImageInfo = &imageInfo;
		vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = m_swapchainRenderPass;
		renderPassInfo.framebuffer = m_swapchainFramebuffers[imageIndex];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = m_swapchainExtent;
		VkClearValue clearValue{};
		clearValue.color = { { 0.05f, 0.08f, 0.22f, 1.f } };
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearValue;

		vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport{};
		viewport.width = static_cast<float>(m_swapchainExtent.width);
		viewport.height = static_cast<float>(m_swapchainExtent.height);
		viewport.minDepth = 0.f;
		viewport.maxDepth = 1.f;
		VkRect2D scissor{ { 0, 0 }, m_swapchainExtent };
		vkCmdSetViewport(cmd, 0, 1, &viewport);
		vkCmdSetScissor(cmd, 0, 1, &scissor);

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_blitPipeline);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_blitPipelineLayout, 0, 1, &m_blitDescriptorSet, 0, nullptr);
		vkCmdDraw(cmd, 3, 1, 0, 0);
		vkCmdEndRenderPass(cmd);
	}

	ShaderModuleHandle VulkanRHI::createShaderModule(const ShaderModuleDesc& desc) {
		std::string spvName = desc.name;
		if (const auto pos = spvName.find_last_of("/\\"); pos != std::string::npos) {
			spvName = spvName.substr(pos + 1);
		}
		const auto code = loadSpirvBytes(spvName);
		VkShaderModuleResource resource{};
		resource.module = createVkShaderModule(m_device, code);
		resource.desc = desc;
		resource.live = true;

		ShaderModuleHandle handle{};
		handle.id = storeResource(m_shaders, m_freeShaders, std::move(resource));
		return handle;
	}

	void VulkanRHI::destroyShaderModule(const ShaderModuleHandle handle) {
		if (auto* shader = getLive(m_shaders, handle.id)) {
			if (shader->module != VK_NULL_HANDLE) {
				vkDestroyShaderModule(m_device, shader->module, nullptr);
			}
			recycleResource(m_shaders, m_freeShaders, handle.id);
		}
	}

	RenderPassHandle VulkanRHI::createRenderPass(const RenderPassDesc& desc) {
		std::vector<VkAttachmentDescription> attachments;
		std::vector<VkAttachmentReference> colorRefs;
		attachments.reserve(desc.colorAttachmentCount + (desc.hasDepthStencil ? 1u : 0u));
		colorRefs.reserve(desc.colorAttachmentCount);

		for (uint32_t i = 0; i < desc.colorAttachmentCount; ++i) {
			const auto& src = desc.colorAttachments[i];
			VkAttachmentDescription attachment{};
			attachment.format = toVkFormat(src.format);
			attachment.samples = VK_SAMPLE_COUNT_1_BIT;
			attachment.loadOp = toVkLoadOp(src.loadOp);
			attachment.storeOp = toVkStoreOp(src.storeOp);
			attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachment.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			attachments.push_back(attachment);

			VkAttachmentReference ref{};
			ref.attachment = i;
			ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			colorRefs.push_back(ref);
		}

		VkAttachmentReference depthRef{};
		const VkAttachmentReference* depthRefPtr = nullptr;
		if (desc.hasDepthStencil) {
			VkAttachmentDescription depthAttachment{};
			depthAttachment.format = toVkFormat(desc.depthAttachment.format);
			depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			depthAttachment.loadOp = toVkLoadOp(desc.depthAttachment.loadOp);
			depthAttachment.storeOp = toVkStoreOp(desc.depthAttachment.storeOp);
			depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			depthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			depthRef.attachment = desc.colorAttachmentCount;
			depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			attachments.push_back(depthAttachment);
			depthRefPtr = &depthRef;
		}

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
		subpass.pColorAttachments = colorRefs.empty() ? nullptr : colorRefs.data();
		subpass.pDepthStencilAttachment = depthRefPtr;

		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		VkRenderPassResource resource{};
		resource.desc = desc;
		resource.live = true;
		if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &resource.renderPass) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: createRenderPass failed");
		}

		RenderPassHandle handle{};
		handle.id = storeResource(m_renderPasses, m_freeRenderPasses, std::move(resource));
		return handle;
	}

	void VulkanRHI::destroyRenderPass(const RenderPassHandle renderPass) {
		if (auto* pass = getLive(m_renderPasses, renderPass.id)) {
			if (pass->renderPass != VK_NULL_HANDLE) {
				vkDestroyRenderPass(m_device, pass->renderPass, nullptr);
			}
			recycleResource(m_renderPasses, m_freeRenderPasses, renderPass.id);
		}
	}

	FramebufferHandle VulkanRHI::createFramebuffer(const FramebufferDesc& desc) {
		const auto* pass = getLive(m_renderPasses, desc.renderPass.id);
		if (!pass) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: createFramebuffer requires valid renderPass");
			return {};
		}

		std::vector<VkImageView> attachments;
		attachments.reserve(desc.colorAttachments.size() + 1);
		for (const auto& color : desc.colorAttachments) {
			const auto* tex = getLive(m_textures, color.texture.id);
			if (!tex || !tex->view) {
				LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: invalid color attachment texture");
				return {};
			}
			attachments.push_back(tex->view);
		}
		if (desc.depthStencilAttachment.has_value()) {
			const auto* tex = getLive(m_textures, desc.depthStencilAttachment->texture.id);
			if (!tex || !tex->view) {
				LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: invalid depth attachment texture");
				return {};
			}
			attachments.push_back(tex->view);
		}

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = pass->renderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = std::max(1u, desc.width);
		framebufferInfo.height = std::max(1u, desc.height);
		framebufferInfo.layers = 1;

		VkFramebufferResource resource{};
		resource.desc = desc;
		resource.live = true;
		if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &resource.framebuffer) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: vkCreateFramebuffer failed");
		}

		FramebufferHandle handle{};
		handle.id = storeResource(m_framebuffers, m_freeFramebuffers, std::move(resource));
		return handle;
	}

	void VulkanRHI::destroyFramebuffer(const FramebufferHandle framebuffer) {
		if (auto* fb = getLive(m_framebuffers, framebuffer.id)) {
			if (fb->framebuffer != VK_NULL_HANDLE) {
				vkDestroyFramebuffer(m_device, fb->framebuffer, nullptr);
			}
			recycleResource(m_framebuffers, m_freeFramebuffers, framebuffer.id);
		}
	}

	FramebufferHandle VulkanRHI::getBackbufferFramebuffer() {
		return FramebufferHandle{ 0 };
	}

	PipelineHandle VulkanRHI::createGraphicsPipeline(const GraphicsPipelineDesc& desc) {
		const auto* vert = getLive(m_shaders, desc.shaders.vertex.id);
		const auto* frag = getLive(m_shaders, desc.shaders.fragment.id);
		if (!vert || !frag) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: createGraphicsPipeline requires vertex/fragment shaders");
			return {};
		}

		const auto* renderPass = getLive(m_renderPasses, desc.renderPass.id);
		if (!renderPass) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: createGraphicsPipeline requires renderPass");
			return {};
		}

		std::vector<VkDescriptorSetLayout> setLayouts;
		setLayouts.reserve(desc.descriptorSetLayouts.size());
		for (const auto layoutHandle : desc.descriptorSetLayouts) {
			const auto* layout = getLive(m_descriptorSetLayouts, layoutHandle.id);
			if (!layout) {
				LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: invalid descriptor set layout in pipeline");
				return {};
			}
			setLayouts.push_back(layout->layout);
		}

		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		{
			VkPushConstantRange pushRange{};
			if (desc.pushConstantSize > 0) {
				pushRange.stageFlags = toVkShaderStage(desc.pushConstantStages);
				pushRange.offset = 0;
				pushRange.size = desc.pushConstantSize;
			}

			VkPipelineLayoutCreateInfo layoutInfo{};
			layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			layoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
			layoutInfo.pSetLayouts = setLayouts.empty() ? nullptr : setLayouts.data();
			layoutInfo.pushConstantRangeCount = desc.pushConstantSize > 0 ? 1u : 0u;
			layoutInfo.pPushConstantRanges = desc.pushConstantSize > 0 ? &pushRange : nullptr;
			if (vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
				LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: pipeline layout creation failed");
				return {};
			}
		}

		std::vector<VkVertexInputBindingDescription> bindings(desc.layout.bindings.size());
		for (size_t i = 0; i < desc.layout.bindings.size(); ++i) {
			bindings[i].binding = desc.layout.bindings[i].binding;
			bindings[i].stride = desc.layout.bindings[i].stride;
			bindings[i].inputRate = desc.layout.bindings[i].inputRate == VertexInputRate::PerInstance
				? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
		}

		std::vector<VkVertexInputAttributeDescription> attributes(desc.layout.attributes.size());
		for (size_t i = 0; i < desc.layout.attributes.size(); ++i) {
			const auto& attr = desc.layout.attributes[i];
			attributes[i].location = attr.location;
			attributes[i].binding = attr.binding;
			attributes[i].format = vertexFormatToVk(attr.format);
			attributes[i].offset = attr.offset;
		}

		VkPipelineVertexInputStateCreateInfo vertexInput{};
		vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInput.vertexBindingDescriptionCount = static_cast<uint32_t>(bindings.size());
		vertexInput.pVertexBindingDescriptions = bindings.empty() ? nullptr : bindings.data();
		vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
		vertexInput.pVertexAttributeDescriptions = attributes.empty() ? nullptr : attributes.data();

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = toVkTopology(desc.topology);

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;

		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.polygonMode = toVkPolygonMode(desc.fillMode);
		rasterizer.cullMode = toVkCullMode(desc.cullMode);
		rasterizer.frontFace = toVkFrontFace(desc.frontFace);
		rasterizer.lineWidth = 1.f;

		VkPipelineMultisampleStateCreateInfo multisampling{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = toVkBool(desc.depthStencilState.depthTest);
		depthStencil.depthWriteEnable = toVkBool(desc.depthStencilState.depthWrite);
		depthStencil.depthCompareOp = toVkCompareOp(desc.depthStencilState.depthCompareOp);

		VkPipelineColorBlendAttachmentState blendAttachment{};
		blendAttachment.blendEnable = toVkBool(desc.blend.enabled);
		blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		const uint32_t colorAttachmentCount = (std::max)(1u, renderPass->desc.colorAttachmentCount);
		std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(colorAttachmentCount, blendAttachment);

		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.attachmentCount = colorAttachmentCount;
		colorBlending.pAttachments = blendAttachments.data();

		const std::array dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicState.pDynamicStates = dynamicStates.data();

		VkPipelineShaderStageCreateInfo stages[2]{};
		stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		stages[0].module = vert->module;
		stages[0].pName = "main";
		stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		stages[1].module = frag->module;
		stages[1].pName = "main";

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = stages;
		pipelineInfo.pVertexInputState = &vertexInput;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = &depthStencil;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = pipelineLayout;
		pipelineInfo.renderPass = renderPass->renderPass;
		pipelineInfo.subpass = desc.subpassIndex;

		VkGraphicsPipelineResource resource{};
		resource.desc = desc;
		resource.layout = pipelineLayout;
		resource.live = true;
		if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &resource.pipeline) != VK_SUCCESS) {
			vkDestroyPipelineLayout(m_device, pipelineLayout, nullptr);
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: vkCreateGraphicsPipelines failed for %s", vert->desc.name.c_str());
			return {};
		}

		PipelineHandle handle{};
		handle.id = storeResource(m_pipelines, m_freePipelines, std::move(resource));
		return handle;
	}

	PipelineHandle VulkanRHI::createComputePipeline(const ComputePipelineDesc& desc) {
		const auto* compute = getLive(m_shaders, desc.compute.id);
		if (!compute) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: createComputePipeline requires compute shader");
			return {};
		}

		std::vector<VkDescriptorSetLayout> setLayouts;
		setLayouts.reserve(desc.descriptorSetLayouts.size());
		for (const auto layoutHandle : desc.descriptorSetLayouts) {
			const auto* layout = getLive(m_descriptorSetLayouts, layoutHandle.id);
			if (!layout) {
				LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: invalid descriptor set layout in compute pipeline");
				return {};
			}
			setLayouts.push_back(layout->layout);
		}

		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
		{
			VkPipelineLayoutCreateInfo layoutInfo{};
			layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			layoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
			layoutInfo.pSetLayouts = setLayouts.empty() ? nullptr : setLayouts.data();
			if (vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
				LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: compute pipeline layout creation failed");
				return {};
			}
		}

		VkPipelineShaderStageCreateInfo stage{};
		stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		stage.module = compute->module;
		stage.pName = "main";

		VkComputePipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipelineInfo.stage = stage;
		pipelineInfo.layout = pipelineLayout;

		VkGraphicsPipelineResource resource{};
		resource.computeDesc = desc;
		resource.layout = pipelineLayout;
		resource.isCompute = true;
		resource.live = true;
		if (vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &resource.pipeline) != VK_SUCCESS) {
			vkDestroyPipelineLayout(m_device, pipelineLayout, nullptr);
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: vkCreateComputePipelines failed");
			return {};
		}

		PipelineHandle handle{};
		handle.id = storeResource(m_pipelines, m_freePipelines, std::move(resource));
		return handle;
	}

	void VulkanRHI::destroyGraphicsPipeline(const PipelineHandle pipeline) {
		if (auto* gp = getLive(m_pipelines, pipeline.id)) {
			if (gp->pipeline != VK_NULL_HANDLE) {
				vkDestroyPipeline(m_device, gp->pipeline, nullptr);
			}
			if (gp->layout != VK_NULL_HANDLE) {
				vkDestroyPipelineLayout(m_device, gp->layout, nullptr);
			}
			recycleResource(m_pipelines, m_freePipelines, pipeline.id);
		}
	}

	void VulkanRHI::cmdBeginRenderPass(const CommandListHandle cmd, const RenderPassBeginInfo& beginInfo) {
		auto* list = getLive(m_commandLists, cmd.id);
		const auto* pass = getLive(m_renderPasses, beginInfo.renderPass.id);
		const auto* fb = getLive(m_framebuffers, beginInfo.framebuffer.id);
		if (!list || !pass || !fb) {
			return;
		}

		std::vector<VkClearValue> clearValues;
		clearValues.reserve(pass->desc.colorAttachmentCount + (pass->desc.hasDepthStencil ? 1u : 0u));
		for (uint32_t i = 0; i < pass->desc.colorAttachmentCount; ++i) {
			VkClearValue value{};
			const Math::Vec4 color = beginInfo.overrideClearColors ? beginInfo.overrideClearColors[i] : pass->desc.colorAttachments[i].clearColor;
			value.color = { { color.x, color.y, color.z, color.w } };
			clearValues.push_back(value);
		}
		if (pass->desc.hasDepthStencil) {
			VkClearValue value{};
			value.depthStencil.depth = beginInfo.overrideClearDepth ? *beginInfo.overrideClearDepth : pass->desc.depthAttachment.clearDepth;
			value.depthStencil.stencil = beginInfo.overrideClearStencil ? *beginInfo.overrideClearStencil : pass->desc.depthAttachment.clearStencil;
			clearValues.push_back(value);
		}

		VkRenderPassBeginInfo rpBegin{};
		rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rpBegin.renderPass = pass->renderPass;
		rpBegin.framebuffer = fb->framebuffer;
		rpBegin.renderArea.offset = { beginInfo.x, beginInfo.y };
		rpBegin.renderArea.extent = {
			static_cast<uint32_t>(std::max(1, beginInfo.width)),
			static_cast<uint32_t>(std::max(1, beginInfo.height))
		};
		rpBegin.clearValueCount = static_cast<uint32_t>(clearValues.size());
		rpBegin.pClearValues = clearValues.empty() ? nullptr : clearValues.data();
		vkCmdBeginRenderPass(list->cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport{};
		viewport.x = static_cast<float>(beginInfo.x);
		viewport.y = static_cast<float>(beginInfo.y);
		viewport.width = static_cast<float>(std::max(1, beginInfo.width));
		viewport.height = static_cast<float>(std::max(1, beginInfo.height));
		viewport.minDepth = 0.f;
		viewport.maxDepth = 1.f;
		VkRect2D scissor{
			{ beginInfo.x, beginInfo.y },
			{ static_cast<uint32_t>(std::max(1, beginInfo.width)), static_cast<uint32_t>(std::max(1, beginInfo.height)) }
		};
		vkCmdSetViewport(list->cmd, 0, 1, &viewport);
		vkCmdSetScissor(list->cmd, 0, 1, &scissor);
	}

	void VulkanRHI::cmdEndRenderPass(const CommandListHandle cmd) {
		if (auto* list = getLive(m_commandLists, cmd.id)) {
			vkCmdEndRenderPass(list->cmd);
		}
	}

	void VulkanRHI::cmdBindPipeline(const CommandListHandle cmd, const PipelineHandle pipeline) {
		auto* list = getLive(m_commandLists, cmd.id);
		const auto* gp = getLive(m_pipelines, pipeline.id);
		if (!list || !gp) {
			return;
		}
		list->boundLayout = gp->layout;
		list->boundBindPoint = gp->isCompute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
		vkCmdBindPipeline(list->cmd, list->boundBindPoint, gp->pipeline);
	}

	void VulkanRHI::cmdBindVertexBuffer(const CommandListHandle cmd, const uint32_t slot, const BufferHandle handle, const uint32_t offset) {
		const auto* list = getLive(m_commandLists, cmd.id);
		const auto* buffer = getLive(m_buffers, handle.id);
		if (!list || !buffer) {
			return;
		}
		const VkDeviceSize offsets[] = { offset };
		vkCmdBindVertexBuffers(list->cmd, slot, 1, &buffer->buffer, offsets);
	}

	void VulkanRHI::cmdBindIndexBuffer(const CommandListHandle cmd, const BufferHandle handle, const IndexType type, const uint32_t offset) {
		const auto* list = getLive(m_commandLists, cmd.id);
		const auto* buffer = getLive(m_buffers, handle.id);
		if (!list || !buffer) {
			return;
		}
		vkCmdBindIndexBuffer(list->cmd, buffer->buffer, offset,
			type == IndexType::Uint16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
	}

	void VulkanRHI::cmdBindDescriptorSets(const CommandListHandle cmd, const PipelineHandle pipeline, const uint32_t firstSet,
		const DescriptorSetHandle* sets, const uint32_t setCount)
	{
		const auto* list = getLive(m_commandLists, cmd.id);
		if (!list || !sets || setCount == 0) {
			return;
		}

		VkPipelineLayout layout = VK_NULL_HANDLE;
		VkPipelineBindPoint bindPoint = list->boundBindPoint;
		if (const auto* gp = getLive(m_pipelines, pipeline.id)) {
			layout = gp->layout;
			bindPoint = gp->isCompute ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
		}
		else {
			layout = list->boundLayout;
		}
		if (layout == VK_NULL_HANDLE) {
			return;
		}

		std::vector<VkDescriptorSet> vkSets;
		vkSets.reserve(setCount);
		for (uint32_t i = 0; i < setCount; ++i) {
			const auto* set = getLive(m_descriptorSets, sets[i].id);
			if (set) {
				vkSets.push_back(set->set);
			}
		}
		if (vkSets.size() != setCount) {
			return;
		}
		if (layout == VK_NULL_HANDLE || list->cmd == VK_NULL_HANDLE || !list->recording) {
			return;
		}
		vkCmdBindDescriptorSets(list->cmd, bindPoint, layout, firstSet,
			static_cast<uint32_t>(vkSets.size()), vkSets.data(), 0, nullptr);
	}

	void VulkanRHI::cmdPushConstants(const CommandListHandle cmd, const PipelineHandle pipeline, const ShaderStage stages, const void* data, const uint32_t size) {
		auto* list = getLive(m_commandLists, cmd.id);
		const auto* gp = getLive(m_pipelines, pipeline.id);
		if (!list || !gp || !data || size == 0 || !list->recording || gp->layout == VK_NULL_HANDLE) {
			return;
		}
		vkCmdPushConstants(list->cmd, gp->layout, toVkShaderStage(stages), 0, size, data);
	}
	void VulkanRHI::cmdDraw(const CommandListHandle cmd, const uint32_t vertexCount, const uint32_t instanceCount, const uint32_t firstVertex, const uint32_t) {
		if (auto* list = getLive(m_commandLists, cmd.id)) {
			vkCmdDraw(list->cmd, vertexCount, instanceCount, firstVertex, 0);
		}
	}
	void VulkanRHI::cmdDrawIndexed(const CommandListHandle cmd, const uint32_t indexCount, const uint32_t instanceCount,
		const uint32_t firstIndex, const int32_t vertexOffset, const uint32_t)
	{
		if (auto* list = getLive(m_commandLists, cmd.id)) {
			vkCmdDrawIndexed(list->cmd, indexCount, instanceCount, firstIndex, vertexOffset, 0);
		}
	}
	void VulkanRHI::cmdDispatch(const CommandListHandle cmd, const uint32_t groupCountX, const uint32_t groupCountY, const uint32_t groupCountZ) {
		if (auto* list = getLive(m_commandLists, cmd.id)) {
			vkCmdDispatch(list->cmd, groupCountX, groupCountY, groupCountZ);
		}
	}
	void VulkanRHI::cmdBlit(const CommandListHandle, const FramebufferHandle, const FramebufferHandle, const BlitMask, const BlitFilter) {}
}
