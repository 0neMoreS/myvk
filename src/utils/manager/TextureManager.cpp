#include "TextureManager.hpp"

void TextureManager::create(RTG & rtg, S72Loader::Document &doc, std::vector<VkDescriptorSetLayout> texture_descriptor_layouts) {
    { //load textures
        textures.reserve(doc.materials.size());
        for(const auto &material : doc.materials) {
            if(material.lambertian.has_value()) {
                if(material.lambertian.value().albedo_texture.has_value()){
                    S72Loader::Texture const &texture = material.lambertian.value().albedo_texture.value();
                    // TODO: handle other types and other formats of textures
                    std::string texture_path = s72_dir + texture.src;
                    // textures[mesh.material_index.value()] = Texture2DLoader::load_png(rtg.helpers, texture_path, VK_FILTER_LINEAR);
                    textures.emplace_back(Texture2DLoader::load_png(rtg.helpers, texture_path, VK_FILTER_LINEAR));
                }
                else if(material.lambertian.value().albedo_value.has_value()) {
                    textures.emplace_back(Texture2DLoader::create_rgb_texture(rtg.helpers, material.lambertian.value().albedo_value.value()));
                }
            }
        }
    }

    { // create the texture descriptor pool
		uint32_t per_texture = uint32_t(textures.size()); //for easier-to-read counting

		std::array< VkDescriptorPoolSize, 1> pool_sizes{
			VkDescriptorPoolSize{
				.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = uint32_t(texture_descriptor_layouts.size()) * 1 * per_texture, // how many descriptors of each texture type
			},
		};
		
		VkDescriptorPoolCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = 0, //because CREATE_FREE_DESCRIPTOR_SET_BIT isn't included, *can't* free individual descriptors allocated from this pool
			.maxSets = uint32_t(texture_descriptor_layouts.size()) * per_texture, //one set per texture
			.poolSizeCount = uint32_t(pool_sizes.size()),
			.pPoolSizes = pool_sizes.data(),
		};

		VK( vkCreateDescriptorPool(rtg.device, &create_info, nullptr, &descriptor_pool) );
	}

	{ //allocate and write the texture descriptor sets
        for(size_t i = 0; i < texture_descriptor_layouts.size(); ++i) {
            //allocate the descriptors (using the same alloc_info):
            VkDescriptorSetAllocateInfo alloc_info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                .descriptorPool = descriptor_pool,
                .descriptorSetCount = 1,
                .pSetLayouts = &texture_descriptor_layouts[i],
            };
            descriptor_sets.assign(textures.size(), VK_NULL_HANDLE);
            for (VkDescriptorSet &descriptor_set : descriptor_sets) {
                VK( vkAllocateDescriptorSets(rtg.device, &alloc_info, &descriptor_set) );
            }

            std::vector< VkDescriptorImageInfo > infos(textures.size());
            std::vector< VkWriteDescriptorSet > writes(textures.size());

            for (auto const &texture : textures) {
                size_t j = &texture - &textures[0];
                
                infos[j] = VkDescriptorImageInfo {
                    .sampler = texture->sampler,
                    .imageView = texture->image_view,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                };
                writes[j] = VkWriteDescriptorSet {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = descriptor_sets[j],
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &infos[j],
                };
            }

            vkUpdateDescriptorSets( rtg.device, uint32_t(writes.size()), writes.data(), 0, nullptr );
        }
    }
}