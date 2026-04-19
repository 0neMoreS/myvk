#include "HDRBufferManager.hpp"

#include <array>
#include <cmath>
#include <iostream>

void HDRBufferManager::create(RTG &rtg, RenderPassManager &, bool use_hdr_tonemap) {
    destroy(rtg);
    use_hdr_tonemap_ = use_hdr_tonemap;

    if (depth_format == VK_FORMAT_UNDEFINED) {
        depth_format = rtg.helpers.find_image_format(
            { VK_FORMAT_D32_SFLOAT, VK_FORMAT_X8_D24_UNORM_PACK32 },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    clears = {
        VkClearValue{ .color{ .float32{63.0f / 255.0f, 63.0f / 255.0f, 63.0f / 255.0f, 1.0f} } },
        VkClearValue{ .depthStencil{ .depth = rtg.configuration.reverse_z ? 0.0f : 1.0f, .stencil = 0 } },
    };

    tonemap_clears = {
        VkClearValue{ .color{ .float32{63.0f / 255.0f, 63.0f / 255.0f, 63.0f / 255.0f, 1.0f} } },
    };

    clear_center_attachment = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .colorAttachment = 0,
        .clearValue = VkClearValue{ .color{ .float32{0.0f, 0.0f, 0.0f, 1.0f} } },
    };

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

void HDRBufferManager::on_swapchain(RTG &rtg, RenderPassManager &render_pass_manager, RTG::SwapchainEvent const &swapchain) {
    for (VkFramebuffer &framebuffer : swapchain_framebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(rtg.device, framebuffer, nullptr);
            framebuffer = VK_NULL_HANDLE;
        }
    }
    swapchain_framebuffers.clear();

    BufferRenderTarget::destroy_target_2d(rtg, depth_target);
    BufferRenderTarget::destroy_target_2d(rtg, hdr_color_target);

    depth_target = BufferRenderTarget::create_target_2d(
        rtg,
        swapchain.extent,
        depth_format,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        VK_NULL_HANDLE
    );

    if (use_hdr_tonemap_) {
        hdr_color_target = BufferRenderTarget::create_target_2d(
            rtg,
            swapchain.extent,
            hdr_format,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_NULL_HANDLE
        );

        std::vector<VkImageView> hdr_attachments{
            hdr_color_target.view,
            depth_target.view,
        };
        hdr_color_target.framebuffer = BufferRenderTarget::create_framebuffer(
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
                depth_target.view,
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

void HDRBufferManager::update_scissor_and_viewport(VkExtent2D const& extent, float aspect) {
    const float swap_aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);

    uint32_t w = extent.width;
    uint32_t h = extent.height;

    if (swap_aspect >= aspect) {
        w = static_cast<uint32_t>(std::round(h * aspect));
    } else {
        h = static_cast<uint32_t>(std::round(w / aspect));
    }

    int32_t offset_x = (static_cast<int32_t>(extent.width) - static_cast<int32_t>(w)) / 2;
    int32_t offset_y = (static_cast<int32_t>(extent.height) - static_cast<int32_t>(h)) / 2;

    scissor = {
        .offset = {.x = offset_x, .y = offset_y},
        .extent = VkExtent2D{w, h},
    };

    viewport = {
        .x = float(offset_x),
        .y = float(offset_y),
        .width = float(w),
        .height = float(h),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    full_scissor = {
        .offset = {.x = 0, .y = 0},
        .extent = VkExtent2D{extent.width, extent.height},
    };

    full_viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = float(extent.width),
        .height = float(extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
}

void HDRBufferManager::destroy(RTG &rtg) {
    for (VkFramebuffer &framebuffer : swapchain_framebuffers) {
        if (framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(rtg.device, framebuffer, nullptr);
            framebuffer = VK_NULL_HANDLE;
        }
    }
    swapchain_framebuffers.clear();

    BufferRenderTarget::destroy_target_2d(rtg, hdr_color_target);
    BufferRenderTarget::destroy_target_2d(rtg, depth_target);

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

    if (hdr_color_target.framebuffer != VK_NULL_HANDLE) {
        std::cerr << "HDRBufferManager: hdr_framebuffer not destroyed" << std::endl;
    }
    if (hdr_color_target.view != VK_NULL_HANDLE) {
        std::cerr << "HDRBufferManager: hdr_color_image_view not destroyed" << std::endl;
    }
    if (depth_target.view != VK_NULL_HANDLE) {
        std::cerr << "HDRBufferManager: depth_image_view not destroyed" << std::endl;
    }
    if (hdr_sampler != VK_NULL_HANDLE) {
        std::cerr << "HDRBufferManager: hdr_sampler not destroyed" << std::endl;
    }
}
