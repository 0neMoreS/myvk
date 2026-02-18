#pragma once

#include "VK.hpp"

#include <stdexcept>
#include <string>
#include <cmath>
#include <algorithm>

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

inline void encode_rgbe(float r, float g, float b, unsigned char* dst) {
    float max_c = std::max({r, g, b});
    if (max_c < 1e-32f) {
        dst[0] = dst[1] = dst[2] = dst[3] = 0;
        return;
    }
    int exp;
    float m = std::frexp(max_c, &exp); // max_c = m * 2^exp, m in [0.5, 1.0)
    float scale = m * 256.0f / max_c;  // = 256 / 2^exp
    dst[0] = static_cast<unsigned char>(std::min(r * scale, 255.0f));
    dst[1] = static_cast<unsigned char>(std::min(g * scale, 255.0f));
    dst[2] = static_cast<unsigned char>(std::min(b * scale, 255.0f));
    dst[3] = static_cast<unsigned char>(exp + 128);
}

inline uint32_t pack_e5b9g9r9(float r, float g, float b) {
    if (!std::isfinite(r) || !std::isfinite(g) || !std::isfinite(b)) {
        return 0u;
    }

    r = std::max(0.0f, r);
    g = std::max(0.0f, g);
    b = std::max(0.0f, b);

    constexpr float max_rgb9e5 = 65408.0f;
    r = std::min(r, max_rgb9e5);
    g = std::min(g, max_rgb9e5);
    b = std::min(b, max_rgb9e5);

    float max_c = std::max(r, std::max(g, b));
    if (max_c < 1.52587890625e-5f) {
        return 0u;
    }

    int exp_shared = static_cast<int>(std::floor(std::log2(max_c)));
    exp_shared = std::max(-15, exp_shared) + 1;
    exp_shared = std::min(16, exp_shared);

    float denom = std::ldexp(1.0f, exp_shared - 9);
    uint32_t r9 = static_cast<uint32_t>(std::lround(r / denom));
    uint32_t g9 = static_cast<uint32_t>(std::lround(g / denom));
    uint32_t b9 = static_cast<uint32_t>(std::lround(b / denom));

    if (exp_shared < 16 && (r9 > 511u || g9 > 511u || b9 > 511u)) {
        exp_shared++;
        denom *= 2.0f;
        r9 = static_cast<uint32_t>(std::lround(r / denom));
        g9 = static_cast<uint32_t>(std::lround(g / denom));
        b9 = static_cast<uint32_t>(std::lround(b / denom));
    }

    r9 = std::min(r9, 511u);
    g9 = std::min(g9, 511u);
    b9 = std::min(b9, 511u);

    uint32_t e = static_cast<uint32_t>(exp_shared + 15);
    return (e << 27) | (b9 << 18) | (g9 << 9) | r9;
}

// Copy a square tile from (src_w x src_h) image into dest buffer (face_w x face_h)
inline void blit_tile_rgba8(
    const unsigned char* src,
    int src_w,
    int src_h,
    int tile_x,
    int tile_y,
    int tile_w,
    int tile_h,
    uint32_t* dst,
    int rotate_deg
) {
    const int channels = 4;
    for (int y = 0; y < tile_h; ++y) {
        for (int x = 0; x < tile_w; ++x) {
            int sx = tile_x + x;
            int sy = tile_y + y;
            switch (rotate_deg) {
                case 90:  // CW
                    sx = tile_x + (tile_w - 1 - y);
                    sy = tile_y + x;
                    break;
                case 180:
                    sx = tile_x + (tile_w - 1 - x);
                    sy = tile_y + (tile_h - 1 - y);
                    break;
                case 270: // CCW
                    sx = tile_x + y;
                    sy = tile_y + (tile_h - 1 - x);
                    break;
                default:
                    break;
            }
            const unsigned char* src_px = src + (sy * src_w + sx) * channels;
            float decoded[4];
            decode_rgbe(src_px, decoded);
            dst[y * tile_w + x] = pack_e5b9g9r9(decoded[0], decoded[1], decoded[2]);
        }
    }
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