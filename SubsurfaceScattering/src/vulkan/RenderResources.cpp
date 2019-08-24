#include "RenderResources.h"
#include <glm/vec4.hpp>
#include "pipelines/ShadowPipeline.h"
#include "pipelines/LightingPipeline.h"
#include "pipelines/SkyboxPipeline.h"
#include "pipelines/SSSBlurPipeline.h"
#include "pipelines/PostprocessingPipeline.h"
#include "utility/Utility.h"
#include "SwapChain.h"
#include "VKUtility.h"

sss::vulkan::RenderResources::RenderResources(VkPhysicalDevice physicalDevice, VkDevice device, VkCommandPool cmdPool, uint32_t width, uint32_t height, SwapChain *swapChain)
	:m_physicalDevice(physicalDevice),
	m_device(device),
	m_commandPool(cmdPool),
	m_swapChain(swapChain)
{
	// create images and views and buffers
	{
		for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
		{
			// constant buffer
			{
				VkBufferCreateInfo createInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
				createInfo.size = sizeof(glm::vec4) * 11;
				createInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
				createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

				m_constantBuffer[i] = std::make_unique<Buffer>(m_physicalDevice, m_device, createInfo, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			}

			// shadow
			{
				VkImageCreateInfo imageCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
				imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
				imageCreateInfo.format = VK_FORMAT_D32_SFLOAT;
				imageCreateInfo.extent.width = SHADOW_RESOLUTION;
				imageCreateInfo.extent.height = SHADOW_RESOLUTION;
				imageCreateInfo.extent.depth = 1;
				imageCreateInfo.mipLevels = 1;
				imageCreateInfo.arrayLayers = 1;
				imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
				imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
				imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
				imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
				imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

				m_shadowImage[i] = std::make_unique<Image>(m_physicalDevice, m_device, imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					0, VK_IMAGE_VIEW_TYPE_2D, VkImageSubresourceRange{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 });
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
		subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpassDescription.pDepthStencilAttachment = &depthAttachmentRef;

		// create renderpass
		{
			VkRenderPassCreateInfo renderPassInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
			renderPassInfo.attachmentCount = 1;
			renderPassInfo.pAttachments = &attachmentDescription;
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpassDescription;

			if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_shadowRenderPass) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create render pass!", EXIT_FAILURE);
			}
		}
	}

	// create main renderpass
	{
		VkAttachmentDescription attachmentDescriptions[3] = {};
		{
			// depth
			attachmentDescriptions[0].format = VK_FORMAT_D32_SFLOAT_S8_UINT;
			attachmentDescriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
			attachmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachmentDescriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			// color
			attachmentDescriptions[1].format = VK_FORMAT_R16G16B16A16_SFLOAT;
			attachmentDescriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
			attachmentDescriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachmentDescriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescriptions[1].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			// diffuse0
			attachmentDescriptions[2].format = VK_FORMAT_R16G16B16A16_SFLOAT;
			attachmentDescriptions[2].samples = VK_SAMPLE_COUNT_1_BIT;
			attachmentDescriptions[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachmentDescriptions[2].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachmentDescriptions[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachmentDescriptions[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachmentDescriptions[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescriptions[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}

		VkAttachmentReference depthAttachmentRef{ 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
		VkAttachmentReference colorAttachmentRef{ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
		VkAttachmentReference diffuse0AttachmentRef{ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		VkAttachmentReference sssLightingPassAttachmentRefs[] = { colorAttachmentRef, diffuse0AttachmentRef };

		VkSubpassDescription subpasses[3]{};

		// lighting subpass
		subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpasses[0].colorAttachmentCount = 1;
		subpasses[0].pColorAttachments = &colorAttachmentRef;
		subpasses[0].pDepthStencilAttachment = &depthAttachmentRef;

		// sss lighting subpass
		subpasses[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpasses[1].colorAttachmentCount = 2;
		subpasses[1].pColorAttachments = sssLightingPassAttachmentRefs;
		subpasses[1].pDepthStencilAttachment = &depthAttachmentRef;

		// skybox subpass
		subpasses[2].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpasses[2].colorAttachmentCount = 1;
		subpasses[2].pColorAttachments = &colorAttachmentRef;
		subpasses[2].pDepthStencilAttachment = &depthAttachmentRef;

		// create renderpass
		{
			VkSubpassDependency dependencies[5]{};

			// shadow map generation -> shadow map sampling (lighting pass)
			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dependencies[0].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dependencies[0].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			dependencies[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			// lighting pass -> sss lighting pass
			dependencies[1].srcSubpass = 0;
			dependencies[1].dstSubpass = 1;
			dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			dependencies[1].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			// sss lighting pass -> skybox
			dependencies[2].srcSubpass = 1;
			dependencies[2].dstSubpass = 2;
			dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dependencies[2].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			dependencies[2].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
			dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			// sss lighting pass -> sss blur
			dependencies[3].srcSubpass = 1;
			dependencies[3].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[3].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dependencies[3].dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			dependencies[3].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			dependencies[3].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			// skybox pass -> post processing
			dependencies[4].srcSubpass = 2;
			dependencies[4].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[4].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			dependencies[4].dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			dependencies[4].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
			dependencies[4].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			dependencies[4].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			VkRenderPassCreateInfo renderPassInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
			renderPassInfo.attachmentCount = static_cast<uint32_t>(sizeof(attachmentDescriptions) / sizeof(attachmentDescriptions[0]));
			renderPassInfo.pAttachments = attachmentDescriptions;
			renderPassInfo.subpassCount = static_cast<uint32_t>(sizeof(subpasses) / sizeof(subpasses[0]));
			renderPassInfo.pSubpasses = subpasses;
			renderPassInfo.dependencyCount = static_cast<uint32_t>(sizeof(dependencies) / sizeof(dependencies[0]));
			renderPassInfo.pDependencies = dependencies;

			if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_mainRenderPass) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create render pass!", EXIT_FAILURE);
			}
		}
	}

	// create gui renderpass
	{
		VkAttachmentDescription attachmentDescription{};
		attachmentDescription.format = m_swapChain->getImageFormat();
		attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
		attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference attachmentRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &attachmentRef;


		// create renderpass
		{
			VkSubpassDependency dependencies[1]{};

			// blit -> gui
			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			VkRenderPassCreateInfo renderPassInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
			renderPassInfo.attachmentCount = 1;
			renderPassInfo.pAttachments = &attachmentDescription;
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;
			renderPassInfo.dependencyCount = static_cast<uint32_t>(sizeof(dependencies) / sizeof(dependencies[0]));
			renderPassInfo.pDependencies = dependencies;

			if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_guiRenderPass) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create render pass!", EXIT_FAILURE);
			}
		}
	}

	// allocate command buffers
	{
		VkCommandBufferAllocateInfo cmdBufAllocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		cmdBufAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdBufAllocInfo.commandBufferCount = FRAMES_IN_FLIGHT * 2;
		cmdBufAllocInfo.commandPool = m_commandPool;

		if (vkAllocateCommandBuffers(m_device, &cmdBufAllocInfo, m_commandBuffers) != VK_SUCCESS)
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
			if (vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_swapChainImageAvailableSemaphores[i]) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create semaphore!", EXIT_FAILURE);
			}

			if (vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create semaphore!", EXIT_FAILURE);
			}

			if (vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_frameFinishedFence[i]) != VK_SUCCESS)
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
			samplerCreateInfo.maxLod = 14.0f;
			samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
			samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

			if (vkCreateSampler(m_device, &samplerCreateInfo, nullptr, &m_shadowSampler) != VK_SUCCESS)
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
			samplerCreateInfo.maxLod = 14.0f;
			samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
			samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

			// clamp
			samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

			if (vkCreateSampler(m_device, &samplerCreateInfo, nullptr, &m_linearSamplerClamp) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create sampler!", EXIT_FAILURE);
			}

			// repeat
			samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

			if (vkCreateSampler(m_device, &samplerCreateInfo, nullptr, &m_linearSamplerRepeat) != VK_SUCCESS)
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
			samplerCreateInfo.maxLod = 14.0f;
			samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
			samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

			// clamp
			samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
			samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

			if (vkCreateSampler(m_device, &samplerCreateInfo, nullptr, &m_pointSamplerClamp) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create sampler!", EXIT_FAILURE);
			}

			// repeat
			samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
			samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

			if (vkCreateSampler(m_device, &samplerCreateInfo, nullptr, &m_pointSamplerRepeat) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create sampler!", EXIT_FAILURE);
			}
		}
	}

	// create descriptor sets
	{
		const size_t textureCount = 10;
		VkDescriptorPoolSize poolSizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, FRAMES_IN_FLIGHT },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, FRAMES_IN_FLIGHT * (1 /*shadow maps*/ + 4 /*depth and diffuse for 2 sss blur passes*/ + 4/* postprocessing input*/) + (textureCount + 4 /*cubemaps*/) + 1 /*imgui*/ },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, FRAMES_IN_FLIGHT * 3 /* 2 sss blur passes + 1 postprocessing pass*/ }
		};

		VkDescriptorPoolCreateInfo poolCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		poolCreateInfo.maxSets = FRAMES_IN_FLIGHT * 4 + 2;
		poolCreateInfo.poolSizeCount = 3;
		poolCreateInfo.pPoolSizes = poolSizes;

		if (vkCreateDescriptorPool(m_device, &poolCreateInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
		{
			util::fatalExit("Failed to create descriptor pool!", EXIT_FAILURE);
		}

		// texture set
		{
			VkSampler immutableTexSamplers[textureCount];
			for (size_t i = 0; i < textureCount; ++i)
			{
				immutableTexSamplers[i] = m_linearSamplerClamp;
			}

			VkDescriptorSetLayoutBinding bindings[] =
			{
				{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, textureCount, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
				{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &m_linearSamplerClamp },
				{ 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &m_linearSamplerClamp },
				{ 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &m_linearSamplerClamp },
				{ 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &m_linearSamplerClamp },
			};

			VkDescriptorSetLayoutCreateInfo layoutCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layoutCreateInfo.bindingCount = static_cast<uint32_t>(sizeof(bindings) / sizeof(bindings[0]));
			layoutCreateInfo.pBindings = bindings;

			if (vkCreateDescriptorSetLayout(m_device, &layoutCreateInfo, nullptr, &m_textureDescriptorSetLayout) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create descriptor set layout!", EXIT_FAILURE);
			}

			VkDescriptorSetAllocateInfo setAllocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
			setAllocInfo.descriptorPool = m_descriptorPool;
			setAllocInfo.descriptorSetCount = 1;
			setAllocInfo.pSetLayouts = &m_textureDescriptorSetLayout;

			if (vkAllocateDescriptorSets(m_device, &setAllocInfo, &m_textureDescriptorSet) != VK_SUCCESS)
			{
				util::fatalExit("Failed to allocate descriptor set!", EXIT_FAILURE);
			}
		}

		// lighting set
		{
			VkDescriptorSetLayoutBinding bindings[] =
			{
				{ 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
				{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, &m_shadowSampler },
			};

			VkDescriptorSetLayoutCreateInfo layoutCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layoutCreateInfo.bindingCount = static_cast<uint32_t>(sizeof(bindings) / sizeof(bindings[0]));
			layoutCreateInfo.pBindings = bindings;

			if (vkCreateDescriptorSetLayout(m_device, &layoutCreateInfo, nullptr, &m_lightingDescriptorSetLayout) != VK_SUCCESS)
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


			if (vkAllocateDescriptorSets(m_device, &setAllocInfo, m_lightingDescriptorSet) != VK_SUCCESS)
			{
				util::fatalExit("Failed to allocate descriptor sets!", EXIT_FAILURE);
			}
		}

		// sss blur sets
		{
			VkDescriptorSetLayoutBinding bindings[] =
			{
				{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, &m_linearSamplerClamp },
				{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, &m_pointSamplerClamp },
				{ 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
			};

			VkDescriptorSetLayoutCreateInfo layoutCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layoutCreateInfo.bindingCount = static_cast<uint32_t>(sizeof(bindings) / sizeof(bindings[0]));
			layoutCreateInfo.pBindings = bindings;

			if (vkCreateDescriptorSetLayout(m_device, &layoutCreateInfo, nullptr, &m_sssBlurDescriptorSetLayout) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create descriptor set layout!", EXIT_FAILURE);
			}

			VkDescriptorSetLayout setLayouts[FRAMES_IN_FLIGHT * 2];
			for (size_t i = 0; i < FRAMES_IN_FLIGHT * 2; ++i)
			{
				setLayouts[i] = m_sssBlurDescriptorSetLayout;
			}

			VkDescriptorSetAllocateInfo setAllocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
			setAllocInfo.descriptorPool = m_descriptorPool;
			setAllocInfo.descriptorSetCount = FRAMES_IN_FLIGHT * 2;
			setAllocInfo.pSetLayouts = setLayouts;

			if (vkAllocateDescriptorSets(m_device, &setAllocInfo, m_sssBlurDescriptorSet) != VK_SUCCESS)
			{
				util::fatalExit("Failed to allocate descriptor sets!", EXIT_FAILURE);
			}
		}

		// postprocessing set
		{
			VkDescriptorSetLayoutBinding bindings[] =
			{
				{ 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
				{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, &m_pointSamplerClamp },
				{ 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, &m_pointSamplerClamp },
				{ 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, &m_pointSamplerClamp },
				{ 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, &m_linearSamplerClamp },
			};

			VkDescriptorSetLayoutCreateInfo layoutCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layoutCreateInfo.bindingCount = static_cast<uint32_t>(sizeof(bindings) / sizeof(bindings[0]));
			layoutCreateInfo.pBindings = bindings;

			if (vkCreateDescriptorSetLayout(m_device, &layoutCreateInfo, nullptr, &m_postprocessingDescriptorSetLayout) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create descriptor set layout!", EXIT_FAILURE);
			}

			VkDescriptorSetLayout setLayouts[FRAMES_IN_FLIGHT * 2];
			for (size_t i = 0; i < FRAMES_IN_FLIGHT * 2; ++i)
			{
				setLayouts[i] = m_postprocessingDescriptorSetLayout;
			}

			VkDescriptorSetAllocateInfo setAllocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
			setAllocInfo.descriptorPool = m_descriptorPool;
			setAllocInfo.descriptorSetCount = FRAMES_IN_FLIGHT;
			setAllocInfo.pSetLayouts = setLayouts;

			if (vkAllocateDescriptorSets(m_device, &setAllocInfo, m_postprocessingDescriptorSet) != VK_SUCCESS)
			{
				util::fatalExit("Failed to allocate descriptor sets!", EXIT_FAILURE);
			}
		}
	}

	VkQueryPoolCreateInfo queryPoolCreateInfo = { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
	queryPoolCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
	queryPoolCreateInfo.queryCount = FRAMES_IN_FLIGHT * 2;

	if (vkCreateQueryPool(m_device, &queryPoolCreateInfo, nullptr, &m_queryPool) != VK_SUCCESS)
	{
		util::fatalExit("Failed to create query pool!", EXIT_FAILURE);
	}

	VkDescriptorSetLayout lightingDescriptorSetLayouts[] = { m_textureDescriptorSetLayout, m_lightingDescriptorSetLayout };

	m_shadowPipeline = ShadowPipeline::create(m_device, m_shadowRenderPass, 0, 0, nullptr);
	m_lightingPipeline = LightingPipeline::create(m_device, m_mainRenderPass, 0, 2, lightingDescriptorSetLayouts, false);
	m_sssLightingPipeline = LightingPipeline::create(m_device, m_mainRenderPass, 1, 2, lightingDescriptorSetLayouts, true);
	m_skyboxPipeline = SkyboxPipeline::create(m_device, m_mainRenderPass, 2, 1, &m_textureDescriptorSetLayout);
	m_sssBlurPipeline0 = SSSBlurPipeline::create(m_device, 1, &m_sssBlurDescriptorSetLayout);
	m_sssBlurPipeline1 = SSSBlurPipeline::create(m_device, 1, &m_sssBlurDescriptorSetLayout);
	m_posprocessingPipeline = PostprocessingPipeline::create(m_device, 1, &m_postprocessingDescriptorSetLayout);

	createResizableResources(width, height);
}

sss::vulkan::RenderResources::~RenderResources()
{
	vkDeviceWaitIdle(m_device);

	destroyResizeableResources();

	vkDestroyPipeline(m_device, m_shadowPipeline.first, nullptr);
	vkDestroyPipelineLayout(m_device, m_shadowPipeline.second, nullptr);
	vkDestroyPipeline(m_device, m_lightingPipeline.first, nullptr);
	vkDestroyPipelineLayout(m_device, m_lightingPipeline.second, nullptr);
	vkDestroyPipeline(m_device, m_sssLightingPipeline.first, nullptr);
	vkDestroyPipelineLayout(m_device, m_sssLightingPipeline.second, nullptr);
	vkDestroyPipeline(m_device, m_skyboxPipeline.first, nullptr);
	vkDestroyPipelineLayout(m_device, m_skyboxPipeline.second, nullptr);
	vkDestroyPipeline(m_device, m_sssBlurPipeline0.first, nullptr);
	vkDestroyPipelineLayout(m_device, m_sssBlurPipeline0.second, nullptr);
	vkDestroyPipeline(m_device, m_sssBlurPipeline1.first, nullptr);
	vkDestroyPipelineLayout(m_device, m_sssBlurPipeline1.second, nullptr);
	vkDestroyPipeline(m_device, m_posprocessingPipeline.first, nullptr);
	vkDestroyPipelineLayout(m_device, m_posprocessingPipeline.second, nullptr);

	vkDestroyQueryPool(m_device, m_queryPool, nullptr);

	vkDestroyDescriptorSetLayout(m_device, m_textureDescriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_lightingDescriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_sssBlurDescriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_postprocessingDescriptorSetLayout, nullptr);
	vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

	vkDestroySampler(m_device, m_shadowSampler, nullptr);
	vkDestroySampler(m_device, m_linearSamplerClamp, nullptr);
	vkDestroySampler(m_device, m_linearSamplerRepeat, nullptr);
	vkDestroySampler(m_device, m_pointSamplerClamp, nullptr);
	vkDestroySampler(m_device, m_pointSamplerRepeat, nullptr);

	for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
	{
		vkDestroySemaphore(m_device, m_swapChainImageAvailableSemaphores[i], nullptr);
		vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
		vkDestroyFence(m_device, m_frameFinishedFence[i], nullptr);
	}

	vkDestroyRenderPass(m_device, m_shadowRenderPass, nullptr);
	vkDestroyRenderPass(m_device, m_mainRenderPass, nullptr);
	vkDestroyRenderPass(m_device, m_guiRenderPass, nullptr);
}

void sss::vulkan::RenderResources::resize(uint32_t width, uint32_t height)
{
	vkDeviceWaitIdle(m_device);
	destroyResizeableResources();
	createResizableResources(width, height);
}

void sss::vulkan::RenderResources::createResizableResources(uint32_t width, uint32_t height)
{
	for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
	{
		VkImageCreateInfo imageCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.extent.width = width;
		imageCreateInfo.extent.height = height;
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		// depth
		{
			imageCreateInfo.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
			imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

			m_depthStencilImage[i] = std::make_unique<Image>(m_physicalDevice, m_device, imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				0, VK_IMAGE_VIEW_TYPE_2D, VkImageSubresourceRange{ VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 });

			VkImageViewCreateInfo viewCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
			viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewCreateInfo.image = m_depthStencilImage[i]->getImage();
			viewCreateInfo.format = imageCreateInfo.format;
			viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };

			if (vkCreateImageView(m_device, &viewCreateInfo, nullptr, &m_depthImageView[i]) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create image view!", EXIT_FAILURE);
			}
		}

		// color
		{
			imageCreateInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
			imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

			m_colorImage[i] = std::make_unique<Image>(m_physicalDevice, m_device, imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				0, VK_IMAGE_VIEW_TYPE_2D, VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
		}

		// diffuse
		{
			imageCreateInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
			imageCreateInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

			m_diffuse0Image[i] = std::make_unique<Image>(m_physicalDevice, m_device, imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				0, VK_IMAGE_VIEW_TYPE_2D, VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

			m_diffuse1Image[i] = std::make_unique<Image>(m_physicalDevice, m_device, imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				0, VK_IMAGE_VIEW_TYPE_2D, VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
		}

		// tonemap result
		{
			imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
			imageCreateInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

			m_tonemappedImage[i] = std::make_unique<Image>(m_physicalDevice, m_device, imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
				0, VK_IMAGE_VIEW_TYPE_2D, VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
		}

		// shadow framebuffer
		{
			VkFramebufferCreateInfo framebufferCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
			framebufferCreateInfo.renderPass = m_shadowRenderPass;
			framebufferCreateInfo.attachmentCount = 1;
			framebufferCreateInfo.pAttachments = &m_shadowImage[i]->getView();
			framebufferCreateInfo.width = SHADOW_RESOLUTION;
			framebufferCreateInfo.height = SHADOW_RESOLUTION;
			framebufferCreateInfo.layers = 1;

			if (vkCreateFramebuffer(m_device, &framebufferCreateInfo, nullptr, &m_shadowFramebuffers[i]) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create framebuffer!", EXIT_FAILURE);
			}
		}

		// main framebuffer
		{
			VkImageView framebufferAttachments[3];
			framebufferAttachments[0] = m_depthStencilImage[i]->getView();
			framebufferAttachments[1] = m_colorImage[i]->getView();
			framebufferAttachments[2] = m_diffuse0Image[i]->getView();

			VkFramebufferCreateInfo framebufferCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
			framebufferCreateInfo.renderPass = m_mainRenderPass;
			framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(sizeof(framebufferAttachments) / sizeof(framebufferAttachments[0]));
			framebufferCreateInfo.pAttachments = framebufferAttachments;
			framebufferCreateInfo.width = width;
			framebufferCreateInfo.height = height;
			framebufferCreateInfo.layers = 1;

			if (vkCreateFramebuffer(m_device, &framebufferCreateInfo, nullptr, &m_mainFramebuffers[i]) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create framebuffer!", EXIT_FAILURE);
			}
		}
	}

	// update descriptor sets
	for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
	{
		VkDescriptorBufferInfo bufferInfos[1];
		VkDescriptorImageInfo imageInfos[12];
		VkWriteDescriptorSet descriptorWrites[13];
		size_t bufferInfoCount = 0;
		size_t imageInfoCount = 0;
		size_t writeCount = 0;

		// lighting set
		{
			// constant buffer
			auto &bufferInfo = bufferInfos[bufferInfoCount++];
			bufferInfo.buffer = m_constantBuffer[i]->getBuffer();
			bufferInfo.offset = 0;
			bufferInfo.range = m_constantBuffer[i]->getSize();

			auto &constantBufferWrite = descriptorWrites[writeCount++];
			constantBufferWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			constantBufferWrite.dstSet = m_lightingDescriptorSet[i];
			constantBufferWrite.dstBinding = 0;
			constantBufferWrite.descriptorCount = 1;
			constantBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			constantBufferWrite.pBufferInfo = &bufferInfo;

			// shadow map
			auto &shadowImageInfo = imageInfos[imageInfoCount++];
			shadowImageInfo.sampler = VK_NULL_HANDLE;
			shadowImageInfo.imageView = m_shadowImage[i]->getView();
			shadowImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			auto &shadowMapWrite = descriptorWrites[writeCount++];
			shadowMapWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			shadowMapWrite.dstSet = m_lightingDescriptorSet[i];
			shadowMapWrite.dstBinding = 1;
			shadowMapWrite.descriptorCount = 1;
			shadowMapWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			shadowMapWrite.pImageInfo = &shadowImageInfo;
		}

		// blur set
		for (size_t j = 0; j < 2; ++j)
		{
			// input
			auto &inputImageInfo = imageInfos[imageInfoCount++];
			inputImageInfo.sampler = VK_NULL_HANDLE;
			inputImageInfo.imageView = (j == 0) ? m_diffuse0Image[i]->getView() : m_diffuse1Image[i]->getView();
			inputImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			auto &inputWrite = descriptorWrites[writeCount++];
			inputWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			inputWrite.dstSet = m_sssBlurDescriptorSet[i * 2 + j];
			inputWrite.dstBinding = 0;
			inputWrite.descriptorCount = 1;
			inputWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			inputWrite.pImageInfo = &inputImageInfo;

			// depth
			auto &depthImageInfo = imageInfos[imageInfoCount++];
			depthImageInfo.sampler = VK_NULL_HANDLE;
			depthImageInfo.imageView = m_depthImageView[i];
			depthImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			auto &depthWrite = descriptorWrites[writeCount++];
			depthWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			depthWrite.dstSet = m_sssBlurDescriptorSet[i * 2 + j];
			depthWrite.dstBinding = 1;
			depthWrite.descriptorCount = 1;
			depthWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			depthWrite.pImageInfo = &depthImageInfo;

			// result
			auto &resultImageInfo = imageInfos[imageInfoCount++];
			resultImageInfo.sampler = VK_NULL_HANDLE;
			resultImageInfo.imageView = (j == 0) ? m_diffuse1Image[i]->getView() : m_diffuse0Image[i]->getView();
			resultImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

			auto &resultWrite = descriptorWrites[writeCount++];
			resultWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			resultWrite.dstSet = m_sssBlurDescriptorSet[i * 2 + j];
			resultWrite.dstBinding = 2;
			resultWrite.descriptorCount = 1;
			resultWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			resultWrite.pImageInfo = &resultImageInfo;
		}

		// postprocessing set
		{
			// result
			auto &resultImageInfo = imageInfos[imageInfoCount++];
			resultImageInfo.sampler = VK_NULL_HANDLE;
			resultImageInfo.imageView = m_tonemappedImage[i]->getView();
			resultImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

			auto &resultWrite = descriptorWrites[writeCount++];
			resultWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			resultWrite.dstSet = m_postprocessingDescriptorSet[i];
			resultWrite.dstBinding = 0;
			resultWrite.descriptorCount = 1;
			resultWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			resultWrite.pImageInfo = &resultImageInfo;

			// color
			auto &colorImageInfo = imageInfos[imageInfoCount++];
			colorImageInfo.sampler = VK_NULL_HANDLE;
			colorImageInfo.imageView = m_colorImage[i]->getView();
			colorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			auto &colorWrite = descriptorWrites[writeCount++];
			colorWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			colorWrite.dstSet = m_postprocessingDescriptorSet[i];
			colorWrite.dstBinding = 1;
			colorWrite.descriptorCount = 1;
			colorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			colorWrite.pImageInfo = &colorImageInfo;

			// diffuse
			auto &diffuseImageInfo = imageInfos[imageInfoCount++];
			diffuseImageInfo.sampler = VK_NULL_HANDLE;
			diffuseImageInfo.imageView = m_diffuse0Image[i]->getView();
			diffuseImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			auto &diffuseWrite = descriptorWrites[writeCount++];
			diffuseWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			diffuseWrite.dstSet = m_postprocessingDescriptorSet[i];
			diffuseWrite.dstBinding = 2;
			diffuseWrite.descriptorCount = 1;
			diffuseWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			diffuseWrite.pImageInfo = &diffuseImageInfo;

			// depth
			auto &depthImageInfo = imageInfos[imageInfoCount++];
			depthImageInfo.sampler = VK_NULL_HANDLE;
			depthImageInfo.imageView = m_depthImageView[i];
			depthImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			auto &depthWrite = descriptorWrites[writeCount++];
			depthWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			depthWrite.dstSet = m_postprocessingDescriptorSet[i];
			depthWrite.dstBinding = 3;
			depthWrite.descriptorCount = 1;
			depthWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			depthWrite.pImageInfo = &depthImageInfo;

			// history
			auto &historyImageInfo = imageInfos[imageInfoCount++];
			historyImageInfo.sampler = VK_NULL_HANDLE;
			historyImageInfo.imageView = m_tonemappedImage[(i + FRAMES_IN_FLIGHT - 1) % FRAMES_IN_FLIGHT]->getView();
			historyImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			auto &historyWrite = descriptorWrites[writeCount++];
			historyWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			historyWrite.dstSet = m_postprocessingDescriptorSet[i];
			historyWrite.dstBinding = 4;
			historyWrite.descriptorCount = 1;
			historyWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			historyWrite.pImageInfo = &historyImageInfo;
		}

		vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(sizeof(descriptorWrites) / sizeof(descriptorWrites[0])), descriptorWrites, 0, nullptr);
	}

	// gui framebuffer
	m_guiFramebuffers.resize(m_swapChain->getImageCount());
	for (size_t i = 0; i < m_swapChain->getImageCount(); ++i)
	{
		VkImageView framebufferAttachment = m_swapChain->getImageView(i);

		VkFramebufferCreateInfo framebufferCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		framebufferCreateInfo.renderPass = m_guiRenderPass;
		framebufferCreateInfo.attachmentCount = 1;
		framebufferCreateInfo.pAttachments = &framebufferAttachment;
		framebufferCreateInfo.width = width;
		framebufferCreateInfo.height = height;
		framebufferCreateInfo.layers = 1;

		if (vkCreateFramebuffer(m_device, &framebufferCreateInfo, nullptr, &m_guiFramebuffers[i]) != VK_SUCCESS)
		{
			util::fatalExit("Failed to create framebuffer!", EXIT_FAILURE);
		}
	}
}

void sss::vulkan::RenderResources::destroyResizeableResources()
{
	for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
	{
		m_depthStencilImage[i] = nullptr;
		m_colorImage[i] = nullptr;
		m_diffuse0Image[i] = nullptr;
		m_diffuse1Image[i] = nullptr;
		m_tonemappedImage[i] = nullptr;

		vkDestroyImageView(m_device, m_depthImageView[i], nullptr);

		vkDestroyFramebuffer(m_device, m_shadowFramebuffers[i], nullptr);
		vkDestroyFramebuffer(m_device, m_mainFramebuffers[i], nullptr);
	}

	for (size_t i = 0; i < m_swapChain->getImageCount(); ++i)
	{
		vkDestroyFramebuffer(m_device, m_guiFramebuffers[i], nullptr);
	}
}
