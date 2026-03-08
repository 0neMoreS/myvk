#include "ShadowMapManager.hpp"

#include <algorithm>
#include <array>
#include <iostream>

void ShadowMapManager::create(
    RTG &rtg,
    RenderPassManager &render_pass_manager,
    std::vector<LightsManager::SunLight> const &shadow_sun_lights,
    std::vector<LightsManager::SphereLight> const &shadow_sphere_lights,
    std::vector<LightsManager::SpotLight> const &shadow_spot_lights
) {
    destroy(rtg);

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

        target.depth_array_image = rtg.helpers.create_image(
            extent,
            render_pass_manager.depth_format,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            Helpers::Unmapped,
            0,
            1,
            SunCascadeCount
        );

        VkImageViewCreateInfo depth_array_view_create_info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = target.depth_array_image.handle,
            .viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
            .format = render_pass_manager.depth_format,
            .subresourceRange{
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = SunCascadeCount,
            },
        };
        VK(vkCreateImageView(rtg.device, &depth_array_view_create_info, nullptr, &target.depth_array_view));

        for (uint32_t cascade = 0; cascade < SunCascadeCount; ++cascade) {
            VkImageViewCreateInfo cascade_view_create_info{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = target.depth_array_image.handle,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = render_pass_manager.depth_format,
                .subresourceRange{
                    .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = cascade,
                    .layerCount = 1,
                },
            };
            VK(vkCreateImageView(rtg.device, &cascade_view_create_info, nullptr, &target.cascade_image_views[cascade]));

            std::array<VkImageView, 1> shadow_attachments{ target.cascade_image_views[cascade] };
            VkFramebufferCreateInfo shadow_framebuffer_create_info{
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass = render_pass_manager.spot_shadow_render_pass,
                .attachmentCount = uint32_t(shadow_attachments.size()),
                .pAttachments = shadow_attachments.data(),
                .width = resolution,
                .height = resolution,
                .layers = 1,
            };
            VK(vkCreateFramebuffer(rtg.device, &shadow_framebuffer_create_info, nullptr, &target.cascade_framebuffers[cascade]));
        }

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

    // Build one depth cubemap target per shadow sphere light.
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

        target.depth_cube_image = rtg.helpers.create_image(
            extent,
            render_pass_manager.depth_format,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            Helpers::Unmapped,
            VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
            1,
            SphereFaceCount
        );

        VkImageViewCreateInfo cube_view_create_info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = target.depth_cube_image.handle,
            .viewType = VK_IMAGE_VIEW_TYPE_CUBE,
            .format = render_pass_manager.depth_format,
            .subresourceRange{
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = SphereFaceCount,
            },
        };
        VK(vkCreateImageView(rtg.device, &cube_view_create_info, nullptr, &target.depth_cube_view));

        for (uint32_t face = 0; face < SphereFaceCount; ++face) {
            VkImageViewCreateInfo face_view_create_info{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = target.depth_cube_image.handle,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = render_pass_manager.depth_format,
                .subresourceRange{
                    .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = face,
                    .layerCount = 1,
                },
            };
            VK(vkCreateImageView(rtg.device, &face_view_create_info, nullptr, &target.face_image_views[face]));

            std::array<VkImageView, 1> shadow_attachments{ target.face_image_views[face] };
            VkFramebufferCreateInfo shadow_framebuffer_create_info{
                .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .renderPass = render_pass_manager.spot_shadow_render_pass,
                .attachmentCount = uint32_t(shadow_attachments.size()),
                .pAttachments = shadow_attachments.data(),
                .width = resolution,
                .height = resolution,
                .layers = 1,
            };
            VK(vkCreateFramebuffer(rtg.device, &shadow_framebuffer_create_info, nullptr, &target.face_framebuffers[face]));
        }

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

        target.depth_image = rtg.helpers.create_image(
            extent,
            render_pass_manager.depth_format,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            Helpers::Unmapped,
            0,
            1,
            1
        );

        VkImageViewCreateInfo depth_view_create_info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = target.depth_image.handle,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = render_pass_manager.depth_format,
            .subresourceRange{
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            },
        };

        VK( vkCreateImageView(rtg.device, &depth_view_create_info, nullptr, &target.depth_image_view) );

        std::array< VkImageView, 1 > shadow_attachments{
            target.depth_image_view,
        };

        VkFramebufferCreateInfo shadow_framebuffer_create_info{
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = render_pass_manager.spot_shadow_render_pass,
            .attachmentCount = uint32_t(shadow_attachments.size()),
            .pAttachments = shadow_attachments.data(),
            .width = resolution,
            .height = resolution,
            .layers = 1,
        };

        VK( vkCreateFramebuffer(rtg.device, &shadow_framebuffer_create_info, nullptr, &target.framebuffer) );

        spot_shadow_targets.emplace_back(std::move(target));
    }
}

void ShadowMapManager::destroy(RTG &rtg) {
    for (SunShadowTarget &target : sun_shadow_targets) {
        for (VkFramebuffer &cascade_framebuffer : target.cascade_framebuffers) {
            if (cascade_framebuffer != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(rtg.device, cascade_framebuffer, nullptr);
                cascade_framebuffer = VK_NULL_HANDLE;
            }
        }

        for (VkImageView &cascade_image_view : target.cascade_image_views) {
            if (cascade_image_view != VK_NULL_HANDLE) {
                vkDestroyImageView(rtg.device, cascade_image_view, nullptr);
                cascade_image_view = VK_NULL_HANDLE;
            }
        }

        if (target.depth_array_view != VK_NULL_HANDLE) {
            vkDestroyImageView(rtg.device, target.depth_array_view, nullptr);
            target.depth_array_view = VK_NULL_HANDLE;
        }

        if (target.depth_array_image.handle != VK_NULL_HANDLE) {
            rtg.helpers.destroy_image(std::move(target.depth_array_image));
        }
    }

    sun_shadow_targets.clear();

    if (sun_shadow_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(rtg.device, sun_shadow_sampler, nullptr);
        sun_shadow_sampler = VK_NULL_HANDLE;
    }

    for (SpotShadowTarget &target : spot_shadow_targets) {
        if (target.framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(rtg.device, target.framebuffer, nullptr);
            target.framebuffer = VK_NULL_HANDLE;
        }

        if (target.depth_image_view != VK_NULL_HANDLE) {
            vkDestroyImageView(rtg.device, target.depth_image_view, nullptr);
            target.depth_image_view = VK_NULL_HANDLE;
        }

        if (target.depth_image.handle != VK_NULL_HANDLE) {
            rtg.helpers.destroy_image(std::move(target.depth_image));
        }
    }

    spot_shadow_targets.clear();

    if (spot_shadow_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(rtg.device, spot_shadow_sampler, nullptr);
        spot_shadow_sampler = VK_NULL_HANDLE;
    }

    for (SphereShadowTarget &target : sphere_shadow_targets) {
        for (VkFramebuffer &face_framebuffer : target.face_framebuffers) {
            if (face_framebuffer != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(rtg.device, face_framebuffer, nullptr);
                face_framebuffer = VK_NULL_HANDLE;
            }
        }

        for (VkImageView &face_image_view : target.face_image_views) {
            if (face_image_view != VK_NULL_HANDLE) {
                vkDestroyImageView(rtg.device, face_image_view, nullptr);
                face_image_view = VK_NULL_HANDLE;
            }
        }

        if (target.depth_cube_view != VK_NULL_HANDLE) {
            vkDestroyImageView(rtg.device, target.depth_cube_view, nullptr);
            target.depth_cube_view = VK_NULL_HANDLE;
        }

        if (target.depth_cube_image.handle != VK_NULL_HANDLE) {
            rtg.helpers.destroy_image(std::move(target.depth_cube_image));
        }
    }

    sphere_shadow_targets.clear();

    if (sphere_shadow_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(rtg.device, sphere_shadow_sampler, nullptr);
        sphere_shadow_sampler = VK_NULL_HANDLE;
    }
}

ShadowMapManager::~ShadowMapManager() {
    for (SunShadowTarget const &target : sun_shadow_targets) {
        if (target.depth_array_view != VK_NULL_HANDLE) {
            std::cerr << "ShadowMapManager: sun shadow depth_array_view not destroyed" << std::endl;
        }
        if (target.depth_array_image.handle != VK_NULL_HANDLE) {
            std::cerr << "ShadowMapManager: sun shadow depth_array_image not destroyed" << std::endl;
        }
        for (VkImageView cascade_image_view : target.cascade_image_views) {
            if (cascade_image_view != VK_NULL_HANDLE) {
                std::cerr << "ShadowMapManager: sun shadow cascade_image_view not destroyed" << std::endl;
                break;
            }
        }
        for (VkFramebuffer cascade_framebuffer : target.cascade_framebuffers) {
            if (cascade_framebuffer != VK_NULL_HANDLE) {
                std::cerr << "ShadowMapManager: sun shadow cascade_framebuffer not destroyed" << std::endl;
                break;
            }
        }
    }

    if (sun_shadow_sampler != VK_NULL_HANDLE) {
        std::cerr << "ShadowMapManager: sun_shadow_sampler not destroyed" << std::endl;
    }

    for (SpotShadowTarget const &target : spot_shadow_targets) {
        if (target.framebuffer != VK_NULL_HANDLE) {
            std::cerr << "ShadowMapManager: spot shadow framebuffer not destroyed" << std::endl;
        }
        if (target.depth_image_view != VK_NULL_HANDLE) {
            std::cerr << "ShadowMapManager: spot shadow depth_image_view not destroyed" << std::endl;
        }
        if (target.depth_image.handle != VK_NULL_HANDLE) {
            std::cerr << "ShadowMapManager: spot shadow depth_image not destroyed" << std::endl;
        }
    }

    if (spot_shadow_sampler != VK_NULL_HANDLE) {
        std::cerr << "ShadowMapManager: spot_shadow_sampler not destroyed" << std::endl;
    }

    for (SphereShadowTarget const &target : sphere_shadow_targets) {
        if (target.depth_cube_view != VK_NULL_HANDLE) {
            std::cerr << "ShadowMapManager: sphere shadow depth_cube_view not destroyed" << std::endl;
        }
        if (target.depth_cube_image.handle != VK_NULL_HANDLE) {
            std::cerr << "ShadowMapManager: sphere shadow depth_cube_image not destroyed" << std::endl;
        }
        for (VkImageView face_image_view : target.face_image_views) {
            if (face_image_view != VK_NULL_HANDLE) {
                std::cerr << "ShadowMapManager: sphere shadow face_image_view not destroyed" << std::endl;
                break;
            }
        }
        for (VkFramebuffer face_framebuffer : target.face_framebuffers) {
            if (face_framebuffer != VK_NULL_HANDLE) {
                std::cerr << "ShadowMapManager: sphere shadow face_framebuffer not destroyed" << std::endl;
                break;
            }
        }
    }

    if (sphere_shadow_sampler != VK_NULL_HANDLE) {
        std::cerr << "ShadowMapManager: sphere_shadow_sampler not destroyed" << std::endl;
    }
}
