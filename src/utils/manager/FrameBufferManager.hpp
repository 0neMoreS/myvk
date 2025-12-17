#pragma once

#include "RTG.hpp"
#include "VK.hpp"
#include "RenderPassManager.hpp"

class FrameBufferManager {
    public:
       	Helpers::AllocatedImage swapchain_depth_image;
        VkImageView swapchain_depth_image_view = VK_NULL_HANDLE;
        std::vector< VkFramebuffer > swapchain_framebuffers; 

        void create(RTG &rtg, RTG::SwapchainEvent const &swapchain, RenderPassManager &render_pass_manager);
        void destroy(RTG &rtg);

        FrameBufferManager() = default;
        ~FrameBufferManager();
};