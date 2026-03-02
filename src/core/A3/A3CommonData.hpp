#pragma once

#include <vulkan/vulkan.h>
#include "VK.hpp"
#include "glm/glm.hpp"
#include <cstdint>

namespace A3CommonData
{
    struct PV{
        glm::mat4 PERSPECTIVE;
        glm::mat4 VIEW;
        glm::vec4 CAMERA_POSITION;
    };

    static_assert(sizeof(PV) == 16*4 + 16*4 + 4*4, "PV is the expected size.");

    struct alignas(16) SunLight {
        float cascadeSplits[4];
        glm::mat4 orthographic[4];
        glm::vec3 direction;
        float angle;
        glm::vec3 tint;
        uint32_t shadow;
    };
    static_assert(sizeof(SunLight) == 304, "SunLight must match std430 layout.");

    struct alignas(16) SphereLight {
        glm::vec3 position;
        float radius;
        glm::vec3 tint;
        float limit;
    };
    static_assert(sizeof(SphereLight) == 32, "SphereLight must match std430 layout.");

    struct alignas(16) SpotLight {
        glm::mat4 perspective;
        glm::vec3 position;
        float radius;
        glm::vec3 direction;
        float fov;
        glm::vec3 tint;
        float blend;
        float limit;
        uint32_t shadow;
        uint32_t _pad_[2];
    };
    static_assert(sizeof(SpotLight) == 128, "SpotLight must match std430 layout.");

    struct alignas(16) LightsHeader {
        uint32_t count;
        uint32_t _pad_[3];
    };
    static_assert(sizeof(LightsHeader) == 16, "LightsHeader must match std430 layout.");

    inline VkDeviceSize sun_lights_buffer_size(uint32_t count) {
        return sizeof(LightsHeader) + sizeof(SunLight) * count;
    }

    inline VkDeviceSize sphere_lights_buffer_size(uint32_t count) {
        return sizeof(LightsHeader) + sizeof(SphereLight) * count;
    }

    inline VkDeviceSize spot_lights_buffer_size(uint32_t count) {
        return sizeof(LightsHeader) + sizeof(SpotLight) * count;
    }

    //types for descriptors:
    struct Transform {
        glm::mat4 MODEL;
        glm::mat4 MODEL_NORMAL;
    };
    static_assert(sizeof(Transform) == 16*4 + 16*4, "Transform is the expected size.");
} // namespace A3CommonData
