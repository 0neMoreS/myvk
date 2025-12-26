#pragma once

#include "Pipeline.hpp"
#include "RTG.hpp"
#include "Vertex.hpp"

#include <glm/glm.hpp>

struct A2BackgroundPipeline : Pipeline {
    // External cubemap descriptor set layout (allocated elsewhere, not owned)
    VkDescriptorSetLayout set0_Transform = VK_NULL_HANDLE;
    VkDescriptorSetLayout set1_CUBEMAP = VK_NULL_HANDLE;

    struct Transform {
        glm::mat4 PERSPECTIVE;
        glm::mat4 VIEW;
    };
    static_assert(sizeof(Transform) == 16*4 + 16*4, "Transform is the expected size.");

    void create(RTG &rtg, VkRenderPass render_pass, uint32_t subpass) override;
    void destroy(RTG &rtg) override;

    A2BackgroundPipeline() = default;
    ~A2BackgroundPipeline() = default;
};
