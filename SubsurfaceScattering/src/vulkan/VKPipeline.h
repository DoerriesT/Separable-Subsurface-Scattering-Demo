#pragma once
#include "volk.h"
#include <cstring>
#include <bitset>

namespace sss
{
	namespace vulkan
	{
		struct VKShaderStageDescription
		{
			enum { MAX_PATH_LENGTH = 255 };

			class SpecializationInfo
			{
			public:
				enum { MAX_ENTRY_COUNT = 32 };

				explicit SpecializationInfo();

				void addEntry(uint32_t constantID, int32_t value);
				void addEntry(uint32_t constantID, uint32_t value);
				void addEntry(uint32_t constantID, float value);
				const VkSpecializationInfo *getInfo() const;

			private:
				VkSpecializationInfo m_info = {};
				uint8_t m_data[MAX_ENTRY_COUNT * 4] = {};
				VkSpecializationMapEntry m_entries[MAX_ENTRY_COUNT] = {};

				void addEntry(uint32_t constantID, void *value);
			};

			char m_path[MAX_PATH_LENGTH + 1] = {};
			SpecializationInfo m_specializationInfo;
		};

		struct VKRenderPassDescription
		{
			enum
			{
				MAX_INPUT_ATTACHMENTS = 8,
				MAX_COLOR_ATTACHMENTS = 8,
				MAX_ATTACHMENTS = MAX_INPUT_ATTACHMENTS + MAX_COLOR_ATTACHMENTS + 1, // +1 is for depth attachment
			};

			struct AttachmentDescription
			{
				VkFormat m_format;
				VkSampleCountFlagBits m_samples;
			};

			uint32_t m_attachmentCount = 0;
			AttachmentDescription m_attachments[MAX_ATTACHMENTS] = {};
			uint32_t m_inputAttachmentCount = 0;
			VkAttachmentReference m_inputAttachments[MAX_INPUT_ATTACHMENTS] = {};
			uint32_t m_colorAttachmentCount = 0;
			VkAttachmentReference m_colorAttachments[MAX_COLOR_ATTACHMENTS] = {};
			std::bitset<MAX_COLOR_ATTACHMENTS> m_resolveAttachments;
			bool m_depthStencilAttachmentPresent = false;
			VkAttachmentReference m_depthStencilAttachment = {};
			size_t m_hashValue;

			// cleans up array elements past array count and precomputes a hash value
			void finalize();
		};

		struct VKGraphicsPipelineDescription
		{
			enum
			{
				MAX_VERTEX_BINDING_DESCRIPTIONS = 8,
				MAX_VERTEX_ATTRIBUTE_DESCRIPTIONS = 8,
				MAX_VIEWPORTS = 1,
				MAX_SCISSORS = 1,
				MAX_COLOR_BLEND_ATTACHMENT_STATES = 8,
				MAX_DYNAMIC_STATES = 9,
			};

			struct VertexInputState
			{
				uint32_t m_vertexBindingDescriptionCount = 0;
				VkVertexInputBindingDescription m_vertexBindingDescriptions[MAX_VERTEX_BINDING_DESCRIPTIONS] = {};
				uint32_t m_vertexAttributeDescriptionCount = 0;
				VkVertexInputAttributeDescription m_vertexAttributeDescriptions[MAX_VERTEX_ATTRIBUTE_DESCRIPTIONS] = {};
			};

			struct InputAssemblyState
			{
				VkPrimitiveTopology m_primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
				bool m_primitiveRestartEnable = false;
			};

			struct TesselationState
			{
				uint32_t m_patchControlPoints = 0;
			};

			struct ViewportState
			{
				uint32_t m_viewportCount = 0;
				VkViewport m_viewports[MAX_VIEWPORTS] = {};
				uint32_t m_scissorCount = 0;
				VkRect2D m_scissors[MAX_SCISSORS] = {};
			};

			struct RasterizationState
			{
				bool m_depthClampEnable = false;
				bool m_rasterizerDiscardEnable = false;
				VkPolygonMode m_polygonMode = VK_POLYGON_MODE_FILL;
				VkCullModeFlags m_cullMode = VK_CULL_MODE_NONE;
				VkFrontFace m_frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
				bool m_depthBiasEnable = false;
				float m_depthBiasConstantFactor = 1.0f;
				float m_depthBiasClamp = 0.0f;
				float m_depthBiasSlopeFactor = 1.0f;
				float m_lineWidth = 1.0f;
			};

			struct MultisampleState
			{
				VkSampleCountFlagBits m_rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
				bool m_sampleShadingEnable = false;
				float m_minSampleShading = 0.0f;
				VkSampleMask m_sampleMask = 0xFFFFFFFF;
				bool m_alphaToCoverageEnable = false;
				bool m_alphaToOneEnable = false;
			};

			struct DepthStencilState
			{
				bool m_depthTestEnable = false;
				bool m_depthWriteEnable = false;
				VkCompareOp m_depthCompareOp = VK_COMPARE_OP_ALWAYS;
				bool m_depthBoundsTestEnable = false;
				bool m_stencilTestEnable = false;
				VkStencilOpState m_front = {};
				VkStencilOpState m_back = {};
				float m_minDepthBounds = 0.0f;
				float m_maxDepthBounds = 1.0f;
			};

			struct BlendState
			{
				bool m_logicOpEnable = false;
				VkLogicOp m_logicOp = VK_LOGIC_OP_COPY;
				uint32_t m_attachmentCount = 0;
				VkPipelineColorBlendAttachmentState m_attachments[MAX_COLOR_BLEND_ATTACHMENT_STATES] = {};
				float m_blendConstants[4] = {};
			};

			struct DynamicState
			{
				uint32_t m_dynamicStateCount = 0;
				VkDynamicState m_dynamicStates[MAX_DYNAMIC_STATES] = {};
			};

			VKShaderStageDescription m_vertexShaderStage;
			VKShaderStageDescription m_tesselationControlShaderStage;
			VKShaderStageDescription m_tesselationEvaluationShaderStage;
			VKShaderStageDescription m_geometryShaderStage;
			VKShaderStageDescription m_fragmentShaderStage;
			VertexInputState m_vertexInputState;
			InputAssemblyState m_inputAssemblyState;
			TesselationState m_tesselationState;
			ViewportState m_viewportState;
			RasterizationState m_rasterizationState;
			MultisampleState m_multiSampleState;
			DepthStencilState m_depthStencilState;
			BlendState m_blendState;
			DynamicState m_dynamicState;
			size_t m_hashValue;

			// cleans up array elements past array count and precomputes a hash value
			void finalize();
		};

		struct VKComputePipelineDescription
		{
			VKShaderStageDescription m_computeShaderStage;
			size_t m_hashValue;

			// cleans up array elements past array count and precomputes a hash value
			void finalize();
		};

		struct VKCombinedGraphicsPipelineRenderPassDescription
		{
			VKGraphicsPipelineDescription m_graphicsPipelineDescription;
			VKRenderPassDescription m_renderPassDescription;
			uint32_t m_subpassIndex;
		};

		inline bool operator==(const VKRenderPassDescription &lhs, const VKRenderPassDescription &rhs)
		{
			return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
		}

		inline bool operator==(const VKGraphicsPipelineDescription &lhs, const VKGraphicsPipelineDescription &rhs)
		{
			return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
		}

		inline bool operator==(const VKComputePipelineDescription &lhs, const VKComputePipelineDescription &rhs)
		{
			return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
		}

		inline bool operator==(const VKCombinedGraphicsPipelineRenderPassDescription &lhs, const VKCombinedGraphicsPipelineRenderPassDescription &rhs)
		{
			return memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
		}

		struct VKRenderPassDescriptionHash
		{
			size_t operator()(const VKRenderPassDescription &value) const;
		};

		struct VKGraphicsPipelineDescriptionHash
		{
			size_t operator()(const VKGraphicsPipelineDescription &value) const;
		};

		struct VKComputePipelineDescriptionHash
		{
			size_t operator()(const VKComputePipelineDescription &value) const;
		};

		struct VKCombinedGraphicsPipelineRenderPassDescriptionHash
		{
			size_t operator()(const VKCombinedGraphicsPipelineRenderPassDescription &value) const;
		};
	}
}