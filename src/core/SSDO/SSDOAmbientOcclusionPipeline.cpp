#include "SSDOAmbientOcclusionPipeline.hpp"

#include "VK.hpp"

#include <array>
#include <vector>

static uint32_t vert_code[] = {
#include "../../shaders/spv/SSDO-ao.vert.inl"
};

static uint32_t frag_code[] = {
#include "../../shaders/spv/SSDO-ao.frag.inl"
};

void SSDOAmbientOcclusionPipeline::create(
    RTG &rtg,
    VkRenderPass render_pass,
    uint32_t subpass,
    const ManagerContext& context
) {
    auto const &texture_manager = *context.texture_manager;

    vert_module = rtg.helpers.create_shader_module(vert_code);
    frag_module = rtg.helpers.create_shader_module(frag_code);

    { // set0: per-workspace PV UBO + kernel samples UBO
        std::array<VkDescriptorSetLayoutBinding, 2> bindings{
            VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
            VkDescriptorSetLayoutBinding{
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
        };

        VkDescriptorSetLayoutCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = uint32_t(bindings.size()),
            .pBindings = bindings.data(),
        };

        VK(vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set0_PV));
    }

    { // set1: depth + normal + albedo from GBuffer
        std::array<VkDescriptorSetLayoutBinding, 3> bindings{
            VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
            VkDescriptorSetLayoutBinding{
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
            VkDescriptorSetLayoutBinding{
                .binding = 2,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
        };

        VkDescriptorSetLayoutCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = uint32_t(bindings.size()),
            .pBindings = bindings.data(),
        };

        VK(vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set1_GBuffer));

        VkDescriptorSetAllocateInfo alloc_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = texture_manager.texture_descriptor_pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &set1_GBuffer,
        };

        VK(vkAllocateDescriptorSets(rtg.device, &alloc_info, &set1_GBuffer_instance));
    }

    { // set2: small tiled noise texture
        std::array<VkDescriptorSetLayoutBinding, 1> bindings{
            VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
        };

        VkDescriptorSetLayoutCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = uint32_t(bindings.size()),
            .pBindings = bindings.data(),
        };
        VK(vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set2_Noise));

        VkDescriptorSetAllocateInfo alloc_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = texture_manager.texture_descriptor_pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &set2_Noise,
        };
        VK(vkAllocateDescriptorSets(rtg.device, &alloc_info, &set2_Noise_instance));
    }

    { // pipeline layout
        std::array<VkDescriptorSetLayout, 3> layouts{
            set0_PV,
            set1_GBuffer,
            set2_Noise,
        };

        VkPipelineLayoutCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = uint32_t(layouts.size()),
            .pSetLayouts = layouts.data(),
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr,
        };

        VK(vkCreatePipelineLayout(rtg.device, &create_info, nullptr, &layout));
    }

    { // fullscreen pipeline
        std::array<VkPipelineShaderStageCreateInfo, 2> stages{
            VkPipelineShaderStageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vert_module,
                .pName = "main",
            },
            VkPipelineShaderStageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = frag_module,
                .pName = "main",
            },
        };

        VkPipelineVertexInputStateCreateInfo vertex_input_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 0,
            .pVertexBindingDescriptions = nullptr,
            .vertexAttributeDescriptionCount = 0,
            .pVertexAttributeDescriptions = nullptr,
        };

        std::vector<VkDynamicState> dynamic_states{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
        VkPipelineDynamicStateCreateInfo dynamic_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = uint32_t(dynamic_states.size()),
            .pDynamicStates = dynamic_states.data(),
        };

        VkPipelineInputAssemblyStateCreateInfo input_assembly_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE,
        };

        VkPipelineViewportStateCreateInfo viewport_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .scissorCount = 1,
        };

        VkPipelineRasterizationStateCreateInfo rasterization_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .depthBiasEnable = VK_FALSE,
            .lineWidth = 1.0f,
        };

        VkPipelineMultisampleStateCreateInfo multisample_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE,
        };

        VkPipelineDepthStencilStateCreateInfo depth_stencil_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_FALSE,
            .depthWriteEnable = VK_FALSE,
            .depthCompareOp = VK_COMPARE_OP_ALWAYS,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE,
        };

        std::array<VkPipelineColorBlendAttachmentState, 1> attachment_states{
            VkPipelineColorBlendAttachmentState{
                .blendEnable = VK_FALSE,
                .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            },
        };

        VkPipelineColorBlendStateCreateInfo color_blend_state{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = VK_FALSE,
            .attachmentCount = uint32_t(attachment_states.size()),
            .pAttachments = attachment_states.data(),
            .blendConstants{0.0f, 0.0f, 0.0f, 0.0f},
        };

        VkGraphicsPipelineCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = uint32_t(stages.size()),
            .pStages = stages.data(),
            .pVertexInputState = &vertex_input_state,
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

        VK(vkCreateGraphicsPipelines(rtg.device, VK_NULL_HANDLE, 1, &create_info, nullptr, &pipeline));
    }

    vkDestroyShaderModule(rtg.device, frag_module, nullptr);
    vkDestroyShaderModule(rtg.device, vert_module, nullptr);
    frag_module = VK_NULL_HANDLE;
    vert_module = VK_NULL_HANDLE;

    {
        auto const &ao_noise_texture = texture_manager.ao_noise_texture;
        VkDescriptorImageInfo noise_info {
            .sampler = ao_noise_texture.sampler,
            .imageView = ao_noise_texture.image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        VkWriteDescriptorSet write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = set2_Noise_instance,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &noise_info,
        };
        vkUpdateDescriptorSets(rtg.device, 1, &write, 0, nullptr);
    }

    block_descriptor_configs.push_back(
        BlockDescriptorConfig{
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .layout = set0_PV,
            .bindings_count = 2,
        }
    ); //PV + Kernel

    block_descriptor_set_name_to_index = {
        {"Global", 0},
    };

    block_binding_name_to_index = {
        {"PV", 0},
        {"SSDOAOKernel", 1},
    };

    pipeline_name_to_index["SSDOAmbientOcclusionPipeline"] = 7;
}

void SSDOAmbientOcclusionPipeline::destroy(RTG &rtg) {
    if (layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(rtg.device, layout, nullptr);
        layout = VK_NULL_HANDLE;
    }

    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(rtg.device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }

    if (set0_PV != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtg.device, set0_PV, nullptr);
        set0_PV = VK_NULL_HANDLE;
    }

    if (set1_GBuffer != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtg.device, set1_GBuffer, nullptr);
        set1_GBuffer = VK_NULL_HANDLE;
    }

    if (set2_Noise != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtg.device, set2_Noise, nullptr);
        set2_Noise = VK_NULL_HANDLE;
    }

    if (set1_GBuffer_instance != VK_NULL_HANDLE) {
        set1_GBuffer_instance = VK_NULL_HANDLE;
    }

    if (set2_Noise_instance != VK_NULL_HANDLE) {
        set2_Noise_instance = VK_NULL_HANDLE;
    }
}

SSDOAmbientOcclusionPipeline::~SSDOAmbientOcclusionPipeline() {
    assert(layout == VK_NULL_HANDLE);
    assert(pipeline == VK_NULL_HANDLE);
    assert(vert_module == VK_NULL_HANDLE);
    assert(frag_module == VK_NULL_HANDLE);
    assert(set0_PV == VK_NULL_HANDLE);
    assert(set1_GBuffer == VK_NULL_HANDLE);
    assert(set1_GBuffer_instance == VK_NULL_HANDLE);
    assert(set2_Noise == VK_NULL_HANDLE);
    assert(set2_Noise_instance == VK_NULL_HANDLE);
}
