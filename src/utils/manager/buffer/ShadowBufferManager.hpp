#pragma once

#include "RTG.hpp"
#include "VK.hpp"
#include "RenderPassManager.hpp"
#include "LightsManager.hpp"

#include <array>
#include <vector>

class ShadowBufferManager {
public:
    static constexpr uint32_t SunCascadeCount = 4;
    static constexpr uint32_t SphereFaceCount = 6;

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

    struct SphereShadowTarget {
        uint32_t resolution = 0;
        Helpers::AllocatedImage depth_cube_image;
        VkImageView depth_cube_view = VK_NULL_HANDLE;
        std::array<VkImageView, SphereFaceCount> face_image_views{};
        std::array<VkFramebuffer, SphereFaceCount> face_framebuffers{};
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
