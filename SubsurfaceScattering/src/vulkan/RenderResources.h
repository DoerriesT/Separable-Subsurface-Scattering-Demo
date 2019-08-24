#pragma once
#include <memory>
#include <vector>
#include "volk.h"
#include "Image.h"
#include "Buffer.h"

namespace sss
{
	namespace vulkan
	{
		enum
		{
			FRAMES_IN_FLIGHT = 2,
			SHADOW_RESOLUTION = 2048,
		};

		class SwapChain;

		struct RenderResources
		{
		public:
			
			VkPhysicalDevice m_physicalDevice;
			VkDevice m_device;
			VkCommandPool m_commandPool;
			SwapChain *m_swapChain;
			VkSemaphore m_swapChainImageAvailableSemaphores[FRAMES_IN_FLIGHT];
			VkSemaphore m_renderFinishedSemaphores[FRAMES_IN_FLIGHT];
			VkFence m_frameFinishedFence[FRAMES_IN_FLIGHT];
			VkCommandBuffer m_commandBuffers[FRAMES_IN_FLIGHT * 2];
			VkRenderPass m_shadowRenderPass;
			VkRenderPass m_mainRenderPass;
			VkRenderPass m_guiRenderPass;
			VkFramebuffer m_shadowFramebuffers[FRAMES_IN_FLIGHT];
			VkFramebuffer m_mainFramebuffers[FRAMES_IN_FLIGHT];
			std::vector<VkFramebuffer> m_guiFramebuffers;
			std::unique_ptr<Image> m_shadowImage[FRAMES_IN_FLIGHT];
			std::unique_ptr<Image> m_depthStencilImage[FRAMES_IN_FLIGHT];
			std::unique_ptr<Image> m_colorImage[FRAMES_IN_FLIGHT];
			std::unique_ptr<Image> m_diffuse0Image[FRAMES_IN_FLIGHT];
			std::unique_ptr<Image> m_diffuse1Image[FRAMES_IN_FLIGHT];
			std::unique_ptr<Image> m_tonemappedImage[FRAMES_IN_FLIGHT];
			std::unique_ptr<Buffer> m_constantBuffer[FRAMES_IN_FLIGHT];
			VkImageView m_depthImageView[FRAMES_IN_FLIGHT];
			std::pair<VkPipeline, VkPipelineLayout> m_shadowPipeline;
			std::pair<VkPipeline, VkPipelineLayout> m_lightingPipeline;
			std::pair<VkPipeline, VkPipelineLayout> m_sssLightingPipeline;
			std::pair<VkPipeline, VkPipelineLayout> m_skyboxPipeline;
			std::pair<VkPipeline, VkPipelineLayout> m_sssBlurPipeline0;
			std::pair<VkPipeline, VkPipelineLayout> m_sssBlurPipeline1;
			std::pair<VkPipeline, VkPipelineLayout> m_posprocessingPipeline;
			VkDescriptorPool m_descriptorPool;
			VkDescriptorSetLayout m_textureDescriptorSetLayout;
			VkDescriptorSetLayout m_lightingDescriptorSetLayout;
			VkDescriptorSetLayout m_sssBlurDescriptorSetLayout;
			VkDescriptorSetLayout m_postprocessingDescriptorSetLayout;
			VkDescriptorSet m_textureDescriptorSet;
			VkDescriptorSet m_lightingDescriptorSet[FRAMES_IN_FLIGHT];
			VkDescriptorSet m_sssBlurDescriptorSet[FRAMES_IN_FLIGHT * 2]; // 2 blur passes
			VkDescriptorSet m_postprocessingDescriptorSet[FRAMES_IN_FLIGHT];
			VkSampler m_shadowSampler;
			VkSampler m_linearSamplerClamp;
			VkSampler m_linearSamplerRepeat;
			VkSampler m_pointSamplerClamp;
			VkSampler m_pointSamplerRepeat;
			VkQueryPool m_queryPool;

			explicit RenderResources(VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool cmdPool, uint32_t width, uint32_t height, SwapChain *swapChain);
			RenderResources(const RenderResources &) = delete;
			RenderResources(const RenderResources &&) = delete;
			RenderResources &operator= (const RenderResources &) = delete;
			RenderResources &operator= (const RenderResources &&) = delete;
			~RenderResources();
			void resize(uint32_t width, uint32_t height);

		private:
			void createResizableResources(uint32_t width, uint32_t height);
			void destroyResizeableResources();
		};
	}
}