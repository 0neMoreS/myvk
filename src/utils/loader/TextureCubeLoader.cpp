#include "TextureCubeLoader.hpp"

#include "VK.hpp"
#include "RTG.hpp"


#include <stb_image.h>

#include <vector>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <cmath>

namespace TextureCubeLoader {

std::unique_ptr<Texture> load_cubemap(
    Helpers &helpers,
    const std::string &filepath,
    VkFilter filter,
    uint32_t mipmap_levels,
    bool generate_mipmaps
) {
    // Load all mipmap levels from files with suffix pattern (.1 through .5)
    std::vector<unsigned char*> pixel_data_levels;
    std::vector<int> widths, heights;

    stbi_set_flip_vertically_on_load(false);
    
    // Load + process mipmaps
    std::vector<std::vector<uint32_t>> all_mipmap_data;

    if(generate_mipmaps){
        int width, height, channels;
        unsigned char *pixel_data = stbi_load(
            filepath.c_str(),
            &width,
            &height,
            &channels,
            4
        );

        if (!pixel_data) {
            std::string error_msg = std::string("Failed to load cubemap base level: ") + filepath;
            if (stbi_failure_reason()) {
                error_msg += std::string(" - ") + stbi_failure_reason();
            }
            throw std::runtime_error(error_msg);
        }

        pixel_data_levels.push_back(pixel_data);

        const int base_face_w = width;
        const int base_face_h = height / 6;
        if (base_face_w != base_face_h) {
            stbi_image_free(pixel_data);
            throw std::runtime_error("Cubemap faces must be square (base level).");
        }

        const uint32_t max_mip_levels = helpers.calc_mip_levels(static_cast<uint32_t>(base_face_w), static_cast<uint32_t>(base_face_h));
        mipmap_levels = max_mip_levels;

        widths.push_back(width);
        heights.push_back(height);

        all_mipmap_data.resize(1);
        {
            const int face_w = widths[0];
            const int face_h = heights[0] / 6;
            const size_t face_offset = static_cast<size_t>(face_w) * static_cast<size_t>(face_h);
            all_mipmap_data[0].resize(face_offset * 6);

            for (int tile = 0; tile < 6; ++tile) {
                const int tx = 0;
                const int ty = static_cast<int>(tile_for_vulkan_face[tile].first) * face_h;
                uint32_t* dst = all_mipmap_data[0].data() + tile * face_offset;
                blit_tile_rgba8(
                    pixel_data_levels[0],
                    widths[0],
                    heights[0],
                    tx,
                    ty,
                    face_w,
                    face_h,
                    dst,
                    static_cast<int>(tile_for_vulkan_face[tile].second)
                );
            }
        }
    } else {
        all_mipmap_data.resize(mipmap_levels);

        for (uint32_t level = 0; level < mipmap_levels; ++level) {
            std::string level_filepath;
            if (mipmap_levels == 1) {
                level_filepath = filepath;
            } else {
                level_filepath = filepath.substr(0, filepath.find_last_of('.')) + "." + std::to_string(level + 1) + ".png";
            }

            int width, height, channels;
            unsigned char *pixel_data = stbi_load(
                level_filepath.c_str(),
                &width,
                &height,
                &channels,
                4
            );

            if (!pixel_data) {
                std::string error_msg = std::string("Failed to load mipmap level ") + std::to_string(level + 1) + ": " + level_filepath;
                if (stbi_failure_reason()) {
                    error_msg += std::string(" - ") + stbi_failure_reason();
                }
                for (auto* data : pixel_data_levels) stbi_image_free(data);
                throw std::runtime_error(error_msg);
            }

            pixel_data_levels.push_back(pixel_data);

            const int face_w = width;
            const int face_h = height / 6;

            if (face_w != face_h) {
                stbi_image_free(pixel_data);
                for (auto* data : pixel_data_levels) stbi_image_free(data);
                throw std::runtime_error("Cubemap faces must be square (level " + std::to_string(level) + ").");
            }

            if (level == 0) {
                widths.push_back(width);
                heights.push_back(height);
            } else {
                const int expected_face_w = std::max(1, (widths[0]) >> level);
                const int expected_face_h = std::max(1, (heights[0] / 6) >> level);
                const int expected_width  = expected_face_w;
                const int expected_height = expected_face_h * 6;

                if (width != expected_width || height != expected_height) {
                    stbi_image_free(pixel_data);
                    for (auto* data : pixel_data_levels) stbi_image_free(data);
                    throw std::runtime_error(
                        "Mipmap level " + std::to_string(level) +
                        " has incorrect dimensions. Expected " +
                        std::to_string(expected_width) + "x" + std::to_string(expected_height));
                }

                widths.push_back(width);
                heights.push_back(height);
            }

            const size_t face_offset = static_cast<size_t>(face_w) * static_cast<size_t>(face_h);
            all_mipmap_data[level].resize(face_offset * 6);

            for (int tile = 0; tile < 6; ++tile) {
                const int tx = 0;
                const int ty = static_cast<int>(tile_for_vulkan_face[tile].first) * face_h;
                uint32_t* dst = all_mipmap_data[level].data() + tile * face_offset;
                blit_tile_rgba8(pixel_data_levels[level], widths[level], heights[level],
                               tx, ty, face_w, face_h, dst, static_cast<int>(tile_for_vulkan_face[tile].second));
            }
        }
    }
    
    // Create GPU cubemap image with mipmaps
    auto texture = std::make_unique<Texture>();
    texture->image = helpers.create_image(
        VkExtent2D{ .width = static_cast<uint32_t>(widths[0]), .height = static_cast<uint32_t>(heights[0] / 6) },
        VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
            (generate_mipmaps ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0),
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        Helpers::Unmapped,
        true,  // is_cube = true
        mipmap_levels
    );
    
    // Transfer each mipmap level to GPU
    std::vector<void*> mipmap_ptrs;
    std::vector<size_t> mipmap_byte_sizes;
    
    const uint32_t upload_levels = generate_mipmaps ? 1u : mipmap_levels;
    for (uint32_t level = 0; level < upload_levels; ++level) {
        mipmap_ptrs.push_back(all_mipmap_data[level].data());
        mipmap_byte_sizes.push_back(all_mipmap_data[level].size() * sizeof(uint32_t));
    }

    helpers.transfer_to_image(mipmap_ptrs, mipmap_byte_sizes, texture->image, 6, generate_mipmaps);
    
    texture->image_view = create_image_view(
        helpers.rtg.device, texture->image.handle, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, true, mipmap_levels
    );
    texture->sampler = create_sampler(
        helpers.rtg.device,
        filter,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        static_cast<float>(mipmap_levels - 1)
    );
    
    // Free all loaded pixel data
    for (auto* data : pixel_data_levels) {
        stbi_image_free(data);
    }
    
    return texture;
}

std::unique_ptr<Texture> create_default_cubemap(
    Helpers &helpers,
    VkFilter filter
) {
    const size_t pixel_count_per_face = 1;
    const size_t face_count = 6;
    std::vector<uint32_t> cubemap_data(pixel_count_per_face * face_count, pack_e5b9g9r9(0.0f, 0.0f, 0.0f));
    
    // Create GPU cubemap image (single mipmap level)
    auto texture = std::make_unique<Texture>();
    texture->image = helpers.create_image(
        VkExtent2D{ .width = 1, .height = 1 },
        VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        Helpers::Unmapped,
        true,  // is_cube = true
        1      // mipmap_levels = 1
    );
    
    // 2. Transfer data
    // Pass a pointer containing data for all 6 faces
    std::vector<void*> mipmap_ptrs(1, cubemap_data.data());
    
    // The size must be the total of all 6 faces, so the created Staging Buffer is large enough (96 bytes instead of 16 bytes)
    std::vector<size_t> mipmap_byte_sizes(1, cubemap_data.size() * sizeof(uint32_t));
    
    helpers.transfer_to_image(mipmap_ptrs, mipmap_byte_sizes, texture->image, face_count);
    
    texture->image_view = create_image_view(
        helpers.rtg.device, texture->image.handle, VK_FORMAT_E5B9G9R9_UFLOAT_PACK32, true
    );
    texture->sampler = create_sampler(
        helpers.rtg.device,
        filter,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        0.0f
    );
    
    return texture;
}

void destroy(std::unique_ptr<Texture> texture, RTG &rtg) {
    if (!texture) return;

    if (texture->sampler != VK_NULL_HANDLE) {
        vkDestroySampler(rtg.helpers.rtg.device, texture->sampler, nullptr);
        texture->sampler = VK_NULL_HANDLE;
    }
    if (texture->image_view != VK_NULL_HANDLE) {
        vkDestroyImageView(rtg.helpers.rtg.device, texture->image_view, nullptr);
        texture->image_view = VK_NULL_HANDLE;
    }
    if (texture->image.handle != VK_NULL_HANDLE) {
        rtg.helpers.destroy_image(std::move(texture->image));
    }
}

Texture::~Texture() {
    if (sampler != VK_NULL_HANDLE || image_view != VK_NULL_HANDLE) {
        std::cerr << "[TextureCubeLoader] Texture destructor called without destroy() being called" << std::endl;
    }
}

} // namespace TextureCubeLoader
