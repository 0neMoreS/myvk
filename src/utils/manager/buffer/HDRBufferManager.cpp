#include "HDRBufferManager.hpp"

#include <array>
#include <iostream>

void HDRBufferManager::create(RTG &rtg, RenderPassManager &, bool use_hdr_tonemap) {
    destroy(rtg);
    use_hdr_tonemap_ = use_hdr_tonemap;

    if (hdr_sampler == VK_NULL_HANDLE) {
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
            .minLod = 0.0f,
            .maxLod = 0.0f,
            .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
            .unnormalizedCoordinates = VK_FALSE,
        };

        VK(vkCreateSampler(rtg.device, &sampler_info, nullptr, &hdr_sampler));
    }
}

void HDRBufferManager::on_swapchain(RTG &rtg, RTG::SwapchainEvent const &swapchain, RenderPassManager &render_pass_manager) {
    for (VkFramebuffer &framebuffer : swapchain_framebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(rtg.device, framebuffer, nullptr);
            framebuffer = VK_NULL_HANDLE;
        }
    }
    swapchain_framebuffers.clear();

    BufferRenderTarget::Target2D depth_target{
        .image = std::move(depth_image),
        .view = depth_image_view,
        .framebuffer = VK_NULL_HANDLE,
    };
    BufferRenderTarget::destroy_target_2d(rtg, depth_target);
    depth_image = std::move(depth_target.image);
    depth_image_view = depth_target.view;

    BufferRenderTarget::Target2D hdr_target{
        .image = std::move(hdr_color_image),
        .view = hdr_color_image_view,
        .framebuffer = hdr_framebuffer,
    };
    BufferRenderTarget::destroy_target_2d(rtg, hdr_target);
    hdr_color_image = std::move(hdr_target.image);
    hdr_color_image_view = hdr_target.view;
    hdr_framebuffer = hdr_target.framebuffer;

    auto new_depth = BufferRenderTarget::create_target_2d(
        rtg,
        swapchain.extent,
        render_pass_manager.depth_format,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        VK_NULL_HANDLE
    );
    depth_image = std::move(new_depth.image);
    depth_image_view = new_depth.view;

    if (use_hdr_tonemap_) {
        auto new_hdr = BufferRenderTarget::create_target_2d(
            rtg,
            swapchain.extent,
            render_pass_manager.hdr_format,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_NULL_HANDLE
        );
        hdr_color_image = std::move(new_hdr.image);
        hdr_color_image_view = new_hdr.view;

        std::vector<VkImageView> hdr_attachments{
            hdr_color_image_view,
            depth_image_view,
        };
        hdr_framebuffer = BufferRenderTarget::create_framebuffer(
            rtg,
            render_pass_manager.hdr_render_pass,
            swapchain.extent,
            hdr_attachments
        );

        swapchain_framebuffers.assign(swapchain.image_views.size(), VK_NULL_HANDLE);
        for (size_t i = 0; i < swapchain.image_views.size(); ++i) {
            std::vector<VkImageView> attachments{ swapchain.image_views[i] };
            swapchain_framebuffers[i] = BufferRenderTarget::create_framebuffer(
                rtg,
                render_pass_manager.tonemap_render_pass,
                swapchain.extent,
                attachments
            );
        }
    } else {
        swapchain_framebuffers.assign(swapchain.image_views.size(), VK_NULL_HANDLE);
        for (size_t i = 0; i < swapchain.image_views.size(); ++i) {
            std::vector<VkImageView> attachments{
                swapchain.image_views[i],
                depth_image_view,
            };
            swapchain_framebuffers[i] = BufferRenderTarget::create_framebuffer(
                rtg,
                render_pass_manager.render_pass,
                swapchain.extent,
                attachments
            );
        }
    }
}

void HDRBufferManager::destroy(RTG &rtg) {
    for (VkFramebuffer &framebuffer : swapchain_framebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(rtg.device, framebuffer, nullptr);
            framebuffer = VK_NULL_HANDLE;
        }
    }
    swapchain_framebuffers.clear();

    BufferRenderTarget::Target2D hdr_target{
        .image = std::move(hdr_color_image),
        .view = hdr_color_image_view,
        .framebuffer = hdr_framebuffer,
    };
    BufferRenderTarget::destroy_target_2d(rtg, hdr_target);
    hdr_color_image = std::move(hdr_target.image);
    hdr_color_image_view = hdr_target.view;
    hdr_framebuffer = hdr_target.framebuffer;

    BufferRenderTarget::Target2D depth_target{
        .image = std::move(depth_image),
        .view = depth_image_view,
        .framebuffer = VK_NULL_HANDLE,
    };
    BufferRenderTarget::destroy_target_2d(rtg, depth_target);
    depth_image = std::move(depth_target.image);
    depth_image_view = depth_target.view;

    if (hdr_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(rtg.device, hdr_sampler, nullptr);
        hdr_sampler = VK_NULL_HANDLE;
    }
}

HDRBufferManager::~HDRBufferManager() {
    for (VkFramebuffer const &framebuffer : swapchain_framebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            std::cerr << "HDRBufferManager: swapchain framebuffer not destroyed" << std::endl;
            break;
        }
    }

    if (hdr_framebuffer != VK_NULL_HANDLE) {
        std::cerr << "HDRBufferManager: hdr_framebuffer not destroyed" << std::endl;
    }
    if (hdr_color_image_view != VK_NULL_HANDLE) {
        std::cerr << "HDRBufferManager: hdr_color_image_view not destroyed" << std::endl;
    }
    if (depth_image_view != VK_NULL_HANDLE) {
        std::cerr << "HDRBufferManager: depth_image_view not destroyed" << std::endl;
    }
    if (hdr_sampler != VK_NULL_HANDLE) {
        std::cerr << "HDRBufferManager: hdr_sampler not destroyed" << std::endl;
    }
}
