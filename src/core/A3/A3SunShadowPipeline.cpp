#include "A3SunShadowPipeline.hpp"

static uint32_t vert_code[] = {
#include "../../shaders/spv/A3-sun-shadow.vert.inl"
};

A3SunShadowPipeline::~A3SunShadowPipeline() {
    assert(layout == VK_NULL_HANDLE);
    assert(pipeline == VK_NULL_HANDLE);
    assert(vert_module == VK_NULL_HANDLE);
    assert(frag_module == VK_NULL_HANDLE);
    assert(set0_Global == VK_NULL_HANDLE);
    assert(set1_Transforms == VK_NULL_HANDLE);
}

void A3SunShadowPipeline::create(
    RTG &rtg,
    VkRenderPass render_pass,
    uint32_t subpass,
    const ManagerContext& context
) {
    (void)context;

    vert_module = rtg.helpers.create_shader_module(vert_code);
    frag_module = VK_NULL_HANDLE;

    {
        std::array< VkDescriptorSetLayoutBinding, 1 > bindings{
            VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
            },
        };

        VkDescriptorSetLayoutCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = uint32_t(bindings.size()),
            .pBindings = bindings.data(),
        };

        VK(vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set0_Global));
    }

    {
        std::array< VkDescriptorSetLayoutBinding, 1 > bindings{
            VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
            },
        };

        VkDescriptorSetLayoutCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = uint32_t(bindings.size()),
            .pBindings = bindings.data(),
        };

        VK(vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set1_Transforms));
    }

    {
        std::array< VkDescriptorSetLayout, 2 > layouts{
            set0_Global,
            set1_Transforms,
        };

        VkPushConstantRange range{
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .offset = 0,
            .size = sizeof(Push),
        };

        VkPipelineLayoutCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = uint32_t(layouts.size()),
            .pSetLayouts = layouts.data(),
            .pushConstantRangeCount = 1,
            .pPushConstantRanges = &range,
        };

        VK(vkCreatePipelineLayout(rtg.device, &create_info, nullptr, &layout));
    }

    create_pipeline(rtg, render_pass, subpass, true, true, false, 0, false);

    vkDestroyShaderModule(rtg.device, vert_module, nullptr);
    vert_module = VK_NULL_HANDLE;

    block_descriptor_configs.push_back(
        BlockDescriptorConfig{
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .layout = set0_Global,
            .bindings_count = 1,
        }
    );
    block_descriptor_configs.push_back(
        BlockDescriptorConfig{
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .layout = set1_Transforms,
            .bindings_count = 1,
        }
    );

    block_descriptor_set_name_to_index = {
        {"Global", 0},
        {"Transforms", 1},
    };

    block_binding_name_to_index = {
        {"ShadowSunLights", 0},
        {"Transforms", 0},
    };

    pipeline_name_to_index["A3SunShadowPipeline"] = 4;
}

void A3SunShadowPipeline::destroy(RTG &rtg) {
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(rtg.device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }

    if (layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(rtg.device, layout, nullptr);
        layout = VK_NULL_HANDLE;
    }

    if (set0_Global != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtg.device, set0_Global, nullptr);
        set0_Global = VK_NULL_HANDLE;
    }

    if (set1_Transforms != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtg.device, set1_Transforms, nullptr);
        set1_Transforms = VK_NULL_HANDLE;
    }
}
