#pragma once

#include "RTG.hpp"
#include "VK.hpp"
#include "RenderPassManager.hpp"
#include "LightsManager.hpp"
#include "RenderTarget.hpp"

#include <array>
#include <vector>

class ShadowBufferManager {
public:
    static constexpr uint32_t SunCascadeCount = 4;
    static constexpr uint32_t SphereFaceCount = 6;

    VkFormat depth_format = VK_FORMAT_UNDEFINED;
    VkClearValue shadow_clear_value{};

    struct SunShadowTarget {
        uint32_t resolution = 0;
        BufferRenderTarget::TargetArray depth_target;
    };

    struct SpotShadowTarget {
        uint32_t resolution = 0;
        BufferRenderTarget::Target2D depth_target;
    };

    struct SphereShadowTarget {
        uint32_t resolution = 0;
        BufferRenderTarget::TargetCube depth_target;
    };

    std::vector<SunShadowTarget> sun_shadow_targets;
    VkSampler sun_shadow_sampler = VK_NULL_HANDLE;

    std::vector<SpotShadowTarget> spot_shadow_targets;
    VkSampler spot_shadow_sampler = VK_NULL_HANDLE;

    std::vector<SphereShadowTarget> sphere_shadow_targets;
    VkSampler sphere_shadow_sampler = VK_NULL_HANDLE;

    void create(
        RTG &rtg,
        RenderPassManager &render_pass_manager,
        std::vector<LightsManager::SunLight> const &shadow_sun_lights,
        std::vector<LightsManager::SphereLight> const &shadow_sphere_lights,
        std::vector<LightsManager::SpotLight> const &shadow_spot_lights
    );

    void destroy(RTG &rtg);

    ShadowBufferManager() = default;
    ~ShadowBufferManager();
};
