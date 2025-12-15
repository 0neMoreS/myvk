#include "TextureManager.hpp"

#include <cassert>

TextureManager::~TextureManager(){
    if(descriptor_pool != VK_NULL_HANDLE) {
        std::cerr << "[TextureManager] Warning: descriptor pool not destroyed before TextureManager destruction" << std::endl;
    }
}

void TextureManager::destroy(RTG &rtg) {
    if (descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(rtg.device, descriptor_pool, nullptr);
        descriptor_pool = VK_NULL_HANDLE;
    }

    for (auto &bucket : textures_by_slot) {
        for (auto &tex : bucket) {
            Texture2DLoader::destroy(tex, rtg);
        }
        bucket.clear();
    }
}

void TextureManager::create(
    RTG & rtg,
    std::shared_ptr<S72Loader::Document> &doc,
    const std::vector<Pipeline::TextureDescriptorConfig> &texture_descriptor_configs
) {
    if (texture_descriptor_configs.empty()) return;

    // clean previous data
    destroy(rtg);

    for (auto &bucket : textures_by_slot) {
        bucket.reserve(doc->materials.size());
    }

    auto push_texture = [&](TextureSlot slot, const std::optional<S72Loader::Texture> &texture_opt, const glm::vec3 &fallback_color) {
        auto &bucket = textures_by_slot[slot];
        if (texture_opt.has_value()) {
            const auto &texture = texture_opt.value();
            std::string texture_path = s72_dir + texture.src;
            bucket.emplace_back(Texture2DLoader::load_png(rtg.helpers, texture_path, VK_FILTER_LINEAR));
        } else {
            bucket.emplace_back(Texture2DLoader::create_rgb_texture(rtg.helpers, fallback_color));
        }
    };

    for (const auto &material : doc->materials) {
        // diffuse / albedo
        std::optional<S72Loader::Texture> albedo_texture;
        glm::vec3 albedo_value{1.0f, 1.0f, 1.0f};

        if (material.pbr && material.pbr->albedo_texture) {
            albedo_texture = material.pbr->albedo_texture;
        } else if (material.lambertian && material.lambertian->albedo_texture) {
            albedo_texture = material.lambertian->albedo_texture;
        }

        if (material.pbr && material.pbr->albedo_value) {
            albedo_value = *material.pbr->albedo_value;
        } else if (material.lambertian && material.lambertian->albedo_value) {
            albedo_value = *material.lambertian->albedo_value;
        }

        push_texture(TextureSlot::Diffuse, albedo_texture, albedo_value);

        // normal
        push_texture(TextureSlot::Normal, material.normal_map, glm::vec3{0.0f, 0.0f, 0.0f});

        // roughness
        std::optional<S72Loader::Texture> roughness_texture;
        float roughness_value = 1.0f;
        if (material.pbr) {
            if (material.pbr->roughness_texture) roughness_texture = material.pbr->roughness_texture;
            if (material.pbr->roughness_value) roughness_value = *material.pbr->roughness_value;
        }
        push_texture(TextureSlot::Roughness, roughness_texture, glm::vec3(roughness_value));

        // metallic
        std::optional<S72Loader::Texture> metallic_texture;
        float metallic_value = 0.0f;
        if (material.pbr) {
            if (material.pbr->metalness_texture) metallic_texture = material.pbr->metalness_texture;
            if (material.pbr->metalness_value) metallic_value = *material.pbr->metalness_value;
        }
        push_texture(TextureSlot::Metallic, metallic_texture, glm::vec3(metallic_value));
    }

    // create the texture descriptor pool
    uint32_t total_descriptors = 0;
    for (const auto &binding : texture_descriptor_configs) {
        uint32_t count = static_cast<uint32_t>(textures_by_slot[binding.slot].size());
        total_descriptors += count;
    }

    if (total_descriptors == 0) return; // nothing to allocate

    std::array<VkDescriptorPoolSize, 1> pool_sizes{
        VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = total_descriptors,
        },
    };

    VkDescriptorPoolCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = 0,
        .maxSets = total_descriptors,
        .poolSizeCount = uint32_t(pool_sizes.size()),
        .pPoolSizes = pool_sizes.data(),
    };

    VK( vkCreateDescriptorPool(rtg.device, &create_info, nullptr, &descriptor_pool) );

    // allocate and write the texture descriptor sets
    descriptor_sets.resize(texture_descriptor_configs.size());
    
    for (size_t i = 0; i < texture_descriptor_configs.size(); ++i) {
        const auto &binding = texture_descriptor_configs[i];
        const auto &bucket = textures_by_slot[binding.slot];

        if (bucket.empty()) continue;

        VkDescriptorSetAllocateInfo alloc_info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = descriptor_pool,
            .descriptorSetCount = 1,
            .pSetLayouts = &binding.layout,
        };

        descriptor_sets[i].assign(bucket.size(), VK_NULL_HANDLE);
        for (VkDescriptorSet &descriptor_set : descriptor_sets[i]) {
            VK( vkAllocateDescriptorSets(rtg.device, &alloc_info, &descriptor_set) );
        }

        std::vector< VkDescriptorImageInfo > infos(bucket.size());
        std::vector< VkWriteDescriptorSet > writes(bucket.size());

        for (size_t j = 0; j < bucket.size(); ++j) {
            const auto &texture = bucket[j];
            
            infos[j] = VkDescriptorImageInfo {
                .sampler = texture->sampler,
                .imageView = texture->image_view,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };
            writes[j] = VkWriteDescriptorSet {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = descriptor_sets[i][j],
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