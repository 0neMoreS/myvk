#pragma once

#include <vulkan/vk_enum_string_helper.h>

#include <glm/glm.hpp>

#include <stdexcept>
#include <unordered_map>

#define VK( FN ) \
	if (VkResult result = FN; result != VK_SUCCESS) { \
		throw std::runtime_error("Call '" #FN "' returned " + std::to_string(result) + " [" + std::string(string_VkResult(result)) + "]." ); \
	}

// Unified error handling macro - prints to stderr and throws exception
#define S72_ERROR(ctx, msg) \
	do { \
		std::string error_msg = std::string(ctx).empty() ? std::string(msg) : (std::string(ctx) + ": " + std::string(msg)); \
		std::cerr << "\033[1;31m[S72 ERROR]\033[0m " << error_msg << std::endl; \
		throw std::runtime_error(error_msg); \
	} while(0)

// Coordinate system conversion matrix: Blender → Vulkan
static const glm::mat3 BLENDER_TO_VULKAN_3 = glm::mat3(
    0.0f,  0.0f,  -1.0f,
    1.0f,  0.0f,  0.0f,
    0.0f, -1.0f,  0.0f
);

static const glm::mat4 BLENDER_TO_VULKAN_4 = glm::mat4(
    0.0f,  0.0f,  -1.0f, 0.0f,
    1.0f,  0.0f,  0.0f, 0.0f,
    0.0f, -1.0f,  0.0f, 0.0f,
    0.0f,  0.0f,  0.0f, 1.0f
);

// Coordinate system conversion matrix: Vulkan → Blender
static const glm::mat3 VULKAN_TO_BLENDER_3 = glm::mat3(
    0.0f,  1.0f,  0.0f,
    0.0f,  0.0f, -1.0f,
   -1.0f,  0.0f,  0.0f
);

static const glm::mat4 VULKAN_TO_BLENDER_4 = glm::mat4(
    0.0f,  1.0f,  0.0f, 0.0f,
    0.0f,  0.0f, -1.0f, 0.0f,
   -1.0f,  0.0f,  0.0f, 0.0f,
    0.0f,  0.0f,  0.0f, 1.0f
);

static std::string s72_dir = "./external/s72/examples/";

enum TextureSlot : size_t {
        Diffuse = 0,
        Normal = 1,
        Roughness = 2,
        Metallic = 3
};

static std::unordered_map<std::string, uint32_t> pipeline_name_to_index; //map pipeline names to indices
