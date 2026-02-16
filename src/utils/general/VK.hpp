#pragma once

#include <vulkan/vk_enum_string_helper.h>

#include <glm/glm.hpp>

#include <stdexcept>
#include <unordered_map>
#include <iostream>
#include <iomanip>

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

#define MAX_TEXTURES 128

// Coordinate system conversion matrix: Blender -> Vulkan
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

// Coordinate system conversion matrix: Vulkan -> Blender
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

inline std::string s72_dir = "./external/s72/examples/";

inline void print_mat4(const glm::mat4& m, const std::string& name = "Matrix") {
    std::cout << name << ":" << std::endl;
    for (int i = 0; i < 4; ++i) {
        std::cout << "| ";
        for (int j = 0; j < 4; ++j) {
            // GLM stores matrices in column-major order (m[col][row]), 
            // but we usually want to print them in row-major order for readability.
            std::cout << std::setw(10) << std::fixed << std::setprecision(4) << m[j][i] << " ";
        }
        std::cout << "|" << std::endl;
    }
    std::cout << std::endl;
}

enum TextureSlot : size_t {
    Normal = 0,
    Displacement = 1,
    Albedo = 2,
    Roughness = 3,
    Metallic = 4
};

inline std::unordered_map<std::string, uint32_t> pipeline_name_to_index; //map pipeline names to indices
