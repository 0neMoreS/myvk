#pragma once

#include "RTG.hpp"
#include "VK.hpp"

#include <array>
#include <vector>

namespace BufferRenderTarget {

struct Target2D {
    Helpers::AllocatedImage image;
    VkImageView view = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
};

struct TargetArray {
    Helpers::AllocatedImage image;
    VkImageView array_view = VK_NULL_HANDLE;
    std::vector<VkImageView> layer_views;
    std::vector<VkFramebuffer> layer_framebuffers;
};

struct TargetCube {
    Helpers::AllocatedImage image;
    VkImageView cube_view = VK_NULL_HANDLE;
    std::array<VkImageView, 6> face_views{};
    std::array<VkFramebuffer, 6> face_framebuffers{};
};

Target2D create_target_2d(
    RTG &rtg,
    VkExtent2D const &extent,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImageAspectFlags aspect,
    VkRenderPass render_pass = VK_NULL_HANDLE,
    VkImageCreateFlags image_create_flags = 0
);

TargetArray create_target_array(
    RTG &rtg,
    VkExtent2D const &extent,
    uint32_t layer_count,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImageAspectFlags aspect,
    VkRenderPass render_pass = VK_NULL_HANDLE,
    VkImageCreateFlags image_create_flags = 0
);

TargetCube create_target_cube(
    RTG &rtg,
    VkExtent2D const &extent,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImageAspectFlags aspect,
    VkRenderPass render_pass = VK_NULL_HANDLE
);

void destroy_target_2d(RTG &rtg, Target2D &target);
void destroy_target_array(RTG &rtg, TargetArray &target);
void destroy_target_cube(RTG &rtg, TargetCube &target);

VkFramebuffer create_framebuffer(
    RTG &rtg,
    VkRenderPass render_pass,
    VkExtent2D const &extent,
    std::vector<VkImageView> const &attachments
);

} // namespace BufferRenderTarget
