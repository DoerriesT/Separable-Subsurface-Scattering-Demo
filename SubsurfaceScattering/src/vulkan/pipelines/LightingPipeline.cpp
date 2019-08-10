#include "LightingPipeline.h"
#include "utility/Utility.h"
#include "ShaderModule.h"

std::pair<VkPipeline, VkPipelineLayout> sss::vulkan::LightingPipeline::create(VkDevice device, VkRenderPass renderPass, uint32_t subpassIndex, uint32_t setLayoutCount, VkDescriptorSetLayout *setLayouts)
{
	VkPipelineLayout pipelineLayout;

	VkPipelineLayoutCreateInfo layoutCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	layoutCreateInfo.setLayoutCount = setLayoutCount;
	layoutCreateInfo.pSetLayouts = setLayouts;
	//layoutCreateInfo.pushConstantRangeCount = reflectionInfo.m_pushConstants.m_size > 0 ? 1 : 0;
	//layoutCreateInfo.pPushConstantRanges = reflectionInfo.m_pushConstants.m_size > 0 ? &pushConstantRange : nullptr;

	if (vkCreatePipelineLayout(device, &layoutCreateInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
	{
		util::fatalExit("Failed to create PipelineLayout!", EXIT_FAILURE);
	}

	ShaderModule vertexShaderModule(device, "resources/shaders/lighting_vert.spv");
	ShaderModule fragmentShaderModule(device, "resources/shaders/lighting_frag.spv");

	VkPipelineShaderStageCreateInfo shaderStages[] =
	{
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertexShaderModule, "main" },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShaderModule, "main" },
	};

	VkVertexInputBindingDescription bindingDescriptions[] =
	{
		{ 0, sizeof(float) * 3, VK_VERTEX_INPUT_RATE_VERTEX },
		{ 1, sizeof(float) * 3, VK_VERTEX_INPUT_RATE_VERTEX },
		{ 2, sizeof(float) * 2, VK_VERTEX_INPUT_RATE_VERTEX }
	};

	VkVertexInputAttributeDescription attributeDescriptions[] = 
	{
		{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
		{ 1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0 },
		{ 2, 2, VK_FORMAT_R32G32_SFLOAT, 0 }
	};

	VkPipelineVertexInputStateCreateInfo vertexInputState{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vertexInputState.vertexBindingDescriptionCount = 3;
	vertexInputState.pVertexBindingDescriptions = bindingDescriptions;
	vertexInputState.vertexAttributeDescriptionCount = 3;
	vertexInputState.pVertexAttributeDescriptions = attributeDescriptions;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkViewport viewport{ 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f };
	VkRect2D scissor{ {0, 0}, {1, 1} };

	VkPipelineViewportStateCreateInfo viewportState{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizationState{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationState.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisamplingState{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	multisamplingState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo depthStencilState{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	depthStencilState.depthTestEnable = VK_TRUE;
	depthStencilState.depthWriteEnable = VK_TRUE;
	depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

	VkPipelineColorBlendAttachmentState defaultBlendAttachment{};
	defaultBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	defaultBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo blendState{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	blendState.attachmentCount = 1;
	blendState.pAttachments = &defaultBlendAttachment;

	VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

	VkPipelineDynamicStateCreateInfo dynamicState{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicStates;

	VkGraphicsPipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputState;
	pipelineInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizationState;
	pipelineInfo.pMultisampleState = &multisamplingState;
	pipelineInfo.pDepthStencilState = &depthStencilState;
	pipelineInfo.pColorBlendState = &blendState;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = pipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = subpassIndex;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = 0;

	VkPipeline pipeline;
	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
	{
		util::fatalExit("Failed to create pipeline!", EXIT_FAILURE);
	}

	return { pipeline, pipelineLayout };
}
