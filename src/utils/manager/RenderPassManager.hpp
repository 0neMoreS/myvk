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
    VkRect2D get_shadow_scissor(uint32_t shadow_resolution) const;
    VkViewport get_shadow_viewport(uint32_t shadow_resolution) const;

    VkRenderPass render_pass = VK_NULL_HANDLE;

    // HDR render pass: scene -> HDR texture (with depth)
    VkRenderPass hdr_render_pass = VK_NULL_HANDLE;

    // GBuffer render pass: scene geometry -> deferred textures (with depth)
    VkRenderPass gbuffer_render_pass = VK_NULL_HANDLE;

    // AO render pass: fullscreen AO resolve -> AO texture (no depth)
    VkRenderPass ao_render_pass = VK_NULL_HANDLE;

    // Tone mapping render pass: HDR texture -> swapchain (no depth)
    VkRenderPass tonemap_render_pass = VK_NULL_HANDLE;

    // Spot shadow render pass: depth-only rendering for spot light shadow maps
    VkRenderPass spot_shadow_render_pass = VK_NULL_HANDLE;

    VkFormat hdr_format = VK_FORMAT_R16G16B16A16_SFLOAT;
    VkFormat depth_format = VK_FORMAT_UNDEFINED;
    VkFormat albedo_format = VK_FORMAT_R8G8B8A8_UNORM;
    VkFormat normal_format = VK_FORMAT_R16G16B16A16_SFLOAT;

    std::array<VkClearValue, 2> clears;
    VkClearAttachment clear_center_attachment;
    VkClearRect clear_center_rect;

    std::array< VkClearValue, 1 > tonemap_clears;
    std::array< VkClearValue, 3 > gbuffer_clears;
    std::array< VkClearValue, 1 > ao_clears;

    VkRect2D scissor;
    VkViewport viewport;
    VkRect2D full_scissor;
    VkViewport full_viewport;
};