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
    VkFilter filter
) {
    int width, height, channels;

    stbi_set_flip_vertically_on_load(false);
    unsigned char *pixel_data = stbi_load(
		filepath.c_str(),
		&width,
		&height,
		&channels,
		4
	);

    if (!pixel_data) {
        std::string error_msg = std::string("Failed to load image: ") + filepath;
		if (stbi_failure_reason()) {
			error_msg += std::string(" - ") + stbi_failure_reason();
		}
		throw std::runtime_error(error_msg);
    }

    const int face_w = width;
    const int face_h = height / 6;

    if (face_w != face_h) {
        stbi_image_free(pixel_data);
        throw std::runtime_error("Cubemap faces must be square.");
    }

    const size_t face_offset = static_cast<size_t>(face_w) * static_cast<size_t>(face_h) * 4UL;
    std::vector<float> rgba_data(face_offset * 6);

    for (int tile = 0; tile < 6; ++tile) {
        const int tx = 0;
        const int ty = tile_for_vulkan_face[tile].first * face_h;
        float* dst = rgba_data.data() + tile * face_offset;
        blit_tile_rgba8(pixel_data, width, height, tx, ty, face_w, face_h, dst, tile_for_vulkan_face[tile].second);
    }

    // Create GPU cubemap image
    auto texture = std::make_shared<Texture>();
    texture->image = helpers.create_image(
        VkExtent2D{ .width = static_cast<uint32_t>(face_w), .height = static_cast<uint32_t>(face_h) },
        VK_FORMAT_R32G32B32A32_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        Helpers::Unmapped,
        true  // is_cube = true
    );

    helpers.transfer_to_image(rgba_data.data(), rgba_data.size() * sizeof(float), texture->image, 6);

    texture->image_view = TextureCommon::create_image_view(helpers.rtg.device, texture->image.handle, VK_FORMAT_R32G32B32A32_SFLOAT, true);
    texture->sampler = TextureCommon::create_sampler(
        helpers.rtg.device,
        filter,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
    );

    stbi_image_free(pixel_data);
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
