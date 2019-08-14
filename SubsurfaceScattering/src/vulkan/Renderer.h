#pragma once
#include "VKContext.h"
#include "SwapChain.h"
#include <glm/mat4x4.hpp>

namespace sss
{
	namespace vulkan
	{
		class Renderer
		{
		public:
			explicit Renderer(void *windowHandle, uint32_t width, uint32_t height);
			~Renderer();
			void render(const glm::mat4 &viewProjection, const glm::mat4 &shadowMatrix);

		private:
			enum 
			{
				FRAMES_IN_FLIGHT = 2,
				SHADOW_RESOLUTION = 2048,
			};

			uint32_t m_width;
			uint32_t m_height;
			uint64_t m_frameIndex = 0;
			VKContext m_context;
			SwapChain m_swapChain;
			VkSemaphore m_swapChainImageAvailableSemaphores[FRAMES_IN_FLIGHT];
			VkSemaphore m_renderFinishedSemaphores[FRAMES_IN_FLIGHT];
			VkFence m_frameFinishedFence[FRAMES_IN_FLIGHT];
			VkCommandBuffer m_commandBuffers[FRAMES_IN_FLIGHT * 2];
			VkRenderPass m_shadowRenderPass;
			VkRenderPass m_mainRenderPass;
			VkFramebuffer m_mainFramebuffers[FRAMES_IN_FLIGHT];
			VkFramebuffer m_shadowFramebuffers[FRAMES_IN_FLIGHT];
			VkImage m_shadowImage[FRAMES_IN_FLIGHT];
			VkImage m_depthImage[FRAMES_IN_FLIGHT];
			VkImage m_colorImage[FRAMES_IN_FLIGHT];
			VkImageView m_shadowImageView[FRAMES_IN_FLIGHT];
			VkImageView m_depthImageView[FRAMES_IN_FLIGHT];
			VkImageView m_colorImageView[FRAMES_IN_FLIGHT];
			VkDeviceMemory m_shadowImageMemory[FRAMES_IN_FLIGHT];
			VkDeviceMemory m_depthImageMemory[FRAMES_IN_FLIGHT];
			VkDeviceMemory m_colorImageMemory[FRAMES_IN_FLIGHT];
			std::pair<VkPipeline, VkPipelineLayout> m_shadowPipeline;
			std::pair<VkPipeline, VkPipelineLayout> m_lightingPipeline;
			VkDescriptorPool m_descriptorPool;
			VkDescriptorSetLayout m_lightingDescriptorSetLayout;
			VkDescriptorSet m_lightingDescriptorSet[FRAMES_IN_FLIGHT];
			VkSampler m_shadowSampler;
			VkSampler m_linearSamplerClamp;
			VkSampler m_linearSamplerRepeat;
			VkSampler m_pointSamplerClamp;
			VkSampler m_pointSamplerRepeat;
		};
	}
}