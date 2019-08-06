#include "VKLightingPass.h"
#include "vulkan/VKPipelineCache.h"

void sss::vulkan::VKLightingPass::execute(VkCommandBuffer cmdBuf, VKPipelineCache &pipelineCache, const VKRenderPassDescription &renderPassDesc, VkRenderPass renderPass, const Data &data)
{
	// create pipeline description
	VKGraphicsPipelineDescription pipelineDesc;
	{
		strcpy_s(pipelineDesc.m_vertexShaderStage.m_path, "Resources/Shaders/geometry_vert.spv");
		strcpy_s(pipelineDesc.m_fragmentShaderStage.m_path, "Resources/Shaders/geometry_frag.spv");

		pipelineDesc.m_vertexInputState.m_vertexBindingDescriptionCount = 3;
		pipelineDesc.m_vertexInputState.m_vertexBindingDescriptions[0] = { 0, 3 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX };
		pipelineDesc.m_vertexInputState.m_vertexBindingDescriptions[1] = { 1, 3 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX };
		pipelineDesc.m_vertexInputState.m_vertexBindingDescriptions[2] = { 2, 2 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX };

		pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptionCount = 3;
		pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptions[0] = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 };
		pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptions[1] = { 1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0 };
		pipelineDesc.m_vertexInputState.m_vertexAttributeDescriptions[2] = { 2, 2, VK_FORMAT_R32G32_SFLOAT, 0 };

		pipelineDesc.m_inputAssemblyState.m_primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		pipelineDesc.m_inputAssemblyState.m_primitiveRestartEnable = false;

		pipelineDesc.m_viewportState.m_viewportCount = 1;
		pipelineDesc.m_viewportState.m_viewports[0] = { 0.0f, 0.0f, static_cast<float>(data.m_width), static_cast<float>(data.m_height), 0.0f, 1.0f };
		pipelineDesc.m_viewportState.m_scissorCount = 1;
		pipelineDesc.m_viewportState.m_scissors[0] = { {0, 0}, {data.m_width, data.m_height} };

		pipelineDesc.m_rasterizationState.m_depthClampEnable = false;
		pipelineDesc.m_rasterizationState.m_rasterizerDiscardEnable = false;
		pipelineDesc.m_rasterizationState.m_polygonMode = VK_POLYGON_MODE_FILL;
		pipelineDesc.m_rasterizationState.m_cullMode = VK_CULL_MODE_BACK_BIT;
		pipelineDesc.m_rasterizationState.m_frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		pipelineDesc.m_rasterizationState.m_depthBiasEnable = false;
		pipelineDesc.m_rasterizationState.m_lineWidth = 1.0f;

		pipelineDesc.m_multiSampleState.m_rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		pipelineDesc.m_multiSampleState.m_sampleShadingEnable = false;
		pipelineDesc.m_multiSampleState.m_sampleMask = 0xFFFFFFFF;

		pipelineDesc.m_depthStencilState.m_depthTestEnable = true;
		pipelineDesc.m_depthStencilState.m_depthWriteEnable = true;
		pipelineDesc.m_depthStencilState.m_depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
		pipelineDesc.m_depthStencilState.m_depthBoundsTestEnable = false;
		pipelineDesc.m_depthStencilState.m_stencilTestEnable = false;

		VkPipelineColorBlendAttachmentState defaultBlendAttachment = {};
		defaultBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		defaultBlendAttachment.blendEnable = VK_FALSE;

		pipelineDesc.m_blendState.m_logicOpEnable = false;
		pipelineDesc.m_blendState.m_logicOp = VK_LOGIC_OP_COPY;
		pipelineDesc.m_blendState.m_attachmentCount = 1;
		pipelineDesc.m_blendState.m_attachments[0] = defaultBlendAttachment;

		pipelineDesc.m_dynamicState.m_dynamicStateCount = 0;

		pipelineDesc.finalize();
	}

	auto pipelineData = pipelineCache.getPipeline(pipelineDesc, renderPassDesc, renderPass, 0);

	vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineData.m_pipeline);

}
