/* FNA3D - 3D Graphics Library for FNA
 *
 * Copyright (c) 2020-2024 Ethan Lee
 *
 * This software is provided 'as-is', without any express or implied warranty.
 * In no event will the authors be held liable for any damages arising from
 * the use of this software.
 */

#ifndef FNA3D_DRIVER_VULKAN_H
#define FNA3D_DRIVER_VULKAN_H

#if FNA3D_DRIVER_VULKAN

#include "FNA3D_Driver.h"
#include <vulkan/vulkan.h>
#include <SDL.h>

/* Constants */
#define VULKAN_MAX_FRAMES_IN_FLIGHT 3
#define VULKAN_MAX_VERTEX_ATTRIBUTES 16
#define VULKAN_MAX_TEXTURE_SAMPLERS 16
#define VULKAN_MAX_RENDER_TARGETS 8
#define VULKAN_STAGING_BUFFER_SIZE (8 * 1024 * 1024)

/* Memory Allocation Pool */
typedef struct VulkanMemoryPool {
	VkDeviceMemory memory;
	VkDeviceSize size;
	VkDeviceSize used;
	uint32_t memoryTypeIndex;
	uint8_t *mappedPointer;
	struct VulkanMemoryPool *next;
} VulkanMemoryPool;

/* Buffer */
typedef struct VulkanBuffer {
	VkBuffer buffer;
	VkDeviceMemory memory;
	VkDeviceSize offset;
	VkDeviceSize size;
	uint8_t *mappedPointer;
	uint8_t isDynamic;
	struct VulkanBuffer *next;
} VulkanBuffer;

/* Texture */
typedef struct VulkanTexture {
	VkImage image;
	VkImageView view;
	VkDeviceMemory memory;
	VkFormat format;
	uint32_t width;
	uint32_t height;
	uint32_t depth;
	uint32_t levelCount;
	uint32_t layerCount;
	VkImageLayout layout;
	uint8_t isRenderTarget;
	uint8_t is3D;
	uint8_t isCube;
	struct VulkanTexture *next;
} VulkanTexture;

/* Sampler */
typedef struct VulkanSampler {
	VkSampler sampler;
	struct VulkanSampler *next;
} VulkanSampler;

/* Renderbuffer */
typedef struct VulkanRenderbuffer {
	VkImage image;
	VkImageView view;
	VkDeviceMemory memory;
	VkFormat format;
	uint32_t width;
	uint32_t height;
	uint32_t sampleCount;
	struct VulkanRenderbuffer *next;
} VulkanRenderbuffer;

/* Render Pass */
typedef struct VulkanRenderPass {
	VkRenderPass renderPass;
	uint32_t colorAttachmentCount;
	uint8_t hasDepthStencil;
} VulkanRenderPass;

/* Framebuffer */
typedef struct VulkanFramebuffer {
	VkFramebuffer framebuffer;
	VulkanRenderPass *renderPass;
	uint32_t width;
	uint32_t height;
	struct VulkanFramebuffer *next;
} VulkanFramebuffer;

/* Pipeline */
typedef struct VulkanPipeline {
	VkPipeline pipeline;
	VkPipelineLayout layout;
	struct VulkanPipeline *next;
} VulkanPipeline;

/* Effect (Shader) */
typedef struct VulkanEffect {
	MOJOSHADER_effect *effect;
	VkShaderModule vertexShader;
	VkShaderModule fragmentShader;
	VkDescriptorSetLayout descriptorSetLayout;
	VkPipelineLayout pipelineLayout;
	struct VulkanEffect *next;
} VulkanEffect;

/* Query */
typedef struct VulkanQuery {
	VkQueryPool queryPool;
	uint32_t index;
	uint8_t active;
	struct VulkanQuery *next;
} VulkanQuery;

/* Frame Data (per-frame resources) */
typedef struct VulkanFrameData {
	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;
	VkFence fence;
	VkSemaphore imageAvailable;
	VkSemaphore renderFinished;
	uint8_t submitted;
	
	/* Staging buffer for this frame */
	VulkanBuffer *stagingBuffer;
	VkDeviceSize stagingOffset;
	
	/* Descriptor pool for this frame */
	VkDescriptorPool descriptorPool;
} VulkanFrameData;

/* Swapchain */
typedef struct VulkanSwapchain {
	VkSwapchainKHR swapchain;
	VkFormat format;
	VkColorSpaceKHR colorSpace;
	VkExtent2D extent;
	uint32_t imageCount;
	VkImage *images;
	VkImageView *imageViews;
	VkFramebuffer *framebuffers;
	VulkanRenderPass *renderPass;
	uint32_t currentImageIndex;
} VulkanSwapchain;

/* Main Renderer */
typedef struct VulkanRenderer {
	/* Vulkan Core */
	VkInstance instance;
	VkPhysicalDevice physicalDevice;
	VkDevice device;
	VkQueue graphicsQueue;
	VkQueue presentQueue;
	uint32_t graphicsQueueFamilyIndex;
	uint32_t presentQueueFamilyIndex;
	
	/* Physical Device Properties */
	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceFeatures deviceFeatures;
	VkPhysicalDeviceMemoryProperties memoryProperties;
	
	/* Surface and Swapchain */
	VkSurfaceKHR surface;
	VulkanSwapchain swapchain;
	
	/* Frame Management */
	VulkanFrameData frames[VULKAN_MAX_FRAMES_IN_FLIGHT];
	uint32_t currentFrame;
	uint32_t frameCount;
	
	/* Current State */
	VkCommandBuffer currentCommandBuffer;
	VulkanRenderPass *currentRenderPass;
	VkFramebuffer currentFramebuffer;
	uint8_t renderPassActive;
	
	/* Backbuffer */
	VulkanTexture *backbufferColor;
	VulkanRenderbuffer *backbufferDepthStencil;
	uint32_t backbufferWidth;
	uint32_t backbufferHeight;
	FNA3D_SurfaceFormat backbufferSurfaceFormat;
	FNA3D_DepthFormat backbufferDepthFormat;
	int32_t backbufferMultiSampleCount;
	
	/* Render Target State */
	VulkanTexture *colorAttachments[VULKAN_MAX_RENDER_TARGETS];
	uint32_t colorAttachmentCount;
	VulkanRenderbuffer *depthStencilAttachment;
	
	/* Pipeline State */
	FNA3D_BlendState blendState;
	FNA3D_DepthStencilState depthStencilState;
	FNA3D_RasterizerState rasterizerState;
	FNA3D_Viewport viewport;
	FNA3D_Rect scissorRect;
	FNA3D_Color blendFactor;
	int32_t multiSampleMask;
	int32_t referenceStencil;
	uint8_t pipelineDirty;
	
	/* Vertex State */
	VulkanBuffer *vertexBuffers[VULKAN_MAX_VERTEX_ATTRIBUTES];
	uint32_t vertexBufferOffsets[VULKAN_MAX_VERTEX_ATTRIBUTES];
	uint32_t vertexBufferCount;
	FNA3D_VertexDeclaration vertexDeclaration;
	uint8_t vertexBuffersDirty;
	
	/* Texture State */
	VulkanTexture *textures[VULKAN_MAX_TEXTURE_SAMPLERS];
	VulkanSampler *samplers[VULKAN_MAX_TEXTURE_SAMPLERS];
	VulkanTexture *vertexTextures[4];
	VulkanSampler *vertexSamplers[4];
	
	/* Current Effect */
	VulkanEffect *currentEffect;
	MOJOSHADER_effectTechnique *currentTechnique;
	uint32_t currentPass;
	
	/* Default Resources */
	VulkanSampler *defaultSampler;
	VulkanTexture *defaultTexture;
	VulkanRenderPass *defaultRenderPass;
	
	/* Query Pool */
	VkQueryPool occlusionQueryPool;
	uint32_t queryCount;
	uint32_t maxQueries;
	
	/* Resource Lists */
	VulkanBuffer *bufferList;
	VulkanTexture *textureList;
	VulkanSampler *samplerList;
	VulkanRenderbuffer *renderbufferList;
	VulkanFramebuffer *framebufferList;
	VulkanPipeline *pipelineList;
	VulkanEffect *effectList;
	VulkanQuery *queryList;
	VulkanMemoryPool *memoryPoolList;
	
	/* Window Reference */
	SDL_Window *window;
	
	/* Debug */
	uint8_t debugMode;
	VkDebugUtilsMessengerEXT debugMessenger;
	
	/* Vulkan Function Pointers (Instance) */
	PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
	PFN_vkDestroyInstance vkDestroyInstance;
	PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevices;
	PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDeviceProperties;
	PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeatures;
	PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
	PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties;
	PFN_vkCreateDevice vkCreateDevice;
	PFN_vkGetPhysicalDeviceSurfaceSupportKHR vkGetPhysicalDeviceSurfaceSupportKHR;
	PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
	PFN_vkGetPhysicalDeviceSurfaceFormatsKHR vkGetPhysicalDeviceSurfaceFormatsKHR;
	PFN_vkGetPhysicalDeviceSurfacePresentModesKHR vkGetPhysicalDeviceSurfacePresentModesKHR;
	PFN_vkDestroySurfaceKHR vkDestroySurfaceKHR;
	
	/* Vulkan Function Pointers (Device) */
	PFN_vkDestroyDevice vkDestroyDevice;
	PFN_vkGetDeviceQueue vkGetDeviceQueue;
	PFN_vkDeviceWaitIdle vkDeviceWaitIdle;
	PFN_vkQueueSubmit vkQueueSubmit;
	PFN_vkQueueWaitIdle vkQueueWaitIdle;
	
	/* Memory */
	PFN_vkAllocateMemory vkAllocateMemory;
	PFN_vkFreeMemory vkFreeMemory;
	PFN_vkMapMemory vkMapMemory;
	PFN_vkUnmapMemory vkUnmapMemory;
	PFN_vkFlushMappedMemoryRanges vkFlushMappedMemoryRanges;
	PFN_vkBindBufferMemory vkBindBufferMemory;
	PFN_vkBindImageMemory vkBindImageMemory;
	PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
	PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
	
	/* Buffer */
	PFN_vkCreateBuffer vkCreateBuffer;
	PFN_vkDestroyBuffer vkDestroyBuffer;
	
	/* Image */
	PFN_vkCreateImage vkCreateImage;
	PFN_vkDestroyImage vkDestroyImage;
	PFN_vkCreateImageView vkCreateImageView;
	PFN_vkDestroyImageView vkDestroyImageView;
	
	/* Sampler */
	PFN_vkCreateSampler vkCreateSampler;
	PFN_vkDestroySampler vkDestroySampler;
	
	/* Shader */
	PFN_vkCreateShaderModule vkCreateShaderModule;
	PFN_vkDestroyShaderModule vkDestroyShaderModule;
	
	/* Pipeline */
	PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
	PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
	PFN_vkCreateGraphicsPipelines vkCreateGraphicsPipelines;
	PFN_vkDestroyPipeline vkDestroyPipeline;
	PFN_vkCreatePipelineCache vkCreatePipelineCache;
	PFN_vkDestroyPipelineCache vkDestroyPipelineCache;
	VkPipelineCache pipelineCache;
	
	/* Descriptor */
	PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
	PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;
	PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
	PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
	PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
	PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
	
	/* Render Pass */
	PFN_vkCreateRenderPass vkCreateRenderPass;
	PFN_vkDestroyRenderPass vkDestroyRenderPass;
	PFN_vkCreateFramebuffer vkCreateFramebuffer;
	PFN_vkDestroyFramebuffer vkDestroyFramebuffer;
	
	/* Command */
	PFN_vkCreateCommandPool vkCreateCommandPool;
	PFN_vkDestroyCommandPool vkDestroyCommandPool;
	PFN_vkResetCommandPool vkResetCommandPool;
	PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
	PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
	PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
	PFN_vkEndCommandBuffer vkEndCommandBuffer;
	
	/* Command Buffer Commands */
	PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass;
	PFN_vkCmdEndRenderPass vkCmdEndRenderPass;
	PFN_vkCmdBindPipeline vkCmdBindPipeline;
	PFN_vkCmdBindVertexBuffers vkCmdBindVertexBuffers;
	PFN_vkCmdBindIndexBuffer vkCmdBindIndexBuffer;
	PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
	PFN_vkCmdSetViewport vkCmdSetViewport;
	PFN_vkCmdSetScissor vkCmdSetScissor;
	PFN_vkCmdSetBlendConstants vkCmdSetBlendConstants;
	PFN_vkCmdSetStencilReference vkCmdSetStencilReference;
	PFN_vkCmdDraw vkCmdDraw;
	PFN_vkCmdDrawIndexed vkCmdDrawIndexed;
	PFN_vkCmdDrawIndexedIndirect vkCmdDrawIndexedIndirect;
	PFN_vkCmdCopyBuffer vkCmdCopyBuffer;
	PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImage;
	PFN_vkCmdCopyImageToBuffer vkCmdCopyImageToBuffer;
	PFN_vkCmdCopyImage vkCmdCopyImage;
	PFN_vkCmdBlitImage vkCmdBlitImage;
	PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
	PFN_vkCmdClearColorImage vkCmdClearColorImage;
	PFN_vkCmdClearDepthStencilImage vkCmdClearDepthStencilImage;
	PFN_vkCmdClearAttachments vkCmdClearAttachments;
	
	/* Query */
	PFN_vkCreateQueryPool vkCreateQueryPool;
	PFN_vkDestroyQueryPool vkDestroyQueryPool;
	PFN_vkCmdBeginQuery vkCmdBeginQuery;
	PFN_vkCmdEndQuery vkCmdEndQuery;
	PFN_vkGetQueryPoolResults vkGetQueryPoolResults;
	PFN_vkCmdResetQueryPool vkCmdResetQueryPool;
	
	/* Sync */
	PFN_vkCreateFence vkCreateFence;
	PFN_vkDestroyFence vkDestroyFence;
	PFN_vkWaitForFences vkWaitForFences;
	PFN_vkResetFences vkResetFences;
	PFN_vkCreateSemaphore vkCreateSemaphore;
	PFN_vkDestroySemaphore vkDestroySemaphore;
	
	/* Swapchain */
	PFN_vkCreateSwapchainKHR vkCreateSwapchainKHR;
	PFN_vkDestroySwapchainKHR vkDestroySwapchainKHR;
	PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR;
	PFN_vkAcquireNextImageKHR vkAcquireNextImageKHR;
	PFN_vkQueuePresentKHR vkQueuePresentKHR;
	
} VulkanRenderer;

/* Helper Functions */
uint32_t VULKAN_INTERNAL_FindMemoryType(
	VulkanRenderer *renderer,
	uint32_t typeFilter,
	VkMemoryPropertyFlags properties
);

VkFormat VULKAN_INTERNAL_GetVkFormat(FNA3D_SurfaceFormat format);
VkFormat VULKAN_INTERNAL_GetVkDepthFormat(FNA3D_DepthFormat format);

#endif /* FNA3D_DRIVER_VULKAN */

#endif /* FNA3D_DRIVER_VULKAN_H */

