#pragma once

#include "Pipeline.hpp"
#include "RTG.hpp"
#include "Vertex.hpp"
#include "TextureManager.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>

struct A1LinesPipeline : Pipeline {
    // type definitions
    VkDescriptorSetLayout set0_PV = VK_NULL_HANDLE;

    void create(
		RTG &, 
		VkRenderPass render_pass, 
		uint32_t subpass,
		const TextureManager& texture_manager
	) override;
    void destroy(RTG &rtg) override;

    A1LinesPipeline() = default;
    ~A1LinesPipeline();
};