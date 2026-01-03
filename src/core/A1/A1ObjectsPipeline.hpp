#pragma once

#include "Pipeline.hpp"
#include "RTG.hpp"
#include "Vertex.hpp"
#include "TextureManager.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>

struct A1ObjectsPipeline : Pipeline {
    // type definitions
    VkDescriptorSetLayout set0_PV = VK_NULL_HANDLE;
    VkDescriptorSetLayout set1_Transforms = VK_NULL_HANDLE;
    VkDescriptorSetLayout set2_TEXTURE = VK_NULL_HANDLE;
    VkDescriptorSet set2_TEXTURE_instance = VK_NULL_HANDLE;

    struct PV {
        glm::mat4 PERSPECTIVE;
        glm::mat4 VIEW;
    };
    static_assert(sizeof(PV) == 16*4 + 16*4, "PV is the expected size.");

    //types for descriptors:
    struct Transform {
        glm::mat4 MODEL;
        glm::mat4 MODEL_NORMAL;
    };
    static_assert(sizeof(Transform) == 16*4 + 16*4, "Transform is the expected size.");

    struct Push{
        uint32_t MATERIAL_INDEX;
    };

    void create(
		class RTG &, 
		VkRenderPass render_pass, 
		uint32_t subpass,
		const TextureManager& texture_manager
	) override;
    void destroy(RTG &rtg) override;

    A1ObjectsPipeline() = default;
    ~A1ObjectsPipeline();
};