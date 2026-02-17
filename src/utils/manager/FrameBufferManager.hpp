#pragma once

#include "RTG.hpp"
#include "VK.hpp"
#include "RenderPassManager.hpp"

class FrameBufferManager {
    public:
        // HDR render target (for scene rendering)
        Helpers::AllocatedImage hdr_color_image;
        VkImageView hdr_color_image_view = VK_NULL_HANDLE;
        Helpers::AllocatedImage hdr_depth_image;
        VkImageView hdr_depth_image_view = VK_NULL_HANDLE;
        VkFramebuffer hdr_framebuffer = VK_NULL_HANDLE;
        VkSampler hdr_sampler = VK_NULL_HANDLE;

        // Swapchain framebuffers (for tone mapping pass, no depth)
        std::vector< VkFramebuffer > swapchain_framebuffers;

        void create(RTG &rtg, RTG::SwapchainEvent const &swapchain, RenderPassManager &render_pass_manager);
        void destroy(RTG &rtg);

        FrameBufferManager() = default;
        ~FrameBufferManager();
};