#pragma once

#include "RTG.hpp"
#include "VK.hpp"
#include "RenderPassManager.hpp"
#include "A3CommonData.hpp"

class ShadowMapManager {
public:
    struct SpotShadowTarget {
        uint32_t resolution = 0;
        Helpers::AllocatedImage depth_image;
        VkImageView depth_image_view = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
    };

    std::vector< SpotShadowTarget > spot_shadow_targets;
    VkSampler spot_shadow_sampler = VK_NULL_HANDLE;

    void create(
        RTG &rtg,
        RenderPassManager &render_pass_manager,
        std::vector<A3CommonData::SpotLight> const &shadow_spot_lights
    );

    void destroy(RTG &rtg);

    ShadowMapManager() = default;
    ~ShadowMapManager();
};
