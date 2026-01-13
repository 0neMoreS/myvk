#include "TextureCubeLoader.hpp"

#include "VK.hpp"
#include "RTG.hpp"


#include <stb_image.h>

#include <vector>
#include <stdexcept>
#include <cstring>

namespace TextureCubeLoader {
using Face = TextureCubeLoader::Face;

namespace {
// Copy a square tile from (src_w x src_h) image into dest buffer (face_w x face_h)
void blit_tile_rgba8(
    const unsigned char* src,
    int src_w,
    int src_h,
    int tile_x,
    int tile_y,
    int tile_w,
    int tile_h,
    float* dst,
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
            float* dst_px = dst + (y * tile_w + x) * channels;
            TextureCommon::decode_rgbe(src_px, dst_px);
        }
    }
}

} // namespace

std::shared_ptr<Texture> load_from_png_atlas(
    Helpers &helpers,
    const std::string &filepath,
    VkFilter filter,
    uint32_t mipmap_levels
) {
    // Load all mipmap levels from files with suffix pattern (.1 through .5)
    std::vector<unsigned char*> pixel_data_levels;
    std::vector<int> widths, heights;

    stbi_set_flip_vertically_on_load(false);
    // Load mipmap levels [0 .. mipmap_levels-1]
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
            // Free previously loaded levels
            for (auto* data : pixel_data_levels) stbi_image_free(data);
            throw std::runtime_error(error_msg);
        }

        const int face_w = width;
        const int face_h = height / 6;

        if (face_w != face_h) {
            stbi_image_free(pixel_data);
            for (auto* data : pixel_data_levels) stbi_image_free(data);
            throw std::runtime_error("Cubemap faces must be square (level " + std::to_string(level) + ").");
        }

        if (level == 0) {
            // Record base atlas (face_w, face_h*6)
            widths.push_back(width);
            heights.push_back(height);
        } else {
            // Expect power-of-two downscale per level
            const int expected_face_w = std::max(1, (widths[0]) >> level);      // widths[0] == base face_w
            const int expected_face_h = std::max(1, (heights[0] / 6) >> level); // heights[0] == base atlas_h, so base face_h = heights[0]/6
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

        pixel_data_levels.push_back(pixel_data);
    }
    
    // Process all mipmap levels and prepare GPU texture
    std::vector<std::vector<float>> all_mipmap_data(mipmap_levels);
    
    for (uint32_t level = 0; level < mipmap_levels; ++level) {
        const int face_w = widths[level];
        const int face_h = heights[level] / 6;
        const size_t face_offset = static_cast<size_t>(face_w) * static_cast<size_t>(face_h) * 4UL;
        
        all_mipmap_data[level].resize(face_offset * 6);
        
        // Blit each of the 6 faces
        for (int tile = 0; tile < 6; ++tile) {
            const int tx = 0;
            const int ty = static_cast<int>(tile_for_vulkan_face[tile].first) * face_h;
            float* dst = all_mipmap_data[level].data() + tile * face_offset;
            blit_tile_rgba8(pixel_data_levels[level], widths[level], heights[level], 
                           tx, ty, face_w, face_h, dst, static_cast<int>(tile_for_vulkan_face[tile].second));
        }
    }
    
    // Create GPU cubemap image with mipmaps
    auto texture = std::make_shared<Texture>();
    texture->image = helpers.create_image(
        VkExtent2D{ .width = static_cast<uint32_t>(widths[0]), .height = static_cast<uint32_t>(heights[0] / 6) },
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        Helpers::Unmapped,
        true,  // is_cube = true
        mipmap_levels
    );
    
    // Transfer each mipmap level to GPU
    std::vector<void*> mipmap_ptrs;
    std::vector<size_t> mipmap_byte_sizes;
    
    for (uint32_t level = 0; level < mipmap_levels; ++level) {
        mipmap_ptrs.push_back(all_mipmap_data[level].data());
        mipmap_byte_sizes.push_back(all_mipmap_data[level].size() * sizeof(float));
    }
    
    helpers.transfer_to_image(mipmap_ptrs, mipmap_byte_sizes, texture->image, 6);
    
    texture->image_view = TextureCommon::create_image_view(
        helpers.rtg.device, texture->image.handle, VK_FORMAT_R32G32B32A32_SFLOAT, true
    );
    texture->sampler = TextureCommon::create_sampler(
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

void destroy(const std::shared_ptr<Texture> &texture, RTG &rtg) {
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
