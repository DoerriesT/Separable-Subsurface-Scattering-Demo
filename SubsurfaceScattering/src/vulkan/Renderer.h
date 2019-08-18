#pragma once
#include "VKContext.h"
#include "SwapChain.h"
#include <glm/mat4x4.hpp>
#include <memory>

namespace sss
{
	namespace vulkan
	{
		class Texture;
		class Mesh;

		class Renderer
		{
		public:
			explicit Renderer(void *windowHandle, uint32_t width, uint32_t height);
			~Renderer();
			void render(const glm::mat4 &viewProjection, const glm::mat4 &shadowMatrix, const glm::vec4 &lightPositionRadius, const glm::vec4 &lightColorInvSqrAttRadius, const glm::vec4 &cameraPosition);

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
			VkImage m_tonemappedImage[FRAMES_IN_FLIGHT];
			VkImageView m_shadowImageView[FRAMES_IN_FLIGHT];
			VkImageView m_depthImageView[FRAMES_IN_FLIGHT];
			VkImageView m_colorImageView[FRAMES_IN_FLIGHT];
			VkImageView m_tonemappedImageView[FRAMES_IN_FLIGHT];
			VkDeviceMemory m_shadowImageMemory[FRAMES_IN_FLIGHT];
			VkDeviceMemory m_depthImageMemory[FRAMES_IN_FLIGHT];
			VkDeviceMemory m_colorImageMemory[FRAMES_IN_FLIGHT];
			VkDeviceMemory m_tonemappedImageMemory[FRAMES_IN_FLIGHT];
			VkBuffer m_constantBuffer[FRAMES_IN_FLIGHT];
			VkDeviceMemory m_constantBufferMemory[FRAMES_IN_FLIGHT];
			uint8_t *m_constantBufferPtr[FRAMES_IN_FLIGHT];
			std::pair<VkPipeline, VkPipelineLayout> m_shadowPipeline;
			std::pair<VkPipeline, VkPipelineLayout> m_lightingPipeline;
			std::pair<VkPipeline, VkPipelineLayout> m_skyboxPipeline;
			std::pair<VkPipeline, VkPipelineLayout> m_tonemapPipeline;
			VkDescriptorPool m_descriptorPool;
			VkDescriptorSetLayout m_textureDescriptorSetLayout;
			VkDescriptorSetLayout m_lightingDescriptorSetLayout;
			VkDescriptorSetLayout m_tonemapDescriptorSetLayout;
			VkDescriptorSet m_textureDescriptorSet;
			VkDescriptorSet m_lightingDescriptorSet[FRAMES_IN_FLIGHT];
			VkDescriptorSet m_tonemapDescriptorSet[FRAMES_IN_FLIGHT];
			VkSampler m_shadowSampler;
			VkSampler m_linearSamplerClamp;
			VkSampler m_linearSamplerRepeat;
			VkSampler m_pointSamplerClamp;
			VkSampler m_pointSamplerRepeat;
			std::shared_ptr<Texture> m_radianceTexture;
			std::shared_ptr<Texture> m_irradianceTexture;
			std::shared_ptr<Texture> m_brdfLUT;
			std::shared_ptr<Texture> m_skyboxTexture;
			std::vector<std::shared_ptr<Texture>> m_textures;
			std::vector<std::vector<std::shared_ptr<Mesh>>> m_meshes;
		};
	}
}