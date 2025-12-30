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

    assert(set0_Global == VK_NULL_HANDLE);
    assert(set1_Transforms == VK_NULL_HANDLE);
    assert(set2_Textures == VK_NULL_HANDLE);
    assert(set2_Textures_instance == VK_NULL_HANDLE);
    assert(set2_Textures_instance_pool == VK_NULL_HANDLE);
}

void A2PBRPipeline::create(
    class RTG &rtg, 
    VkRenderPass render_pass, 
    uint32_t subpass,
    const TextureManager& texture_manager
){
    vert_module = rtg.helpers.create_shader_module(vert_code);
    frag_module = rtg.helpers.create_shader_module(frag_code);

    { //the set0_Global layout holds a PV matrix in a uniform buffer used in the vertex shader:
        std::array< VkDescriptorSetLayoutBinding, 1 > bindings{
            VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 2,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
            },
        };
        
        VkDescriptorSetLayoutCreateInfo create_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = uint32_t(bindings.size()),
            .pBindings = bindings.data(),
        };

        VK( vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set0_Global) );
    }

    { // the set1_Transforms layout holds an array of Transform structures in a storage buffer used in the vertex shader:
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

    { // the set2_Textures
        std::array< VkDescriptorSetLayoutBinding, 1 > bindings{
			VkDescriptorSetLayoutBinding{
				.binding = 0,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = MAX_TEXTURES,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
			},
		};
		
		VkDescriptorSetLayoutCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = uint32_t(bindings.size()),
			.pBindings = bindings.data(),
		};

		VK( vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set2_Textures) );
    }

    { // allocate texture descriptor and update data
        uint32_t total_descriptors = 1; // BRDF LUT

        { // the set2_Textures_instance_pool
            for (const auto &material_slots : texture_manager.raw_2d_textures_by_material) {
                for (const auto &texture_opt : material_slots) {
                    if (texture_opt) {
                        ++total_descriptors;
                    }
                }
            }
            
            // Add environment cubemaps to descriptor count
            total_descriptors += texture_manager.raw_environment_cubemap_texture.size();

            std::array<VkDescriptorPoolSize, 1> pool_sizes{
                VkDescriptorPoolSize{
                    .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = total_descriptors,
                },
            };

            VkDescriptorPoolCreateInfo pool_create_info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .flags = 0,
                .maxSets = total_descriptors,
                .poolSizeCount = uint32_t(pool_sizes.size()),
                .pPoolSizes = pool_sizes.data(),
            };

            VK( vkCreateDescriptorPool(rtg.device, &pool_create_info, nullptr, &set2_Textures_instance_pool) );
        }

        { // the set2_Textures_instance
            VkDescriptorSetAllocateInfo alloc_info{
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                    .descriptorPool = set2_Textures_instance_pool,
                    .descriptorSetCount = 1,
                    .pSetLayouts = &set2_Textures,
                };

            VK(vkAllocateDescriptorSets(rtg.device, &alloc_info, &set2_Textures_instance));
        }

        { //update the set2_Textures_instance descriptor set
            std::vector<VkDescriptorImageInfo> image_info(total_descriptors);
            size_t index = 0;

            // PBRTLUT
            image_info[index] = VkDescriptorImageInfo{
                .sampler = texture_manager.raw_brdf_LUT_texture->sampler,
                .imageView = texture_manager.raw_brdf_LUT_texture->image_view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };
            ++index;

            if(texture_manager.raw_environment_cubemap_texture.size() > 1){
                for(;index < 3; ++index){
                    image_info[index] = VkDescriptorImageInfo{
                        .sampler = texture_manager.raw_environment_cubemap_texture[index]->sampler,
                        .imageView = texture_manager.raw_environment_cubemap_texture[index]->image_view,
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    };
                }
            }

            for(const auto &material_slots : texture_manager.raw_2d_textures_by_material) {
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

            VkWriteDescriptorSet write{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = set2_Textures_instance,
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = image_info.data(),
            };

            vkUpdateDescriptorSets(rtg.device, 1, &write, 0, nullptr); 
        }
    }

    { //create pipeline layout:
		std::array< VkDescriptorSetLayout, 3 > layouts{
			set0_Global,
            set1_Transforms,
            set2_Textures
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

		VK( vkCreatePipelineLayout(rtg.device, &create_info, nullptr, &layout) );
	}

    create_pipeline(rtg, render_pass, subpass);

    vkDestroyShaderModule(rtg.device, frag_module, nullptr);
    vkDestroyShaderModule(rtg.device, vert_module, nullptr);
    frag_module = VK_NULL_HANDLE;
    vert_module = VK_NULL_HANDLE;

    block_descriptor_configs.push_back(BlockDescriptorConfig{ .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .layout = set0_Global}); //Global
    block_descriptor_configs.push_back(BlockDescriptorConfig{ .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .layout = set1_Transforms}); //Transform

    pipeline_name_to_index["A2PBRPipeline"] = 3;
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

    if(set0_Global != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtg.device, set0_Global, nullptr);
        set0_Global = VK_NULL_HANDLE;
    }

    if(set1_Transforms != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtg.device, set1_Transforms, nullptr);
        set1_Transforms = VK_NULL_HANDLE;
    }

    if(set2_Textures != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(rtg.device, set2_Textures, nullptr);
        set2_Textures = VK_NULL_HANDLE;
    }

    if(set2_Textures_instance_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(rtg.device, set2_Textures_instance_pool, nullptr);
        set2_Textures_instance_pool = VK_NULL_HANDLE;
    }

    if(set2_Textures_instance != VK_NULL_HANDLE) {
        set2_Textures_instance = VK_NULL_HANDLE;
    }
}