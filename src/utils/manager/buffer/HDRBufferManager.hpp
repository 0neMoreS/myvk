#pragma once

#include "RTG.hpp"
#include "VK.hpp"
#include "RenderPassManager.hpp"
#include "RenderTarget.hpp"

#include <array>
#include <vector>

class HDRBufferManager {
public:
    VkFormat hdr_format = VK_FORMAT_R16G16B16A16_SFLOAT;
    VkFormat depth_format = VK_FORMAT_UNDEFINED;

    std::array<VkClearValue, 2> clears;
    std::array<VkClearValue, 1> tonemap_clears;
    VkClearAttachment clear_center_attachment{};

    VkRect2D scissor{};
    VkViewport viewport{};
    VkRect2D full_scissor{};
    VkViewport full_viewport{};

    // Scene HDR color target and scene depth target
    BufferRenderTarget::Target2D hdr_color_target;
    BufferRenderTarget::Target2D depth_target;

    VkSampler hdr_sampler = VK_NULL_HANDLE;

    // Swapchain framebuffers (tone mapping or direct rendering)
    std::vector<VkFramebuffer> swapchain_framebuffers;

    void create(RTG &rtg, RenderPassManager &render_pass_manager, bool use_hdr_tonemap);
    void on_swapchain(RTG &rtg, RenderPassManager &render_pass_manager, RTG::SwapchainEvent const &swapchain);
    void update_scissor_and_viewport(VkExtent2D const& extent, float aspect);
    void destroy(RTG &rtg);

    HDRBufferManager() = default;
    ~HDRBufferManager();

private:
    bool use_hdr_tonemap_ = true;
};
