#include "Renderer.h"
#include <glm/trigonometric.hpp>
#include <glm/packing.hpp>
#include "utility/Utility.h"
#include "VKUtility.h"
#include "vulkan/Mesh.h"
#include "vulkan/Texture.h"
#include "pipelines/ShadowPipeline.h"
#include "pipelines/LightingPipeline.h"
#include "pipelines/SkyboxPipeline.h"
#include "pipelines/SSSBlurPipeline.h"
#include "pipelines/PostprocessingPipeline.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

static void check_vk_result(VkResult err)
{
	if (err == 0)
		return;
	printf("VkResult %d\n", err);
	if (err < 0)
		abort();
}


sss::vulkan::Renderer::Renderer(void *windowHandle, uint32_t width, uint32_t height)
	:m_width(width),
	m_height(height),
	m_context(windowHandle),
	m_swapChain(m_context, m_width, m_height)
{
	const char *texturePaths[] =
	{
		"resources/textures/head_albedo.dds",
		"resources/textures/head_normal.dds",
		"resources/textures/head_gloss.dds",
		"resources/textures/head_specular.dds",
		"resources/textures/head_cavity.dds",
		"resources/textures/jacket_albedo.dds",
		"resources/textures/jacket_normal.dds",
		"resources/textures/jacket_gloss.dds",
		"resources/textures/jacket_specular.dds",
	};

	for (const auto &path : texturePaths)
	{
		m_textures.push_back(Texture::load(m_context.getPhysicalDevice(), m_context.getDevice(), m_context.getGraphicsQueue(), m_context.getGraphicsCommandPool(), path));
	}

	m_skyboxTexture = Texture::load(m_context.getPhysicalDevice(), m_context.getDevice(), m_context.getGraphicsQueue(), m_context.getGraphicsCommandPool(), "resources/textures/skybox.dds", true);
	m_radianceTexture = Texture::load(m_context.getPhysicalDevice(), m_context.getDevice(), m_context.getGraphicsQueue(), m_context.getGraphicsCommandPool(), "resources/textures/prefilterMap.dds", true);
	m_irradianceTexture = Texture::load(m_context.getPhysicalDevice(), m_context.getDevice(), m_context.getGraphicsQueue(), m_context.getGraphicsCommandPool(), "resources/textures/irradianceMap.dds", true);
	m_brdfLUT = Texture::load(m_context.getPhysicalDevice(), m_context.getDevice(), m_context.getGraphicsQueue(), m_context.getGraphicsCommandPool(), "resources/textures/brdfLut.dds");

	// load meshes
	{
		Material headMaterial;
		headMaterial.gloss = 0.638f;
		headMaterial.specular = 0.097f;
		headMaterial.albedo = 0xFFFFFFFF;
		headMaterial.albedoTexture = 1;
		headMaterial.normalTexture = 2;
		headMaterial.glossTexture = 3;
		headMaterial.specularTexture = 4;
		headMaterial.cavityTexture = 5;

		Material jacketMaterial;
		jacketMaterial.gloss = 0.376f;
		jacketMaterial.specular = 0.162f;
		jacketMaterial.albedo = 0xFFFFFFFF;
		jacketMaterial.albedoTexture = 6;
		jacketMaterial.normalTexture = 7;
		jacketMaterial.glossTexture = 8;
		jacketMaterial.specularTexture = 9;
		jacketMaterial.cavityTexture = 0;

		Material browsMaterial;
		browsMaterial.gloss = 0.0f;
		browsMaterial.specular = 0.007f;
		browsMaterial.albedo = glm::packUnorm4x8(glm::vec4(50.0f, 36.0f, 26.0f, 255.0f) / 255.0f);
		browsMaterial.albedoTexture = 0;
		browsMaterial.normalTexture = 0;
		browsMaterial.glossTexture = 0;
		browsMaterial.specularTexture = 0;
		browsMaterial.cavityTexture = 0;

		Material eyelashesMaterial;
		eyelashesMaterial.gloss = 0.43f;
		eyelashesMaterial.specular = 0.162f;
		eyelashesMaterial.albedo = glm::packUnorm4x8(glm::vec4(24.0f, 24.0f, 24.0f, 255.0f) / 255.0f);
		eyelashesMaterial.albedoTexture = 0;
		eyelashesMaterial.normalTexture = 0;
		eyelashesMaterial.glossTexture = 0;
		eyelashesMaterial.specularTexture = 0;
		eyelashesMaterial.cavityTexture = 0;

		std::pair<Material, bool> materials[] = { {headMaterial, true}, {jacketMaterial, false} };// , { browsMaterial, false }, { eyelashesMaterial, false } };
		const char *meshPaths[] = { "resources/meshes/head.mesh", "resources/meshes/jacket.mesh" };//, "resources/meshes/brows.mesh", "resources/meshes/eyelashes.mesh" };

		for (size_t i = 0; i < sizeof(meshPaths) / sizeof(meshPaths[0]); ++i)
		{
			m_meshes.push_back(Mesh::load(m_context.getPhysicalDevice(), m_context.getDevice(), m_context.getGraphicsQueue(), m_context.getGraphicsCommandPool(), meshPaths[i]));
			m_materials.push_back(materials[i]);
		}
	}


	const size_t textureCount = sizeof(texturePaths) / sizeof(texturePaths[0]);

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

				if (vkutil::createBuffer(m_context.getPhysicalDevice(),
					m_context.getDevice(),
					createInfo,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					m_constantBuffer[i],
					m_constantBufferMemory[i]) != VK_SUCCESS)
				{
					util::fatalExit("Failed to create buffer!", EXIT_FAILURE);
				}

				if (vkMapMemory(m_context.getDevice(), m_constantBufferMemory[i], 0, createInfo.size, 0, (void **)&m_constantBufferPtr[i]) != VK_SUCCESS)
				{
					util::fatalExit("Failed to map constant buffer memory!", EXIT_FAILURE);
				}
			}

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
				viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };

				if (vkCreateImageView(m_context.getDevice(), &viewCreateInfo, nullptr, &m_shadowImageView[i]) != VK_SUCCESS)
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
					VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					0,
					m_depthStencilImage[i],
					m_depthStencilImageMemory[i]) != VK_SUCCESS)
				{
					util::fatalExit("Failed to create image!", EXIT_FAILURE);
				}

				VkImageViewCreateInfo viewCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
				viewCreateInfo.image = m_depthStencilImage[i];
				viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				viewCreateInfo.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
				viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1 };

				// depth AND stencil aspect
				if (vkCreateImageView(m_context.getDevice(), &viewCreateInfo, nullptr, &m_depthStencilImageView[i]) != VK_SUCCESS)
				{
					util::fatalExit("Failed to create image view!", EXIT_FAILURE);
				}

				viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };

				// depth aspect only
				if (vkCreateImageView(m_context.getDevice(), &viewCreateInfo, nullptr, &m_depthImageView[i]) != VK_SUCCESS)
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
					VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
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

			// diffuse
			{
				VkImage images[2];
				VkDeviceMemory memory[2];
				VkImageView views[2];

				for (size_t j = 0; j < 2; ++j)
				{
					if (vkutil::create2dImage(m_context.getPhysicalDevice(),
						m_context.getDevice(),
						VK_FORMAT_R16G16B16A16_SFLOAT,
						m_width,
						m_height,
						VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
						VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
						0,
						images[j],
						memory[j]) != VK_SUCCESS)
					{
						util::fatalExit("Failed to create image!", EXIT_FAILURE);
					}

					VkImageViewCreateInfo viewCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
					viewCreateInfo.image = images[j];
					viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
					viewCreateInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
					viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

					if (vkCreateImageView(m_context.getDevice(), &viewCreateInfo, nullptr, &views[j]) != VK_SUCCESS)
					{
						util::fatalExit("Failed to create image view!", EXIT_FAILURE);
					}
				}

				m_diffuse0Image[i] = images[0];
				m_diffuse1Image[i] = images[1];
				m_diffuse0ImageMemory[i] = memory[0];
				m_diffuse1ImageMemory[i] = memory[1];
				m_diffuse0ImageView[i] = views[0];
				m_diffuse1ImageView[i] = views[1];
			}

			// tonemap result
			{
				if (vkutil::create2dImage(m_context.getPhysicalDevice(),
					m_context.getDevice(),
					VK_FORMAT_R8G8B8A8_UNORM,
					m_width,
					m_height,
					VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
					VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					0,
					m_tonemappedImage[i],
					m_tonemappedImageMemory[i]) != VK_SUCCESS)
				{
					util::fatalExit("Failed to create image!", EXIT_FAILURE);
				}

				VkImageViewCreateInfo viewCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
				viewCreateInfo.image = m_tonemappedImage[i];
				viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				viewCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
				viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

				if (vkCreateImageView(m_context.getDevice(), &viewCreateInfo, nullptr, &m_tonemappedImageView[i]) != VK_SUCCESS)
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
		{
			subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpasses[0].colorAttachmentCount = 1;
			subpasses[0].pColorAttachments = &colorAttachmentRef;
			subpasses[0].pDepthStencilAttachment = &depthAttachmentRef;
		}

		// sss lighting subpass
		{
			subpasses[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpasses[1].colorAttachmentCount = 2;
			subpasses[1].pColorAttachments = sssLightingPassAttachmentRefs;
			subpasses[1].pDepthStencilAttachment = &depthAttachmentRef;
		}

		// skybox subpass
		{
			subpasses[2].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpasses[2].colorAttachmentCount = 1;
			subpasses[2].pColorAttachments = &colorAttachmentRef;
			subpasses[2].pDepthStencilAttachment = &depthAttachmentRef;
		}

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

			if (vkCreateRenderPass(m_context.getDevice(), &renderPassInfo, nullptr, &m_mainRenderPass) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create render pass!", EXIT_FAILURE);
			}
		}


		// create framebuffers
		{
			for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
			{
				VkImageView framebufferAttachments[3];
				framebufferAttachments[0] = m_depthStencilImageView[i];
				framebufferAttachments[1] = m_colorImageView[i];
				framebufferAttachments[2] = m_diffuse0ImageView[i];

				VkFramebufferCreateInfo framebufferCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
				framebufferCreateInfo.renderPass = m_mainRenderPass;
				framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(sizeof(framebufferAttachments) / sizeof(framebufferAttachments[0]));
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

	// create gui renderpass
	{
		VkAttachmentDescription attachmentDescription{};
		attachmentDescription.format = VK_FORMAT_R8G8B8A8_UNORM;
		attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
		attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
		attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

		VkAttachmentReference attachmentRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &attachmentRef;


		// create renderpass
		{
			VkSubpassDependency dependencies[2]{};

			// tonemap -> gui
			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			// gui -> blit
			dependencies[1].srcSubpass = 0;
			dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[1].dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			VkRenderPassCreateInfo renderPassInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
			renderPassInfo.attachmentCount = 1;
			renderPassInfo.pAttachments = &attachmentDescription;
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;
			renderPassInfo.dependencyCount = static_cast<uint32_t>(sizeof(dependencies) / sizeof(dependencies[0]));
			renderPassInfo.pDependencies = dependencies;

			if (vkCreateRenderPass(m_context.getDevice(), &renderPassInfo, nullptr, &m_guiRenderPass) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create render pass!", EXIT_FAILURE);
			}
		}


		// create framebuffers
		{
			for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
			{
				VkImageView framebufferAttachment = m_tonemappedImageView[i];

				VkFramebufferCreateInfo framebufferCreateInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
				framebufferCreateInfo.renderPass = m_guiRenderPass;
				framebufferCreateInfo.attachmentCount = 1;
				framebufferCreateInfo.pAttachments = &framebufferAttachment;
				framebufferCreateInfo.width = m_width;
				framebufferCreateInfo.height = m_height;
				framebufferCreateInfo.layers = 1;

				if (vkCreateFramebuffer(m_context.getDevice(), &framebufferCreateInfo, nullptr, &m_guiFramebuffers[i]) != VK_SUCCESS)
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
			samplerCreateInfo.maxLod = 14.0f;
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
			samplerCreateInfo.maxLod = 14.0f;
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
			samplerCreateInfo.maxLod = 14.0f;
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
		VkDescriptorPoolSize poolSizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, FRAMES_IN_FLIGHT },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, FRAMES_IN_FLIGHT * (1 /*shadow maps*/ + 4 /*depth and diffuse for 2 sss blur passes*/ + 2/* postprocessing input*/) + (textureCount + 4 /*cubemaps*/) + 1 /*imgui*/ },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, FRAMES_IN_FLIGHT * 3 /* 2 sss blur passes + 1 postprocessing pass*/ }
		};

		VkDescriptorPoolCreateInfo poolCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		poolCreateInfo.maxSets = FRAMES_IN_FLIGHT * 4 + 2;
		poolCreateInfo.poolSizeCount = 3;
		poolCreateInfo.pPoolSizes = poolSizes;

		if (vkCreateDescriptorPool(m_context.getDevice(), &poolCreateInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
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

			if (vkCreateDescriptorSetLayout(m_context.getDevice(), &layoutCreateInfo, nullptr, &m_textureDescriptorSetLayout) != VK_SUCCESS)
			{
				util::fatalExit("Failed to create descriptor set layout!", EXIT_FAILURE);
			}

			VkDescriptorSetAllocateInfo setAllocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
			setAllocInfo.descriptorPool = m_descriptorPool;
			setAllocInfo.descriptorSetCount = 1;
			setAllocInfo.pSetLayouts = &m_textureDescriptorSetLayout;

			if (vkAllocateDescriptorSets(m_context.getDevice(), &setAllocInfo, &m_textureDescriptorSet) != VK_SUCCESS)
			{
				util::fatalExit("Failed to allocate descriptor set!", EXIT_FAILURE);
			}

			VkDescriptorImageInfo textureImageInfos[textureCount + 4];
			{
				for (size_t i = 0; i < textureCount; ++i)
				{
					auto &textureImageInfo = textureImageInfos[i];
					textureImageInfo.sampler = m_linearSamplerClamp;
					textureImageInfo.imageView = m_textures[i]->getView();
					textureImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				}

				auto &brdfLutImageInfo = textureImageInfos[textureCount];
				brdfLutImageInfo.sampler = nullptr;
				brdfLutImageInfo.imageView = m_brdfLUT->getView();
				brdfLutImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				auto &radianceTexImageInfo = textureImageInfos[textureCount + 1];
				radianceTexImageInfo.sampler = nullptr;
				radianceTexImageInfo.imageView = m_radianceTexture->getView();
				radianceTexImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				auto &irradianceTexImageInfo = textureImageInfos[textureCount + 2];
				irradianceTexImageInfo.sampler = nullptr;
				irradianceTexImageInfo.imageView = m_irradianceTexture->getView();
				irradianceTexImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				auto &skyboxTexImageInfo = textureImageInfos[textureCount + 3];
				skyboxTexImageInfo.sampler = nullptr;
				skyboxTexImageInfo.imageView = m_skyboxTexture->getView();
				skyboxTexImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}

			VkWriteDescriptorSet descriptorWrites[5];
			{
				auto &textureWrite = descriptorWrites[0];
				textureWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				textureWrite.dstSet = m_textureDescriptorSet;
				textureWrite.dstBinding = 0;
				textureWrite.descriptorCount = textureCount;
				textureWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				textureWrite.pImageInfo = textureImageInfos;

				auto &brdfLutWrite = descriptorWrites[1];
				brdfLutWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				brdfLutWrite.dstSet = m_textureDescriptorSet;
				brdfLutWrite.dstBinding = 1;
				brdfLutWrite.descriptorCount = 1;
				brdfLutWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				brdfLutWrite.pImageInfo = &textureImageInfos[textureCount];

				auto &radianceTexWrite = descriptorWrites[2];
				radianceTexWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				radianceTexWrite.dstSet = m_textureDescriptorSet;
				radianceTexWrite.dstBinding = 2;
				radianceTexWrite.descriptorCount = 1;
				radianceTexWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				radianceTexWrite.pImageInfo = &textureImageInfos[textureCount + 1];

				auto &irradianceTexWrite = descriptorWrites[3];
				irradianceTexWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				irradianceTexWrite.dstSet = m_textureDescriptorSet;
				irradianceTexWrite.dstBinding = 3;
				irradianceTexWrite.descriptorCount = 1;
				irradianceTexWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				irradianceTexWrite.pImageInfo = &textureImageInfos[textureCount + 2];

				auto &skyboxTexWrite = descriptorWrites[4];
				skyboxTexWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				skyboxTexWrite.dstSet = m_textureDescriptorSet;
				skyboxTexWrite.dstBinding = 4;
				skyboxTexWrite.descriptorCount = 1;
				skyboxTexWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				skyboxTexWrite.pImageInfo = &textureImageInfos[textureCount + 3];
			}

			vkUpdateDescriptorSets(m_context.getDevice(), static_cast<uint32_t>(sizeof(descriptorWrites) / sizeof(descriptorWrites[0])), descriptorWrites, 0, nullptr);
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

			VkDescriptorBufferInfo constantBufferInfos[FRAMES_IN_FLIGHT];
			VkDescriptorImageInfo shadowImageInfos[FRAMES_IN_FLIGHT];
			VkWriteDescriptorSet descriptorWrites[FRAMES_IN_FLIGHT * 2];

			for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
			{
				// constant buffer
				auto &bufferInfo = constantBufferInfos[i];
				bufferInfo.buffer = m_constantBuffer[i];
				bufferInfo.offset = 0;
				bufferInfo.range = sizeof(glm::vec4) * 11;

				auto &constantBufferWrite = descriptorWrites[i * 2];
				constantBufferWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				constantBufferWrite.dstSet = m_lightingDescriptorSet[i];
				constantBufferWrite.dstBinding = 0;
				constantBufferWrite.descriptorCount = 1;
				constantBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				constantBufferWrite.pBufferInfo = &bufferInfo;

				// shadow map
				auto &shadowImageInfo = shadowImageInfos[i];
				shadowImageInfo.sampler = nullptr;
				shadowImageInfo.imageView = m_shadowImageView[i];
				shadowImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				auto &shadowMapWrite = descriptorWrites[i * 2 + 1];
				shadowMapWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				shadowMapWrite.dstSet = m_lightingDescriptorSet[i];
				shadowMapWrite.dstBinding = 1;
				shadowMapWrite.descriptorCount = 1;
				shadowMapWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				shadowMapWrite.pImageInfo = &shadowImageInfo;
			}

			vkUpdateDescriptorSets(m_context.getDevice(), static_cast<uint32_t>(sizeof(descriptorWrites) / sizeof(descriptorWrites[0])), descriptorWrites, 0, nullptr);
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

			if (vkCreateDescriptorSetLayout(m_context.getDevice(), &layoutCreateInfo, nullptr, &m_sssBlurDescriptorSetLayout) != VK_SUCCESS)
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

			if (vkAllocateDescriptorSets(m_context.getDevice(), &setAllocInfo, m_sssBlurDescriptorSet) != VK_SUCCESS)
			{
				util::fatalExit("Failed to allocate descriptor sets!", EXIT_FAILURE);
			}

			VkDescriptorImageInfo inputImageInfos[FRAMES_IN_FLIGHT * 2];
			VkDescriptorImageInfo depthImageInfos[FRAMES_IN_FLIGHT * 2];
			VkDescriptorImageInfo resultImageInfos[FRAMES_IN_FLIGHT * 2];
			VkWriteDescriptorSet descriptorWrites[FRAMES_IN_FLIGHT * 6];

			for (size_t i = 0; i < FRAMES_IN_FLIGHT * 2; ++i)
			{
				// input
				auto &inputImageInfo = inputImageInfos[i];
				inputImageInfo.sampler = nullptr;
				inputImageInfo.imageView = (i % 2 == 0) ? m_diffuse0ImageView[i / 2] : m_diffuse1ImageView[i / 2];
				inputImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				auto &inputWrite = descriptorWrites[i * 3];
				inputWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				inputWrite.dstSet = m_sssBlurDescriptorSet[i];
				inputWrite.dstBinding = 0;
				inputWrite.descriptorCount = 1;
				inputWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				inputWrite.pImageInfo = &inputImageInfo;

				// depth
				auto &depthImageInfo = depthImageInfos[i];
				depthImageInfo.sampler = nullptr;
				depthImageInfo.imageView = m_depthImageView[i / 2];
				depthImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				auto &depthWrite = descriptorWrites[i * 3 + 1];
				depthWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				depthWrite.dstSet = m_sssBlurDescriptorSet[i];
				depthWrite.dstBinding = 1;
				depthWrite.descriptorCount = 1;
				depthWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				depthWrite.pImageInfo = &depthImageInfo;

				// result
				auto &resultImageInfo = resultImageInfos[i];
				resultImageInfo.sampler = nullptr;
				resultImageInfo.imageView = (i % 2 == 0) ? m_diffuse1ImageView[i / 2] : m_diffuse0ImageView[i / 2];
				resultImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

				auto &resultWrite = descriptorWrites[i * 3 + 2];
				resultWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				resultWrite.dstSet = m_sssBlurDescriptorSet[i];
				resultWrite.dstBinding = 2;
				resultWrite.descriptorCount = 1;
				resultWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				resultWrite.pImageInfo = &resultImageInfo;
			}

			vkUpdateDescriptorSets(m_context.getDevice(), static_cast<uint32_t>(sizeof(descriptorWrites) / sizeof(descriptorWrites[0])), descriptorWrites, 0, nullptr);
		}

		// postprocessing set
		{
			VkDescriptorSetLayoutBinding bindings[] =
			{
				{ 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, &m_pointSamplerClamp },
				{ 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, &m_pointSamplerClamp },
				{ 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr },
			};

			VkDescriptorSetLayoutCreateInfo layoutCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			layoutCreateInfo.bindingCount = static_cast<uint32_t>(sizeof(bindings) / sizeof(bindings[0]));
			layoutCreateInfo.pBindings = bindings;

			if (vkCreateDescriptorSetLayout(m_context.getDevice(), &layoutCreateInfo, nullptr, &m_postprocessingDescriptorSetLayout) != VK_SUCCESS)
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

			if (vkAllocateDescriptorSets(m_context.getDevice(), &setAllocInfo, m_postprocessingDescriptorSet) != VK_SUCCESS)
			{
				util::fatalExit("Failed to allocate descriptor sets!", EXIT_FAILURE);
			}

			VkDescriptorImageInfo colorImageInfos[FRAMES_IN_FLIGHT];
			VkDescriptorImageInfo diffuseImageInfos[FRAMES_IN_FLIGHT];
			VkDescriptorImageInfo resultImageInfos[FRAMES_IN_FLIGHT];
			VkWriteDescriptorSet descriptorWrites[FRAMES_IN_FLIGHT * 3];

			for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
			{
				// color
				auto &colorImageInfo = colorImageInfos[i];
				colorImageInfo.sampler = nullptr;
				colorImageInfo.imageView = m_colorImageView[i];
				colorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				auto &colorWrite = descriptorWrites[i * 3];
				colorWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				colorWrite.dstSet = m_postprocessingDescriptorSet[i];
				colorWrite.dstBinding = 0;
				colorWrite.descriptorCount = 1;
				colorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				colorWrite.pImageInfo = &colorImageInfo;

				// diffuse
				auto &diffuseImageInfo = diffuseImageInfos[i];
				diffuseImageInfo.sampler = nullptr;
				diffuseImageInfo.imageView = m_diffuse0ImageView[i];
				diffuseImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				auto &diffuseWrite = descriptorWrites[i * 3 + 1];
				diffuseWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				diffuseWrite.dstSet = m_postprocessingDescriptorSet[i];
				diffuseWrite.dstBinding = 1;
				diffuseWrite.descriptorCount = 1;
				diffuseWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				diffuseWrite.pImageInfo = &diffuseImageInfo;

				// result
				auto &resultImageInfo = resultImageInfos[i];
				resultImageInfo.sampler = nullptr;
				resultImageInfo.imageView = m_tonemappedImageView[i];
				resultImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

				auto &resultWrite = descriptorWrites[i * 3 + 2];
				resultWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
				resultWrite.dstSet = m_postprocessingDescriptorSet[i];
				resultWrite.dstBinding = 2;
				resultWrite.descriptorCount = 1;
				resultWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				resultWrite.pImageInfo = &resultImageInfo;
			}

			vkUpdateDescriptorSets(m_context.getDevice(), static_cast<uint32_t>(sizeof(descriptorWrites) / sizeof(descriptorWrites[0])), descriptorWrites, 0, nullptr);
		}
	}

	VkQueryPoolCreateInfo queryPoolCreateInfo = { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
	queryPoolCreateInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
	queryPoolCreateInfo.queryCount = FRAMES_IN_FLIGHT * 2;

	if (vkCreateQueryPool(m_context.getDevice(), &queryPoolCreateInfo, nullptr, &m_queryPool) != VK_SUCCESS)
	{
		util::fatalExit("Failed to create query pool!", EXIT_FAILURE);
	}

	VkDescriptorSetLayout lightingDescriptorSetLayouts[] = { m_textureDescriptorSetLayout, m_lightingDescriptorSetLayout };

	m_shadowPipeline = ShadowPipeline::create(m_context.getDevice(), m_shadowRenderPass, 0, 0, nullptr);
	m_lightingPipeline = LightingPipeline::create(m_context.getDevice(), m_mainRenderPass, 0, 2, lightingDescriptorSetLayouts, false);
	m_sssLightingPipeline = LightingPipeline::create(m_context.getDevice(), m_mainRenderPass, 1, 2, lightingDescriptorSetLayouts, true);
	m_skyboxPipeline = SkyboxPipeline::create(m_context.getDevice(), m_mainRenderPass, 2, 1, &m_textureDescriptorSetLayout);
	m_sssBlurPipeline0 = SSSBlurPipeline::create(m_context.getDevice(), 1, &m_sssBlurDescriptorSetLayout);
	m_sssBlurPipeline1 = SSSBlurPipeline::create(m_context.getDevice(), 1, &m_sssBlurDescriptorSetLayout);
	m_posprocessingPipeline = PostprocessingPipeline::create(m_context.getDevice(), 1, &m_postprocessingDescriptorSetLayout);

	// imgui
	{
		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		//ImGui::StyleColorsClassic();

		// Setup Platform/Renderer bindings
		ImGui_ImplGlfw_InitForVulkan((GLFWwindow *)windowHandle, true);
		ImGui_ImplVulkan_InitInfo init_info = {};
		init_info.Instance = m_context.getInstance();
		init_info.PhysicalDevice = m_context.getPhysicalDevice();
		init_info.Device = m_context.getDevice();
		init_info.QueueFamily = m_context.getGraphicsQueueFamilyIndex();
		init_info.Queue = m_context.getGraphicsQueue();
		init_info.PipelineCache = VK_NULL_HANDLE;
		init_info.DescriptorPool = m_descriptorPool;
		init_info.Allocator = nullptr;
		init_info.MinImageCount = m_swapChain.getImageCount();
		init_info.ImageCount = m_swapChain.getImageCount();
		init_info.CheckVkResultFn = check_vk_result;
		ImGui_ImplVulkan_Init(&init_info, m_guiRenderPass);

		auto cmdBuf = vkutil::beginSingleTimeCommands(m_context.getDevice(), m_context.getGraphicsCommandPool());
		ImGui_ImplVulkan_CreateFontsTexture(cmdBuf);
		vkutil::endSingleTimeCommands(m_context.getDevice(), m_context.getGraphicsQueue(), m_context.getGraphicsCommandPool(), cmdBuf);
	}
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
			vkDestroyImage(device, m_depthStencilImage[i], nullptr);
			vkDestroyImageView(device, m_depthStencilImageView[i], nullptr);
			vkFreeMemory(device, m_depthStencilImageMemory[i], nullptr);
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
	vkDestroyPipeline(device, m_sssLightingPipeline.first, nullptr);
	vkDestroyPipelineLayout(device, m_sssLightingPipeline.second, nullptr);

}

void sss::vulkan::Renderer::render(const glm::mat4 &viewProjection, const glm::mat4 &shadowMatrix, const glm::vec4 &lightPositionRadius, const glm::vec4 &lightColorInvSqrAttRadius, const glm::vec4 &cameraPosition, bool subsurfaceScatteringEnabled)
{
	uint32_t resourceIndex = m_frameIndex % FRAMES_IN_FLIGHT;

	// wait until gpu finished work on all per frame resources
	vkWaitForFences(m_context.getDevice(), 1, &m_frameFinishedFence[resourceIndex], VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(m_context.getDevice(), 1, &m_frameFinishedFence[resourceIndex]);

	// retrieve gpu timestamp queries of sss passes
	if (m_frameIndex >= FRAMES_IN_FLIGHT)
	{
		uint64_t data[2];
		if (vkGetQueryPoolResults(m_context.getDevice(), m_queryPool, resourceIndex, 2, sizeof(data), data, sizeof(data[0]), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT) != VK_SUCCESS)
		{
			util::fatalExit("Failed to rerieve gpu timestamp queries!", EXIT_FAILURE);
		}
		m_sssTime = static_cast<float>((data[1] - data[0]) * (m_context.getDeviceProperties().limits.timestampPeriod * (1.0 / 1e6)));
	}

	// update constant buffer content
	((glm::mat4 *)m_constantBufferPtr[resourceIndex])[0] = viewProjection;
	((glm::mat4 *)m_constantBufferPtr[resourceIndex])[1] = shadowMatrix;
	((glm::vec4 *)m_constantBufferPtr[resourceIndex])[8] = lightPositionRadius;
	((glm::vec4 *)m_constantBufferPtr[resourceIndex])[9] = lightColorInvSqrAttRadius;
	((glm::vec4 *)m_constantBufferPtr[resourceIndex])[10] = cameraPosition;

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

				VkViewport viewport{ 0.0f, 0.0f, static_cast<float>(SHADOW_RESOLUTION), static_cast<float>(SHADOW_RESOLUTION), 0.0f, 1.0f };
				VkRect2D scissor{ { 0, 0 }, { SHADOW_RESOLUTION, SHADOW_RESOLUTION } };

				vkCmdSetViewport(curCmdBuf, 0, 1, &viewport);
				vkCmdSetScissor(curCmdBuf, 0, 1, &scissor);

				vkCmdSetViewport(curCmdBuf, 0, 1, &viewport);
				vkCmdSetScissor(curCmdBuf, 0, 1, &scissor);

				for (const auto &submesh : m_meshes)
				{
					vkCmdBindIndexBuffer(curCmdBuf, submesh->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

					VkBuffer vertexBuffer = submesh->getVertexBuffer();
					VkDeviceSize vertexBufferOffset = 0;

					vkCmdBindVertexBuffers(curCmdBuf, 0, 1, &vertexBuffer, &vertexBufferOffset);

					vkCmdPushConstants(curCmdBuf, m_shadowPipeline.second, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(shadowMatrix), &shadowMatrix);

					vkCmdDrawIndexed(curCmdBuf, submesh->getIndexCount(), 1, 0, 0, 0);
				}
			}
			vkCmdEndRenderPass(curCmdBuf);
		}

		// main renderpass
		{
			VkClearValue clearValues[3];

			// depth/stencil
			clearValues[0].depthStencil.depth = 1.0f;
			clearValues[0].depthStencil.stencil = 0;

			// color
			clearValues[1].color.float32[0] = 0.0f;
			clearValues[1].color.float32[1] = 0.0f;
			clearValues[1].color.float32[2] = 0.0f;
			clearValues[1].color.float32[3] = 0.0f;

			// diffuse0
			clearValues[2].color.float32[0] = 0.0f;
			clearValues[2].color.float32[1] = 0.0f;
			clearValues[2].color.float32[2] = 0.0f;
			clearValues[2].color.float32[3] = 0.0f;

			VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
			renderPassInfo.renderPass = m_mainRenderPass;
			renderPassInfo.framebuffer = m_mainFramebuffers[resourceIndex];
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = { m_width, m_height };
			renderPassInfo.clearValueCount = 3;
			renderPassInfo.pClearValues = clearValues;

			vkCmdBeginRenderPass(curCmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			// lighting
			{
				vkCmdBindPipeline(curCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lightingPipeline.first);

				VkDescriptorSet sets[] = { m_textureDescriptorSet, m_lightingDescriptorSet[resourceIndex] };
				vkCmdBindDescriptorSets(curCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lightingPipeline.second, 0, 2, sets, 0, nullptr);

				VkViewport viewport{ 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
				VkRect2D scissor{ { 0, 0 }, { m_width, m_height } };

				vkCmdSetViewport(curCmdBuf, 0, 1, &viewport);
				vkCmdSetScissor(curCmdBuf, 0, 1, &scissor);


				for (size_t i = 0; i < m_meshes.size(); ++i)
				{
					const auto &submesh = m_meshes[i];
					const auto &material = m_materials[i];

					// test if mesh is supposed to be rendered without SSS
					if (material.second)
					{
						continue;
					}

					vkCmdBindIndexBuffer(curCmdBuf, submesh->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

					VkBuffer vertexBuffer = submesh->getVertexBuffer();
					uint32_t vertexCount = submesh->getVertexCount();
					VkBuffer vertexBuffers[] = { vertexBuffer, vertexBuffer, vertexBuffer };
					VkDeviceSize vertexBufferOffsets[] = { 0, vertexCount * sizeof(float) * 3, vertexCount * sizeof(float) * 6 };

					vkCmdBindVertexBuffers(curCmdBuf, 0, 3, vertexBuffers, vertexBufferOffsets);

					vkCmdPushConstants(curCmdBuf, m_lightingPipeline.second, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(material.first), &material.first);

					vkCmdDrawIndexed(curCmdBuf, submesh->getIndexCount(), 1, 0, 0, 0);
				}
			}

			// sss lighting
			{
				vkCmdNextSubpass(curCmdBuf, VK_SUBPASS_CONTENTS_INLINE);

				vkCmdBindPipeline(curCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_sssLightingPipeline.first);

				VkDescriptorSet sets[] = { m_textureDescriptorSet, m_lightingDescriptorSet[resourceIndex] };
				vkCmdBindDescriptorSets(curCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_sssLightingPipeline.second, 0, 2, sets, 0, nullptr);

				VkViewport viewport{ 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
				VkRect2D scissor{ { 0, 0 }, { m_width, m_height } };

				vkCmdSetViewport(curCmdBuf, 0, 1, &viewport);
				vkCmdSetScissor(curCmdBuf, 0, 1, &scissor);


				for (size_t i = 0; i < m_meshes.size(); ++i)
				{
					const auto &submesh = m_meshes[i];
					const auto &material = m_materials[i];

					// test if mesh is supposed to be rendered with SSS
					if (!material.second)
					{
						continue;
					}

					vkCmdBindIndexBuffer(curCmdBuf, submesh->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

					VkBuffer vertexBuffer = submesh->getVertexBuffer();
					uint32_t vertexCount = submesh->getVertexCount();
					VkBuffer vertexBuffers[] = { vertexBuffer, vertexBuffer, vertexBuffer };
					VkDeviceSize vertexBufferOffsets[] = { 0, vertexCount * sizeof(float) * 3, vertexCount * sizeof(float) * 6 };

					vkCmdBindVertexBuffers(curCmdBuf, 0, 3, vertexBuffers, vertexBufferOffsets);

					vkCmdPushConstants(curCmdBuf, m_sssLightingPipeline.second, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(material.first), &material.first);

					vkCmdDrawIndexed(curCmdBuf, submesh->getIndexCount(), 1, 0, 0, 0);
				}
			}

			// skybox
			{
				vkCmdNextSubpass(curCmdBuf, VK_SUBPASS_CONTENTS_INLINE);

				vkCmdBindPipeline(curCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipeline.first);

				vkCmdBindDescriptorSets(curCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyboxPipeline.second, 0, 1, &m_textureDescriptorSet, 0, nullptr);

				VkViewport viewport{ 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
				VkRect2D scissor{ { 0, 0 }, { m_width, m_height } };

				vkCmdSetViewport(curCmdBuf, 0, 1, &viewport);
				vkCmdSetScissor(curCmdBuf, 0, 1, &scissor);

				glm::mat4 invModelViewProjectionMatrix = glm::inverse(viewProjection);

				vkCmdPushConstants(curCmdBuf, m_skyboxPipeline.second, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(invModelViewProjectionMatrix), &invModelViewProjectionMatrix);

				vkCmdDraw(curCmdBuf, 3, 1, 0, 0);
			}
			vkCmdEndRenderPass(curCmdBuf);
		}

		vkCmdResetQueryPool(curCmdBuf, m_queryPool, resourceIndex, 2);
		vkCmdWriteTimestamp(curCmdBuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPool, resourceIndex);

		if (subsurfaceScatteringEnabled)
		{
			// sss blur 0
			{
				// transition diffuse1 image layout to VK_IMAGE_LAYOUT_GENERAL
				{
					VkImageMemoryBarrier imageBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
					imageBarrier.srcAccessMask = 0;
					imageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
					imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
					imageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
					imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					imageBarrier.image = m_diffuse1Image[resourceIndex];
					imageBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

					vkCmdPipelineBarrier(curCmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
				}

				vkCmdBindPipeline(curCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_sssBlurPipeline0.first);

				vkCmdBindDescriptorSets(curCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_sssBlurPipeline0.second, 0, 1, &m_sssBlurDescriptorSet[resourceIndex * 2], 0, nullptr);

				using namespace glm;
				struct PushConsts
				{
					vec2 texelSize;
					vec2 dir;
					float sssWidth;
				};

				PushConsts pushConsts;
				pushConsts.texelSize = 1.0f / glm::vec2(m_width, m_height);
				pushConsts.dir = glm::vec2(1.0f, 0.0f);
				pushConsts.sssWidth = 0.01f * 1.0f / tanf(glm::radians(40.0f) * 0.5f) * (m_height / static_cast<float>(m_width));

				vkCmdPushConstants(curCmdBuf, m_sssBlurPipeline0.second, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

				vkCmdDispatch(curCmdBuf, (m_width + 15) / 16, (m_height + 15) / 16, 1);
			}

			// sss blur 1
			{
				// barriers
				{
					VkImageMemoryBarrier imageBarriers[2];

					// transition diffuse1 image layout to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
					imageBarriers[0] = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
					imageBarriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
					imageBarriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
					imageBarriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
					imageBarriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					imageBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					imageBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					imageBarriers[0].image = m_diffuse1Image[resourceIndex];
					imageBarriers[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

					// transition diffuse0 image layout to VK_IMAGE_LAYOUT_GENERAL
					imageBarriers[1] = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
					imageBarriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
					imageBarriers[1].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
					imageBarriers[1].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					imageBarriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
					imageBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					imageBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					imageBarriers[1].image = m_diffuse0Image[resourceIndex];
					imageBarriers[1].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

					vkCmdPipelineBarrier(curCmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 2, imageBarriers);
				}

				vkCmdBindPipeline(curCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_sssBlurPipeline1.first);

				vkCmdBindDescriptorSets(curCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_sssBlurPipeline1.second, 0, 1, &m_sssBlurDescriptorSet[resourceIndex * 2 + 1], 0, nullptr);

				using namespace glm;
				struct PushConsts
				{
					vec2 texelSize;
					vec2 dir;
					float sssWidth;
				};

				PushConsts pushConsts;
				pushConsts.texelSize = 1.0f / glm::vec2(m_width, m_height);
				pushConsts.dir = glm::vec2(0.0f, 1.0f);
				pushConsts.sssWidth = 0.01f * 1.0f / tanf(glm::radians(40.0f) * 0.5f) * (m_height / static_cast<float>(m_width));

				vkCmdPushConstants(curCmdBuf, m_sssBlurPipeline1.second, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

				vkCmdDispatch(curCmdBuf, (m_width + 15) / 16, (m_height + 15) / 16, 1);
			}
		}

		vkCmdWriteTimestamp(curCmdBuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_queryPool, resourceIndex + 1);

		// postprocessing
		{
			// barriers
			{
				VkImageMemoryBarrier imageBarriers[2];

				// transition tonemapped image layout to VK_IMAGE_LAYOUT_GENERAL
				imageBarriers[0] = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
				imageBarriers[0].srcAccessMask = 0;
				imageBarriers[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				imageBarriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				imageBarriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
				imageBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageBarriers[0].image = m_tonemappedImage[resourceIndex];
				imageBarriers[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

				// transition diffuse0 image layout to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				imageBarriers[1] = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
				imageBarriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				imageBarriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				imageBarriers[1].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
				imageBarriers[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageBarriers[1].image = m_diffuse0Image[resourceIndex];
				imageBarriers[1].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

				vkCmdPipelineBarrier(curCmdBuf, subsurfaceScatteringEnabled ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, subsurfaceScatteringEnabled ? 2 : 1, imageBarriers);
			}

			vkCmdBindPipeline(curCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_posprocessingPipeline.first);

			vkCmdBindDescriptorSets(curCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_posprocessingPipeline.second, 0, 1, &m_postprocessingDescriptorSet[resourceIndex], 0, nullptr);

			float exposure = 1.0f;

			vkCmdPushConstants(curCmdBuf, m_posprocessingPipeline.second, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(exposure), &exposure);

			vkCmdDispatch(curCmdBuf, (m_width + 15) / 16, (m_height + 15) / 16, 1);
		}

		// gui renderpass
		{
			VkClearValue clearValue;

			VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
			renderPassInfo.renderPass = m_guiRenderPass;
			renderPassInfo.framebuffer = m_guiFramebuffers[resourceIndex];
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = { m_width, m_height };
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearValue;

			vkCmdBeginRenderPass(curCmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), curCmdBuf);

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
		// barriers
		{
			VkImageMemoryBarrier imageBarriers[1];
			// transition backbuffer image layout to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
			imageBarriers[0] = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			imageBarriers[0].srcAccessMask = 0;
			imageBarriers[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			imageBarriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageBarriers[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageBarriers[0].image = m_swapChain.getImage(swapChainImageIndex);
			imageBarriers[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

			vkCmdPipelineBarrier(curCmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, imageBarriers);
		}

		// blit color image to backbuffer
		{
			VkImageBlit region{};
			region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			region.srcOffsets[1] = { static_cast<int32_t>(m_width), static_cast<int32_t>(m_height), 1 };
			region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			region.dstOffsets[1] = { static_cast<int32_t>(m_width), static_cast<int32_t>(m_height), 1 };

			vkCmdBlitImage(curCmdBuf, m_tonemappedImage[resourceIndex], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_swapChain.getImage(swapChainImageIndex), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_NEAREST);
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

float sss::vulkan::Renderer::getSSSEffectTiming() const
{
	return m_sssTime;
}
