#pragma once

#include "VK.hpp"

#include <vulkan/vulkan.h>
#include <array>
#include <iostream>

struct RTG;

class RenderPassManager {
public:
    RenderPassManager() = default;
    ~RenderPassManager();

    void create(RTG& rtg, float aspect);
    void destroy(RTG& rtg);

    void update_scissor_and_viewport(RTG& rtg, VkExtent2D const& extent, float aspect);

    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkFormat depth_format = VK_FORMAT_UNDEFINED;

    std::array<VkClearValue, 2> clears;
    VkClearAttachment clear_center_attachment;
    VkClearRect clear_center_rect;

    VkRect2D scissor;
    VkViewport viewport;
};