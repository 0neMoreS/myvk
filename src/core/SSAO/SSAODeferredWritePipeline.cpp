#include "SSAODeferredWritePipeline.hpp"

#include <array>

static uint32_t vert_code[] = {
#include "../../shaders/spv/SSAO-deferred-write.vert.inl"
};

static uint32_t frag_code[] = {
#include "../../shaders/spv/SSAO-deferred-write.frag.inl"
};

SSAODeferredWritePipeline::~SSAODeferredWritePipeline() {
    assert(layout == VK_NULL_HANDLE);
    assert(pipeline == VK_NULL_HANDLE);
    assert(vert_module == VK_NULL_HANDLE);
    assert(frag_module == VK_NULL_HANDLE);
    assert(set0_PV == VK_NULL_HANDLE);
    assert(set1_Transforms == VK_NULL_HANDLE);
    assert(set2_Textures == VK_NULL_HANDLE);
    assert(set2_Textures_instance == VK_NULL_HANDLE);
}

void SSAODeferredWritePipeline::create(
    RTG &rtg,
    VkRenderPass render_pass,
    uint32_t subpass,
    const ManagerContext& context
) {
    auto const &texture_manager = *context.texture_manager;

    vert_module = rtg.helpers.create_shader_module(vert_code);
    frag_module = rtg.helpers.create_shader_module(frag_code);

    { // set0: PV UBO
        std::array< VkDescriptorSetLayoutBinding, 1 > bindings{
            VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
            },
        };

        VkDescriptorSetLayoutCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = uint32_t(bindings.size()),
            .pBindings = bindings.data(),
        };

        VK(vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set0_PV));
    }

    { // set1: transform SSBO
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

    uint32_t total_2d_descriptors = 1; // BRDF LUT
    for (const auto &material_slots : texture_manager.raw_2d_textures_by_material) {
        for (const auto &texture_opt : material_slots) {
            if (texture_opt) {
                ++total_2d_descriptors;
            }
        }
    }

    { // set2: 2D textures array at binding=1
        VkDescriptorSetLayoutBinding binding{
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = total_2d_descriptors,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        };

        VkDescriptorSetLayoutCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings = &binding,
        };

        VK(vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set2_Textures));
    }

    { // allocate + write set2 descriptor data
        VkDescriptorSetAllocateInfo alloc_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = texture_manager.texture_descriptor_pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &set2_Textures,
        };

        VK(vkAllocateDescriptorSets(rtg.device, &alloc_info, &set2_Textures_instance));

        std::vector<VkDescriptorImageInfo> image_info(total_2d_descriptors);
        size_t index = 0;

        image_info[index] = VkDescriptorImageInfo{
            .sampler = texture_manager.raw_brdf_LUT_texture->sampler,
            .imageView = texture_manager.raw_brdf_LUT_texture->image_view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        ++index;

        for (const auto &material_slots : texture_manager.raw_2d_textures_by_material) {
            for (const auto &texture_opt : material_slots) {
                if (texture_opt) {
                    const auto &texture = texture_opt.value();
                    image_info[index] = VkDescriptorImageInfo{
                        .sampler = texture->sampler,
                        .imageView = texture->image_view,
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    };
                    ++index;
                }
            }
        }

        VkWriteDescriptorSet write_2d{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = set2_Textures_instance,
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = total_2d_descriptors,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = image_info.data(),
        };

        vkUpdateDescriptorSets(rtg.device, 1, &write_2d, 0, nullptr);
    }

    { // pipeline layout
        std::array< VkDescriptorSetLayout, 3 > layouts{
            set0_PV,
            set1_Transforms,
            set2_Textures,
        };

        VkPushConstantRange range{
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
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

    create_pipeline(rtg, render_pass, subpass, true, true, false, 2);

    vkDestroyShaderModule(rtg.device, frag_module, nullptr);
    vkDestroyShaderModule(rtg.device, vert_module, nullptr);
    frag_module = VK_NULL_HANDLE;
    vert_module = VK_NULL_HANDLE;

    block_descriptor_configs.push_back(
        BlockDescriptorConfig{
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .layout = set0_PV,
            .bindings_count = 1,
        }
    ); // PV

    block_descriptor_configs.push_back(
        BlockDescriptorConfig{
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .layout = set1_Transforms,
            .bindings_count = 1,
        }
    ); // Transforms

    block_descriptor_set_name_to_index = {
        {"PV", 0},
        {"Transforms", 1},
    };

    block_binding_name_to_index = {
        {"PV", 0},
        {"Transforms", 0},
    };

    pipeline_name_to_index["SSAODeferredWritePipeline"] = 1;
}

void SSAODeferredWritePipeline::destroy(RTG &rtg) {
    if (pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(rtg.device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }

    if (layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(rtg.device, layout, nullptr);
        layout = VK_NULL_HANDLE;
    }

    if (set0_PV != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtg.device, set0_PV, nullptr);
        set0_PV = VK_NULL_HANDLE;
    }

    if (set1_Transforms != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtg.device, set1_Transforms, nullptr);
        set1_Transforms = VK_NULL_HANDLE;
    }

    if (set2_Textures != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtg.device, set2_Textures, nullptr);
        set2_Textures = VK_NULL_HANDLE;
    }

    if (set2_Textures_instance != VK_NULL_HANDLE) {
        set2_Textures_instance = VK_NULL_HANDLE;
    }
}
