#pragma once

#include "Helpers.hpp"
#include "glm/glm.hpp"

namespace TextureCommon {

void decode_rgbe(const unsigned char* src, float* dst);

VkSampler create_sampler(
    VkDevice device,
    VkFilter filter,
    VkSamplerAddressMode address_mode_u,
    VkSamplerAddressMode address_mode_v,
    VkSamplerAddressMode address_mode_w,
    VkBorderColor border_color
);

VkImageView create_image_view(
    VkDevice device,
    VkImage image,
    VkFormat format,
    bool cube
);

} // namespace TextureCommon
