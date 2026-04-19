#pragma once

#include "RTG.hpp"
#include "VK.hpp"
#include "RenderPassManager.hpp"
#include "RenderTarget.hpp"

#include <vector>

class HDRBufferManager {
public:
    // HDR render target
    Helpers::AllocatedImage hdr_color_image;
    VkImageView hdr_color_image_view = VK_NULL_HANDLE;
    VkFramebuffer hdr_framebuffer = VK_NULL_HANDLE;
    VkSampler hdr_sampler = VK_NULL_HANDLE;

    // Depth image for scene render target
    Helpers::AllocatedImage depth_image;
    VkImageView depth_image_view = VK_NULL_HANDLE;

    // Swapchain framebuffers (tone mapping or direct rendering)
    std::vector<VkFramebuffer> swapchain_framebuffers;

    void create(RTG &rtg, RenderPassManager &render_pass_manager, bool use_hdr_tonemap);
    void on_swapchain(RTG &rtg, RTG::SwapchainEvent const &swapchain, RenderPassManager &render_pass_manager);
    void destroy(RTG &rtg);

    HDRBufferManager() = default;
    ~HDRBufferManager();

private:
    bool use_hdr_tonemap_ = true;
};
