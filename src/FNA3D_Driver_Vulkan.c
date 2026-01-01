/* FNA3D - 3D Graphics Library for FNA
 * Native Vulkan Driver - Based on Godot Engine's Vulkan implementation
 *
 * Copyright (c) 2020-2024 Ethan Lee
 */

#if FNA3D_DRIVER_VULKAN

#define VK_NO_PROTOTYPES
#include "FNA3D_Driver_Vulkan.h"
#include "FNA3D_Driver.h"
#include <SDL.h>
#include <SDL_vulkan.h>

/* Logging */
#define VK_LOG_INFO(msg, ...) FNA3D_LogInfo("Vulkan: " msg, ##__VA_ARGS__)
#define VK_LOG_WARN(msg, ...) FNA3D_LogWarn("Vulkan: " msg, ##__VA_ARGS__)
#define VK_LOG_ERROR(msg, ...) FNA3D_LogError("Vulkan: " msg, ##__VA_ARGS__)

#define VK_CHECK(res) do { \
	if ((res) != VK_SUCCESS) { \
		VK_LOG_ERROR("VkResult=%d at %s:%d", (res), __FILE__, __LINE__); \
		return 0; \
	} \
} while(0)

#define VK_CHECK_RET(res, ret) do { \
	if ((res) != VK_SUCCESS) { \
		VK_LOG_ERROR("VkResult=%d at %s:%d", (res), __FILE__, __LINE__); \
		return (ret); \
	} \
} while(0)

/* Format Conversion Tables */
static const VkFormat FNA3DToVkFormat[] = {
	VK_FORMAT_R8G8B8A8_UNORM,          /* FNA3D_SURFACEFORMAT_COLOR */
	VK_FORMAT_B5G6R5_UNORM_PACK16,     /* FNA3D_SURFACEFORMAT_BGR565 */
	VK_FORMAT_B5G5R5A1_UNORM_PACK16,   /* FNA3D_SURFACEFORMAT_BGRA5551 */
	VK_FORMAT_B4G4R4A4_UNORM_PACK16,   /* FNA3D_SURFACEFORMAT_BGRA4444 */
	VK_FORMAT_BC1_RGBA_UNORM_BLOCK,    /* FNA3D_SURFACEFORMAT_DXT1 */
	VK_FORMAT_BC2_UNORM_BLOCK,         /* FNA3D_SURFACEFORMAT_DXT3 */
	VK_FORMAT_BC3_UNORM_BLOCK,         /* FNA3D_SURFACEFORMAT_DXT5 */
	VK_FORMAT_R8G8_SNORM,              /* FNA3D_SURFACEFORMAT_NORMALIZEDBYTE2 */
	VK_FORMAT_R8G8B8A8_SNORM,          /* FNA3D_SURFACEFORMAT_NORMALIZEDBYTE4 */
	VK_FORMAT_A2R10G10B10_UNORM_PACK32,/* FNA3D_SURFACEFORMAT_RGBA1010102 */
	VK_FORMAT_R16G16_UNORM,            /* FNA3D_SURFACEFORMAT_RG32 */
	VK_FORMAT_R16G16B16A16_UNORM,      /* FNA3D_SURFACEFORMAT_RGBA64 */
	VK_FORMAT_R8_UNORM,                /* FNA3D_SURFACEFORMAT_ALPHA8 */
	VK_FORMAT_R32_SFLOAT,              /* FNA3D_SURFACEFORMAT_SINGLE */
	VK_FORMAT_R32G32_SFLOAT,           /* FNA3D_SURFACEFORMAT_VECTOR2 */
	VK_FORMAT_R32G32B32A32_SFLOAT,     /* FNA3D_SURFACEFORMAT_VECTOR4 */
	VK_FORMAT_R16_SFLOAT,              /* FNA3D_SURFACEFORMAT_HALFSINGLE */
	VK_FORMAT_R16G16_SFLOAT,           /* FNA3D_SURFACEFORMAT_HALFVECTOR2 */
	VK_FORMAT_R16G16B16A16_SFLOAT,     /* FNA3D_SURFACEFORMAT_HALFVECTOR4 */
	VK_FORMAT_R16G16B16A16_SFLOAT,     /* FNA3D_SURFACEFORMAT_HDRBLENDABLE */
	VK_FORMAT_R8G8B8A8_SRGB,           /* FNA3D_SURFACEFORMAT_COLORBGRA_EXT */
	VK_FORMAT_BC7_UNORM_BLOCK,         /* FNA3D_SURFACEFORMAT_COLORSRGB_EXT */
};

static const VkFormat FNA3DToVkDepthFormat[] = {
	VK_FORMAT_UNDEFINED,               /* FNA3D_DEPTHFORMAT_NONE */
	VK_FORMAT_D16_UNORM,               /* FNA3D_DEPTHFORMAT_D16 */
	VK_FORMAT_D24_UNORM_S8_UINT,       /* FNA3D_DEPTHFORMAT_D24 */
	VK_FORMAT_D24_UNORM_S8_UINT,       /* FNA3D_DEPTHFORMAT_D24S8 */
};

VkFormat VULKAN_INTERNAL_GetVkFormat(FNA3D_SurfaceFormat format)
{
	return FNA3DToVkFormat[format];
}

VkFormat VULKAN_INTERNAL_GetVkDepthFormat(FNA3D_DepthFormat format)
{
	return FNA3DToVkDepthFormat[format];
}

/* Memory Type Helper */
uint32_t VULKAN_INTERNAL_FindMemoryType(
	VulkanRenderer *renderer,
	uint32_t typeFilter,
	VkMemoryPropertyFlags properties
) {
	uint32_t i;
	for (i = 0; i < renderer->memoryProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) &&
		    (renderer->memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
	return UINT32_MAX;
}

/* Load Vulkan Functions */
static uint8_t VULKAN_LoadFunctions(VulkanRenderer *renderer)
{
	PFN_vkGetDeviceProcAddr vkGetDeviceProcAddr;
	
	/* Instance Functions */
	#define LOAD_INSTANCE_FUNC(fn) \
		renderer->fn = (PFN_##fn)renderer->vkGetInstanceProcAddr(renderer->instance, #fn); \
		if (!renderer->fn) { VK_LOG_ERROR("Failed to load " #fn); return 0; }
	
	LOAD_INSTANCE_FUNC(vkDestroyInstance)
	LOAD_INSTANCE_FUNC(vkEnumeratePhysicalDevices)
	LOAD_INSTANCE_FUNC(vkGetPhysicalDeviceProperties)
	LOAD_INSTANCE_FUNC(vkGetPhysicalDeviceFeatures)
	LOAD_INSTANCE_FUNC(vkGetPhysicalDeviceMemoryProperties)
	LOAD_INSTANCE_FUNC(vkGetPhysicalDeviceQueueFamilyProperties)
	LOAD_INSTANCE_FUNC(vkCreateDevice)
	LOAD_INSTANCE_FUNC(vkGetPhysicalDeviceSurfaceSupportKHR)
	LOAD_INSTANCE_FUNC(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
	LOAD_INSTANCE_FUNC(vkGetPhysicalDeviceSurfaceFormatsKHR)
	LOAD_INSTANCE_FUNC(vkGetPhysicalDeviceSurfacePresentModesKHR)
	LOAD_INSTANCE_FUNC(vkDestroySurfaceKHR)
	
	#undef LOAD_INSTANCE_FUNC
	
	/* Device Functions */
	vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)
		renderer->vkGetInstanceProcAddr(renderer->instance, "vkGetDeviceProcAddr");
	if (!vkGetDeviceProcAddr) {
		VK_LOG_ERROR("Failed to load vkGetDeviceProcAddr");
		return 0;
	}
	
	#define LOAD_DEVICE_FUNC(fn) \
		renderer->fn = (PFN_##fn)vkGetDeviceProcAddr(renderer->device, #fn); \
		if (!renderer->fn) { VK_LOG_ERROR("Failed to load " #fn); return 0; }
	
	LOAD_DEVICE_FUNC(vkDestroyDevice)
	LOAD_DEVICE_FUNC(vkGetDeviceQueue)
	LOAD_DEVICE_FUNC(vkDeviceWaitIdle)
	LOAD_DEVICE_FUNC(vkQueueSubmit)
	LOAD_DEVICE_FUNC(vkQueueWaitIdle)
	LOAD_DEVICE_FUNC(vkAllocateMemory)
	LOAD_DEVICE_FUNC(vkFreeMemory)
	LOAD_DEVICE_FUNC(vkMapMemory)
	LOAD_DEVICE_FUNC(vkUnmapMemory)
	LOAD_DEVICE_FUNC(vkFlushMappedMemoryRanges)
	LOAD_DEVICE_FUNC(vkBindBufferMemory)
	LOAD_DEVICE_FUNC(vkBindImageMemory)
	LOAD_DEVICE_FUNC(vkGetBufferMemoryRequirements)
	LOAD_DEVICE_FUNC(vkGetImageMemoryRequirements)
	LOAD_DEVICE_FUNC(vkCreateBuffer)
	LOAD_DEVICE_FUNC(vkDestroyBuffer)
	LOAD_DEVICE_FUNC(vkCreateImage)
	LOAD_DEVICE_FUNC(vkDestroyImage)
	LOAD_DEVICE_FUNC(vkCreateImageView)
	LOAD_DEVICE_FUNC(vkDestroyImageView)
	LOAD_DEVICE_FUNC(vkCreateSampler)
	LOAD_DEVICE_FUNC(vkDestroySampler)
	LOAD_DEVICE_FUNC(vkCreateShaderModule)
	LOAD_DEVICE_FUNC(vkDestroyShaderModule)
	LOAD_DEVICE_FUNC(vkCreatePipelineLayout)
	LOAD_DEVICE_FUNC(vkDestroyPipelineLayout)
	LOAD_DEVICE_FUNC(vkCreateGraphicsPipelines)
	LOAD_DEVICE_FUNC(vkDestroyPipeline)
	LOAD_DEVICE_FUNC(vkCreatePipelineCache)
	LOAD_DEVICE_FUNC(vkDestroyPipelineCache)
	LOAD_DEVICE_FUNC(vkCreateDescriptorSetLayout)
	LOAD_DEVICE_FUNC(vkDestroyDescriptorSetLayout)
	LOAD_DEVICE_FUNC(vkCreateDescriptorPool)
	LOAD_DEVICE_FUNC(vkDestroyDescriptorPool)
	LOAD_DEVICE_FUNC(vkAllocateDescriptorSets)
	LOAD_DEVICE_FUNC(vkUpdateDescriptorSets)
	LOAD_DEVICE_FUNC(vkCreateRenderPass)
	LOAD_DEVICE_FUNC(vkDestroyRenderPass)
	LOAD_DEVICE_FUNC(vkCreateFramebuffer)
	LOAD_DEVICE_FUNC(vkDestroyFramebuffer)
	LOAD_DEVICE_FUNC(vkCreateCommandPool)
	LOAD_DEVICE_FUNC(vkDestroyCommandPool)
	LOAD_DEVICE_FUNC(vkResetCommandPool)
	LOAD_DEVICE_FUNC(vkAllocateCommandBuffers)
	LOAD_DEVICE_FUNC(vkFreeCommandBuffers)
	LOAD_DEVICE_FUNC(vkBeginCommandBuffer)
	LOAD_DEVICE_FUNC(vkEndCommandBuffer)
	LOAD_DEVICE_FUNC(vkCmdBeginRenderPass)
	LOAD_DEVICE_FUNC(vkCmdEndRenderPass)
	LOAD_DEVICE_FUNC(vkCmdBindPipeline)
	LOAD_DEVICE_FUNC(vkCmdBindVertexBuffers)
	LOAD_DEVICE_FUNC(vkCmdBindIndexBuffer)
	LOAD_DEVICE_FUNC(vkCmdBindDescriptorSets)
	LOAD_DEVICE_FUNC(vkCmdSetViewport)
	LOAD_DEVICE_FUNC(vkCmdSetScissor)
	LOAD_DEVICE_FUNC(vkCmdSetBlendConstants)
	LOAD_DEVICE_FUNC(vkCmdSetStencilReference)
	LOAD_DEVICE_FUNC(vkCmdDraw)
	LOAD_DEVICE_FUNC(vkCmdDrawIndexed)
	LOAD_DEVICE_FUNC(vkCmdCopyBuffer)
	LOAD_DEVICE_FUNC(vkCmdCopyBufferToImage)
	LOAD_DEVICE_FUNC(vkCmdCopyImageToBuffer)
	LOAD_DEVICE_FUNC(vkCmdCopyImage)
	LOAD_DEVICE_FUNC(vkCmdBlitImage)
	LOAD_DEVICE_FUNC(vkCmdPipelineBarrier)
	LOAD_DEVICE_FUNC(vkCmdClearColorImage)
	LOAD_DEVICE_FUNC(vkCmdClearDepthStencilImage)
	LOAD_DEVICE_FUNC(vkCmdClearAttachments)
	LOAD_DEVICE_FUNC(vkCreateQueryPool)
	LOAD_DEVICE_FUNC(vkDestroyQueryPool)
	LOAD_DEVICE_FUNC(vkCmdBeginQuery)
	LOAD_DEVICE_FUNC(vkCmdEndQuery)
	LOAD_DEVICE_FUNC(vkGetQueryPoolResults)
	LOAD_DEVICE_FUNC(vkCmdResetQueryPool)
	LOAD_DEVICE_FUNC(vkCreateFence)
	LOAD_DEVICE_FUNC(vkDestroyFence)
	LOAD_DEVICE_FUNC(vkWaitForFences)
	LOAD_DEVICE_FUNC(vkResetFences)
	LOAD_DEVICE_FUNC(vkCreateSemaphore)
	LOAD_DEVICE_FUNC(vkDestroySemaphore)
	LOAD_DEVICE_FUNC(vkCreateSwapchainKHR)
	LOAD_DEVICE_FUNC(vkDestroySwapchainKHR)
	LOAD_DEVICE_FUNC(vkGetSwapchainImagesKHR)
	LOAD_DEVICE_FUNC(vkAcquireNextImageKHR)
	LOAD_DEVICE_FUNC(vkQueuePresentKHR)
	
	#undef LOAD_DEVICE_FUNC
	
	return 1;
}

/* Create Vulkan Instance */
static uint8_t VULKAN_INTERNAL_CreateInstance(VulkanRenderer *renderer, uint8_t debugMode)
{
	PFN_vkCreateInstance vkCreateInstance;
	VkResult result;
	uint32_t extensionCount = 0;
	const char **extensions;
	
	VkApplicationInfo appInfo = {0};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "FNA3D";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "FNA3D";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_1;
	
	if (!SDL_Vulkan_GetInstanceExtensions(renderer->window, &extensionCount, NULL)) {
		VK_LOG_ERROR("SDL_Vulkan_GetInstanceExtensions failed: %s", SDL_GetError());
		return 0;
	}
	
	extensions = (const char **)SDL_malloc(sizeof(const char*) * (extensionCount + 1));
	if (!SDL_Vulkan_GetInstanceExtensions(renderer->window, &extensionCount, extensions)) {
		SDL_free((void*)extensions);
		VK_LOG_ERROR("SDL_Vulkan_GetInstanceExtensions failed: %s", SDL_GetError());
		return 0;
	}
	
	VkInstanceCreateInfo createInfo = {0};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;
	createInfo.enabledExtensionCount = extensionCount;
	createInfo.ppEnabledExtensionNames = extensions;
	
	if (debugMode) {
		const char *layers[] = { "VK_LAYER_KHRONOS_validation" };
		createInfo.enabledLayerCount = 1;
		createInfo.ppEnabledLayerNames = layers;
	}
	
	vkCreateInstance = (PFN_vkCreateInstance)
		renderer->vkGetInstanceProcAddr(NULL, "vkCreateInstance");
	if (!vkCreateInstance) {
		SDL_free((void*)extensions);
		VK_LOG_ERROR("Failed to load vkCreateInstance");
		return 0;
	}
	
	result = vkCreateInstance(&createInfo, NULL, &renderer->instance);
	SDL_free((void*)extensions);
	
	if (result != VK_SUCCESS) {
		VK_LOG_ERROR("vkCreateInstance failed: %d", result);
		return 0;
	}
	
	VK_LOG_INFO("Vulkan instance created");
	return 1;
}

/* Select Physical Device */
static uint8_t VULKAN_SelectPhysicalDevice(VulkanRenderer *renderer)
{
	uint32_t deviceCount = 0;
	VkPhysicalDevice *devices;
	uint32_t i;
	VkResult result;
	
	result = renderer->vkEnumeratePhysicalDevices(renderer->instance, &deviceCount, NULL);
	if (result != VK_SUCCESS || deviceCount == 0) {
		VK_LOG_ERROR("No Vulkan devices found");
		return 0;
	}
	
	devices = (VkPhysicalDevice*)SDL_malloc(sizeof(VkPhysicalDevice) * deviceCount);
	result = renderer->vkEnumeratePhysicalDevices(renderer->instance, &deviceCount, devices);
	if (result != VK_SUCCESS) {
		SDL_free(devices);
		return 0;
	}
	
	/* Select first discrete GPU, or first device */
	renderer->physicalDevice = devices[0];
	for (i = 0; i < deviceCount; i++) {
		VkPhysicalDeviceProperties props;
		renderer->vkGetPhysicalDeviceProperties(devices[i], &props);
		if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			renderer->physicalDevice = devices[i];
			break;
		}
	}
	
	SDL_free(devices);
	
	/* Get device properties */
	renderer->vkGetPhysicalDeviceProperties(renderer->physicalDevice, &renderer->deviceProperties);
	renderer->vkGetPhysicalDeviceFeatures(renderer->physicalDevice, &renderer->deviceFeatures);
	renderer->vkGetPhysicalDeviceMemoryProperties(renderer->physicalDevice, &renderer->memoryProperties);
	
	VK_LOG_INFO("Selected GPU: %s", renderer->deviceProperties.deviceName);
	return 1;
}

/* Find Queue Families */
static uint8_t VULKAN_FindQueueFamilies(VulkanRenderer *renderer)
{
	uint32_t queueFamilyCount = 0;
	VkQueueFamilyProperties *queueFamilies;
	uint32_t i;
	VkBool32 presentSupport;
	
	renderer->graphicsQueueFamilyIndex = UINT32_MAX;
	renderer->presentQueueFamilyIndex = UINT32_MAX;
	
	renderer->vkGetPhysicalDeviceQueueFamilyProperties(
		renderer->physicalDevice, &queueFamilyCount, NULL);
	
	queueFamilies = (VkQueueFamilyProperties*)
		SDL_malloc(sizeof(VkQueueFamilyProperties) * queueFamilyCount);
	renderer->vkGetPhysicalDeviceQueueFamilyProperties(
		renderer->physicalDevice, &queueFamilyCount, queueFamilies);
	
	for (i = 0; i < queueFamilyCount; i++) {
		if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			renderer->graphicsQueueFamilyIndex = i;
		}
		
		renderer->vkGetPhysicalDeviceSurfaceSupportKHR(
			renderer->physicalDevice, i, renderer->surface, &presentSupport);
		if (presentSupport) {
			renderer->presentQueueFamilyIndex = i;
		}
		
		if (renderer->graphicsQueueFamilyIndex != UINT32_MAX &&
		    renderer->presentQueueFamilyIndex != UINT32_MAX) {
			break;
		}
	}
	
	SDL_free(queueFamilies);
	
	if (renderer->graphicsQueueFamilyIndex == UINT32_MAX) {
		VK_LOG_ERROR("No graphics queue family found");
		return 0;
	}
	
	return 1;
}

/* Create Logical Device */
static uint8_t VULKAN_INTERNAL_CreateDevice(VulkanRenderer *renderer)
{
	VkResult result;
	float queuePriority = 1.0f;
	uint32_t queueCreateInfoCount = 1;
	VkDeviceQueueCreateInfo queueCreateInfos[2] = {0};
	
	const char *deviceExtensions[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};
	
	queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfos[0].queueFamilyIndex = renderer->graphicsQueueFamilyIndex;
	queueCreateInfos[0].queueCount = 1;
	queueCreateInfos[0].pQueuePriorities = &queuePriority;
	
	if (renderer->presentQueueFamilyIndex != renderer->graphicsQueueFamilyIndex) {
		queueCreateInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfos[1].queueFamilyIndex = renderer->presentQueueFamilyIndex;
		queueCreateInfos[1].queueCount = 1;
		queueCreateInfos[1].pQueuePriorities = &queuePriority;
		queueCreateInfoCount = 2;
	}
	
	VkPhysicalDeviceFeatures deviceFeatures = {0};
	deviceFeatures.samplerAnisotropy = VK_TRUE;
	deviceFeatures.fillModeNonSolid = VK_TRUE;
	deviceFeatures.depthClamp = VK_TRUE;
	
	VkDeviceCreateInfo createInfo = {0};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = queueCreateInfoCount;
	createInfo.pQueueCreateInfos = queueCreateInfos;
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = 1;
	createInfo.ppEnabledExtensionNames = deviceExtensions;
	
	result = renderer->vkCreateDevice(
		renderer->physicalDevice, &createInfo, NULL, &renderer->device);
	if (result != VK_SUCCESS) {
		VK_LOG_ERROR("vkCreateDevice failed: %d", result);
		return 0;
	}
	
	VK_LOG_INFO("Vulkan device created");
	return 1;
}

/* Create Swapchain */
static uint8_t VULKAN_CreateSwapchain(VulkanRenderer *renderer, uint32_t width, uint32_t height)
{
	VkResult result;
	VkSurfaceCapabilitiesKHR caps;
	VkSurfaceFormatKHR *formats;
	VkPresentModeKHR *presentModes;
	uint32_t formatCount, presentModeCount, imageCount, i;
	VkSurfaceFormatKHR surfaceFormat = { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
	
	renderer->vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		renderer->physicalDevice, renderer->surface, &caps);
	
	/* Get formats */
	renderer->vkGetPhysicalDeviceSurfaceFormatsKHR(
		renderer->physicalDevice, renderer->surface, &formatCount, NULL);
	formats = (VkSurfaceFormatKHR*)SDL_malloc(sizeof(VkSurfaceFormatKHR) * formatCount);
	renderer->vkGetPhysicalDeviceSurfaceFormatsKHR(
		renderer->physicalDevice, renderer->surface, &formatCount, formats);
	
	for (i = 0; i < formatCount; i++) {
		if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
		    formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			surfaceFormat = formats[i];
			break;
		}
	}
	SDL_free(formats);
	
	/* Get present modes */
	renderer->vkGetPhysicalDeviceSurfacePresentModesKHR(
		renderer->physicalDevice, renderer->surface, &presentModeCount, NULL);
	presentModes = (VkPresentModeKHR*)SDL_malloc(sizeof(VkPresentModeKHR) * presentModeCount);
	renderer->vkGetPhysicalDeviceSurfacePresentModesKHR(
		renderer->physicalDevice, renderer->surface, &presentModeCount, presentModes);
	
	for (i = 0; i < presentModeCount; i++) {
		if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
			break;
		}
	}
	SDL_free(presentModes);
	
	/* Clamp extent */
	VkExtent2D extent = { width, height };
	if (caps.currentExtent.width != UINT32_MAX) {
		extent = caps.currentExtent;
	} else {
		if (extent.width < caps.minImageExtent.width) extent.width = caps.minImageExtent.width;
		if (extent.width > caps.maxImageExtent.width) extent.width = caps.maxImageExtent.width;
		if (extent.height < caps.minImageExtent.height) extent.height = caps.minImageExtent.height;
		if (extent.height > caps.maxImageExtent.height) extent.height = caps.maxImageExtent.height;
	}
	
	imageCount = caps.minImageCount + 1;
	if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
		imageCount = caps.maxImageCount;
	}
	
	VkSwapchainCreateInfoKHR createInfo = {0};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = renderer->surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.preTransform = caps.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = renderer->swapchain.swapchain;
	
	result = renderer->vkCreateSwapchainKHR(
		renderer->device, &createInfo, NULL, &renderer->swapchain.swapchain);
	if (result != VK_SUCCESS) {
		VK_LOG_ERROR("vkCreateSwapchainKHR failed: %d", result);
		return 0;
	}
	
	renderer->swapchain.format = surfaceFormat.format;
	renderer->swapchain.colorSpace = surfaceFormat.colorSpace;
	renderer->swapchain.extent = extent;
	
	/* Get swapchain images */
	renderer->vkGetSwapchainImagesKHR(
		renderer->device, renderer->swapchain.swapchain, &renderer->swapchain.imageCount, NULL);
	renderer->swapchain.images = (VkImage*)SDL_malloc(sizeof(VkImage) * renderer->swapchain.imageCount);
	renderer->vkGetSwapchainImagesKHR(
		renderer->device, renderer->swapchain.swapchain,
		&renderer->swapchain.imageCount, renderer->swapchain.images);
	
	/* Create image views */
	renderer->swapchain.imageViews = (VkImageView*)SDL_malloc(sizeof(VkImageView) * renderer->swapchain.imageCount);
	for (i = 0; i < renderer->swapchain.imageCount; i++) {
		VkImageViewCreateInfo viewInfo = {0};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = renderer->swapchain.images[i];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = renderer->swapchain.format;
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;
		
		result = renderer->vkCreateImageView(
			renderer->device, &viewInfo, NULL, &renderer->swapchain.imageViews[i]);
		if (result != VK_SUCCESS) {
			VK_LOG_ERROR("vkCreateImageView failed: %d", result);
			return 0;
		}
	}
	
	VK_LOG_INFO("Swapchain created: %dx%d, %d images",
		extent.width, extent.height, renderer->swapchain.imageCount);
	return 1;
}

/* Create Frame Resources */
static uint8_t VULKAN_CreateFrameResources(VulkanRenderer *renderer)
{
	uint32_t i;
	VkResult result;
	
	VkCommandPoolCreateInfo poolInfo = {0};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = renderer->graphicsQueueFamilyIndex;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	
	VkFenceCreateInfo fenceInfo = {0};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	
	VkSemaphoreCreateInfo semaphoreInfo = {0};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	
	for (i = 0; i < VULKAN_MAX_FRAMES_IN_FLIGHT; i++) {
		VulkanFrameData *frame = &renderer->frames[i];
		
		/* Command Pool */
		result = renderer->vkCreateCommandPool(
			renderer->device, &poolInfo, NULL, &frame->commandPool);
		VK_CHECK_RET(result, 0);
		
		/* Command Buffer */
		VkCommandBufferAllocateInfo allocInfo = {0};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = frame->commandPool;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;
		
		result = renderer->vkAllocateCommandBuffers(
			renderer->device, &allocInfo, &frame->commandBuffer);
		VK_CHECK_RET(result, 0);
		
		/* Fence */
		result = renderer->vkCreateFence(
			renderer->device, &fenceInfo, NULL, &frame->fence);
		VK_CHECK_RET(result, 0);
		
		/* Semaphores */
		result = renderer->vkCreateSemaphore(
			renderer->device, &semaphoreInfo, NULL, &frame->imageAvailable);
		VK_CHECK_RET(result, 0);
		
		result = renderer->vkCreateSemaphore(
			renderer->device, &semaphoreInfo, NULL, &frame->renderFinished);
		VK_CHECK_RET(result, 0);
	}
	
	VK_LOG_INFO("Frame resources created");
	return 1;
}

/* Forward declarations for driver functions */
static void VULKAN_DestroyDevice(FNA3D_Device *device);
static void VULKAN_SwapBuffers(FNA3D_Renderer *driverData, FNA3D_Rect *src, FNA3D_Rect *dst, void* overrideWindowHandle);
static void VULKAN_Clear(FNA3D_Renderer *driverData, FNA3D_ClearOptions options, FNA3D_Vec4 *color, float depth, int32_t stencil);
static void VULKAN_DrawIndexedPrimitives(FNA3D_Renderer *driverData, FNA3D_PrimitiveType primitiveType, int32_t baseVertex, int32_t minVertexIndex, int32_t numVertices, int32_t startIndex, int32_t primitiveCount, FNA3D_Buffer *indices, FNA3D_IndexElementSize indexElementSize);
static void VULKAN_DrawInstancedPrimitives(FNA3D_Renderer *driverData, FNA3D_PrimitiveType primitiveType, int32_t baseVertex, int32_t minVertexIndex, int32_t numVertices, int32_t startIndex, int32_t primitiveCount, int32_t instanceCount, FNA3D_Buffer *indices, FNA3D_IndexElementSize indexElementSize);
static void VULKAN_DrawPrimitives(FNA3D_Renderer *driverData, FNA3D_PrimitiveType primitiveType, int32_t vertexStart, int32_t primitiveCount);

/* Implementation continues in next section... */
#include "FNA3D_Driver_Vulkan_Impl.h"

/* Driver Registration */
static uint8_t VULKAN_PrepareWindowAttributes(uint32_t *flags)
{
	if (SDL_Vulkan_LoadLibrary(NULL) < 0) {
		VK_LOG_ERROR("Failed to load Vulkan library: %s", SDL_GetError());
		return 0;
	}
	
	*flags = SDL_WINDOW_VULKAN;
	VK_LOG_INFO("Vulkan driver prepared");
	return 1;
}

static FNA3D_Device* VULKAN_CreateDevice(
	FNA3D_PresentationParameters *presentationParameters,
	uint8_t debugMode
) {
	FNA3D_Device *device;
	VulkanRenderer *renderer;
	
	VK_LOG_INFO("Creating Vulkan device...");
	
	/* Allocate device */
	device = (FNA3D_Device*)SDL_malloc(sizeof(FNA3D_Device));
	SDL_memset(device, 0, sizeof(FNA3D_Device));
	
	/* Allocate renderer */
	renderer = (VulkanRenderer*)SDL_malloc(sizeof(VulkanRenderer));
	SDL_memset(renderer, 0, sizeof(VulkanRenderer));
	
	renderer->debugMode = debugMode;
	renderer->backbufferWidth = presentationParameters->backBufferWidth;
	renderer->backbufferHeight = presentationParameters->backBufferHeight;
	renderer->window = (SDL_Window*)presentationParameters->deviceWindowHandle;
	
	/* Load vkGetInstanceProcAddr */
	renderer->vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)
		SDL_Vulkan_GetVkGetInstanceProcAddr();
	if (!renderer->vkGetInstanceProcAddr) {
		VK_LOG_ERROR("Failed to get vkGetInstanceProcAddr");
		goto cleanup;
	}
	
	/* Create instance */
	if (!VULKAN_INTERNAL_CreateInstance(renderer, debugMode)) {
		goto cleanup;
	}
	
	/* Create surface */
	if (!SDL_Vulkan_CreateSurface(renderer->window, renderer->instance, &renderer->surface)) {
		VK_LOG_ERROR("SDL_Vulkan_CreateSurface failed: %s", SDL_GetError());
		goto cleanup;
	}
	
	/* Load instance functions (needed before device creation) */
	renderer->vkEnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)
		renderer->vkGetInstanceProcAddr(renderer->instance, "vkEnumeratePhysicalDevices");
	renderer->vkGetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)
		renderer->vkGetInstanceProcAddr(renderer->instance, "vkGetPhysicalDeviceProperties");
	renderer->vkGetPhysicalDeviceFeatures = (PFN_vkGetPhysicalDeviceFeatures)
		renderer->vkGetInstanceProcAddr(renderer->instance, "vkGetPhysicalDeviceFeatures");
	renderer->vkGetPhysicalDeviceMemoryProperties = (PFN_vkGetPhysicalDeviceMemoryProperties)
		renderer->vkGetInstanceProcAddr(renderer->instance, "vkGetPhysicalDeviceMemoryProperties");
	renderer->vkGetPhysicalDeviceQueueFamilyProperties = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)
		renderer->vkGetInstanceProcAddr(renderer->instance, "vkGetPhysicalDeviceQueueFamilyProperties");
	renderer->vkCreateDevice = (PFN_vkCreateDevice)
		renderer->vkGetInstanceProcAddr(renderer->instance, "vkCreateDevice");
	renderer->vkGetPhysicalDeviceSurfaceSupportKHR = (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)
		renderer->vkGetInstanceProcAddr(renderer->instance, "vkGetPhysicalDeviceSurfaceSupportKHR");
	renderer->vkGetPhysicalDeviceSurfaceCapabilitiesKHR = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
		renderer->vkGetInstanceProcAddr(renderer->instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
	renderer->vkGetPhysicalDeviceSurfaceFormatsKHR = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)
		renderer->vkGetInstanceProcAddr(renderer->instance, "vkGetPhysicalDeviceSurfaceFormatsKHR");
	renderer->vkGetPhysicalDeviceSurfacePresentModesKHR = (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)
		renderer->vkGetInstanceProcAddr(renderer->instance, "vkGetPhysicalDeviceSurfacePresentModesKHR");
	renderer->vkDestroySurfaceKHR = (PFN_vkDestroySurfaceKHR)
		renderer->vkGetInstanceProcAddr(renderer->instance, "vkDestroySurfaceKHR");
	renderer->vkDestroyInstance = (PFN_vkDestroyInstance)
		renderer->vkGetInstanceProcAddr(renderer->instance, "vkDestroyInstance");
	
	/* Select physical device */
	if (!VULKAN_SelectPhysicalDevice(renderer)) {
		goto cleanup;
	}
	
	/* Find queue families */
	if (!VULKAN_FindQueueFamilies(renderer)) {
		goto cleanup;
	}
	
	/* Create logical device */
	if (!VULKAN_INTERNAL_CreateDevice(renderer)) {
		goto cleanup;
	}
	
	/* Load all device functions */
	if (!VULKAN_LoadFunctions(renderer)) {
		goto cleanup;
	}
	
	/* Get device queues */
	renderer->vkGetDeviceQueue(renderer->device, renderer->graphicsQueueFamilyIndex, 0, &renderer->graphicsQueue);
	renderer->vkGetDeviceQueue(renderer->device, renderer->presentQueueFamilyIndex, 0, &renderer->presentQueue);
	
	/* Create swapchain */
	if (!VULKAN_CreateSwapchain(renderer, renderer->backbufferWidth, renderer->backbufferHeight)) {
		goto cleanup;
	}
	
	/* Create frame resources */
	if (!VULKAN_CreateFrameResources(renderer)) {
		goto cleanup;
	}
	
	/* Create pipeline cache */
	VkPipelineCacheCreateInfo cacheInfo = {0};
	cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	renderer->vkCreatePipelineCache(renderer->device, &cacheInfo, NULL, &renderer->pipelineCache);
	
	/* Assign function pointers - defined in FNA3D_Driver_Vulkan_Impl.h */
	device->driverData = (FNA3D_Renderer*)renderer;
	VULKAN_AssignDeviceFunctions(device);
	
	VK_LOG_INFO("Vulkan device created successfully");
	return device;

cleanup:
	if (renderer->device) {
		renderer->vkDestroyDevice(renderer->device, NULL);
	}
	if (renderer->surface) {
		renderer->vkDestroySurfaceKHR(renderer->instance, renderer->surface, NULL);
	}
	if (renderer->instance) {
		renderer->vkDestroyInstance(renderer->instance, NULL);
	}
	SDL_Vulkan_UnloadLibrary();
	SDL_free(renderer);
	SDL_free(device);
	return NULL;
}

FNA3D_Driver VulkanDriver = {
	"Vulkan",
	VULKAN_PrepareWindowAttributes,
	VULKAN_CreateDevice
};

#else

extern int this_tu_is_empty;

#endif /* FNA3D_DRIVER_VULKAN */
