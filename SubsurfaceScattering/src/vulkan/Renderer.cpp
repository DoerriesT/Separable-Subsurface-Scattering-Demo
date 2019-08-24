#include "Renderer.h"
#include <glm/trigonometric.hpp>
#include <glm/packing.hpp>
#include "utility/Utility.h"
#include "VKUtility.h"
#include "vulkan/Mesh.h"
#include "vulkan/Texture.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"
#include <glm/ext.hpp>

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
	m_swapChain(m_context.getPhysicalDevice(), m_context.getDevice(), m_context.getSurface(), m_width, m_height),
	m_renderResources(m_context.getPhysicalDevice(), m_context.getDevice(), m_context.getGraphicsCommandPool(), m_width, m_height, &m_swapChain)
{
	const char *texturePaths[] =
	{
		"resources/textures/head_albedo.dds",
		"resources/textures/head_normal.dds",
		"resources/textures/head_gloss.dds",
		"resources/textures/head_specular.dds",
		"resources/textures/head_cavity.dds",
		"resources/textures/head_detail_normal.dds",
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
		headMaterial.detailNormalScale = 130.0f;
		headMaterial.albedo = 0xFFFFFFFF;
		headMaterial.albedoTexture = 1;
		headMaterial.normalTexture = 2;
		headMaterial.glossTexture = 3;
		headMaterial.specularTexture = 4;
		headMaterial.cavityTexture = 5;
		headMaterial.detailNormalTexture = 6;

		Material jacketMaterial;
		jacketMaterial.gloss = 0.376f;
		jacketMaterial.specular = 0.162f;
		jacketMaterial.detailNormalScale = 0.0f;
		jacketMaterial.albedo = 0xFFFFFFFF;
		jacketMaterial.albedoTexture = 7;
		jacketMaterial.normalTexture = 8;
		jacketMaterial.glossTexture = 9;
		jacketMaterial.specularTexture = 10;
		jacketMaterial.cavityTexture = 0;
		jacketMaterial.detailNormalTexture = 0;

		Material browsMaterial;
		browsMaterial.gloss = 0.0f;
		browsMaterial.specular = 0.007f;
		browsMaterial.detailNormalScale = 0.0f;
		browsMaterial.albedo = glm::packUnorm4x8(glm::vec4(50.0f, 36.0f, 26.0f, 255.0f) / 255.0f);
		browsMaterial.albedoTexture = 0;
		browsMaterial.normalTexture = 0;
		browsMaterial.glossTexture = 0;
		browsMaterial.specularTexture = 0;
		browsMaterial.cavityTexture = 0;
		browsMaterial.detailNormalTexture = 0;

		Material eyelashesMaterial;
		eyelashesMaterial.gloss = 0.43f;
		eyelashesMaterial.specular = 0.162f;
		eyelashesMaterial.detailNormalScale = 0.0f;
		eyelashesMaterial.albedo = glm::packUnorm4x8(glm::vec4(4.0f, 4.0f, 4.0f, 255.0f) / 255.0f);
		eyelashesMaterial.albedoTexture = 0;
		eyelashesMaterial.normalTexture = 0;
		eyelashesMaterial.glossTexture = 0;
		eyelashesMaterial.specularTexture = 0;
		eyelashesMaterial.cavityTexture = 0;
		eyelashesMaterial.detailNormalTexture = 0;

		std::pair<Material, bool> materials[] = { {headMaterial, true}, {jacketMaterial, false}, { browsMaterial, false }, { eyelashesMaterial, false } };
		const char *meshPaths[] = { "resources/meshes/head.mesh", "resources/meshes/jacket.mesh", "resources/meshes/brows.mesh", "resources/meshes/eyelashes.mesh" };

		for (size_t i = 0; i < sizeof(meshPaths) / sizeof(meshPaths[0]); ++i)
		{
			m_meshes.push_back(Mesh::load(m_context.getPhysicalDevice(), m_context.getDevice(), m_context.getGraphicsQueue(), m_context.getGraphicsCommandPool(), meshPaths[i]));
			m_materials.push_back(materials[i]);
		}
	}

	const size_t textureCount = sizeof(texturePaths) / sizeof(texturePaths[0]);

	// update texture descriptor set
	{
		VkDescriptorImageInfo textureImageInfos[textureCount + 4];
		{
			for (size_t i = 0; i < textureCount; ++i)
			{
				auto &textureImageInfo = textureImageInfos[i];
				textureImageInfo.sampler = m_renderResources.m_linearSamplerRepeat;
				textureImageInfo.imageView = m_textures[i]->getView();
				textureImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}

			auto &brdfLutImageInfo = textureImageInfos[textureCount];
			brdfLutImageInfo.sampler = VK_NULL_HANDLE;
			brdfLutImageInfo.imageView = m_brdfLUT->getView();
			brdfLutImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			auto &radianceTexImageInfo = textureImageInfos[textureCount + 1];
			radianceTexImageInfo.sampler = VK_NULL_HANDLE;
			radianceTexImageInfo.imageView = m_radianceTexture->getView();
			radianceTexImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			auto &irradianceTexImageInfo = textureImageInfos[textureCount + 2];
			irradianceTexImageInfo.sampler = VK_NULL_HANDLE;
			irradianceTexImageInfo.imageView = m_irradianceTexture->getView();
			irradianceTexImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			auto &skyboxTexImageInfo = textureImageInfos[textureCount + 3];
			skyboxTexImageInfo.sampler = VK_NULL_HANDLE;
			skyboxTexImageInfo.imageView = m_skyboxTexture->getView();
			skyboxTexImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		}

		VkWriteDescriptorSet descriptorWrites[5];
		{
			auto &textureWrite = descriptorWrites[0];
			textureWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			textureWrite.dstSet = m_renderResources.m_textureDescriptorSet;
			textureWrite.dstBinding = 0;
			textureWrite.descriptorCount = textureCount;
			textureWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			textureWrite.pImageInfo = textureImageInfos;

			auto &brdfLutWrite = descriptorWrites[1];
			brdfLutWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			brdfLutWrite.dstSet = m_renderResources.m_textureDescriptorSet;
			brdfLutWrite.dstBinding = 1;
			brdfLutWrite.descriptorCount = 1;
			brdfLutWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			brdfLutWrite.pImageInfo = &textureImageInfos[textureCount];

			auto &radianceTexWrite = descriptorWrites[2];
			radianceTexWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			radianceTexWrite.dstSet = m_renderResources.m_textureDescriptorSet;
			radianceTexWrite.dstBinding = 2;
			radianceTexWrite.descriptorCount = 1;
			radianceTexWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			radianceTexWrite.pImageInfo = &textureImageInfos[textureCount + 1];

			auto &irradianceTexWrite = descriptorWrites[3];
			irradianceTexWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			irradianceTexWrite.dstSet = m_renderResources.m_textureDescriptorSet;
			irradianceTexWrite.dstBinding = 3;
			irradianceTexWrite.descriptorCount = 1;
			irradianceTexWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			irradianceTexWrite.pImageInfo = &textureImageInfos[textureCount + 2];

			auto &skyboxTexWrite = descriptorWrites[4];
			skyboxTexWrite = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
			skyboxTexWrite.dstSet = m_renderResources.m_textureDescriptorSet;
			skyboxTexWrite.dstBinding = 4;
			skyboxTexWrite.descriptorCount = 1;
			skyboxTexWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			skyboxTexWrite.pImageInfo = &textureImageInfos[textureCount + 3];
		}

		vkUpdateDescriptorSets(m_context.getDevice(), static_cast<uint32_t>(sizeof(descriptorWrites) / sizeof(descriptorWrites[0])), descriptorWrites, 0, nullptr);
	}

	// transition tonemapped output image to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL to be used as taa input
	transitionHistoryImages();

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
		init_info.DescriptorPool = m_renderResources.m_descriptorPool;
		init_info.Allocator = nullptr;
		init_info.MinImageCount = static_cast<uint32_t>(m_swapChain.getImageCount());
		init_info.ImageCount = static_cast<uint32_t>(m_swapChain.getImageCount());
		init_info.CheckVkResultFn = check_vk_result;
		ImGui_ImplVulkan_Init(&init_info, m_renderResources.m_guiRenderPass);

		auto cmdBuf = vkutil::beginSingleTimeCommands(m_context.getDevice(), m_context.getGraphicsCommandPool());
		ImGui_ImplVulkan_CreateFontsTexture(cmdBuf);
		vkutil::endSingleTimeCommands(m_context.getDevice(), m_context.getGraphicsQueue(), m_context.getGraphicsCommandPool(), cmdBuf);
	}

	// generate halton jitter offsets
	{
		auto halton = [](size_t index, size_t base)
		{
			float f = 1.0f;
			float r = 0.0f;

			while (index > 0)
			{
				f /= base;
				r += f * (index % base);
				index /= base;
			}

			return r;
		};

		for (size_t i = 0; i < 8; ++i)
		{
			m_haltonX[i] = halton(i + 1, 2) * 2.0f - 1.0f;
			m_haltonY[i] = halton(i + 1, 3) * 2.0f - 1.0f;
		}
	}
}

sss::vulkan::Renderer::~Renderer()
{
	vkDeviceWaitIdle(m_context.getDevice());
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void sss::vulkan::Renderer::render(const glm::mat4 &viewProjection, 
	const glm::mat4 &shadowMatrix, 
	const glm::vec4 &lightPositionRadius, 
	const glm::vec4 &lightColorInvSqrAttRadius, 
	const glm::vec4 &cameraPosition, 
	bool subsurfaceScatteringEnabled,
	float sssWidth,
	bool taaEnabled)
{
	RenderResources &rr = m_renderResources;
	uint32_t resourceIndex = m_frameIndex % FRAMES_IN_FLIGHT;

	const glm::mat4 jitterMatrix = glm::translate(glm::vec3(m_haltonX[m_frameIndex % 8] / m_width, m_haltonY[m_frameIndex % 8] / m_height, 0.0f));
	const glm::mat4 jitteredViewProjection = taaEnabled ? jitterMatrix * viewProjection : viewProjection;

	// wait until gpu finished work on all per frame resources
	vkWaitForFences(m_context.getDevice(), 1, &rr.m_frameFinishedFence[resourceIndex], VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(m_context.getDevice(), 1, &rr.m_frameFinishedFence[resourceIndex]);

	// retrieve gpu timestamp queries of sss passes
	if (m_frameIndex >= FRAMES_IN_FLIGHT)
	{
		uint64_t data[2];
		if (vkGetQueryPoolResults(m_context.getDevice(), rr.m_queryPool, resourceIndex, 2, sizeof(data), data, sizeof(data[0]), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT) != VK_SUCCESS)
		{
			util::fatalExit("Failed to rerieve gpu timestamp queries!", EXIT_FAILURE);
		}
		m_sssTime = static_cast<float>((data[1] - data[0]) * (m_context.getDeviceProperties().limits.timestampPeriod * (1.0 / 1e6)));
	}

	// update constant buffer content
	uint8_t *mappedPtr = rr.m_constantBuffer[resourceIndex]->map();
	((glm::mat4 *)mappedPtr)[0] = jitteredViewProjection;
	((glm::mat4 *)mappedPtr)[1] = shadowMatrix;
	((glm::vec4 *)mappedPtr)[8] = lightPositionRadius;
	((glm::vec4 *)mappedPtr)[9] = lightColorInvSqrAttRadius;
	((glm::vec4 *)mappedPtr)[10] = cameraPosition;

	// command buffer for the first half of the frame...
	vkResetCommandBuffer(rr.m_commandBuffers[resourceIndex * 2], 0);
	// ... and for the second half
	vkResetCommandBuffer(rr.m_commandBuffers[resourceIndex * 2 + 1], 0);

	VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VkCommandBuffer curCmdBuf = rr.m_commandBuffers[resourceIndex * 2];

	// swapchain image independent part of the frame
	vkBeginCommandBuffer(curCmdBuf, &beginInfo);
	{
		// shadow renderpass
		{
			VkClearValue clearValue;
			clearValue.depthStencil.depth = 1.0f;
			clearValue.depthStencil.stencil = 0;

			VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
			renderPassInfo.renderPass = rr.m_shadowRenderPass;
			renderPassInfo.framebuffer = rr.m_shadowFramebuffers[resourceIndex];
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = { SHADOW_RESOLUTION, SHADOW_RESOLUTION };
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearValue;

			vkCmdBeginRenderPass(curCmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			// actual shadow rendering
			{
				vkCmdBindPipeline(curCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rr.m_shadowPipeline.first);

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

					vkCmdPushConstants(curCmdBuf, rr.m_shadowPipeline.second, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(shadowMatrix), &shadowMatrix);

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
			renderPassInfo.renderPass = rr.m_mainRenderPass;
			renderPassInfo.framebuffer = rr.m_mainFramebuffers[resourceIndex];
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = { m_width, m_height };
			renderPassInfo.clearValueCount = 3;
			renderPassInfo.pClearValues = clearValues;

			vkCmdBeginRenderPass(curCmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			// lighting
			{
				vkCmdBindPipeline(curCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rr.m_lightingPipeline.first);

				VkDescriptorSet sets[] = { rr.m_textureDescriptorSet,  rr.m_lightingDescriptorSet[resourceIndex] };
				vkCmdBindDescriptorSets(curCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rr.m_lightingPipeline.second, 0, 2, sets, 0, nullptr);

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

					vkCmdPushConstants(curCmdBuf, rr.m_lightingPipeline.second, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(material.first), &material.first);

					vkCmdDrawIndexed(curCmdBuf, submesh->getIndexCount(), 1, 0, 0, 0);
				}
			}

			// sss lighting
			{
				vkCmdNextSubpass(curCmdBuf, VK_SUBPASS_CONTENTS_INLINE);

				vkCmdBindPipeline(curCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rr.m_sssLightingPipeline.first);

				VkDescriptorSet sets[] = { rr.m_textureDescriptorSet,  rr.m_lightingDescriptorSet[resourceIndex] };
				vkCmdBindDescriptorSets(curCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rr.m_sssLightingPipeline.second, 0, 2, sets, 0, nullptr);

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

					vkCmdPushConstants(curCmdBuf, rr.m_sssLightingPipeline.second, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(material.first), &material.first);

					vkCmdDrawIndexed(curCmdBuf, submesh->getIndexCount(), 1, 0, 0, 0);
				}
			}

			// skybox
			{
				vkCmdNextSubpass(curCmdBuf, VK_SUBPASS_CONTENTS_INLINE);

				vkCmdBindPipeline(curCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rr.m_skyboxPipeline.first);

				vkCmdBindDescriptorSets(curCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rr.m_skyboxPipeline.second, 0, 1, &rr.m_textureDescriptorSet, 0, nullptr);

				VkViewport viewport{ 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
				VkRect2D scissor{ { 0, 0 }, { m_width, m_height } };

				vkCmdSetViewport(curCmdBuf, 0, 1, &viewport);
				vkCmdSetScissor(curCmdBuf, 0, 1, &scissor);

				glm::mat4 invModelViewProjectionMatrix = glm::inverse(jitteredViewProjection);

				vkCmdPushConstants(curCmdBuf, rr.m_skyboxPipeline.second, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(invModelViewProjectionMatrix), &invModelViewProjectionMatrix);

				vkCmdDraw(curCmdBuf, 3, 1, 0, 0);
			}
			vkCmdEndRenderPass(curCmdBuf);
		}

		vkCmdResetQueryPool(curCmdBuf, rr.m_queryPool, resourceIndex, 2);
		vkCmdWriteTimestamp(curCmdBuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, rr.m_queryPool, resourceIndex);

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
					imageBarrier.image = rr.m_diffuse1Image[resourceIndex]->getImage();
					imageBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

					vkCmdPipelineBarrier(curCmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);
				}

				vkCmdBindPipeline(curCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, rr.m_sssBlurPipeline0.first);

				vkCmdBindDescriptorSets(curCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, rr.m_sssBlurPipeline0.second, 0, 1, &rr.m_sssBlurDescriptorSet[resourceIndex * 2], 0, nullptr);

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
				pushConsts.sssWidth = sssWidth * 1.0f / tanf(glm::radians(40.0f) * 0.5f) * (m_height / static_cast<float>(m_width));

				vkCmdPushConstants(curCmdBuf, rr.m_sssBlurPipeline0.second, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

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
					imageBarriers[0].image = rr.m_diffuse1Image[resourceIndex]->getImage();
					imageBarriers[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

					// transition diffuse0 image layout to VK_IMAGE_LAYOUT_GENERAL
					imageBarriers[1] = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
					imageBarriers[1].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
					imageBarriers[1].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
					imageBarriers[1].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					imageBarriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
					imageBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					imageBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					imageBarriers[1].image = rr.m_diffuse0Image[resourceIndex]->getImage();
					imageBarriers[1].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

					vkCmdPipelineBarrier(curCmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 2, imageBarriers);
				}

				vkCmdBindPipeline(curCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, rr.m_sssBlurPipeline1.first);

				vkCmdBindDescriptorSets(curCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, rr.m_sssBlurPipeline1.second, 0, 1, &rr.m_sssBlurDescriptorSet[resourceIndex * 2 + 1], 0, nullptr);

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
				pushConsts.sssWidth = sssWidth * 1.0f / tanf(glm::radians(40.0f) * 0.5f) * (m_height / static_cast<float>(m_width));

				vkCmdPushConstants(curCmdBuf, rr.m_sssBlurPipeline1.second, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

				vkCmdDispatch(curCmdBuf, (m_width + 15) / 16, (m_height + 15) / 16, 1);
			}
		}

		vkCmdWriteTimestamp(curCmdBuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, rr.m_queryPool, resourceIndex + 1);

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
				imageBarriers[0].image = rr.m_tonemappedImage[resourceIndex]->getImage();
				imageBarriers[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

				// transition diffuse0 image layout to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
				imageBarriers[1] = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
				imageBarriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
				imageBarriers[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				imageBarriers[1].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
				imageBarriers[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageBarriers[1].image = rr.m_diffuse0Image[resourceIndex]->getImage();
				imageBarriers[1].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

				vkCmdPipelineBarrier(curCmdBuf, subsurfaceScatteringEnabled ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, subsurfaceScatteringEnabled ? 2 : 1, imageBarriers);
			}

			vkCmdBindPipeline(curCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, rr.m_posprocessingPipeline.first);

			vkCmdBindDescriptorSets(curCmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, rr.m_posprocessingPipeline.second, 0, 1, &rr.m_postprocessingDescriptorSet[resourceIndex], 0, nullptr);

			using namespace glm;
			struct PushConsts
			{
				mat4 reprojectionMatrix;
				vec2 texelSize;
				float exposure;
				uint taa;
			};

			PushConsts pushConsts;
			pushConsts.reprojectionMatrix = m_previousViewProjection * glm::inverse(viewProjection);
			pushConsts.texelSize = 1.0f / glm::vec2(m_width, m_height);
			pushConsts.exposure = 1.0f;
			pushConsts.taa = taaEnabled ? 1 : 0;

			vkCmdPushConstants(curCmdBuf, rr.m_posprocessingPipeline.second, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConsts), &pushConsts);

			vkCmdDispatch(curCmdBuf, (m_width + 15) / 16, (m_height + 15) / 16, 1);
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
		VkResult result = vkAcquireNextImageKHR(m_context.getDevice(), m_swapChain, std::numeric_limits<uint64_t>::max(), rr.m_swapChainImageAvailableSemaphores[resourceIndex], VK_NULL_HANDLE, &swapChainImageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		{
			m_swapChain.recreate(m_width, m_height);
			return;
		}
		else if (result != VK_SUCCESS)
		{
			util::fatalExit("Failed to acquire swap chain image!", EXIT_FAILURE);
		}
	}


	curCmdBuf = rr.m_commandBuffers[resourceIndex * 2 + 1];

	// swapchain image dependent part of the frame
	vkBeginCommandBuffer(curCmdBuf, &beginInfo);
	{
		// barriers
		{
			VkImageMemoryBarrier imageBarriers[2];
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

			// transition tonemapped image layout to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
			imageBarriers[1] = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			imageBarriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			imageBarriers[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			imageBarriers[1].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			imageBarriers[1].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			imageBarriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageBarriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageBarriers[1].image = rr.m_tonemappedImage[resourceIndex]->getImage();
			imageBarriers[1].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

			vkCmdPipelineBarrier(curCmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2, imageBarriers);
		}

		// blit color image to backbuffer
		{
			VkImageBlit region{};
			region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			region.srcOffsets[1] = { static_cast<int32_t>(m_width), static_cast<int32_t>(m_height), 1 };
			region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
			region.dstOffsets[1] = { static_cast<int32_t>(m_width), static_cast<int32_t>(m_height), 1 };

			vkCmdBlitImage(curCmdBuf, rr.m_tonemappedImage[resourceIndex]->getImage(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_swapChain.getImage(swapChainImageIndex), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_NEAREST);
		}

		// gui renderpass
		{
			VkClearValue clearValue;

			VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
			renderPassInfo.renderPass = rr.m_guiRenderPass;
			renderPassInfo.framebuffer = rr.m_guiFramebuffers[swapChainImageIndex];
			renderPassInfo.renderArea.offset = { 0, 0 };
			renderPassInfo.renderArea.extent = { m_width, m_height };
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearValue;

			vkCmdBeginRenderPass(curCmdBuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), curCmdBuf);

			vkCmdEndRenderPass(curCmdBuf);
		}

		// transition tonemapped image layout to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL for taa in next frame
		{
			VkImageMemoryBarrier imageBarriers[1];
			imageBarriers[0] = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
			imageBarriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			imageBarriers[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			imageBarriers[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			imageBarriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageBarriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageBarriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageBarriers[0].image = rr.m_tonemappedImage[resourceIndex]->getImage();
			imageBarriers[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

			vkCmdPipelineBarrier(curCmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, imageBarriers);
		}
		
	}
	vkEndCommandBuffer(curCmdBuf);

	// submit swapchain image dependent work to queue
	{
		VkPipelineStageFlags waitStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;

		VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &rr.m_swapChainImageAvailableSemaphores[resourceIndex];
		submitInfo.pWaitDstStageMask = &waitStageFlags;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &curCmdBuf;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &rr.m_renderFinishedSemaphores[resourceIndex];

		if (vkQueueSubmit(m_context.getGraphicsQueue(), 1, &submitInfo, rr.m_frameFinishedFence[resourceIndex]) != VK_SUCCESS)
		{
			util::fatalExit("Failed to submit to queue!", EXIT_FAILURE);
		}
	}

	// present swapchain image
	{
		VkSwapchainKHR swapChain = m_swapChain;

		VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &rr.m_renderFinishedSemaphores[resourceIndex];
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &swapChain;
		presentInfo.pImageIndices = &swapChainImageIndex;

		VkResult result = vkQueuePresentKHR(m_context.getGraphicsQueue(), &presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		{
			m_swapChain.recreate(m_width, m_height);
			return;
		}
		else if (result != VK_SUCCESS)
		{
			util::fatalExit("Failed to present swap chain image!", EXIT_FAILURE);
		}
	}

	++m_frameIndex;
	m_previousViewProjection = viewProjection;
}

float sss::vulkan::Renderer::getSSSEffectTiming() const
{
	return m_sssTime;
}

void sss::vulkan::Renderer::resize(uint32_t width, uint32_t height)
{
	m_swapChain.recreate(width, height);
	m_renderResources.resize(width, height);
	m_width = width;
	m_height = height;
	transitionHistoryImages();
}

void sss::vulkan::Renderer::transitionHistoryImages()
{
	// transition tonemapped output image to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL to be used as taa input
	{
		auto cmdBuf = vkutil::beginSingleTimeCommands(m_context.getDevice(), m_context.getGraphicsCommandPool());
		{
			VkImageMemoryBarrier imageBarriers[FRAMES_IN_FLIGHT];
			for (size_t i = 0; i < FRAMES_IN_FLIGHT; ++i)
			{
				imageBarriers[i] = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
				imageBarriers[i].srcAccessMask = 0;
				imageBarriers[i].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
				imageBarriers[i].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				imageBarriers[i].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				imageBarriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageBarriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageBarriers[i].image = m_renderResources.m_tonemappedImage[i]->getImage();
				imageBarriers[i].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			}

			vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, FRAMES_IN_FLIGHT, imageBarriers);
		}
		vkutil::endSingleTimeCommands(m_context.getDevice(), m_context.getGraphicsQueue(), m_context.getGraphicsCommandPool(), cmdBuf);
	}
}
