#pragma once

#include <vector>
#include <vulkan/vulkan.h>
#include "VK.hpp"


struct Pipeline
{
    struct BlockDescriptorConfig
    {
        VkDescriptorType type;
        VkDescriptorSetLayout layout;
    };

    struct TextureDescriptorConfig 
    {
        TextureSlot slot;
        VkDescriptorSetLayout layout;
    };

    VkPipelineLayout layout = VK_NULL_HANDLE;	
    VkPipeline pipeline = VK_NULL_HANDLE;

    std::vector<BlockDescriptorConfig> block_descriptor_configs{};
    std::vector<TextureDescriptorConfig> texture_descriptor_configs{};  // mapping texture slots to layouts

    virtual void create(class RTG &, VkRenderPass render_pass, uint32_t subpass) = 0;
    virtual void destroy(class RTG &) = 0;
};
