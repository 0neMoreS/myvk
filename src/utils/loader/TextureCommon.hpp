#pragma once

#include "Helpers.hpp"
#include "glm/glm.hpp"

namespace TextureCommon {

glm::vec3 decode_rgbe(const glm::u8vec4 &encoded);

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
