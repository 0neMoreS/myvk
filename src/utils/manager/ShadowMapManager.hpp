#pragma once

#include "RTG.hpp"
#include "VK.hpp"
#include "RenderPassManager.hpp"
#include "A3CommonData.hpp"

#include <array>

class ShadowMapManager {
public:
    static constexpr uint32_t SunCascadeCount = 4;

    struct SunShadowTarget {
        uint32_t resolution = 0;
        Helpers::AllocatedImage depth_array_image;
        VkImageView depth_array_view = VK_NULL_HANDLE;
        std::array<VkImageView, SunCascadeCount> cascade_image_views{};
        std::array<VkFramebuffer, SunCascadeCount> cascade_framebuffers{};
    };

    struct SpotShadowTarget {
        uint32_t resolution = 0;
        Helpers::AllocatedImage depth_image;
        VkImageView depth_image_view = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
    };

    std::vector< SunShadowTarget > sun_shadow_targets;
    VkSampler sun_shadow_sampler = VK_NULL_HANDLE;

    std::vector< SpotShadowTarget > spot_shadow_targets;
    VkSampler spot_shadow_sampler = VK_NULL_HANDLE;

    void create(
        RTG &rtg,
        RenderPassManager &render_pass_manager,
        std::vector<A3CommonData::SunLight> const &shadow_sun_lights,
        std::vector<A3CommonData::SpotLight> const &shadow_spot_lights
    );

    void create(
        RTG &rtg,
        RenderPassManager &render_pass_manager,
        std::vector<A3CommonData::SpotLight> const &shadow_spot_lights
    );

    void destroy(RTG &rtg);

    ShadowMapManager() = default;
    ~ShadowMapManager();
};
