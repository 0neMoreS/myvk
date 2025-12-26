#pragma once

#include <vulkan/vulkan.h>

#include "VK.hpp"
#include "RTG.hpp"
#include "Pipeline.hpp"

// Manages shared descriptor set layouts that are reused across pipelines.
struct CommonLayouts {
    VkDescriptorSetLayout pv_matrix = VK_NULL_HANDLE;   // set: camera/world UBO
    VkDescriptorSetLayout cubemap = VK_NULL_HANDLE; // set: samplerCube

    struct PV{
        glm::mat4 PERSPECTIVE;
        glm::mat4 VIEW;
    };

    static_assert(sizeof(PV) == 16*4 + 16*4, "PV is the expected size.");

    void create(RTG& rtg); 
    void destroy(RTG& rtg);

    ~CommonLayouts();
    CommonLayouts() = default;
};
