#include "A1ObjectsPipeline.hpp"

#include "Helpers.hpp"
#include "VK.hpp"

static uint32_t vert_code[] = {
#include "../../shaders/spv/A1-load.vert.inl"
};

static uint32_t frag_code[] = {
#include "../../shaders/spv/A1-load.frag.inl"
};

A1ObjectsPipeline::~A1ObjectsPipeline(){
	assert(layout == VK_NULL_HANDLE);
	assert(pipeline == VK_NULL_HANDLE);
	assert(set0_PV == VK_NULL_HANDLE);
	assert(set1_Transforms == VK_NULL_HANDLE);
	assert(set2_TEXTURE == VK_NULL_HANDLE);
	assert(set2_TEXTURE_instance == VK_NULL_HANDLE);
}

void A1ObjectsPipeline::create(
	RTG &rtg, 
	VkRenderPass render_pass, 
	uint32_t subpass,
	const TextureManager& texture_manager
) {
	vert_module = rtg.helpers.create_shader_module(vert_code);
	frag_module = rtg.helpers.create_shader_module(frag_code);

	{ //the set0_PV layout holds PV info in a uniform buffer used in the vertex shader:
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

		VK( vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set0_PV) );
	}
	
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

	{ //the set2_TEXTURE layout has a single descriptor for a sampler2D used in the fragment shader:
		uint32_t total_2d_descriptors = 0;

		{
			for (const auto &material_slots : texture_manager.raw_2d_textures_by_material) {
				for (const auto &texture_opt : material_slots) {
					std::cout << "material_slots size: " << material_slots.size() << " texture_opt: " << (texture_opt.has_value() ? "present" : "absent") << "\n";
					if (texture_opt) {
						++total_2d_descriptors;
					}
				}
				std::cout << "----\n";
			}

			std::array< VkDescriptorSetLayoutBinding, 1 > bindings{
				VkDescriptorSetLayoutBinding{
					.binding = 0,
					.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.descriptorCount = total_2d_descriptors,
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
				},
			};

			std::array<VkDescriptorBindingFlags, 1> binding_flags{
				VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
			};

			VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_info{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
				.bindingCount = 1,
				.pBindingFlags = binding_flags.data()
			};

			VkDescriptorSetLayoutCreateInfo create_info{
				.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				.pNext = &binding_flags_info,
				.bindingCount = uint32_t(bindings.size()),
				.pBindings = bindings.data(),
			};

			VK( vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &set2_TEXTURE) );
		}

		{ // allocate texture descriptor and update data
			{ // the set2_TEXTURE_instance
                VkDescriptorSetVariableDescriptorCountAllocateInfo var_count_alloc_info{
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
                    .descriptorSetCount = 1,
                    .pDescriptorCounts = &total_2d_descriptors  // runtime-defined size
                };

                VkDescriptorSetAllocateInfo alloc_info{
                        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                        .pNext = &var_count_alloc_info,
                        .descriptorPool = texture_manager.texture_descriptor_pool,
                        .descriptorSetCount = 1,
                        .pSetLayouts = &set2_TEXTURE,
				};

                VK(vkAllocateDescriptorSets(rtg.device, &alloc_info, &set2_TEXTURE_instance));
            }

			{ //update the set2_TEXTURE_instance descriptor set
                std::vector<VkDescriptorImageInfo> image_info(total_2d_descriptors);
                size_t index = 0;

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

                VkWriteDescriptorSet write_2d{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = set2_TEXTURE_instance,
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = total_2d_descriptors,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = image_info.data(),
                };

                vkUpdateDescriptorSets(rtg.device, 1, &write_2d, 0, nullptr); 
            }
		}
	}

	{ //create pipeline layout:
		std::array< VkDescriptorSetLayout, 3 > layouts{
			set0_PV, //we'd like to say "VK_NULL_HANDLE" here, but that's not valid without an extension
			set1_Transforms,
			set2_TEXTURE,
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

	create_pipeline(rtg, render_pass, subpass, true);

	vkDestroyShaderModule(rtg.device, frag_module, nullptr);
    vkDestroyShaderModule(rtg.device, vert_module, nullptr);
    frag_module = VK_NULL_HANDLE;
    vert_module = VK_NULL_HANDLE;

	block_descriptor_configs.push_back(
        BlockDescriptorConfig{
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 
        .layout = set0_PV, 
        .bindings_count = 1
    }); //Global
    block_descriptor_configs.push_back(
        BlockDescriptorConfig{
        .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 
        .layout = set1_Transforms, 
        .bindings_count = 1
    }); //Transform
	
	block_descriptor_set_name_to_index = {
        {"PV", 0},
        {"Transforms", 1}
    };

    block_binding_name_to_index = {
        // PV
        {"PV", 0},
        // Transforms
        {"Transforms", 0},
    };

	pipeline_name_to_index["A1ObjectsPipeline"] = 0;
}

void A1ObjectsPipeline::destroy(RTG &rtg) {
	if (set1_Transforms != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(rtg.device, set1_Transforms, nullptr);
		set1_Transforms = VK_NULL_HANDLE;
	}

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

	if(set1_Transforms != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(rtg.device, set1_Transforms, nullptr);
		set1_Transforms = VK_NULL_HANDLE;
	}

	if (set2_TEXTURE != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(rtg.device, set2_TEXTURE, nullptr);
		set2_TEXTURE = VK_NULL_HANDLE;
	}

	if( set2_TEXTURE_instance != VK_NULL_HANDLE) {
		// Note: Descriptor sets are freed when the descriptor pool is destroyed or reset.
		set2_TEXTURE_instance = VK_NULL_HANDLE;
	}
}