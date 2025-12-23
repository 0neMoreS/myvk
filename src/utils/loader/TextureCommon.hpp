#pragma once

#include "Helpers.hpp"

namespace TextureCommon {

VkFormat channel_count_to_format(int channels);

VkSampler create_sampler(
    VkDevice device,
    VkFilter filter,
    VkSamplerAddressMode address_mode_u,
    VkSamplerAddressMode address_mode_v,
    VkSamplerAddressMode address_mode_w
);

VkImageView create_image_view(
    VkDevice device,
    VkImage image,
    VkFormat format,
    bool cube
);

} // namespace TextureCommon
