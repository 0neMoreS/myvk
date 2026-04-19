#pragma once

#include "RTG.hpp"
#include "VK.hpp"
#include "RenderPassManager.hpp"
#include "RenderTarget.hpp"

#include <vector>

class HDRBufferManager {
public:
    // Scene HDR color target and scene depth target
    BufferRenderTarget::Target2D hdr_color_target;
    BufferRenderTarget::Target2D depth_target;

    VkSampler hdr_sampler = VK_NULL_HANDLE;

    // Swapchain framebuffers (tone mapping or direct rendering)
    std::vector<VkFramebuffer> swapchain_framebuffers;

    void create(RTG &rtg, RenderPassManager &render_pass_manager, bool use_hdr_tonemap);
    void on_swapchain(RTG &rtg, RenderPassManager &render_pass_manager, RTG::SwapchainEvent const &swapchain);
    void destroy(RTG &rtg);

    HDRBufferManager() = default;
    ~HDRBufferManager();

private:
    bool use_hdr_tonemap_ = true;
};
