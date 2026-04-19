#pragma once

#include "VK.hpp"

#include <vulkan/vulkan.h>
#include <array>
#include <iostream>

struct RTG;
class HDRBufferManager;
struct GBufferManager;
class ShadowBufferManager;

class RenderPassManager {
public:
    RenderPassManager() = default;
    ~RenderPassManager();

    void create(
        RTG& rtg,
        HDRBufferManager const& hdr_buffer_manager,
        GBufferManager const* gbuffer_manager = nullptr,
        ShadowBufferManager const* shadow_buffer_manager = nullptr
    );
    void destroy(RTG& rtg);

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
    VkRenderPass shadow_render_pass = VK_NULL_HANDLE;

};