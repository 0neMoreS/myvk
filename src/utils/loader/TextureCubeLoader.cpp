#include "TextureCubeLoader.hpp"

#include "VK.hpp"
#include "RTG.hpp"

#include <stb_image.h>

#include <vector>
#include <stdexcept>
#include <cstring>

namespace TextureCubeLoader {

namespace {

VkFormat default_format() {
    return VK_FORMAT_R8G8B8A8_UNORM;
}

VkImageView create_cube_image_view(
    VkDevice device,
    VkImage image,
    VkFormat format
) {
    VkImageViewCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_CUBE,
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
            .layerCount = 6,
        },
    };

    VkImageView image_view;
    VK( vkCreateImageView(device, &create_info, nullptr, &image_view) );
    return image_view;
}

VkSampler create_sampler(VkDevice device, VkFilter filter) {
    VkSamplerCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = filter,
        .minFilter = filter,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };

    VkSampler sampler;
    VK( vkCreateSampler(device, &create_info, nullptr, &sampler) );
    return sampler;
}

// Copy a square tile from (src_w x src_h) image into dest buffer (face_w x face_h)
void blit_tile_rgba8(
    const unsigned char* src,
    int src_w,
    int src_h,
    int tile_x,
    int tile_y,
    int tile_w,
    int tile_h,
    unsigned char* dst
) {
    const int channels = 4;
    for (int y = 0; y < tile_h; ++y) {
        const unsigned char* src_row = src + ((tile_y + y) * src_w + tile_x) * channels;
        unsigned char* dst_row = dst + (y * tile_w) * channels;
        std::memcpy(dst_row, src_row, static_cast<size_t>(tile_w) * channels);
    }
}

} // namespace

std::shared_ptr<Texture> load_from_png_atlas(
    Helpers &helpers,
    const std::string &filepath,
    VkFilter filter
) {
    int img_w, img_h, channels;
    stbi_set_flip_vertically_on_load(false); // keep as-is for atlas
    unsigned char* pixels = stbi_load(filepath.c_str(), &img_w, &img_h, &channels, 4);
    if (!pixels) {
        std::string msg = std::string("Failed to load image: ") + filepath;
        if (stbi_failure_reason()) msg += std::string(" - ") + stbi_failure_reason();
        throw std::runtime_error(msg);
    }
    channels = 4; // forced

    if (img_h % 6 != 0) {
        stbi_image_free(pixels);
        throw std::runtime_error("Atlas height must be divisible by 6 (single column of faces).");
    }

    const int face_w = img_w;
    const int face_h = img_h / 6;

    if (face_w != face_h) {
        stbi_image_free(pixels);
        throw std::runtime_error("Cubemap faces must be square.");
    }

    const size_t face_bytes = static_cast<size_t>(face_w) * static_cast<size_t>(face_h) * 4;
    std::vector<unsigned char> faces(face_bytes * 6);

    auto copy_face = [&](uint32_t face_index, int tile_index) {
        const int tx = 0;
        const int ty = tile_index * face_h;
        unsigned char* dst = faces.data() + static_cast<size_t>(face_index) * face_bytes;
        blit_tile_rgba8(pixels, img_w, img_h, tx, ty, face_w, face_h, dst);
    };

    // Atlas is a vertical strip: top->bottom tiles map directly to Vulkan face order PX, NX, PY, NY, PZ, NZ
    // If your atlas uses a different order, adjust the mapping here.
    const int tile_for_face[6] = { 0, 1, 2, 3, 4, 5 };
    for (uint32_t f = 0; f < 6u; ++f) copy_face(f, tile_for_face[f]);

    // Create GPU cubemap image
    auto texture = std::make_shared<Texture>();
    texture->image = helpers.create_image(
        VkExtent2D{ .width = static_cast<uint32_t>(face_w), .height = static_cast<uint32_t>(face_h) },
        default_format(),
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        Helpers::Unmapped,
        true  // is_cube = true
    );

    helpers.transfer_to_image(faces.data(), faces.size(), texture->image, 6);

    texture->image_view = create_cube_image_view(helpers.rtg.device, texture->image.handle, default_format());
    texture->sampler = create_sampler(helpers.rtg.device, filter);

    stbi_image_free(pixels);
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
