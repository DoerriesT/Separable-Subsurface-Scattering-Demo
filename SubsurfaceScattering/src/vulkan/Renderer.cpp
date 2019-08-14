#include "Renderer.h"
#include "utility/Utility.h"
#include "VKUtility.h"
#include "vulkan/Mesh.h"
#include "pipelines/ShadowPipeline.h"
#include "pipelines/LightingPipeline.h"

std::vector<std::unique_ptr<sss::vulkan::Mesh>> meshes;

sss::vulkan::Renderer::Renderer(void *windowHandle, uint32_t width, uint32_t height)
	:m_width(width),
	m_height(height),
	m_context(windowHandle),
	m_swapChain(m_context, m_width, m_height)
{
	meshes = vulkan::Mesh::load(m_context.getPhysicalDevice(), m_context.getDevice(), m_context.getGraphicsQueue(), m_context.getGraphicsCommandPool(), "resources/meshes/head.obj");

	// create images and views
	{
		for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
		{
			// shadow
			{
				if (vkutil::create2dImage(m_context.getPhysicalDevice(), 
					m_context.getDevice(), 
					VK_FORMAT_D32_SFLOAT,
					SHADOW_RESOLUTION, 
					SHADOW_RESOLUTION, 
					VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
					0, 
					m_shadowImage[i], 
					m_shadowImageMemory[i]) != VK_SUCCESS)
				{
					util::fatalExit("Failed to create image!", EXIT_FAILURE);
				}

				VkImageViewCreateInfo viewCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
				viewCreateInfo.image = m_shadowImage[i];
				viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				viewCreateInfo.format = VK_FORMAT_D32_SFLOAT;
				viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};

				if (vkCreateImageView(m_context.getDevice(), &viewCreateInfo, nullptr, &m_shadowImageView[i]) != VK_SUCCESS)
				{
					util::fatalExit("Failed to create image view!", EXIT_FAILURE);
				}
			}
			
			// color
			{
				if (vkutil::create2dImage(m_context.getPhysicalDevice(),
					m_context.getDevice(),
					VK_FORMAT_R16G16B16A16_SFLOAT,
					m_width,
					m_height,
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					0,
					m_colorImage[i],
					m_colorImageMemory[i]) != VK_SUCCESS)
				{
					util::fatalExit("Failed to create image!", EXIT_FAILURE);
				}

				VkImageViewCreateInfo viewCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
				viewCreateInfo.image = m_colorImage[i];
				viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				viewCreateInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
				viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

				if (vkCreateImageView(m_context.getDevice(), &viewCreateInfo, nullptr, &m_colorImageView[i]) != VK_SUCCESS)
				{
					util::fatalExit("Failed to create image view!", EXIT_FAILURE);
				}
			}

			// depth
			{
				if (vkutil::create2dImage(m_context.getPhysicalDevice(),
					m_context.getDevice(),
					VK_FORMAT_D32_SFLOAT_S8_UINT,
					m_width,
					m_height,
					VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					0,
					m_depthImage[i],
					m_depthImageMemory[i]) != VK_SUCCESS)
				{
					util::fatalExit("Failed to create image!", EXIT_FAILURE);
				}

				VkImageViewCreateInfo viewCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
				viewCreateInfo.image = m_depthImage[i];
				viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				viewCreateInfo.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
				viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 };

				if (vkCreateImageView(m_context.getDevice(), &viewCreateInfo, nullptr, &m_depthImageView[i]) != VK_SUCCESS)
				{
					util::fatalExit("Failed to create image view!", EXIT_FAILURE);
				}
			}
		}
	}

	// create shadow renderpass
	{
		VkAttachmentDescription attachmentDescription{};
		attachmentDescription.format = VK_FORMAT_D32_SFLOAT;
		attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
		attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkAttachmentReference depthAttachmentRef{ 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

		// shadow subpass
		VkSubpassDescription subpassDescription{};
		{
			subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpassDescription.pDepthStencilAttachment = &depthAttachmentRef;
		}

		// create renderpass
		{
			VkRenderPassCreateInfo renderPassInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
			renderPassInfo.attachmentCount = 1;
			renderPassInfo.pAttachments = &attachmentDescription;
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpassDescription;

			if (vkCreateRenderPass(m_context.getDevice(), &renderPassInfo, nullptr, &m_shadowRenderPass) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create render pass!", EXIT_FAILURE);
			}
		}


		// create framebuffers
		{
			for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
			{
				VkFramebufferCreateInfo framebufferCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
				framebufferCreateInfo.renderPass = m_shadowRenderPass;
				framebufferCreateInfo.attachmentCount = 1;
				framebufferCreateInfo.pAttachments = &m_shadowImageView[i];
				framebufferCreateInfo.width = SHADOW_RESOLUTION;
				framebufferCreateInfo.height = SHADOW_RESOLUTION;
				framebufferCreateInfo.layers = 1;

				if (vkCreateFramebuffer(m_context.getDevice(), &framebufferCreateInfo, nullptr, &m_shadowFramebuffers[i]) != VK_SUCCESS)
				{
					util::fatalExit("Failed to create framebuffer!", EXIT_FAILURE);
				}
			}
		}
	}

	// create main renderpass
	{
		VkAttachmentDescription attachmentDescriptions[2] = {};
		{
			// color
			attachmentDescriptions[0].format = VK_FORMAT_R16G16B16A16_SFLOAT;
			attachmentDescriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
			attachmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachmentDescriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

			// depth
			attachmentDescriptions[1].format = VK_FORMAT_D32_SFLOAT_S8_UINT;
			attachmentDescriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
			attachmentDescriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachmentDescriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescriptions[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		VkAttachmentReference colorAttachmentRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
		VkAttachmentReference depthAttachmentRef{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

		// lighting subpass
		VkSubpassDescription lightingSubpassDescription{};
		{
			lightingSubpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			lightingSubpassDescription.colorAttachmentCount = 1;
			lightingSubpassDescription.pColorAttachments = &colorAttachmentRef;
			lightingSubpassDescription.pDepthStencilAttachment = &depthAttachmentRef;
		}

		// create renderpass
		{
			VkSubpassDependency dependencies[2] = {};

			// shadow map generation -> shadow map sampling
			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dependencies[0].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dependencies[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			dependencies[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			// lighting -> blit to backbuffer
			dependencies[1].srcSubpass = 0;
			dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[1].dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			VkRenderPassCreateInfo renderPassInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
			renderPassInfo.attachmentCount = static_cast<uint32_t>(sizeof(attachmentDescriptions) / sizeof(attachmentDescriptions[0]));
			renderPassInfo.pAttachments = attachmentDescriptions;
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &lightingSubpassDescription;
			renderPassInfo.dependencyCount = 2;
			renderPassInfo.pDependencies = dependencies;

			if (vkCreateRenderPass(m_context.getDevice(), &renderPassInfo, nullptr, &m_mainRenderPass) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create render pass!", EXIT_FAILURE);
			}
		}


		// create framebuffers
		{
			for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
			{
				VkImageView framebufferAttachments[2];
				framebufferAttachments[0] = m_colorImageView[i];
				framebufferAttachments[1] = m_depthImageView[i];

				VkFramebufferCreateInfo framebufferCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
				framebufferCreateInfo.renderPass = m_mainRenderPass;
				framebufferCreateInfo.attachmentCount = 2;
				framebufferCreateInfo.pAttachments = framebufferAttachments;
				framebufferCreateInfo.width = m_width;
				framebufferCreateInfo.height = m_height;
				framebufferCreateInfo.layers = 1;

				if (vkCreateFramebuffer(m_context.getDevice(), &framebufferCreateInfo, nullptr, &m_mainFramebuffers[i]) != VK_SUCCESS)
				{
					util::fatalExit("Failed to create framebuffer!", EXIT_FAILURE);
				}
			}
		}
	}

	// allocate command buffers
	{
		VkCommandBufferAllocateInfo cmdBufAllocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		cmdBufAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdBufAllocInfo.commandBufferCount = FRAMES_IN_FLIGHT * 2;
		cmdBufAllocInfo.commandPool = m_context.getGraphicsCommandPool();

		if (vkAllocateCommandBuffers(m_context.getDevice(), &cmdBufAllocInfo, m_commandBuffers) != VK_SUCCESS)
		{
			util::fatalExit("Failed to allocate command buffers!", EXIT_FAILURE);
		}
	}

	// create sync primitives
	{
		VkSemaphoreCreateInfo semaphoreCreateInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };

		VkFenceCreateInfo fenceCreateInfo{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
		{
			if (vkCreateSemaphore(m_context.getDevice(), &semaphoreCreateInfo, nullptr, &m_swapChainImageAvailableSemaphores[i]) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create semaphore!", EXIT_FAILURE);
			}

			if (vkCreateSemaphore(m_context.getDevice(), &semaphoreCreateInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create semaphore!", EXIT_FAILURE);
			}

			if (vkCreateFence(m_context.getDevice(), &fenceCreateInfo, nullptr, &m_frameFinishedFence[i]) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create fence!", EXIT_FAILURE);
			}
		}
	}

	// create samplers
	{
		// shadow sampler
		{
			VkSamplerCreateInfo samplerCreateInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
			samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
			samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
			samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCreateInfo.mipLodBias = 0.0f;
			samplerCreateInfo.anisotropyEnable = VK_FALSE;
			samplerCreateInfo.maxAnisotropy = 1.0f;
			samplerCreateInfo.compareEnable = VK_TRUE;
			samplerCreateInfo.compareOp = VK_COMPARE_OP_GREATER;
			samplerCreateInfo.minLod = 0.0f;
			samplerCreateInfo.maxLod = 1;
			samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
			samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

			if (vkCreateSampler(m_context.getDevice(), &samplerCreateInfo, nullptr, &m_shadowSampler) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create sampler!", EXIT_FAILURE);
			}
		}

		// linear sampler
		{
			VkSamplerCreateInfo samplerCreateInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
			samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
			samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
			samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			samplerCreateInfo.mipLodBias = 0.0f;
			samplerCreateInfo.anisotropyEnable = VK_FALSE;
			samplerCreateInfo.maxAnisotropy = 1.0f;
			samplerCreateInfo.compareEnable = VK_FALSE;
			samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
			samplerCreateInfo.minLod = 0.0f;
			samplerCreateInfo.maxLod = 1;
			samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
			samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

			// clamp
			samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

			if (vkCreateSampler(m_context.getDevice(), &samplerCreateInfo, nullptr, &m_linearSamplerClamp) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create sampler!", EXIT_FAILURE);
			}

			// repeat
			samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

			if (vkCreateSampler(m_context.getDevice(), &samplerCreateInfo, nullptr, &m_linearSamplerRepeat) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create sampler!", EXIT_FAILURE);
			}
		}

		// point sampler
		{
			VkSamplerCreateInfo samplerCreateInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
			samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
			samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
			samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
			samplerCreateInfo.mipLodBias = 0.0f;
			samplerCreateInfo.anisotropyEnable = VK_FALSE;
			samplerCreateInfo.maxAnisotropy = 1.0f;
			samplerCreateInfo.compareEnable = VK_FALSE;
			samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
			samplerCreateInfo.minLod = 0.0f;
			samplerCreateInfo.maxLod = 1;
			samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
			samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

			// clamp
			samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

			if (vkCreateSampler(m_context.getDevice(), &samplerCreateInfo, nullptr, &m_pointSamplerClamp) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create sampler!", EXIT_FAILURE);
			}

			// repeat
			samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

			if (vkCreateSampler(m_context.getDevice(), &samplerCreateInfo, nullptr, &m_pointSamplerRepeat) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create sampler!", EXIT_FAILURE);
			}
		}
	}

	// create descriptor sets
	{
		VkDescriptorPoolSize poolSizes[] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, FRAMES_IN_FLIGHT };

		VkDescriptorPoolCreateInfo poolCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		poolCreateInfo.maxSets = FRAMES_IN_FLIGHT;
		poolCreateInfo.poolSizeCount = 1;
		poolCreateInfo.pPoolSizes = poolSizes;

		if (vkCreateDescriptorPool(m_context.getDevice(), &poolCreateInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
		{
			util::fatalExit("Failed to create descriptor pool!", EXIT_FAILURE);
		}

		VkDescriptorSetLayoutBinding bindings[] =
		{
			{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &m_shadowSampler },
		};

		VkDescriptorSetLayoutCreateInfo layoutCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		layoutCreateInfo.bindingCount = 1;
		layoutCreateInfo.pBindings = bindings;

		if (vkCreateDescriptorSetLayout(m_context.getDevice(), &layoutCreateInfo, nullptr, &m_lightingDescriptorSetLayout) != VK_SUCCESS)
		{
			util::fatalExit("Failed to create descriptor set layout!", EXIT_FAILURE);
		}

		VkDescriptorSetLayout setLayouts[FRAMES_IN_FLIGHT];
		for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
		{
			setLayouts[i] = m_lightingDescriptorSetLayout;
		}

		VkDescriptorSetAllocateInfo setAllocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		setAllocInfo.descriptorPool = m_descriptorPool;
		setAllocInfo.descriptorSetCount = FRAMES_IN_FLIGHT;
		setAllocInfo.pSetLayouts = setLayouts;


		if (vkAllocateDescriptorSets(m_context.getDevice(), &setAllocInfo, m_lightingDescriptorSet) != VK_SUCCESS)
		{
			util::fatalExit("Failed to allocate descriptor sets!", EXIT_FAILURE);
		}

		VkDescriptorImageInfo shadowImageInfos[FRAMES_IN_FLIGHT];
		VkWriteDescriptorSet descriptorWrites[FRAMES_IN_FLIGHT];

		for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
		{
			auto &imageInfo = shadowImageInfos[i];
			imageInfo.sampler = nullptr;
			imageInfo.imageView = m_shadowImageView[i];
			imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			auto &write = descriptorWrites[i];
			write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			write.dstSet = m_lightingDescriptorSet[i];
			write.dstBinding = 0;
			write.descriptorCount = 1;
			write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			write.pImageInfo = &imageInfo;
		}


		vkUpdateDescriptorSets(m_context.getDevice(), FRAMES_IN_FLIGHT, descriptorWrites, 0, nullptr);
	}

	m_shadowPipeline = ShadowPipeline::create(m_context.getDevice(), m_shadowRenderPass, 0, 0, nullptr);
	m_lightingPipeline = LightingPipeline::create(m_context.getDevice(), m_mainRenderPass, 0, 1, &m_lightingDescriptorSetLayout);
}

sss::vulkan::Renderer::~Renderer()
{
	VkDevice device = m_context.getDevice();

	vkDeviceWaitIdle(device);

	for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
	{
		// destroy images and views
		{
			// shadow
			vkDestroyImage(device, m_shadowImage[i], nullptr);
			vkDestroyImageView(device, m_shadowImageView[i], nullptr);
			vkFreeMemory(device, m_shadowImageMemory[i], nullptr);

			// color
			vkDestroyImage(device, m_colorImage[i], nullptr);
			vkDestroyImageView(device, m_colorImageView[i], nullptr);
			vkFreeMemory(device, m_colorImageMemory[i], nullptr);

			// depth
			vkDestroyImage(device, m_depthImage[i], nullptr);
			vkDestroyImageView(device, m_depthImageView[i], nullptr);
			vkFreeMemory(device, m_depthImageMemory[i], nullptr);
		}

		// destroy framebuffers
		vkDestroyFramebuffer(device, m_shadowFramebuffers[i], nullptr);
		vkDestroyFramebuffer(device, m_mainFramebuffers[i], nullptr);

		// free sync primitives
		vkDestroySemaphore(device, m_swapChainImageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(device, m_renderFinishedSemaphores[i], nullptr);
		vkDestroyFence(device, m_frameFinishedFence[i], nullptr);
	}

	// destroy renderpasses
	vkDestroyRenderPass(device, m_shadowRenderPass, nullptr);
	vkDestroyRenderPass(device, m_mainRenderPass, nullptr);

	// free command buffers
	vkFreeCommandBuffers(device, m_context.getGraphicsCommandPool(), FRAMES_IN_FLIGHT * 2, m_commandBuffers);

	// destroy pipelines
	vkDestroyPipeline(device, m_lightingPipeline.first, nullptr);
	vkDestroyPipelineLayout(device, m_lightingPipeline.second, nullptr);

}

void sss::vulkan::Renderer::render(const glm::mat4 &viewProjection, const glm::mat4 &shadowMatrix)
{
	uint32_t resourceIndex = m_frameIndex % FRAMES_IN_FLIGHT;

	// wait until gpu finished work on all per frame resources
	vkWaitForFences(m_context.getDevice(), 1, &m_frameFinishedFence[resourceIndex], VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(m_context.getDevice(), 1, &m_frameFinishedFence[resourceIndex]);

	// command buffer for the first half of the frame...
	vkResetCommandBuffer(m_commandBuffers[resourceIndex * 2], 0);
	// ... and for the second half
	vkResetCommandBuffer(m_commandBuffers[resourceIndex * 2 + 1], 0);

	VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VkCommandBuffer curCmdBuf = m_commandBuffers[resourceIndex * 2];

	// swapchain image independent part of the frame
	vkBeginCommandBuffer(curCmdBuf, &beginInfo);
	{
		// shadow renderpass
		{
			VkClearValue clearValue;
			clearValue.depthStencil.depth = 1.0f;
			clearValue.depthStencil.stencil = 0;

			VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
			renderPassInfo.renderPass = m_shadowRenderPass;
			renderPassInfo.framebuffer = m_shadowFramebuffers[resourceIndex];
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = { SHADOW_RESOLUTION, SHADOW_RESOLUTION };
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearValue;

			vkCmdBeginRenderPass(curCmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			// actual shadow rendering
			{
				vkCmdBindPipeline(curCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline.first);

				VkViewport viewport;
				viewport.x = 0.0f;
				viewport.y = 0.0f;
				viewport.width = static_cast<float>(SHADOW_RESOLUTION);
				viewport.height = static_cast<float>(SHADOW_RESOLUTION);
				viewport.minDepth = 0.0f;
				viewport.maxDepth = 1.0f;

				VkRect2D scissor;
				scissor.offset = { 0, 0 };
				scissor.extent = { SHADOW_RESOLUTION, SHADOW_RESOLUTION };

				vkCmdSetViewport(curCmdBuf, 0, 1, &viewport);
				vkCmdSetScissor(curCmdBuf, 0, 1, &scissor);

				using namespace glm;
				struct PushConsts
				{
					mat4 viewProjectionMatrix;
				};

				PushConsts pushConsts;
				pushConsts.viewProjectionMatrix = shadowMatrix;

				for (const auto &mesh : meshes)
				{
					vkCmdBindIndexBuffer(curCmdBuf, mesh->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT16);

					VkBuffer vertexBuffer = mesh->getVertexBuffer();
					VkDeviceSize vertexBufferOffset = 0;

					vkCmdBindVertexBuffers(curCmdBuf, 0, 1, &vertexBuffer, &vertexBufferOffset);

					vkCmdPushConstants(curCmdBuf, m_shadowPipeline.second, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConsts), &pushConsts);

					vkCmdDrawIndexed(curCmdBuf, mesh->getIndexCount(), 1, 0, 0, 0);
				}
			}

			vkCmdEndRenderPass(curCmdBuf);
		}

		// main renderpass
		{
			VkClearValue clearValues[2];
			// first element of clearValues intentionally left empty, as the color attachment is not cleared and this element is thus ignored
			clearValues[0].color.float32[0] = 0.0f;
			clearValues[0].color.float32[1] = 0.0f;
			clearValues[0].color.float32[2] = 0.0f;
			clearValues[0].color.float32[3] = 1.0f;

			// depth/stencil clear values:
			clearValues[1].depthStencil.depth = 1.0f;
			clearValues[1].depthStencil.stencil = 0;

			VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
			renderPassInfo.renderPass = m_mainRenderPass;
			renderPassInfo.framebuffer = m_mainFramebuffers[resourceIndex];
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = { m_width, m_height };
			renderPassInfo.clearValueCount = 2;
			renderPassInfo.pClearValues = clearValues;

			vkCmdBeginRenderPass(curCmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			// actual lighting
			{
				vkCmdBindPipeline(curCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lightingPipeline.first);

				vkCmdBindDescriptorSets(curCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lightingPipeline.second, 0, 1, &m_lightingDescriptorSet[resourceIndex], 0, nullptr);

				VkViewport viewport;
				viewport.x = 0.0f;
				viewport.y = 0.0f;
				viewport.width = static_cast<float>(m_width);
				viewport.height = static_cast<float>(m_height);
				viewport.minDepth = 0.0f;
				viewport.maxDepth = 1.0f;

				VkRect2D scissor;
				scissor.offset = { 0, 0 };
				scissor.extent = { m_width, m_height };

				vkCmdSetViewport(curCmdBuf, 0, 1, &viewport);
				vkCmdSetScissor(curCmdBuf, 0, 1, &scissor);

				using namespace glm;
				struct PushConsts
				{
					mat4 viewProjectionMatrix;
					mat4 shadowMatrix;
				};

				PushConsts pushConsts;
				pushConsts.viewProjectionMatrix = viewProjection;
				pushConsts.shadowMatrix = shadowMatrix;

				for (const auto &mesh : meshes)
				{
					vkCmdBindIndexBuffer(curCmdBuf, mesh->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT16);

					VkBuffer vertexBuffer = mesh->getVertexBuffer();
					uint32_t vertexCount = mesh->getVertexCount();
					VkBuffer vertexBuffers[] = { vertexBuffer, vertexBuffer, vertexBuffer };
					VkDeviceSize vertexBufferOffsets[] = { 0, vertexCount * sizeof(float) * 3, vertexCount * sizeof(float) * 6 };

					vkCmdBindVertexBuffers(curCmdBuf, 0, 3, vertexBuffers, vertexBufferOffsets);

					vkCmdPushConstants(curCmdBuf, m_lightingPipeline.second, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConsts), &pushConsts);

					vkCmdDrawIndexed(curCmdBuf, mesh->getIndexCount(), 1, 0, 0, 0);
				}
			}

			vkCmdEndRenderPass(curCmdBuf);
		}
	}
	vkEndCommandBuffer(curCmdBuf);

	// submit swapchain image independent work to queue
	{
		VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &curCmdBuf;

		if (vkQueueSubmit(m_context.getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
		{
			util::fatalExit("Failed to submit to queue!", EXIT_FAILURE);
		}
	}

	// acquire swapchain image
	uint32_t swapChainImageIndex = 0;
	{
		VkResult result = vkAcquireNextImageKHR(m_context.getDevice(), m_swapChain, std::numeric_limits<uint64_t>::max(), m_swapChainImageAvailableSemaphores[resourceIndex], VK_NULL_HANDLE, &swapChainImageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			m_swapChain.recreate(m_width, m_height);
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			util::fatalExit("Failed to acquire swap chain image!", EXIT_FAILURE);
		}
	}


	curCmdBuf = m_commandBuffers[resourceIndex * 2 + 1];

	// swapchain image dependent part of the frame
	vkBeginCommandBuffer(curCmdBuf, &beginInfo);
	{
		// transition backbuffer image layout to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
		{
			VkImageMemoryBarrier imageBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			imageBarrier.srcAccessMask = 0;
			imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageBarrier.image = m_swapChain.getImage(swapChainImageIndex);
			imageBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

			vkCmdPipelineBarrier(curCmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
		}
		
		// blit color image to backbuffer
		{
			VkImageBlit region{};
			region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			region.srcOffsets[1] = { static_cast<int32_t>(m_width), static_cast<int32_t>(m_height), 1 };
			region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			region.dstOffsets[1] = { static_cast<int32_t>(m_width), static_cast<int32_t>(m_height), 1 };

			vkCmdBlitImage(curCmdBuf, m_colorImage[resourceIndex], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_swapChain.getImage(swapChainImageIndex), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_NEAREST);
		}
		
		// transition backbuffer image layout to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		{
			VkImageMemoryBarrier imageBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			imageBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageBarrier.dstAccessMask = 0;
			imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageBarrier.image = m_swapChain.getImage(swapChainImageIndex);
			imageBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

			vkCmdPipelineBarrier(curCmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
		}
	}
	vkEndCommandBuffer(curCmdBuf);

	// submit swapchain image dependent work to queue
	{
		VkPipelineStageFlags waitStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;

		VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &m_swapChainImageAvailableSemaphores[resourceIndex];
		submitInfo.pWaitDstStageMask = &waitStageFlags;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &curCmdBuf;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &m_renderFinishedSemaphores[resourceIndex];

		if (vkQueueSubmit(m_context.getGraphicsQueue(), 1, &submitInfo, m_frameFinishedFence[resourceIndex]) != VK_SUCCESS)
		{
			util::fatalExit("Failed to submit to queue!", EXIT_FAILURE);
		}
	}

	// present swapchain image
	{
		VkSwapchainKHR swapChain = m_swapChain;

		VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &m_renderFinishedSemaphores[resourceIndex];
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapChain;
		presentInfo.pImageIndices = &swapChainImageIndex;

		if (vkQueuePresentKHR(m_context.getGraphicsQueue(), &presentInfo) != VK_SUCCESS)
		{
			util::fatalExit("Failed to present!", EXIT_FAILURE);
		}
	}

	++m_frameIndex;
}
