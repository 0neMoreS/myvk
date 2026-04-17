#include "TextureManager.hpp"

#include <cassert>
#include <algorithm>

void TextureManager::destroy(RTG &rtg) {
    for (auto &material_slots : raw_2d_textures_by_material) {
        for (auto &texture_opt : material_slots) {
            if (texture_opt) {
                TextureCommon::destroy_texture(*(*texture_opt), rtg.device, rtg.helpers);
            }
            texture_opt.reset();
        }
    }

    raw_2d_textures_by_material.clear();

    for (auto &cubemap_texture : raw_environment_cubemap_texture) {
        if (cubemap_texture) {
            TextureCommon::destroy_texture(*cubemap_texture, rtg.device, rtg.helpers);
            cubemap_texture.reset();
        }
    }
    raw_environment_cubemap_texture.clear();

    if (raw_brdf_LUT_texture) {
        TextureCommon::destroy_texture(*raw_brdf_LUT_texture, rtg.device, rtg.helpers);
    }
    raw_brdf_LUT_texture = nullptr;

    // Destroy dummy shadow textures
    if (dummy_shadow_2d_array_view != VK_NULL_HANDLE) {
        vkDestroyImageView(rtg.device, dummy_shadow_2d_array_view, nullptr);
        dummy_shadow_2d_array_view = VK_NULL_HANDLE;
    }
    
    TextureCommon::destroy_texture(dummy_shadow_2d, rtg.device, rtg.helpers);
    TextureCommon::destroy_texture(dummy_shadow_cubemap, rtg.device, rtg.helpers);

    if(texture_descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(rtg.device, texture_descriptor_pool, nullptr);
        texture_descriptor_pool = VK_NULL_HANDLE;
    }
}

void TextureManager::create(
    RTG &rtg,
    std::shared_ptr<S72Loader::Document> &doc,
    uint32_t pipeline_count,
    uint32_t shadow_sun_count,
    uint32_t shadow_sphere_count,
    uint32_t shadow_spot_count
) {
    // Clean previous data
    destroy(rtg);

    shadow_sun_light_count = shadow_sun_count;
    shadow_sphere_light_count = shadow_sphere_count;
    shadow_spot_light_count = shadow_spot_count;

    sun_shadow_descriptor_count = (shadow_sun_light_count > 0) ? shadow_sun_light_count : 1u;
    sphere_shadow_descriptor_count = (shadow_sphere_light_count > 0) ? shadow_sphere_light_count : 1u;
    spot_shadow_descriptor_count = (shadow_spot_light_count > 0) ? shadow_spot_light_count : 1u;

    { // Load raw textures from document
        raw_2d_textures_by_material.resize(doc->materials.size());

        auto push_texture = [&](size_t material_index, TextureSlot slot, const std::optional<S72Loader::Texture> &texture_opt, const glm::vec3 &fallback_color, bool generate_mipmaps) {
            auto &texture_element = raw_2d_textures_by_material[material_index][slot];
            if (texture_opt.has_value()) {
                const auto &texture = texture_opt.value();
                std::string texture_path = s72_dir + texture.src;
                texture_element = Texture2DLoader::load_image(rtg.helpers, texture_path, VK_FILTER_LINEAR, texture.format == "srgb", generate_mipmaps);
            } else {
                texture_element = Texture2DLoader::create_rgb_texture(rtg.helpers, fallback_color);
            }
        };

        for (const auto &material : doc->materials) {
            // diffuse / albedo
            std::optional<S72Loader::Texture> albedo_texture;
            glm::vec3 albedo_value{1.0f, 1.0f, 1.0f};
            size_t material_index = &material - &doc->materials[0];

            // normal
            push_texture(material_index, TextureSlot::Normal, material.normal_map, glm::vec3{0.5f, 0.5f, 1.0f}, false);

            // displacement
            push_texture(material_index, TextureSlot::Displacement, material.displacement_map, glm::vec3{0.0f, 0.0f, 0.0f}, false);

            // albedo
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

            push_texture(material_index, TextureSlot::Albedo, albedo_texture, albedo_value, true);

            // roughness
            std::optional<S72Loader::Texture> roughness_texture;
            float roughness_value = 1.0f;
            if (material.pbr) {
                if (material.pbr->roughness_texture) roughness_texture = material.pbr->roughness_texture;
                if (material.pbr->roughness_value) roughness_value = *material.pbr->roughness_value;
            }
            push_texture(material_index, TextureSlot::Roughness, roughness_texture, glm::vec3(roughness_value), false);

            // metallic
            std::optional<S72Loader::Texture> metallic_texture;
            float metallic_value = 0.0f;
            if (material.pbr) {
                if (material.pbr->metalness_texture) metallic_texture = material.pbr->metalness_texture;
                if (material.pbr->metalness_value) metallic_value = *material.pbr->metalness_value;
            }
            push_texture(material_index, TextureSlot::Metallic, metallic_texture, glm::vec3(metallic_value), false);
        }
    }

    {
        bool has_cubemap = doc->environments.size() > 0;
        raw_environment_cubemap_texture.resize(3);

        { // Load environment and IBL cubemaps
            if(has_cubemap){
                const auto &env = doc->environments[0];
                const auto &radiance = env.radiance;
                std::string texture_path = s72_dir + radiance.src;
                std::string lambertion_path = s72_dir + radiance.src.substr(0, radiance.src.find_last_of('.')) + ".lambertian.png";
                
                raw_environment_cubemap_texture[0] = TextureCubeLoader::load_cubemap(rtg.helpers, texture_path, VK_FILTER_LINEAR, 1);
                raw_environment_cubemap_texture[1] = TextureCubeLoader::load_cubemap(rtg.helpers, lambertion_path, VK_FILTER_LINEAR, 1);
                raw_environment_cubemap_texture[2] = TextureCubeLoader::load_cubemap(rtg.helpers, texture_path, VK_FILTER_LINEAR, 5);
            } else {
                raw_environment_cubemap_texture[0] = TextureCubeLoader::create_default_cubemap(rtg.helpers, VK_FILTER_LINEAR);
                raw_environment_cubemap_texture[1] = TextureCubeLoader::create_default_cubemap(rtg.helpers, VK_FILTER_LINEAR);
                raw_environment_cubemap_texture[2] = TextureCubeLoader::create_default_cubemap(rtg.helpers, VK_FILTER_LINEAR);
            }
        }

        { // Load BRDF LUT texture
            std::string brdf_lut_path = s72_dir + "brdf_LUT.png";
            raw_brdf_LUT_texture = Texture2DLoader::load_image(rtg.helpers, brdf_lut_path, VK_FILTER_LINEAR, false, false);
            
        }

        { // Create dummy shadow textures for fallback
            // Create dummy 2D shadow texture (1x1)
            {
                VkExtent2D extent{1, 1};
                auto dummy_2d_img = rtg.helpers.create_image(
                    extent,
                    VK_FORMAT_D32_SFLOAT,
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    Helpers::Unmapped,
                    0,
                    1,
                    1
                );
                dummy_shadow_2d.image = std::move(dummy_2d_img);

                // Create 2D view for spot shadow sampling
                VkImageViewCreateInfo view_create_info{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .image = dummy_shadow_2d.image.handle,
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format = VK_FORMAT_D32_SFLOAT,
                    .components = {
                        .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                    },
                    .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
                };
                VK( vkCreateImageView(rtg.device, &view_create_info, nullptr, &dummy_shadow_2d.image_view) );

                // Create 2D_ARRAY view for sun shadow sampling
                view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
                view_create_info.subresourceRange.layerCount = 1;
                VK( vkCreateImageView(rtg.device, &view_create_info, nullptr, &dummy_shadow_2d_array_view) );

                // Create sampler for 2D shadow
                VkSamplerCreateInfo sampler_info{
                    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                    .magFilter = VK_FILTER_LINEAR,
                    .minFilter = VK_FILTER_LINEAR,
                    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                    .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                    .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                    .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
                    .mipLodBias = 0.0f,
                    .anisotropyEnable = VK_FALSE,
                    .maxAnisotropy = 1.0f,
                    .compareEnable = VK_FALSE,
                    .compareOp = VK_COMPARE_OP_ALWAYS,
                    .minLod = 0.0f,
                    .maxLod = 0.0f,
                    .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
                    .unnormalizedCoordinates = VK_FALSE,
                };
                VK( vkCreateSampler(rtg.device, &sampler_info, nullptr, &dummy_shadow_2d.sampler) );
            }

            // Create dummy cubemap shadow texture (1x1x6)
            {
                VkExtent2D extent{1, 1};
                auto dummy_cube_img = rtg.helpers.create_image(
                    extent,
                    VK_FORMAT_D32_SFLOAT,
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    Helpers::Unmapped,
                    VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
                    1,
                    6  // 6 cube faces
                );
                dummy_shadow_cubemap.image = std::move(dummy_cube_img);

                // Create cube view for sphere shadow sampling
                VkImageViewCreateInfo view_create_info{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .image = dummy_shadow_cubemap.image.handle,
                    .viewType = VK_IMAGE_VIEW_TYPE_CUBE,
                    .format = VK_FORMAT_D32_SFLOAT,
                    .components = {
                        .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                        .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                    },
                    .subresourceRange = {
                        .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                        .baseMipLevel = 0,
                        .levelCount = 1,
                        .baseArrayLayer = 0,
                        .layerCount = 6,
                    },
                };
                VK( vkCreateImageView(rtg.device, &view_create_info, nullptr, &dummy_shadow_cubemap.image_view) );

                // Create sampler for cube shadow
                VkSamplerCreateInfo sampler_info{
                    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                    .magFilter = VK_FILTER_LINEAR,
                    .minFilter = VK_FILTER_LINEAR,
                    .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
                    .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                    .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                    .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                    .mipLodBias = 0.0f,
                    .anisotropyEnable = VK_FALSE,
                    .maxAnisotropy = 1.0f,
                    .compareEnable = VK_FALSE,
                    .compareOp = VK_COMPARE_OP_ALWAYS,
                    .minLod = 0.0f,
                    .maxLod = 0.0f,
                    .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
                    .unnormalizedCoordinates = VK_FALSE,
                };
                VK( vkCreateSampler(rtg.device, &sampler_info, nullptr, &dummy_shadow_cubemap.sampler) );
            }
        }

        { // the descriptor pool for texture descriptors
            uint32_t total_2d_descriptors = 2; // BRDF LUT + Tone mapping target (swapchain image)
            uint32_t total_cubemap_descriptors = 2; // IrradianceMap + PrefilterMap
            uint32_t shadow_descriptors_per_pipeline = sun_shadow_descriptor_count + sphere_shadow_descriptor_count + spot_shadow_descriptor_count;
            uint32_t gbuffer_descriptors_per_pipeline = 5; // depth/albedo/normal + AO + AO-pass gbuffer reads

            for (const auto &material_slots : raw_2d_textures_by_material) {
                for (const auto &texture_opt : material_slots) {
                    if (texture_opt) {
                        ++total_2d_descriptors;
                    }
                }
            }

            // Texture descriptor set usage in SSAO now exceeds pipeline_count because
            // several pipelines allocate multiple texture sets (e.g. PBR, AO, tone mapping).
            const uint32_t max_texture_sets = std::max(16u, pipeline_count * 4u);

            std::array<VkDescriptorPoolSize, 1> pool_sizes{
                VkDescriptorPoolSize{
                    .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = (total_2d_descriptors + total_cubemap_descriptors + shadow_descriptors_per_pipeline + gbuffer_descriptors_per_pipeline) * max_texture_sets,
                },
            };

            VkDescriptorPoolCreateInfo pool_create_info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
                .maxSets = max_texture_sets,
                .poolSizeCount = uint32_t(pool_sizes.size()),
                .pPoolSizes = pool_sizes.data(),
            };

            VK( vkCreateDescriptorPool(rtg.device, &pool_create_info, nullptr, &texture_descriptor_pool) );
        }
    }
}

TextureManager::~TextureManager() {
    assert(texture_descriptor_pool == VK_NULL_HANDLE);
}