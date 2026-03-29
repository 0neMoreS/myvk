#pragma once

#include "RTG.hpp"
#include "VK.hpp"
#include "RenderPassManager.hpp"

class FrameBufferManager {
    public:
        // HDR render target
        Helpers::AllocatedImage hdr_color_image;
        VkImageView hdr_color_image_view = VK_NULL_HANDLE;
        VkFramebuffer hdr_framebuffer = VK_NULL_HANDLE;
        VkSampler hdr_sampler = VK_NULL_HANDLE;

        // Deferred GBuffer render target
        Helpers::AllocatedImage gbuffer_position_depth_image;
        Helpers::AllocatedImage gbuffer_normal_image;
        Helpers::AllocatedImage gbuffer_albedo_image;
        Helpers::AllocatedImage gbuffer_pbr_image;
        VkImageView gbuffer_position_depth_view = VK_NULL_HANDLE;
        VkImageView gbuffer_normal_view = VK_NULL_HANDLE;
        VkImageView gbuffer_albedo_view = VK_NULL_HANDLE;
        VkImageView gbuffer_pbr_view = VK_NULL_HANDLE;
        VkFramebuffer gbuffer_framebuffer = VK_NULL_HANDLE;
        VkSampler gbuffer_sampler = VK_NULL_HANDLE;

        // Depth image for HDR render target
        Helpers::AllocatedImage depth_image;
        VkImageView depth_image_view = VK_NULL_HANDLE;

        // Swapchain framebuffers (for tone mapping pass, no depth)
        std::vector< VkFramebuffer > swapchain_framebuffers;

        void create(RTG &rtg, RTG::SwapchainEvent const &swapchain, RenderPassManager &render_pass_manager, bool use_hdr_tonemap);
        void destroy(RTG &rtg);

        FrameBufferManager() = default;
        ~FrameBufferManager();
};