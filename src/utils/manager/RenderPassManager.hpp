#pragma once

#include "VK.hpp"

#include <vulkan/vulkan.h>
#include <vector>
#include <iostream>

class RTG;

class RenderPassManager {
public:
    RenderPassManager() = default;
    ~RenderPassManager();

    void create(RTG& rtg);
    void destroy(RTG& rtg);

    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkFormat depth_format = VK_FORMAT_UNDEFINED;

    std::array<VkClearValue, 2> clears;

    VkRect2D scissor;
    VkViewport viewport;
};