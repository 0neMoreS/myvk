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

    void create(RTG& rtg);
    void destroy(RTG& rtg);

    void update_scissor_and_viewport(RTG& rtg, VkExtent2D const& extent);

    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkFormat depth_format = VK_FORMAT_UNDEFINED;

    std::array<VkClearValue, 2> clears;

    VkRect2D scissor;
    VkViewport viewport;
};