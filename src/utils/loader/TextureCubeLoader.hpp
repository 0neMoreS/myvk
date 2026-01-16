#pragma once

#include "Helpers.hpp"
#include "TextureCommon.hpp"

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
enum Face : size_t{
    PX = 0,
    NX = 1,
    PY = 2,
    NY = 3,
    PZ = 4,
    NZ = 5
};

// PNG atlas order: +X, -X, +Y, -Y, +Z, -Z (top to bottom)
// Maps directly to Vulkan cubemap face order: PX, NX, PY, NY, PZ, NZ
// const std::pair<Face, size_t> tile_for_vulkan_face[6] = {
//     {PX, 270}, // tile 0 (+X)  -> Vulkan layer 0 (PX)
//     {NX, 90},
//     {PZ, 0},
//     {NZ, 180},
//     {NY, 0},
//     {PY, 180},
// };

const std::pair<Face, size_t> tile_for_vulkan_face[6] = {
    {PX, 90}, // tile 0 (+X)  -> Vulkan layer 0 (PX)
    {NX, 270},
    {NZ, 180},
    {PZ, 0},
    {PY, 0},
    {NY, 180},
};

// Load a cubemap from a single PNG atlas containing 6 square faces stacked vertically
std::unique_ptr<Texture> load_from_png_atlas(
    Helpers &helpers,
    const std::string &filepath,
    VkFilter filter = VK_FILTER_LINEAR
);

std::unique_ptr<Texture> load_from_png_atlas(
    Helpers &helpers,
    const std::string &filepath,
    VkFilter filter = VK_FILTER_LINEAR,
    uint32_t mipmap_levels = 1
);

void destroy(std::unique_ptr<Texture> texture, RTG &rtg);

} // namespace TextureCubeLoader
