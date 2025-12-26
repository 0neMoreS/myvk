#include "CommonLayouts.hpp"

void CommonLayouts::create(RTG& rtg) {
    { // World UBO layout (vertex+fragment)
        std::array< VkDescriptorSetLayoutBinding, 1 > bindings{
			VkDescriptorSetLayoutBinding{
				.binding = 0,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_VERTEX_BIT
			},
		};
		
		VkDescriptorSetLayoutCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = uint32_t(bindings.size()),
			.pBindings = bindings.data(),
		};

		VK( vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &pv_matrix) );
    }

    { // Cubemap sampler layout (fragment)
        std::array< VkDescriptorSetLayoutBinding, 1 > bindings{
			VkDescriptorSetLayoutBinding{
				.binding = 0,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
			},
		};
		
		VkDescriptorSetLayoutCreateInfo create_info{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = uint32_t(bindings.size()),
			.pBindings = bindings.data(),
		};

		VK( vkCreateDescriptorSetLayout(rtg.device, &create_info, nullptr, &cubemap) );
    }

    pipeline_name_to_index["CommonLayouts"] = 0;
}

void CommonLayouts::destroy(RTG& rtg) {
    VkDevice device = rtg.device;

    if (pv_matrix != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, pv_matrix, nullptr);
        pv_matrix = VK_NULL_HANDLE;
    }

    if (cubemap != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, cubemap, nullptr);
        cubemap = VK_NULL_HANDLE;
    }
}
