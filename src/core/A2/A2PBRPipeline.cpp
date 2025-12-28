#include "A2PBRPipeline.hpp"

static uint32_t vert_code[] = {
#include "../../shaders/spv/A2-pbr.vert.inl"
};

static uint32_t frag_code[] = {
#include "../../shaders/spv/A2-pbr.frag.inl"
};

A2PBRPipeline::~A2PBRPipeline(){
    assert(layout == VK_NULL_HANDLE); //should have been destroyed already
    assert(pipeline == VK_NULL_HANDLE);

    assert(vert_module == VK_NULL_HANDLE);
    assert(frag_module == VK_NULL_HANDLE);

    assert(set1_Transforms == VK_NULL_HANDLE);
    assert(set2_Light == VK_NULL_HANDLE);
    assert(set3_NormalTexture == VK_NULL_HANDLE);
    assert(set4_DisplacementTexture == VK_NULL_HANDLE);
    assert(set5_AlbedoTexture == VK_NULL_HANDLE);
    assert(set6_RoughnessTexture == VK_NULL_HANDLE);
    assert(set7_MetalnessTexture == VK_NULL_HANDLE);
    assert(set8_IrradianceMap == VK_NULL_HANDLE);
    assert(set9_PrefilterMap == VK_NULL_HANDLE);
    assert(set10_BRDFLUT == VK_NULL_HANDLE);
}

void A2PBRPipeline::create(RTG &rtg, VkRenderPass render_pass, uint32_t subpass){
    vert_module = rtg.helpers.create_shader_module(vert_code);
    frag_module = rtg.helpers.create_shader_module(frag_code);

    assert(set0_PV != VK_NULL_HANDLE && "should have been set before creating object pipeline layout");

    { //the set1_Transforms layout holds an array of Transform structures in a storage buffer used in the vertex shader:
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

        VK( vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set1_Transforms) );
    }

    { //the set2_Light layout holds a Light structure in a uniform buffer used in the fragment shader:
        std::array< VkDescriptorSetLayoutBinding, 1 > bindings{
            VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
            },
        };
        
        VkDescriptorSetLayoutCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = uint32_t(bindings.size()),
            .pBindings = bindings.data(),
        };

        VK( vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set2_Light) );
    }

    std::array<VkDescriptorSetLayout*, 8> textureSetPointers{
        &set3_NormalTexture,
        &set4_DisplacementTexture,
        &set5_AlbedoTexture,
        &set6_RoughnessTexture,
        &set7_MetalnessTexture,
        &set8_IrradianceMap,
        &set9_PrefilterMap,
        &set10_BRDFLUT
    };

    for(size_t i = 0; i < textureSetPointers.size(); ++i) {
        std::array< VkDescriptorSetLayoutBinding, 1 > bindings{
            VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
            },
        };
        
        VkDescriptorSetLayoutCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = uint32_t(bindings.size()),
            .pBindings = bindings.data(),
        };

        VK( vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, textureSetPointers[i]) );
    }

    { //create the pipeline layout:
        std::array<VkDescriptorSetLayout, 11> layouts{
            set0_PV,
            set1_Transforms,
            set2_Light,
            set3_NormalTexture,
            set4_DisplacementTexture,
            set5_AlbedoTexture,
            set6_RoughnessTexture,
            set7_MetalnessTexture,
            set8_IrradianceMap,
            set9_PrefilterMap,
            set10_BRDFLUT
        };

        VkPipelineLayoutCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = uint32_t(layouts.size()),
			.pSetLayouts = layouts.data(),
			.pushConstantRangeCount = 0,
			.pPushConstantRanges = nullptr,
		};

		VK( vkCreatePipelineLayout(rtg.device, &create_info, nullptr, &layout) );
    }

    create_pipeline(rtg, render_pass, subpass);

    vkDestroyShaderModule(rtg.device, frag_module, nullptr);
    vkDestroyShaderModule(rtg.device, vert_module, nullptr);
    frag_module = VK_NULL_HANDLE;
    vert_module = VK_NULL_HANDLE;

    block_descriptor_configs.push_back(BlockDescriptorConfig{ .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .layout = set1_Transforms}); //Transform
    block_descriptor_configs.push_back(BlockDescriptorConfig{ .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .layout = set2_Light}); //Light

    texture_descriptor_configs.push_back(TextureDescriptorConfig{ .slot = TextureSlot::Normal, .layout = set3_NormalTexture}); //NormalTexture
    texture_descriptor_configs.push_back(TextureDescriptorConfig{ .slot = TextureSlot::Displacement, .layout = set4_DisplacementTexture}); //DisplacementTexture
    texture_descriptor_configs.push_back(TextureDescriptorConfig{ .slot = TextureSlot::Albedo, .layout = set5_AlbedoTexture}); //AlbedoTexture
    texture_descriptor_configs.push_back(TextureDescriptorConfig{ .slot = TextureSlot::Roughness, .layout = set6_RoughnessTexture}); //RoughnessTexture
    texture_descriptor_configs.push_back(TextureDescriptorConfig{ .slot = TextureSlot::Metallic, .layout = set7_MetalnessTexture}); //MetalnessTexture
}

void A2PBRPipeline::destroy(RTG &rtg) {
    if(pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(rtg.device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }

    if(layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(rtg.device, layout, nullptr);
        layout = VK_NULL_HANDLE;
    }

    if(set1_Transforms != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtg.device, set1_Transforms, nullptr);
        set1_Transforms = VK_NULL_HANDLE;
    }

    if(set2_Light != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtg.device, set2_Light, nullptr);
        set2_Light = VK_NULL_HANDLE;
    }

    std::array<VkDescriptorSetLayout*, 8> textureSetPointers{
        &set3_NormalTexture,
        &set4_DisplacementTexture,
        &set5_AlbedoTexture,
        &set6_RoughnessTexture,
        &set7_MetalnessTexture,
        &set8_IrradianceMap,
        &set9_PrefilterMap,
        &set10_BRDFLUT
    };

    for(size_t i = 0; i < textureSetPointers.size(); ++i) {
        if(*textureSetPointers[i] != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(rtg.device, *textureSetPointers[i], nullptr);
            *textureSetPointers[i] = VK_NULL_HANDLE;
        }
    }
}