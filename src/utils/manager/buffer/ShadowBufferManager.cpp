#include "ShadowBufferManager.hpp"

#include "RenderTarget.hpp"

#include <array>
#include <iostream>

void ShadowBufferManager::create(
    RTG &rtg,
    RenderPassManager &render_pass_manager,
    std::vector<LightsManager::SunLight> const &shadow_sun_lights,
    std::vector<LightsManager::SphereLight> const &shadow_sphere_lights,
    std::vector<LightsManager::SpotLight> const &shadow_spot_lights
) {
    destroy(rtg);

    if (depth_format == VK_FORMAT_UNDEFINED) {
        depth_format = rtg.helpers.find_image_format(
            { VK_FORMAT_D32_SFLOAT, VK_FORMAT_X8_D24_UNORM_PACK32 },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    shadow_clear_value = VkClearValue{
        .depthStencil{ .depth = rtg.configuration.reverse_z ? 0.0f : 1.0f, .stencil = 0 },
    };

    VkSamplerCreateInfo sun_sampler_info{
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
    VK(vkCreateSampler(rtg.device, &sun_sampler_info, nullptr, &sun_shadow_sampler));

    sun_shadow_targets.clear();
    sun_shadow_targets.reserve(shadow_sun_lights.size());
    for (const auto &light : shadow_sun_lights) {
        uint32_t resolution = static_cast<uint32_t>(light.shadow);
        if (resolution == 0) {
            continue;
        }

        SunShadowTarget target{};
        target.resolution = resolution;

        VkExtent2D extent{
            .width = resolution,
            .height = resolution,
        };

        auto array_target = BufferRenderTarget::create_target_array(
            rtg,
            extent,
            SunCascadeCount,
            depth_format,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT,
            render_pass_manager.shadow_render_pass
        );
        target.depth_target = std::move(array_target);

        sun_shadow_targets.emplace_back(std::move(target));
    }

    VkSamplerCreateInfo spot_sampler_info{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
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
    VK(vkCreateSampler(rtg.device, &spot_sampler_info, nullptr, &spot_shadow_sampler));

    VkSamplerCreateInfo sphere_sampler_info{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
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
    VK(vkCreateSampler(rtg.device, &sphere_sampler_info, nullptr, &sphere_shadow_sampler));

    sphere_shadow_targets.clear();
    sphere_shadow_targets.reserve(shadow_sphere_lights.size());
    for (const auto &light : shadow_sphere_lights) {
        const uint32_t resolution = static_cast<uint32_t>(light.shadow);
        if (resolution == 0) {
            continue;
        }

        SphereShadowTarget target{};
        target.resolution = resolution;

        VkExtent2D extent{
            .width = resolution,
            .height = resolution,
        };

        auto cube_target = BufferRenderTarget::create_target_cube(
            rtg,
            extent,
            depth_format,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT,
            render_pass_manager.shadow_render_pass
        );
        target.depth_target = std::move(cube_target);

        sphere_shadow_targets.emplace_back(std::move(target));
    }

    spot_shadow_targets.clear();
    spot_shadow_targets.reserve(shadow_spot_lights.size());
    for (const auto &light : shadow_spot_lights) {
        uint32_t resolution = static_cast<uint32_t>(light.shadow);
        if (resolution == 0) {
            continue;
        }

        SpotShadowTarget target{};
        target.resolution = resolution;

        VkExtent2D extent{
            .width = resolution,
            .height = resolution,
        };

        auto spot_target = BufferRenderTarget::create_target_2d(
            rtg,
            extent,
            depth_format,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT,
            render_pass_manager.shadow_render_pass
        );
        target.depth_target = std::move(spot_target);

        spot_shadow_targets.emplace_back(std::move(target));
    }
}

void ShadowBufferManager::destroy(RTG &rtg) {
    for (SunShadowTarget &target : sun_shadow_targets) {
        BufferRenderTarget::destroy_target_array(rtg, target.depth_target);
    }
    sun_shadow_targets.clear();

    if (sun_shadow_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(rtg.device, sun_shadow_sampler, nullptr);
        sun_shadow_sampler = VK_NULL_HANDLE;
    }

    for (SpotShadowTarget &target : spot_shadow_targets) {
        BufferRenderTarget::destroy_target_2d(rtg, target.depth_target);
    }
    spot_shadow_targets.clear();

    if (spot_shadow_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(rtg.device, spot_shadow_sampler, nullptr);
        spot_shadow_sampler = VK_NULL_HANDLE;
    }

    for (SphereShadowTarget &target : sphere_shadow_targets) {
        BufferRenderTarget::destroy_target_cube(rtg, target.depth_target);
    }
    sphere_shadow_targets.clear();

    if (sphere_shadow_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(rtg.device, sphere_shadow_sampler, nullptr);
        sphere_shadow_sampler = VK_NULL_HANDLE;
    }
}

ShadowBufferManager::~ShadowBufferManager() {
    for (SunShadowTarget const &target : sun_shadow_targets) {
        if (target.depth_target.array_view != VK_NULL_HANDLE) {
            std::cerr << "ShadowBufferManager: sun shadow depth_array_view not destroyed" << std::endl;
        }
        if (target.depth_target.image.handle != VK_NULL_HANDLE) {
            std::cerr << "ShadowBufferManager: sun shadow depth_array_image not destroyed" << std::endl;
        }
    }

    if (sun_shadow_sampler != VK_NULL_HANDLE) {
        std::cerr << "ShadowBufferManager: sun_shadow_sampler not destroyed" << std::endl;
    }

    for (SpotShadowTarget const &target : spot_shadow_targets) {
        if (target.depth_target.framebuffer != VK_NULL_HANDLE) {
            std::cerr << "ShadowBufferManager: spot shadow framebuffer not destroyed" << std::endl;
        }
        if (target.depth_target.view != VK_NULL_HANDLE) {
            std::cerr << "ShadowBufferManager: spot shadow depth_image_view not destroyed" << std::endl;
        }
        if (target.depth_target.image.handle != VK_NULL_HANDLE) {
            std::cerr << "ShadowBufferManager: spot shadow depth_image not destroyed" << std::endl;
        }
    }

    if (spot_shadow_sampler != VK_NULL_HANDLE) {
        std::cerr << "ShadowBufferManager: spot_shadow_sampler not destroyed" << std::endl;
    }

    for (SphereShadowTarget const &target : sphere_shadow_targets) {
        if (target.depth_target.cube_view != VK_NULL_HANDLE) {
            std::cerr << "ShadowBufferManager: sphere shadow depth_cube_view not destroyed" << std::endl;
        }
        if (target.depth_target.image.handle != VK_NULL_HANDLE) {
            std::cerr << "ShadowBufferManager: sphere shadow depth_cube_image not destroyed" << std::endl;
        }
    }

    if (sphere_shadow_sampler != VK_NULL_HANDLE) {
        std::cerr << "ShadowBufferManager: sphere_shadow_sampler not destroyed" << std::endl;
    }
}
