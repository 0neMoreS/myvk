#pragma once

#include "Pipeline.hpp"
#include "RTG.hpp"
#include "Vertex.hpp"

#include <glm/glm.hpp>

struct A3BackgroundPipeline : Pipeline {
    // External cubemap descriptor set layout (allocated elsewhere, not owned)
    VkDescriptorSetLayout set0_PV = VK_NULL_HANDLE;
    VkDescriptorSetLayout set1_CUBEMAP = VK_NULL_HANDLE;
    VkDescriptorSet set1_CUBEMAP_instance = VK_NULL_HANDLE;

    void create(
		RTG &, 
		VkRenderPass render_pass, 
		uint32_t subpass,
		const TextureManager& texture_manager
	) override;
    void destroy(RTG &rtg) override;

    A3BackgroundPipeline() = default;
    ~A3BackgroundPipeline();
};
