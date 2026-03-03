#pragma once

#include "Pipeline.hpp"
#include "RTG.hpp"
#include "Vertex.hpp"

#include <iostream>

struct A3SpotShadowPipeline : Pipeline {
    VkDescriptorSetLayout set0_Global = VK_NULL_HANDLE;
    VkDescriptorSetLayout set1_Transforms = VK_NULL_HANDLE;

    struct Push {
        uint32_t LIGHT_INDEX;
    };

    void create(
        RTG &,
        VkRenderPass render_pass,
        uint32_t subpass,
        const ManagerContext& context
    ) override;

    void destroy(RTG &rtg) override;

    A3SpotShadowPipeline() = default;
    ~A3SpotShadowPipeline();
};
