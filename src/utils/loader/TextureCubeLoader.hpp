#pragma once

#include "Helpers.hpp"

#include <string>
#include <memory>
#include <cstdint>

namespace TextureCubeLoader {

struct Texture {
    Helpers::AllocatedImage image;
    VkImageView image_view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;

    Texture() = default;
    ~Texture();
};

// Order of cubemap faces expected by Vulkan
// +X, -X, +Y, -Y, +Z, -Z
enum Face : uint32_t {
    PX = 0,
    NX = 1,
    PY = 2,
    NY = 3,
    PZ = 4,
    NZ = 5
};

// Load a cubemap from a single PNG atlas containing 6 square faces stacked vertically
// (top->bottom). Faces are uploaded in Vulkan order PX,NX,PY,NY,PZ,NZ.
std::shared_ptr<Texture> load_from_png_atlas(
    Helpers &helpers,
    const std::string &filepath,
    VkFilter filter = VK_FILTER_LINEAR
);

void destroy(const std::shared_ptr<Texture> &texture, RTG &rtg);

} // namespace TextureCubeLoader
