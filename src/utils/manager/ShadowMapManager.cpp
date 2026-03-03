#include "ShadowMapManager.hpp"

#include <array>
#include <iostream>

void ShadowMapManager::create(
    RTG &rtg,
    RenderPassManager &render_pass_manager,
    std::vector<A3CommonData::SpotLight> const &shadow_spot_lights
) {
    destroy(rtg);

    VkSamplerCreateInfo sampler_info{
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
    VK(vkCreateSampler(rtg.device, &sampler_info, nullptr, &spot_shadow_sampler));

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
            Helpers::Unmapped
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
}

ShadowMapManager::~ShadowMapManager() {
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
}
