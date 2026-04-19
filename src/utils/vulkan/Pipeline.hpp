#pragma once

#include <vector>
#include <unordered_map>
#include <vulkan/vulkan.h>
#include "VK.hpp"
#include "TextureManager.hpp"
#include "PosColVertex.hpp"
#include "Vertex.hpp"
#include "RTG.hpp"

class ShadowBufferManager;

struct Pipeline
{
    struct BlockDescriptorConfig
    {
        VkDescriptorType type;
		// Optional per-binding descriptor types. If non-empty, size should match bindings_count.
		std::vector<VkDescriptorType> binding_types;
        VkDescriptorSetLayout layout;
		uint32_t bindings_count; //names of bindings in this block
    };

	struct ManagerContext {
		const TextureManager* texture_manager;
		const ShadowBufferManager* shadow_buffer_manager;
	};

    VkPipelineLayout layout = VK_NULL_HANDLE;	
    VkPipeline pipeline = VK_NULL_HANDLE;

    std::vector<BlockDescriptorConfig> block_descriptor_configs{};
	std::unordered_map<std::string, uint32_t> block_descriptor_set_name_to_index{};
	std::unordered_map<std::string, uint32_t> block_binding_name_to_index{};
	std::unordered_map<std::string, uint32_t> data_buffer_name_to_index{};

    VkShaderModule frag_module;
    VkShaderModule vert_module;

    virtual void create(
		RTG &, 
		VkRenderPass render_pass, 
		uint32_t subpass,
		const ManagerContext& context
	) = 0;
    virtual void destroy(RTG &) = 0;
    
	void create_pipeline(RTG& rtg, VkRenderPass render_pass, uint32_t subpass, bool enable_depth = true, bool enable_cull = true, bool lines_draw = false, uint32_t color_attachment_count = 1, bool enable_fragment_stage = true, bool enable_vertex_attributes = true) {
        //shader code for vertex and fragment pipeline stages:
		std::vector< VkPipelineShaderStageCreateInfo > stages;
		stages.emplace_back(VkPipelineShaderStageCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vert_module,
			.pName = "main"
		});

		if (enable_fragment_stage) {
			stages.emplace_back(VkPipelineShaderStageCreateInfo{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
				.module = frag_module,
				.pName = "main"
			});
		}

		//the viewport and scissor state will be set at runtime for the pipeline:
		std::vector< VkDynamicState > dynamic_states{
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamic_state{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = uint32_t(dynamic_states.size()),
			.pDynamicStates = dynamic_states.data()
		};

		//this pipeline will draw triangles:
		VkPipelineInputAssemblyStateCreateInfo input_assembly_state{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = lines_draw ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.primitiveRestartEnable = VK_FALSE
		};

		//this pipeline will render to one viewport and scissor rectangle:
		VkPipelineViewportStateCreateInfo viewport_state{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			.scissorCount = 1,
		};

		//the rasterizer will cull back faces and fill polygons:
		VkPipelineRasterizationStateCreateInfo rasterization_state{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.depthClampEnable = VK_FALSE,
			.rasterizerDiscardEnable = VK_FALSE,
			.polygonMode =  lines_draw ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL,
			.cullMode = static_cast<VkCullModeFlags>(
				enable_cull ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE
			),
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
			.depthBiasEnable = VK_FALSE,
			.lineWidth = 1.0f,
		};

		//multisampling will be disabled (one sample per pixel):
		VkPipelineMultisampleStateCreateInfo multisample_state{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
			.sampleShadingEnable = VK_FALSE,
		};

		//depth and stencil tests will be disabled:
		VkPipelineDepthStencilStateCreateInfo depth_stencil_state{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable = enable_depth ? VK_TRUE : VK_FALSE,
			.depthWriteEnable = enable_depth ? VK_TRUE : VK_FALSE,
			.depthCompareOp = enable_depth ? (rtg.configuration.reverse_z ? VK_COMPARE_OP_GREATER : VK_COMPARE_OP_LESS) : VK_COMPARE_OP_ALWAYS,
			.depthBoundsTestEnable = VK_FALSE,
			.stencilTestEnable = VK_FALSE,
		};

		//there will be one color attachment with blending disabled:
		VkPipelineColorBlendAttachmentState attachment_state{
			.blendEnable = VK_FALSE,
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
		};
		std::vector<VkPipelineColorBlendAttachmentState> attachment_states(color_attachment_count, attachment_state);
		VkPipelineColorBlendStateCreateInfo color_blend_state{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.logicOpEnable = VK_FALSE,
			.attachmentCount = color_attachment_count,
			.pAttachments = color_attachment_count > 0 ? attachment_states.data() : nullptr,
			.blendConstants{0.0f, 0.0f, 0.0f, 0.0f},
		};

		//all of the above structures get bundled together into one very large create_info:
		VkPipelineVertexInputStateCreateInfo empty_vertex_input_state{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = 0,
			.pVertexBindingDescriptions = nullptr,
			.vertexAttributeDescriptionCount = 0,
			.pVertexAttributeDescriptions = nullptr,
		};

		VkGraphicsPipelineCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.stageCount = uint32_t(stages.size()),
			.pStages = stages.data(),
			.pVertexInputState = enable_vertex_attributes
				? (lines_draw ? &PosColVertex::array_input_state : &Vertex::array_input_state)
				: &empty_vertex_input_state,
			.pInputAssemblyState = &input_assembly_state,
			.pViewportState = &viewport_state,
			.pRasterizationState = &rasterization_state,
			.pMultisampleState = &multisample_state,
			.pDepthStencilState = &depth_stencil_state,
			.pColorBlendState = &color_blend_state,
			.pDynamicState = &dynamic_state,
			.layout = layout,
			.renderPass = render_pass,
			.subpass = subpass,
		};

		VK( vkCreateGraphicsPipelines(rtg.device, VK_NULL_HANDLE, 1, &create_info, nullptr, &pipeline) );
    }
};
