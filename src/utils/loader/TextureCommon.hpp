#pragma once

#include "VK.hpp"

#include <stdexcept>
#include <string>

inline void decode_rgbe(const unsigned char* src, float* dst){
    unsigned char r = src[0];
    unsigned char g = src[1];
    unsigned char b = src[2];
    unsigned char e = src[3];
    if (r == 0 && g == 0 && b == 0 && e == 0){
        dst[0] = 0.0f;
        dst[1] = 0.0f;
        dst[2] = 0.0f;
        dst[3] = 1.0f;
        return;
    };

	int exp = int(e) - 128;
	dst[0] = std::ldexp((r + 0.5f) / 256.0f, exp);
	dst[1] = std::ldexp((g + 0.5f) / 256.0f, exp);
	dst[2] = std::ldexp((b + 0.5f) / 256.0f, exp);
	dst[3] = 1.0f;
}

inline VkSampler create_sampler(
    VkDevice device,
    VkFilter filter,
    VkSamplerAddressMode address_mode_u,
    VkSamplerAddressMode address_mode_v,
    VkSamplerAddressMode address_mode_w,
    VkBorderColor border_color,
    float max_lod
) {
    VkSamplerCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = filter,
        .minFilter = filter,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = address_mode_u,
        .addressModeV = address_mode_v,
        .addressModeW = address_mode_w,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = max_lod,
        .borderColor = border_color,
        .unnormalizedCoordinates = VK_FALSE,
    };

    VkSampler sampler;
    VK( vkCreateSampler(device, &create_info, nullptr, &sampler) );
    return sampler;
}

inline VkImageView create_image_view(
    VkDevice device,
    VkImage image,
    VkFormat format,
    bool cube
) {
    VkImageViewCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = image,
        .viewType = cube ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = cube ? 6u : 1u,
        },
    };

    VkImageView image_view;
    VK( vkCreateImageView(device, &create_info, nullptr, &image_view) );
    return image_view;
}
