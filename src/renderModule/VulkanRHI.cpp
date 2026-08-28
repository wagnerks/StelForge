#include "VulkanRHI.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "core/Engine.h"
#include "logsModule/logger.h"
#include "windowsModule/Window.h"

namespace SFE::Render {
	namespace {
		constexpr int kMaxFramesInFlight = 2;
		constexpr VkClearColorValue kClearColor{ { 0.05f, 0.08f, 0.22f, 1.f } };

		struct QueueFamilyIndices {
			uint32_t graphics = UINT32_MAX;
			uint32_t present = UINT32_MAX;

			bool complete() const {
				return graphics != UINT32_MAX && present != UINT32_MAX;
			}
		};

		struct SwapchainSupport {
			VkSurfaceCapabilitiesKHR capabilities{};
			std::vector<VkSurfaceFormatKHR> formats;
			std::vector<VkPresentModeKHR> presentModes;
		};

		QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
			QueueFamilyIndices indices;
			uint32_t count = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
			std::vector<VkQueueFamilyProperties> families(count);
			vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

			for (uint32_t i = 0; i < count; ++i) {
				if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
					indices.graphics = i;
				}

				VkBool32 presentSupport = VK_FALSE;
				vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
				if (presentSupport) {
					indices.present = i;
				}

				if (indices.complete()) {
					break;
				}
			}
			return indices;
		}

		SwapchainSupport querySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
			SwapchainSupport details;
			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
			uint32_t formatCount = 0;
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
			if (formatCount > 0) {
				details.formats.resize(formatCount);
				vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
			}
			uint32_t presentCount = 0;
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentCount, nullptr);
			if (presentCount > 0) {
				details.presentModes.resize(presentCount);
				vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentCount, details.presentModes.data());
			}
			return details;
		}

		bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
			uint32_t count = 0;
			vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
			std::vector<VkExtensionProperties> available(count);
			vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());
			for (const char* required : { VK_KHR_SWAPCHAIN_EXTENSION_NAME }) {
				bool found = false;
				for (const auto& ext : available) {
					if (std::strcmp(ext.extensionName, required) == 0) {
						found = true;
						break;
					}
				}
				if (!found) {
					return false;
				}
			}
			return true;
		}

		bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) {
			const auto indices = findQueueFamilies(device, surface);
			if (!indices.complete() || !checkDeviceExtensionSupport(device)) {
				return false;
			}
			const auto support = querySwapchainSupport(device, surface);
			return !support.formats.empty() && !support.presentModes.empty();
		}

		VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
			for (const auto& format : formats) {
				if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
					return format;
				}
			}
			return formats.front();
		}

		VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& modes) {
			for (const auto mode : modes) {
				if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
					return mode;
				}
			}
			return VK_PRESENT_MODE_FIFO_KHR;
		}

		VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window) {
			if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
				return capabilities.currentExtent;
			}
			int width = 0;
			int height = 0;
			glfwGetFramebufferSize(window, &width, &height);
			VkExtent2D extent{
				static_cast<uint32_t>(std::max(1, width)),
				static_cast<uint32_t>(std::max(1, height))
			};
			extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
			return extent;
		}
	}

	VulkanRHI::VulkanRHI() = default;

	VulkanRHI::~VulkanRHI() {
		cleanup();
	}

	void VulkanRHI::init() {
		if (m_inited) {
			return;
		}

		auto* window = Engine::instance()->getWindow();
		if (!window || !window->getWindow()) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI::init: window is not created");
			return;
		}

		const auto previousResize = window->onFramebufferResize;
		window->onFramebufferResize = [this, previousResize](int width, int height) {
			if (previousResize) {
				previousResize(width, height);
			}
			m_framebufferResized = true;
		};

		createInstance();
		createSurface();
		pickPhysicalDevice();
		createDevice();
		createDescriptorPool();

		VkCommandPoolCreateInfo uploadPoolInfo{};
		uploadPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		uploadPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		uploadPoolInfo.queueFamilyIndex = m_graphicsQueueFamily;
		if (vkCreateCommandPool(m_device, &uploadPoolInfo, nullptr, &m_uploadCommandPool) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: failed to create upload command pool");
		}

		createSwapchain();
		createImageViews();
		createSwapchainRenderPass();
		createFramebuffers();
		createCommandPool();
		createSceneCommandPool();
		createSyncObjects();
		createSwapchainSyncObjects();
		createCommandBuffers();
		createBlitPipeline();

		m_inited = true;
		LogsModule::Logger::LOG_INFO("VulkanRHI initialized (Phase 3: geometry pass + present blit)");
	}

	void VulkanRHI::cleanup() {
		if (!m_inited && m_instance == VK_NULL_HANDLE) {
			return;
		}

		if (m_device != VK_NULL_HANDLE) {
			vkDeviceWaitIdle(m_device);
		}

		for (auto& buffer : m_buffers) {
			if (buffer.live) {
				destroyBufferInternal(buffer);
			}
		}
		m_buffers.clear();
		m_freeBuffers.clear();

		for (auto& texture : m_textures) {
			if (texture.live) {
				destroyTextureInternal(texture);
			}
		}
		m_textures.clear();
		m_freeTextures.clear();

		for (auto& sampler : m_samplers) {
			if (sampler.live && sampler.sampler != VK_NULL_HANDLE) {
				vkDestroySampler(m_device, sampler.sampler, nullptr);
			}
		}
		m_samplers.clear();
		m_freeSamplers.clear();

		for (auto& layout : m_descriptorSetLayouts) {
			if (layout.live && layout.layout != VK_NULL_HANDLE) {
				vkDestroyDescriptorSetLayout(m_device, layout.layout, nullptr);
			}
		}
		m_descriptorSetLayouts.clear();
		m_freeDescriptorSetLayouts.clear();
		m_descriptorSets.clear();
		m_freeDescriptorSets.clear();
		m_commandLists.clear();
		m_freeCommandLists.clear();

		for (auto& shader : m_shaders) {
			if (shader.live && shader.module != VK_NULL_HANDLE) {
				vkDestroyShaderModule(m_device, shader.module, nullptr);
			}
		}
		m_shaders.clear();
		m_freeShaders.clear();

		for (auto& pipeline : m_pipelines) {
			if (pipeline.live) {
				if (pipeline.pipeline != VK_NULL_HANDLE) vkDestroyPipeline(m_device, pipeline.pipeline, nullptr);
				if (pipeline.layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(m_device, pipeline.layout, nullptr);
			}
		}
		m_pipelines.clear();
		m_freePipelines.clear();

		for (auto& pass : m_renderPasses) {
			if (pass.live && pass.renderPass != VK_NULL_HANDLE) {
				vkDestroyRenderPass(m_device, pass.renderPass, nullptr);
			}
		}
		m_renderPasses.clear();
		m_freeRenderPasses.clear();

		for (auto& fb : m_framebuffers) {
			if (fb.live && fb.framebuffer != VK_NULL_HANDLE) {
				vkDestroyFramebuffer(m_device, fb.framebuffer, nullptr);
			}
		}
		m_framebuffers.clear();
		m_freeFramebuffers.clear();

		destroyBlitPipeline();

		if (m_sceneFence != VK_NULL_HANDLE) {
			vkDestroyFence(m_device, m_sceneFence, nullptr);
			m_sceneFence = VK_NULL_HANDLE;
		}
		if (m_sceneCommandPool != VK_NULL_HANDLE) {
			vkDestroyCommandPool(m_device, m_sceneCommandPool, nullptr);
			m_sceneCommandPool = VK_NULL_HANDLE;
		}

		destroyDescriptorPool();

		if (m_uploadCommandPool != VK_NULL_HANDLE) {
			vkDestroyCommandPool(m_device, m_uploadCommandPool, nullptr);
			m_uploadCommandPool = VK_NULL_HANDLE;
		}

		destroySwapchainSyncObjects();
		cleanupSwapchain();

		for (size_t i = 0; i < m_inFlightFences.size(); ++i) {
			if (m_inFlightFences[i] != VK_NULL_HANDLE) {
				vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
			}
			if (i < m_imageAvailableSemaphores.size() && m_imageAvailableSemaphores[i] != VK_NULL_HANDLE) {
				vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
			}
		}
		m_inFlightFences.clear();
		m_imageAvailableSemaphores.clear();

		if (m_commandPool != VK_NULL_HANDLE) {
			vkDestroyCommandPool(m_device, m_commandPool, nullptr);
			m_commandPool = VK_NULL_HANDLE;
		}

		if (m_device != VK_NULL_HANDLE) {
			vkDestroyDevice(m_device, nullptr);
			m_device = VK_NULL_HANDLE;
		}

		if (m_surface != VK_NULL_HANDLE) {
			vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
			m_surface = VK_NULL_HANDLE;
		}

		if (m_instance != VK_NULL_HANDLE) {
			vkDestroyInstance(m_instance, nullptr);
			m_instance = VK_NULL_HANDLE;
		}

		m_physicalDevice = VK_NULL_HANDLE;
		m_inited = false;
	}

	void VulkanRHI::createInstance() {
		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "StelForgeRuntime";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "StelForgeEngine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_2;

		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = glfwExtensionCount;
		createInfo.ppEnabledExtensionNames = glfwExtensions;

		if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: vkCreateInstance failed");
		}
	}

	void VulkanRHI::createSurface() {
		auto* glfwWindow = Engine::instance()->getWindow()->getWindow();
		if (glfwCreateWindowSurface(m_instance, glfwWindow, nullptr, &m_surface) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: glfwCreateWindowSurface failed");
		}
	}

	void VulkanRHI::pickPhysicalDevice() {
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
		if (deviceCount == 0) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: no Vulkan-capable GPU found");
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

		for (const auto device : devices) {
			if (isDeviceSuitable(device, m_surface)) {
				m_physicalDevice = device;
				break;
			}
		}

		if (m_physicalDevice == VK_NULL_HANDLE) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: no suitable Vulkan GPU found");
		}
	}

	void VulkanRHI::createDevice() {
		const auto indices = findQueueFamilies(m_physicalDevice, m_surface);
		m_graphicsQueueFamily = indices.graphics;
		m_presentQueueFamily = indices.present;

		std::set<uint32_t> uniqueFamilies{ indices.graphics, indices.present };
		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		const float queuePriority = 1.f;
		for (const uint32_t family : uniqueFamilies) {
			VkDeviceQueueCreateInfo queueInfo{};
			queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueFamilyIndex = family;
			queueInfo.queueCount = 1;
			queueInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueInfo);
		}

		VkPhysicalDeviceFeatures supportedFeatures{};
		vkGetPhysicalDeviceFeatures(m_physicalDevice, &supportedFeatures);

		VkPhysicalDeviceFeatures deviceFeatures{};
		if (supportedFeatures.samplerAnisotropy) {
			deviceFeatures.samplerAnisotropy = VK_TRUE;
		}

		const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = 1;
		createInfo.ppEnabledExtensionNames = deviceExtensions;

		if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: vkCreateDevice failed");
		}

		vkGetDeviceQueue(m_device, indices.graphics, 0, &m_graphicsQueue);
		vkGetDeviceQueue(m_device, indices.present, 0, &m_presentQueue);
	}

	void VulkanRHI::createSwapchain() {
		const auto support = querySwapchainSupport(m_physicalDevice, m_surface);
		const auto surfaceFormat = chooseSwapSurfaceFormat(support.formats);
		const auto presentMode = chooseSwapPresentMode(support.presentModes);
		const auto extent = chooseSwapExtent(support.capabilities, Engine::instance()->getWindow()->getWindow());

		uint32_t imageCount = support.capabilities.minImageCount + 1;
		if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount) {
			imageCount = support.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = m_surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		const auto indices = findQueueFamilies(m_physicalDevice, m_surface);
		const uint32_t queueFamilyIndices[] = { indices.graphics, indices.present };
		if (indices.graphics != indices.present) {
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else {
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}

		createInfo.preTransform = support.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: vkCreateSwapchainKHR failed");
		}

		uint32_t swapImageCount = 0;
		vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapImageCount, nullptr);
		m_swapchainImages.resize(swapImageCount);
		vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapImageCount, m_swapchainImages.data());

		m_swapchainFormat = surfaceFormat.format;
		m_swapchainExtent = extent;
	}

	void VulkanRHI::createImageViews() {
		m_swapchainImageViews.resize(m_swapchainImages.size());
		for (size_t i = 0; i < m_swapchainImages.size(); ++i) {
			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = m_swapchainImages[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = m_swapchainFormat;
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			if (vkCreateImageView(m_device, &createInfo, nullptr, &m_swapchainImageViews[i]) != VK_SUCCESS) {
				LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: vkCreateImageView failed");
			}
		}
	}

	void VulkanRHI::createSwapchainRenderPass() {
		VkAttachmentDescription colorAttachment{};
		colorAttachment.format = m_swapchainFormat;
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;

		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

		if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_swapchainRenderPass) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: vkCreateRenderPass failed");
		}
	}

	void VulkanRHI::createFramebuffers() {
		m_swapchainFramebuffers.resize(m_swapchainImageViews.size());
		for (size_t i = 0; i < m_swapchainImageViews.size(); ++i) {
			VkImageView attachments[] = { m_swapchainImageViews[i] };

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = m_swapchainRenderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = m_swapchainExtent.width;
			framebufferInfo.height = m_swapchainExtent.height;
			framebufferInfo.layers = 1;

			if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_swapchainFramebuffers[i]) != VK_SUCCESS) {
				LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: vkCreateFramebuffer failed");
			}
		}
	}

	void VulkanRHI::createCommandPool() {
		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = m_graphicsQueueFamily;

		if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: vkCreateCommandPool failed");
		}
	}

	void VulkanRHI::createSyncObjects() {
		m_imageAvailableSemaphores.resize(kMaxFramesInFlight);
		m_inFlightFences.resize(kMaxFramesInFlight);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (int i = 0; i < kMaxFramesInFlight; ++i) {
			if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
				LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: failed to create sync objects");
			}
		}
	}

	void VulkanRHI::destroySwapchainSyncObjects() {
		for (auto semaphore : m_renderFinishedSemaphores) {
			if (semaphore != VK_NULL_HANDLE) {
				vkDestroySemaphore(m_device, semaphore, nullptr);
			}
		}
		m_renderFinishedSemaphores.clear();
		m_imagesInFlight.clear();
	}

	void VulkanRHI::createSwapchainSyncObjects() {
		destroySwapchainSyncObjects();

		const size_t imageCount = m_swapchainImages.size();
		m_renderFinishedSemaphores.resize(imageCount, VK_NULL_HANDLE);
		m_imagesInFlight.resize(imageCount, VK_NULL_HANDLE);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		for (size_t i = 0; i < imageCount; ++i) {
			if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS) {
				LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: failed to create swapchain sync objects");
			}
		}
	}

	void VulkanRHI::createCommandBuffers() {
		m_commandBuffers.resize(kMaxFramesInFlight);
		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = m_commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

		if (vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: vkAllocateCommandBuffers failed");
		}
	}

	void VulkanRHI::cleanupSwapchain() {
		for (auto framebuffer : m_swapchainFramebuffers) {
			if (framebuffer != VK_NULL_HANDLE) {
				vkDestroyFramebuffer(m_device, framebuffer, nullptr);
			}
		}
		m_swapchainFramebuffers.clear();

		for (auto view : m_swapchainImageViews) {
			if (view != VK_NULL_HANDLE) {
				vkDestroyImageView(m_device, view, nullptr);
			}
		}
		m_swapchainImageViews.clear();
		m_swapchainImages.clear();

		if (m_swapchainRenderPass != VK_NULL_HANDLE) {
			vkDestroyRenderPass(m_device, m_swapchainRenderPass, nullptr);
			m_swapchainRenderPass = VK_NULL_HANDLE;
		}

		if (m_swapchain != VK_NULL_HANDLE) {
			vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
			m_swapchain = VK_NULL_HANDLE;
		}
	}

	void VulkanRHI::recreateSwapchain() {
		int width = 0;
		int height = 0;
		glfwGetFramebufferSize(Engine::instance()->getWindow()->getWindow(), &width, &height);
		while (width == 0 || height == 0) {
			glfwGetFramebufferSize(Engine::instance()->getWindow()->getWindow(), &width, &height);
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(m_device);
		destroyBlitPipeline();
		cleanupSwapchain();
		createSwapchain();
		createImageViews();
		createSwapchainRenderPass();
		createFramebuffers();
		createSwapchainSyncObjects();
		createBlitPipeline();
		m_framebufferResized = false;
	}

	void VulkanRHI::recordClearCommandBuffer(const uint32_t imageIndex) {
		VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
		vkResetCommandBuffer(cmd, 0);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: vkBeginCommandBuffer failed");
		}

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = m_swapchainRenderPass;
		renderPassInfo.framebuffer = m_swapchainFramebuffers[imageIndex];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = m_swapchainExtent;
		renderPassInfo.clearValueCount = 1;
		VkClearValue clearValue{};
		clearValue.color = kClearColor;
		renderPassInfo.pClearValues = &clearValue;

		vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdEndRenderPass(cmd);

		if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: vkEndCommandBuffer failed");
		}
	}

	void VulkanRHI::drawFrame() {
		if (m_framebufferResized) {
			recreateSwapchain();
		}

		vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);

		if (m_hasPendingScene) {
			vkWaitForFences(m_device, 1, &m_sceneFence, VK_TRUE, UINT64_MAX);
			if (auto* sceneList = getLive(m_commandLists, m_pendingSceneCmd.id)) {
				vkFreeCommandBuffers(m_device, m_sceneCommandPool, 1, &sceneList->cmd);
				sceneList->cmd = VK_NULL_HANDLE;
				recycleResource(m_commandLists, m_freeCommandLists, m_pendingSceneCmd.id);
			}
			vkResetFences(m_device, 1, &m_sceneFence);
			m_hasPendingScene = false;
			m_pendingSceneCmd = {};
		}

		uint32_t imageIndex = 0;
		const VkResult acquireResult = vkAcquireNextImageKHR(
			m_device, m_swapchain, UINT64_MAX,
			m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);

		if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
			recreateSwapchain();
			return;
		}
		if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: vkAcquireNextImageKHR failed");
		}

		if (imageIndex < m_imagesInFlight.size() && m_imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
			vkWaitForFences(m_device, 1, &m_imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
		}

		vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);
		if (imageIndex < m_imagesInFlight.size()) {
			m_imagesInFlight[imageIndex] = m_inFlightFences[m_currentFrame];
		}

		VkCommandBuffer cmd = m_commandBuffers[m_currentFrame];
		vkResetCommandBuffer(cmd, 0);
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: vkBeginCommandBuffer failed");
		}

		if (m_presentBlitTexture) {
			recordPresentBlit(cmd, imageIndex);
		}
		else {
			VkRenderPassBeginInfo renderPassInfo{};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = m_swapchainRenderPass;
			renderPassInfo.framebuffer = m_swapchainFramebuffers[imageIndex];
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = m_swapchainExtent;
			VkClearValue clearValue{};
			clearValue.color = kClearColor;
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearValue;
			vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
			vkCmdEndRenderPass(cmd);
		}

		if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: vkEndCommandBuffer failed");
		}

		VkSemaphore waitSemaphores[] = { m_imageAvailableSemaphores[m_currentFrame] };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		if (imageIndex >= m_renderFinishedSemaphores.size()) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: swapchain image index out of range");
		}
		VkSemaphore signalSemaphores[] = { m_renderFinishedSemaphores[imageIndex] };

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmd;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]) != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: vkQueueSubmit failed");
		}

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &m_swapchain;
		presentInfo.pImageIndices = &imageIndex;

		const VkResult presentResult = vkQueuePresentKHR(m_presentQueue, &presentInfo);
		if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
			m_framebufferResized = true;
		}
		else if (presentResult != VK_SUCCESS) {
			LogsModule::Logger::LOG_FATAL(false, "VulkanRHI: vkQueuePresentKHR failed");
		}

		m_currentFrame = (m_currentFrame + 1) % kMaxFramesInFlight;
	}

	void VulkanRHI::present() {
		if (!m_inited) {
			return;
		}
		drawFrame();
	}
}
